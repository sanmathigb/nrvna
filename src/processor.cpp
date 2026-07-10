/*
 * nrvna ai - Asynchronous Inference Primitive
 * Copyright (c) 2025 Sanmathi Bharamgouda
 * SPDX-License-Identifier: MIT
 */

#include "nrvna/processor.hpp"
#include "nrvna/contract.hpp"
#include "nrvna/meta.hpp"
#include "nrvna/runner.hpp"
#include "nrvna/runner_tts.hpp"
#include "nrvna/logger.hpp"
#include "llama_util.hpp"
#include <chrono>
#include <cstdio>
#include <ctime>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <mutex>

namespace {
std::mutex g_output_mutex;

std::string timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    struct tm tm_buf;
    localtime_r(&time, &tm_buf);
    char buf[16];
    std::strftime(buf, sizeof(buf), "%H:%M:%S", &tm_buf);
    return buf;
}

void writeCompletionMeta(const std::filesystem::path& jobPath,
                         double elapsed_s,
                         const std::vector<std::string>& artifacts,
                         const std::string& status) {
    auto meta = nrvnaai::readMetaJson(jobPath).value_or(nrvnaai::JobMeta{});
    if (meta.submitted_at.empty()) {
        meta.submitted_at = nrvnaai::formatTimestamp();
    }
    if (meta.mode.empty()) {
        meta.mode = nrvnaai::contract::toString(nrvnaai::JobType::Text);
    }
    meta.completed_at = nrvnaai::formatTimestamp();
    meta.duration_s = elapsed_s;
    meta.artifacts = artifacts;
    meta.status = status;
    (void)nrvnaai::writeMetaJson(jobPath, meta);
}

static const char* kColorRunning  = "\033[33m"; // yellow
static const char* kColorDone     = "\033[32m"; // green
static const char* kColorFailed   = "\033[31m"; // red

void printJobStatus(const nrvnaai::JobId& id, const std::string& status, double elapsed = -1.0, const std::string& detail = "") {
    std::lock_guard<std::mutex> lock(g_output_mutex);
    const char* color = kColorRunning;
    if (status == nrvnaai::contract::toString(nrvnaai::Status::Done))   color = kColorDone;
    if (status == nrvnaai::contract::toString(nrvnaai::Status::Failed)) color = kColorFailed;

    std::cout << "    \033[90m" << timestamp() << "\033[0m  " << id << "  " << color << status << "\033[0m";
    if (elapsed >= 0.0) {
        char buf[16];
        std::snprintf(buf, sizeof(buf), "  %5.1fs", elapsed);
        std::cout << buf;
    }
    if (!detail.empty()) {
        std::cout << "  " << detail;
    }
    std::cout << "\n" << std::flush;
}

void completeJob(const std::filesystem::path& jobPath,
                 double elapsed,
                 const std::vector<std::string>& artifacts,
                 const std::string& status) {
    writeCompletionMeta(jobPath, elapsed, artifacts, status);
}

}

namespace nrvnaai {

Processor::Processor(const std::filesystem::path& workspace, const std::string& modelPath, const std::string& mmprojPath, const std::string& vocoderPath)
    : workspace_(workspace), modelPath_(modelPath), mmprojPath_(mmprojPath), vocoderPath_(vocoderPath) {
    LOG_DEBUG("Processor created for workspace: " + workspace_.string() + " with model: " + modelPath_);
}

ProcessResult Processor::process(const JobId& jobId, int workerId) noexcept {
    LOG_DEBUG("Processing job: " + jobId);

    try {
        // Step 1: Move from ready to processing (atomic)
        if (!moveReadyToProcessing(jobId)) {
            LOG_DEBUG("Job not found or already claimed by another worker: " + jobId);
            return ProcessResult::NotFound;
        }

        printJobStatus(jobId, contract::toString(Status::Running));
        auto startTime = std::chrono::steady_clock::now();

        // Step 2: Read prompt and route metadata
        PromptReadResult promptRead = readPrompt(jobId);
        JobType jobType = readJobType(jobId);
        std::vector<std::filesystem::path> imagePaths = readImages(jobId);
        std::vector<std::filesystem::path> audioPaths = readAudio(jobId);
        if (!promptRead.ok) {
            completeJob(getJobPath(contract::kProcessingDir, jobId), 0.0, {contract::kErrorFile}, contract::toString(Status::Failed));
            printJobStatus(jobId, contract::toString(Status::Failed), 0.0, "prompt read error");
            (void)finalizeFailure(jobId, promptRead.error);
            return ProcessResult::Failed;
        }

        const std::string& prompt = promptRead.content;
        const bool allowEmptyPrompt = prompt.empty() &&
            ((jobType == JobType::Embed && !imagePaths.empty()) || (jobType == JobType::Stt && !audioPaths.empty()));
        if (prompt.empty() && !allowEmptyPrompt) {
            completeJob(getJobPath(contract::kProcessingDir, jobId), 0.0, {contract::kErrorFile}, contract::toString(Status::Failed));
            printJobStatus(jobId, contract::toString(Status::Failed), 0.0, "empty prompt");
            (void)finalizeFailure(jobId, "Empty prompt");
            return ProcessResult::Failed;
        }

        // TTS dispatches to its own runner — no text Runner needed
        if (jobType == JobType::Tts) {
            if (vocoderPath_.empty()) {
                auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - startTime).count();
                completeJob(getJobPath(contract::kProcessingDir, jobId), elapsed, {contract::kErrorFile}, contract::toString(Status::Failed));
                printJobStatus(jobId, contract::toString(Status::Failed), elapsed);
                (void)finalizeFailure(jobId, "TTS requires --vocoder flag");
                return ProcessResult::Failed;
            }

            TtsRunner* ttsRunner = getTtsRunnerForWorker(workerId);
            if (!ttsRunner) {
                auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - startTime).count();
                completeJob(getJobPath(contract::kProcessingDir, jobId), elapsed, {contract::kErrorFile}, contract::toString(Status::Failed));
                printJobStatus(jobId, contract::toString(Status::Failed), elapsed, "no TTS runner");
                (void)finalizeFailure(jobId, "No TTS runner available");
                return ProcessResult::SystemError;
            }

            auto ttsResult = ttsRunner->run(prompt);
            auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - startTime).count();
            if (ttsResult.ok) {
                completeJob(getJobPath(contract::kProcessingDir, jobId), elapsed, {contract::kAudioFile}, contract::toString(Status::Done));
                if (finalizeAudio(jobId, ttsResult.audio, ttsResult.sample_rate)) {
                    printJobStatus(jobId, contract::toString(Status::Done), elapsed);
                    LOG_INFO("TTS COMPLETED: " + jobId + " -> " + std::to_string(ttsResult.audio.size()) + " samples");
                    return ProcessResult::Success;
                } else {
                    LOG_ERROR("Failed to finalize TTS job: " + jobId);
                    completeJob(getJobPath(contract::kProcessingDir, jobId), elapsed, {contract::kErrorFile}, contract::toString(Status::Failed));
                    if (!finalizeFailure(jobId, "Failed to write audio to output directory")) {
                        LOG_ERROR("STUCK JOB: " + jobId + " trapped in processing/ — manual intervention required");
                    }
                    return ProcessResult::SystemError;
                }
            } else {
                printJobStatus(jobId, contract::toString(Status::Failed), elapsed);
                completeJob(getJobPath(contract::kProcessingDir, jobId), elapsed, {contract::kErrorFile}, contract::toString(Status::Failed));
                (void)finalizeFailure(jobId, ttsResult.error);
                LOG_WARN("TTS job failed: " + jobId + " - " + ttsResult.error);
                return ProcessResult::Failed;
            }
        }

        // Text, embed, or vision — these all need the text Runner
        Runner* runner = getRunnerForWorker(workerId);
        if (!runner) {
            auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - startTime).count();
            completeJob(getJobPath(contract::kProcessingDir, jobId), elapsed, {contract::kErrorFile}, contract::toString(Status::Failed));
            printJobStatus(jobId, contract::toString(Status::Failed), elapsed, "no runner");
            (void)finalizeFailure(jobId, "No runner available");
            return ProcessResult::SystemError;
        }

        if (jobType == JobType::Stt) {
            auto sttResult = runner->transcribe(prompt, audioPaths);
            auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - startTime).count();
            if (sttResult.ok) {
                completeJob(getJobPath(contract::kProcessingDir, jobId), elapsed, {contract::kTranscriptFile}, contract::toString(Status::Done));
                if (finalizeTranscript(jobId, sttResult.output)) {
                    printJobStatus(jobId, contract::toString(Status::Done), elapsed);
                    LOG_INFO("STT COMPLETED: " + jobId + " -> " + std::to_string(sttResult.output.size()) + " chars");
                    return ProcessResult::Success;
                } else {
                    LOG_ERROR("Failed to finalize STT job: " + jobId);
                    completeJob(getJobPath(contract::kProcessingDir, jobId), elapsed, {contract::kErrorFile}, contract::toString(Status::Failed));
                    if (!finalizeFailure(jobId, "Failed to write transcript to output directory")) {
                        LOG_ERROR("STUCK JOB: " + jobId + " trapped in processing/ — manual intervention required");
                    }
                    return ProcessResult::SystemError;
                }
            } else {
                printJobStatus(jobId, contract::toString(Status::Failed), elapsed);
                completeJob(getJobPath(contract::kProcessingDir, jobId), elapsed, {contract::kErrorFile}, contract::toString(Status::Failed));
                (void)finalizeFailure(jobId, sttResult.error);
                LOG_WARN("STT job failed: " + jobId + " - " + sttResult.error);
                return ProcessResult::Failed;
            }
        }

        if (jobType == JobType::Embed) {
            auto embedResult = imagePaths.empty()
                ? runner->embed(prompt)
                : runner->embedVision(prompt, imagePaths);
            auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - startTime).count();
            if (embedResult.ok) {
                completeJob(getJobPath(contract::kProcessingDir, jobId), elapsed, {contract::kEmbeddingFile}, contract::toString(Status::Done));
                if (finalizeEmbedding(jobId, embedResult.embedding)) {
                    printJobStatus(jobId, contract::toString(Status::Done), elapsed);
                    LOG_INFO("EMBED COMPLETED: " + jobId + " -> " + std::to_string(embedResult.embedding.size()) + " dims");
                    return ProcessResult::Success;
                } else {
                    LOG_ERROR("Failed to finalize embedding job: " + jobId);
                    completeJob(getJobPath(contract::kProcessingDir, jobId), elapsed, {contract::kErrorFile}, contract::toString(Status::Failed));
                    if (!finalizeFailure(jobId, "Failed to write embedding to output directory")) {
                        LOG_ERROR("STUCK JOB: " + jobId + " trapped in processing/ — manual intervention required");
                    }
                    return ProcessResult::SystemError;
                }
            } else {
                printJobStatus(jobId, contract::toString(Status::Failed), elapsed);
                completeJob(getJobPath(contract::kProcessingDir, jobId), elapsed, {contract::kErrorFile}, contract::toString(Status::Failed));
                (void)finalizeFailure(jobId, embedResult.error);
                LOG_WARN("Embed job failed: " + jobId + " - " + embedResult.error);
                return ProcessResult::Failed;
            }
        }

        RunResult result;
        if (imagePaths.empty()) {
            result = runner->run(prompt);
        } else {
            result = runner->run(prompt, imagePaths);
        }

        auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - startTime).count();
        if (result.ok) {
            completeJob(getJobPath(contract::kProcessingDir, jobId), elapsed, {contract::kResultFile}, contract::toString(Status::Done));
            if (finalizeSuccess(jobId, result.output)) {
                printJobStatus(jobId, contract::toString(Status::Done), elapsed);
                LOG_INFO("JOB COMPLETED: " + jobId + " -> " + std::to_string(result.output.size()) + " chars");
                return ProcessResult::Success;
            } else {
                LOG_ERROR("Failed to finalize successful job: " + jobId);
                completeJob(getJobPath(contract::kProcessingDir, jobId), elapsed, {contract::kErrorFile}, contract::toString(Status::Failed));
                if (!finalizeFailure(jobId, "Failed to write result to output directory")) {
                    LOG_ERROR("STUCK JOB: " + jobId + " trapped in processing/ — manual intervention required");
                }
                return ProcessResult::SystemError;
            }
        } else {
            printJobStatus(jobId, contract::toString(Status::Failed), elapsed);
            completeJob(getJobPath(contract::kProcessingDir, jobId), elapsed, {contract::kErrorFile}, contract::toString(Status::Failed));
            (void)finalizeFailure(jobId, result.error);
            LOG_WARN("Job failed during inference: " + jobId + " - " + result.error);
            return ProcessResult::Failed;
        }
        
    } catch (const std::exception& e) {
        LOG_ERROR("Exception processing job " + jobId + ": " + std::string(e.what()));
        (void)finalizeFailure(jobId, "Internal processing error: " + std::string(e.what()));
        return ProcessResult::SystemError;
    } catch (...) {
        LOG_ERROR("Unknown exception processing job: " + jobId);
        (void)finalizeFailure(jobId, "Unknown internal processing error");
        return ProcessResult::SystemError;
    }
}

bool Processor::moveReadyToProcessing(const JobId& jobId) noexcept {
    try {
        auto readyPath = getJobPath(contract::kReadyDir, jobId);
        auto processingPath = getJobPath(contract::kProcessingDir, jobId);
        
        // Use std::error_code to handle race condition gracefully
        std::error_code ec;
        std::filesystem::rename(readyPath, processingPath, ec);
        
        if (ec) {
            // Another worker already claimed it, or job disappeared
            LOG_DEBUG("Job already claimed or missing: " + jobId);
            return false;
        }
        
        LOG_DEBUG("Job moved to processing: " + jobId);
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to move job to processing: " + std::string(e.what()));
        return false;
    } catch (...) {
        LOG_ERROR("Unknown error moving job to processing");
        return false;
    }
}

bool Processor::finalizeSuccess(const JobId& jobId, const std::string& result) noexcept {
    try {
        auto processingPath = getJobPath(contract::kProcessingDir, jobId);
        auto outputPath = getJobPath(contract::kOutputDir, jobId);
        
        // Write result to temporary file first
        auto tempResultPath = processingPath / (std::string(contract::kResultFile) + ".tmp");
        {
            std::ofstream file(tempResultPath, std::ios::binary);
            if (!file) return false;
            file << result;
            file.flush();
            if (!file.good()) return false;
        }
        
        // Rename temp file to final name
        auto finalResultPath = processingPath / contract::kResultFile;
        std::filesystem::rename(tempResultPath, finalResultPath);
        
        // Atomic move entire job to output
        std::filesystem::rename(processingPath, outputPath);
        
        LOG_DEBUG("Job finalized successfully: " + jobId);
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to finalize success for job " + jobId + ": " + std::string(e.what()));
        return false;
    } catch (...) {
        LOG_ERROR("Unknown error finalizing success for job: " + jobId);
        return false;
    }
}

bool Processor::finalizeEmbedding(const JobId& jobId, const std::vector<float>& embedding) noexcept {
    try {
        if (!std::all_of(embedding.begin(), embedding.end(), [](float value) {
                return std::isfinite(value);
            })) {
            LOG_ERROR("Refusing to write non-finite embedding for job: " + jobId);
            return false;
        }

        auto processingPath = getJobPath(contract::kProcessingDir, jobId);
        auto outputPath = getJobPath(contract::kOutputDir, jobId);

        // Write embedding as JSON
        auto tempPath = processingPath / (std::string(contract::kEmbeddingFile) + ".tmp");
        {
            std::ofstream file(tempPath, std::ios::binary);
            if (!file) return false;

            file << "{\n  \"dim\": " << embedding.size() << ",\n  \"vector\": [";
            for (size_t i = 0; i < embedding.size(); ++i) {
                if (i > 0) file << ", ";
                if (i % 10 == 0 && i > 0) file << "\n    ";
                file << embedding[i];
            }
            file << "\n  ]\n}\n";
            file.flush();
            if (!file.good()) return false;
        }

        // Rename temp to final
        auto finalPath = processingPath / contract::kEmbeddingFile;
        std::filesystem::rename(tempPath, finalPath);

        // Atomic move to output
        std::filesystem::rename(processingPath, outputPath);

        LOG_DEBUG("Embedding job finalized: " + jobId);
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to finalize embedding for job " + jobId + ": " + std::string(e.what()));
        return false;
    } catch (...) {
        LOG_ERROR("Unknown error finalizing embedding for job: " + jobId);
        return false;
    }
}

bool Processor::finalizeTranscript(const JobId& jobId, const std::string& transcript) noexcept {
    try {
        auto processingPath = getJobPath(contract::kProcessingDir, jobId);
        auto outputPath = getJobPath(contract::kOutputDir, jobId);

        auto tempPath = processingPath / (std::string(contract::kTranscriptFile) + ".tmp");
        {
            std::ofstream file(tempPath, std::ios::binary);
            if (!file) return false;
            file << transcript;
            if (!transcript.empty() && transcript.back() != '\n') {
                file << '\n';
            }
            file.flush();
            if (!file.good()) return false;
        }

        auto finalPath = processingPath / contract::kTranscriptFile;
        std::filesystem::rename(tempPath, finalPath);
        std::filesystem::rename(processingPath, outputPath);

        LOG_DEBUG("Transcript job finalized: " + jobId);
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to finalize transcript for job " + jobId + ": " + std::string(e.what()));
        return false;
    } catch (...) {
        LOG_ERROR("Unknown error finalizing transcript for job: " + jobId);
        return false;
    }
}

bool Processor::finalizeFailure(const JobId& jobId, const std::string& error) noexcept {
    try {
        auto processingPath = getJobPath(contract::kProcessingDir, jobId);
        auto failedPath = getJobPath(contract::kFailedDir, jobId);
        
        // Write error to file
        auto errorPath = processingPath / contract::kErrorFile;
        {
            std::ofstream file(errorPath, std::ios::binary);
            if (file) {
                file << error;
                file.flush();
            }
            // Continue even if error file write fails
        }
        
        // Atomic move to failed directory
        std::filesystem::rename(processingPath, failedPath);
        
        LOG_DEBUG("Job moved to failed: " + jobId);
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to finalize failure for job " + jobId + ": " + std::string(e.what()));
        return false;
    } catch (...) {
        LOG_ERROR("Unknown error finalizing failure for job: " + jobId);
        return false;
    }
}

std::vector<std::filesystem::path> Processor::readImages(const JobId& jobId) const noexcept {
    std::vector<std::filesystem::path> imagePaths;
    try {
        auto imagesDir = getJobPath(contract::kProcessingDir, jobId) / contract::kImagesDir;
        if (!std::filesystem::exists(imagesDir) || !std::filesystem::is_directory(imagesDir)) {
            return imagePaths;
        }

        for (const auto& entry : std::filesystem::directory_iterator(imagesDir)) {
            // Accept regular files or symlinks (symlinks used for local files)
            if (entry.is_regular_file() || entry.is_symlink()) {
                imagePaths.push_back(entry.path());
            }
        }

        std::sort(imagePaths.begin(), imagePaths.end());
    } catch (...) {
        return imagePaths;
    }
    return imagePaths;
}

std::vector<std::filesystem::path> Processor::readAudio(const JobId& jobId) const noexcept {
    std::vector<std::filesystem::path> audioPaths;
    try {
        auto audioDir = getJobPath(contract::kProcessingDir, jobId) / contract::kAudioInputDir;
        if (!std::filesystem::exists(audioDir) || !std::filesystem::is_directory(audioDir)) {
            return audioPaths;
        }

        for (const auto& entry : std::filesystem::directory_iterator(audioDir)) {
            if (entry.is_regular_file() || entry.is_symlink()) {
                audioPaths.push_back(entry.path());
            }
        }

        std::sort(audioPaths.begin(), audioPaths.end());
    } catch (...) {
        return audioPaths;
    }
    return audioPaths;
}

JobType Processor::readJobType(const JobId& jobId) const noexcept {
    try {
        auto typePath = getJobPath(contract::kProcessingDir, jobId) / contract::kTypeFile;

        if (!std::filesystem::exists(typePath)) {
            return JobType::Text;  // No type.txt means text
        }

        std::ifstream file(typePath, std::ios::binary);
        if (!file) {
            return JobType::Text;
        }

        std::string type;
        std::getline(file, type);
        return contract::parseJobType(type);
    } catch (...) {
        return JobType::Text;
    }
}

PromptReadResult Processor::readPrompt(const JobId& jobId) const noexcept {
    try {
        auto promptPath = getJobPath(contract::kProcessingDir, jobId) / contract::kPromptFile;
        
        std::error_code ec;
        auto st = std::filesystem::symlink_status(promptPath, ec);
        if (ec || !std::filesystem::exists(st)) {
            LOG_ERROR("Prompt file not found: " + promptPath.string());
            return {false, "", "Prompt file not found"};
        }
        if (std::filesystem::is_symlink(st) || !std::filesystem::is_regular_file(st)) {
            LOG_ERROR("Prompt file is not a regular file: " + promptPath.string());
            return {false, "", "Prompt file is not a regular file"};
        }

        const auto maxPromptBytes = static_cast<std::uintmax_t>(env_positive_int("NRVNA_MAX_PROMPT_SIZE", 10'000'000));
        auto promptBytes = std::filesystem::file_size(promptPath, ec);
        if (ec) {
            LOG_ERROR("Failed to stat prompt file: " + promptPath.string() + " - " + ec.message());
            return {false, "", "Failed to stat prompt file"};
        }
        if (promptBytes > maxPromptBytes) {
            LOG_ERROR("Prompt file too large: " + promptPath.string());
            return {false, "", "Prompt file too large: " + std::to_string(promptBytes) +
                " bytes (limit " + std::to_string(maxPromptBytes) + ")"};
        }
        
        std::ifstream file(promptPath, std::ios::binary);
        if (!file) {
            LOG_ERROR("Failed to open prompt file: " + promptPath.string());
            return {false, "", "Failed to open prompt file"};
        }
        
        std::string content((std::istreambuf_iterator<char>(file)), 
                           std::istreambuf_iterator<char>());
        
        if (content.empty()) {
            LOG_WARN("Empty prompt file: " + promptPath.string());
        }
        
        return {true, content, ""};
    } catch (const std::exception& e) {
        LOG_ERROR("Exception reading prompt for job " + jobId + ": " + std::string(e.what()));
        return {false, "", "Failed to read prompt: " + std::string(e.what())};
    } catch (...) {
        LOG_ERROR("Unknown error reading prompt for job: " + jobId);
        return {false, "", "Unknown error reading prompt"};
    }
}

std::filesystem::path Processor::getJobPath(const char* phase, const JobId& jobId) const noexcept {
    try {
        return workspace_ / phase / jobId;
    } catch (...) {
        return {};
    }
}

// Pre-initialize all Runner instances before worker threads start
// This ensures ggml_backend_load_all() is called sequentially from main thread
bool Processor::initializeRunners(int numWorkers) {
    std::lock_guard<std::mutex> lock(runnersMutex_);

    try {
        for (int i = 0; i < numWorkers; ++i) {
            LOG_DEBUG("Pre-creating Runner instance for worker " + std::to_string(i));
            if (mmprojPath_.empty()) {
                runners_[i] = std::make_unique<Runner>(modelPath_);
            } else {
                // Pass numWorkers so each Runner divides CPU threads appropriately
                runners_[i] = std::make_unique<Runner>(modelPath_, mmprojPath_, numWorkers);
            }
        }
        LOG_DEBUG("All " + std::to_string(numWorkers) + " Runner instances initialized");
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to initialize runners: " + std::string(e.what()));
        return false;
    }
}

// CRITICAL: Metal-compatible per-thread Runner management
Runner* Processor::getRunnerForWorker(int workerId) {
    std::lock_guard<std::mutex> lock(runnersMutex_);

    auto it = runners_.find(workerId);
    if (it == runners_.end()) {
        LOG_ERROR("Runner not found for worker " + std::to_string(workerId) + " - was initializeRunners() called?");
        throw std::runtime_error("Runner not initialized for worker " + std::to_string(workerId));
    }

    return it->second.get();
}

bool Processor::initializeTtsRunners(int numWorkers) {
    if (vocoderPath_.empty()) {
        LOG_DEBUG("No vocoder path, skipping TTS runner init");
        return true;
    }

    std::lock_guard<std::mutex> lock(ttsRunnersMutex_);
    try {
        for (int i = 0; i < numWorkers; ++i) {
            LOG_DEBUG("Pre-creating TtsRunner instance for worker " + std::to_string(i));
            ttsRunners_[i] = std::make_unique<TtsRunner>(modelPath_, vocoderPath_);
        }
        LOG_DEBUG("All " + std::to_string(numWorkers) + " TtsRunner instances initialized");
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to initialize TTS runners: " + std::string(e.what()));
        return false;
    }
}

TtsRunner* Processor::getTtsRunnerForWorker(int workerId) {
    std::lock_guard<std::mutex> lock(ttsRunnersMutex_);

    auto it = ttsRunners_.find(workerId);
    if (it == ttsRunners_.end()) {
        LOG_ERROR("TtsRunner not found for worker " + std::to_string(workerId));
        throw std::runtime_error("TtsRunner not initialized for worker " + std::to_string(workerId));
    }

    return it->second.get();
}

bool Processor::finalizeAudio(const JobId& jobId, const std::vector<float>& audio, int sampleRate) noexcept {
    try {
        auto processingPath = getJobPath(contract::kProcessingDir, jobId);
        auto outputPath = getJobPath(contract::kOutputDir, jobId);

        // Write WAV to temp file first
        auto tempPath = processingPath / (std::string(contract::kAudioFile) + ".tmp");
        {
            std::ofstream file(tempPath, std::ios::binary);
            if (!file) return false;

            // WAV header
            struct {
                char riff[4] = {'R', 'I', 'F', 'F'};
                uint32_t chunk_size;
                char wave[4] = {'W', 'A', 'V', 'E'};
                char fmt[4] = {'f', 'm', 't', ' '};
                uint32_t fmt_chunk_size = 16;
                uint16_t audio_format = 1;
                uint16_t num_channels = 1;
                uint32_t sample_rate;
                uint32_t byte_rate;
                uint16_t block_align;
                uint16_t bits_per_sample = 16;
                char data[4] = {'d', 'a', 't', 'a'};
                uint32_t data_size;
            } header;
            static_assert(sizeof(header) == 44, "WAV header struct has unexpected padding");

            header.sample_rate = static_cast<uint32_t>(sampleRate);
            header.byte_rate = header.sample_rate * header.num_channels * (header.bits_per_sample / 8);
            header.block_align = header.num_channels * (header.bits_per_sample / 8);
            header.data_size = static_cast<uint32_t>(audio.size() * (header.bits_per_sample / 8));
            header.chunk_size = 36 + header.data_size;

            file.write(reinterpret_cast<const char*>(&header), sizeof(header));
            for (const auto& sample : audio) {
                int16_t pcm = static_cast<int16_t>(std::max(-32768.0, std::min(32767.0, sample * 32767.0)));
                file.write(reinterpret_cast<const char*>(&pcm), sizeof(pcm));
            }
            file.flush();
            if (!file.good()) return false;
        }

        // Rename temp to final
        auto finalPath = processingPath / contract::kAudioFile;
        std::filesystem::rename(tempPath, finalPath);

        // Atomic move to output
        std::filesystem::rename(processingPath, outputPath);

        LOG_DEBUG("TTS job finalized: " + jobId);
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to finalize audio for job " + jobId + ": " + std::string(e.what()));
        return false;
    } catch (...) {
        LOG_ERROR("Unknown error finalizing audio for job: " + jobId);
        return false;
    }
}

}
