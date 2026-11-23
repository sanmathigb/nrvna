/*
 * nrvna - Durable Local Inference Primitives
 * Copyright (c) 2025 Sanmathi Bharamgouda
 * SPDX-License-Identifier: MIT
 */

#include "nrvna/work.hpp"
#include "nrvna/contract.hpp"
#include "nrvna/meta.hpp"
#include "nrvna/logger.hpp"
#include <filesystem>
#include <fstream>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <atomic>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cerrno>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

namespace nrvna {

namespace {
class CallerIdLock final {
public:
    CallerIdLock() = default;
    CallerIdLock(const CallerIdLock&) = delete;
    CallerIdLock& operator=(const CallerIdLock&) = delete;
    ~CallerIdLock() {
        if (fd_ >= 0) {
            (void)::flock(fd_, LOCK_UN);
            (void)::close(fd_);
        }
    }

    bool acquire(const std::filesystem::path& path) noexcept {
        fd_ = ::open(path.c_str(), O_CREAT | O_RDWR | O_CLOEXEC | O_NOFOLLOW, S_IRUSR | S_IWUSR);
        if (fd_ < 0) return false;
        while (::flock(fd_, LOCK_EX) != 0) {
            if (errno == EINTR) continue;
            (void)::close(fd_);
            fd_ = -1;
            return false;
        }
        return true;
    }

private:
    int fd_ = -1;
};

bool findCallerId(const std::filesystem::path& workspace, const JobId& id,
                  bool& existing, std::string& error) {
    // State moves are atomic and only move forward. Scan in lifecycle order
    // so a job moving during this check is found in its next state.
    std::error_code ec;
    const char* states[] = {contract::kReadyDir, contract::kProcessingDir,
                            contract::kOutputDir, contract::kFailedDir};
    for (const auto* state : states) {
        const auto path = workspace / state / id;
        ec.clear();
        const auto status = std::filesystem::symlink_status(path, ec);
        if (!ec && std::filesystem::is_symlink(status)) {
            error = "Caller job ID path is a symlink";
            return false;
        }
        if (!ec && std::filesystem::is_directory(status)) {
            existing = true;
            return true;
        }
        if (!ec && std::filesystem::exists(status)) {
            error = "Caller job ID path is not a job directory";
            return false;
        }
        if (ec && ec != std::errc::no_such_file_or_directory) {
            error = "Could not inspect caller job ID";
            return false;
        }
    }
    return true;
}

bool lockCallerIdPublication(const std::filesystem::path& workspace, CallerIdLock& lock,
                             std::string& error) {
    // One short publication lock closes the race across all caller-ID states.
    const auto lockPath = workspace / contract::kIdSubmitLock;
    std::error_code ec;
    std::filesystem::create_directories(lockPath.parent_path(), ec);
    if (ec || !lock.acquire(lockPath)) {
        error = "Could not lock caller-ID publication";
        return false;
    }
    return true;
}

std::size_t env_size(const char* name, std::size_t defv) {
    const char* val = std::getenv(name);
    if (!val || !*val) {
        return defv;
    }
    try {
        std::size_t parsed = static_cast<std::size_t>(std::stoull(val));
        return parsed == 0 ? defv : parsed;
    } catch (...) {
        return defv;
    }
}

std::string toLowerCopy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

bool validateImagePath(const std::filesystem::path& path, SubmissionError& code, std::string& error) {
    if (!std::filesystem::exists(path)) {
        error = "Image file not found: " + path.string();
        code = SubmissionError::InvalidContent;
        return false;
    }
    if (!std::filesystem::is_regular_file(path)) {
        error = "Image path is not a file: " + path.string();
        code = SubmissionError::InvalidContent;
        return false;
    }
    std::string ext = toLowerCopy(path.extension().string());
    if (ext.empty()) {
        error = "Image file has no extension: " + path.string();
        code = SubmissionError::InvalidContent;
        return false;
    }
    static const std::vector<std::string> valid_ext = {".jpg", ".jpeg", ".png", ".gif", ".webp"};
    if (std::find(valid_ext.begin(), valid_ext.end(), ext) == valid_ext.end()) {
        error = "Unsupported image extension: " + path.string();
        code = SubmissionError::InvalidContent;
        return false;
    }
    std::size_t max_bytes = env_size("NRVNA_MAX_IMAGE_SIZE", 50ULL * 1024 * 1024);
    std::error_code ec;
    auto size = std::filesystem::file_size(path, ec);
    if (ec) {
        error = "Failed to read image size: " + path.string();
        code = SubmissionError::IoError;
        return false;
    }
    if (size > max_bytes) {
        error = "Image exceeds size limit (" + std::to_string(max_bytes) + " bytes): " + path.string();
        code = SubmissionError::InvalidSize;
        return false;
    }
    return true;
}

bool validateAudioPath(const std::filesystem::path& path, SubmissionError& code, std::string& error) {
    if (!std::filesystem::exists(path)) {
        error = "Audio file not found: " + path.string();
        code = SubmissionError::InvalidContent;
        return false;
    }
    if (!std::filesystem::is_regular_file(path)) {
        error = "Audio path is not a file: " + path.string();
        code = SubmissionError::InvalidContent;
        return false;
    }
    std::string ext = toLowerCopy(path.extension().string());
    if (ext.empty()) {
        error = "Audio file has no extension: " + path.string();
        code = SubmissionError::InvalidContent;
        return false;
    }
    static const std::vector<std::string> valid_ext = {".wav", ".mp3", ".flac"};
    if (std::find(valid_ext.begin(), valid_ext.end(), ext) == valid_ext.end()) {
        error = "Unsupported audio extension: " + path.string();
        code = SubmissionError::InvalidContent;
        return false;
    }
    std::size_t max_bytes = env_size("NRVNA_MAX_AUDIO_SIZE", 200ULL * 1024 * 1024);
    std::error_code ec;
    auto size = std::filesystem::file_size(path, ec);
    if (ec) {
        error = "Failed to read audio size: " + path.string();
        code = SubmissionError::IoError;
        return false;
    }
    if (size > max_bytes) {
        error = "Audio exceeds size limit (" + std::to_string(max_bytes) + " bytes): " + path.string();
        code = SubmissionError::InvalidSize;
        return false;
    }
    return true;
}

}
bool Work::isValidTag(const std::string& tag) noexcept {
    if (tag.empty() || tag.size() > 64) {
        return false;
    }
    return std::all_of(tag.begin(), tag.end(), [](unsigned char c) {
        return std::isalnum(c) || c == '-' || c == '_';
    });
}

Work::Work(const std::filesystem::path& workspace, bool createIfMissing)
    : workspace_(workspace) {
    if (!createWorkspace(createIfMissing)) {
        LOG_ERROR("Failed to initialize workspace: " + workspace_.string());
    }
}

SubmitResult Work::submit(const std::string& prompt, JobType type, const std::vector<std::filesystem::path>& imagePaths, const SubmitOptions& opts) {
    if (!opts.id.empty() && !contract::isValidCallerJobId(opts.id)) {
        return {false, "", SubmissionError::InvalidContent, "Invalid caller job ID"};
    }
    JobId jobId = opts.id.empty() ? generateId() : opts.id;
    JobId stagingId = opts.id.empty() ? jobId : generateId();
    if (!opts.id.empty()) {
        bool existing = false;
        std::string error;
        CallerIdLock lock;
        if (!lockCallerIdPublication(workspace_, lock, error) ||
            !findCallerId(workspace_, jobId, existing, error)) {
            return {false, "", SubmissionError::IoError, error};
        }
        if (existing) return {true, jobId, SubmissionError::None, ""};
    }

    const bool hasStructuredOutput = !opts.output_format.empty() || !opts.schema.empty() || !opts.grammar.empty();
    const bool validStructuredOutput =
        !hasStructuredOutput ||
        ((type == JobType::Text || type == JobType::Vision) &&
         !opts.grammar.empty() &&
         opts.grammar.size() <= contract::kMaxStructuredOutputBytes &&
         ((opts.output_format == "json_schema" && !opts.schema.empty() &&
           opts.schema.size() <= contract::kMaxStructuredOutputBytes) ||
          (opts.output_format == "gbnf" && opts.schema.empty())));
    if (!validStructuredOutput) {
        return {false, "", SubmissionError::InvalidContent, "Invalid structured output options"};
    }

    const bool allowEmptyPrompt = type == JobType::Embed && !imagePaths.empty();
    if ((!allowEmptyPrompt && !isValidPrompt(prompt)) || (allowEmptyPrompt && prompt.size() > maxBytes_)) {
        if (prompt.empty()) {
            LOG_DEBUG("Invalid prompt: empty");
            return {false, "", SubmissionError::InvalidContent, "Prompt is empty"};
        } else {
            LOG_DEBUG("Prompt exceeds size limit: " + std::to_string(prompt.size()) + " > " + std::to_string(maxBytes_));
            return {false, "", SubmissionError::InvalidSize, "Prompt exceeds maximum size limit (" + std::to_string(maxBytes_) + " bytes)"};
        }
    }

    if (!imagePaths.empty()) {
        for (const auto& path : imagePaths) {
            std::string error;
            SubmissionError code = SubmissionError::None;
            if (!validateImagePath(path, code, error)) {
                LOG_ERROR(error);
                return {false, "", code, error};
            }
        }
    }

    LOG_DEBUG("Using job ID: " + jobId);

    if (!createJobDirectory(stagingId)) {
        LOG_ERROR("Failed to create job directory for: " + jobId);
        return {false, "", SubmissionError::IoError, "Failed to create job directory"};
    }

    if (!writePromptFile(stagingId, prompt)) {
        LOG_ERROR("Failed to write prompt file for: " + jobId);
        cleanupFailedJob(stagingId);
        return {false, "", SubmissionError::IoError, "Failed to write prompt file"};
    }

    if (!imagePaths.empty()) {
        if (opts.keep_files && !writeImageFiles(stagingId, imagePaths)) {
            LOG_ERROR("Failed to write image files for: " + jobId);
            cleanupFailedJob(stagingId);
            return {false, "", SubmissionError::IoError, "Failed to write image files"};
        }
        if (!writeTypeFile(stagingId, type)) {
            LOG_ERROR("Failed to write type file for: " + jobId);
            cleanupFailedJob(stagingId);
            return {false, "", SubmissionError::IoError, "Failed to write type file"};
        }
    } else if (type != JobType::Text) {
        if (!writeTypeFile(stagingId, type)) {
            LOG_ERROR("Failed to write type file for: " + jobId);
            cleanupFailedJob(stagingId);
            return {false, "", SubmissionError::IoError, "Failed to write type file"};
        }
    }

    if (!writeStructuredOutputFiles(stagingId, opts)) {
        LOG_ERROR("Failed to write structured output files for: " + jobId);
        cleanupFailedJob(stagingId);
        return {false, "", SubmissionError::IoError, "Failed to write structured output files"};
    }

    if (!writeMetaFile(stagingId, type, opts, imagePaths)) {
        // Metadata is part of the contract now: tags, lineage, and set
        // collection all read it. A job without meta.json is invisible to
        // --tag/--children, so refuse to publish rather than lose lineage.
        LOG_ERROR("Failed to write meta.json for: " + jobId);
        cleanupFailedJob(stagingId);
        return {false, "", SubmissionError::IoError, "Failed to write job metadata"};
    }

    CallerIdLock callerIdLock;
    if (!opts.id.empty()) {
        bool existing = false;
        std::string error;
        if (!lockCallerIdPublication(workspace_, callerIdLock, error) ||
            !findCallerId(workspace_, jobId, existing, error)) {
            cleanupFailedJob(stagingId);
            return {false, "", SubmissionError::IoError, error};
        }
        if (existing) {
            cleanupFailedJob(stagingId);
            LOG_INFO("Returning existing job: " + jobId);
            return {true, jobId, SubmissionError::None, ""};
        }
    }
    if (!atomicPublish(stagingId, jobId)) {
        LOG_ERROR("Failed to publish job: " + jobId);
        cleanupFailedJob(stagingId);
        return {false, "", SubmissionError::IoError, "Failed to publish job"};
    }

    LOG_INFO("Job submitted successfully: " + jobId);
    return {true, jobId, SubmissionError::None, ""};
}

SubmitResult Work::submitAudio(const std::string& prompt, const std::vector<std::filesystem::path>& audioPaths, const SubmitOptions& opts) {
    if (!opts.id.empty() && !contract::isValidCallerJobId(opts.id)) {
        return {false, "", SubmissionError::InvalidContent, "Invalid caller job ID"};
    }
    JobId jobId = opts.id.empty() ? generateId() : opts.id;
    JobId stagingId = opts.id.empty() ? jobId : generateId();
    if (!opts.id.empty()) {
        bool existing = false;
        std::string error;
        CallerIdLock lock;
        if (!lockCallerIdPublication(workspace_, lock, error) ||
            !findCallerId(workspace_, jobId, existing, error)) {
            return {false, "", SubmissionError::IoError, error};
        }
        if (existing) return {true, jobId, SubmissionError::None, ""};
    }

    if (!opts.output_format.empty() || !opts.schema.empty() || !opts.grammar.empty()) {
        return {false, "", SubmissionError::InvalidContent, "Structured output is not supported for audio jobs"};
    }
    if (prompt.size() > maxBytes_) {
        LOG_DEBUG("Prompt exceeds size limit: " + std::to_string(prompt.size()) + " > " + std::to_string(maxBytes_));
        return {false, "", SubmissionError::InvalidSize, "Prompt exceeds maximum size limit (" + std::to_string(maxBytes_) + " bytes)"};
    }

    if (audioPaths.empty()) {
        return {false, "", SubmissionError::InvalidContent, "No audio file provided"};
    }

    for (const auto& path : audioPaths) {
        std::string error;
        SubmissionError code = SubmissionError::None;
        if (!validateAudioPath(path, code, error)) {
            LOG_ERROR(error);
            return {false, "", code, error};
        }
    }

    LOG_DEBUG("Using audio job ID: " + jobId);

    if (!createJobDirectory(stagingId)) {
        LOG_ERROR("Failed to create job directory for: " + jobId);
        return {false, "", SubmissionError::IoError, "Failed to create job directory"};
    }

    if (!writePromptFile(stagingId, prompt)) {
        LOG_ERROR("Failed to write prompt file for: " + jobId);
        cleanupFailedJob(stagingId);
        return {false, "", SubmissionError::IoError, "Failed to write prompt file"};
    }

    if (opts.keep_files && !writeAudioFiles(stagingId, audioPaths)) {
        LOG_ERROR("Failed to write audio files for: " + jobId);
        cleanupFailedJob(stagingId);
        return {false, "", SubmissionError::IoError, "Failed to write audio files"};
    }

    if (!writeTypeFile(stagingId, JobType::Stt)) {
        LOG_ERROR("Failed to write type file for: " + jobId);
        cleanupFailedJob(stagingId);
        return {false, "", SubmissionError::IoError, "Failed to write type file"};
    }

    if (!writeMetaFile(stagingId, JobType::Stt, opts, audioPaths)) {
        LOG_ERROR("Failed to write meta.json for: " + jobId);
        cleanupFailedJob(stagingId);
        return {false, "", SubmissionError::IoError, "Failed to write job metadata"};
    }

    CallerIdLock callerIdLock;
    if (!opts.id.empty()) {
        bool existing = false;
        std::string error;
        if (!lockCallerIdPublication(workspace_, callerIdLock, error) ||
            !findCallerId(workspace_, jobId, existing, error)) {
            cleanupFailedJob(stagingId);
            return {false, "", SubmissionError::IoError, error};
        }
        if (existing) {
            cleanupFailedJob(stagingId);
            LOG_INFO("Returning existing job: " + jobId);
            return {true, jobId, SubmissionError::None, ""};
        }
    }
    if (!atomicPublish(stagingId, jobId)) {
        LOG_ERROR("Failed to publish job: " + jobId);
        cleanupFailedJob(stagingId);
        return {false, "", SubmissionError::IoError, "Failed to publish job"};
    }

    LOG_INFO("Audio job submitted successfully: " + jobId);
    return {true, jobId, SubmissionError::None, ""};
}

bool Work::createWorkspace(bool createIfMissing) noexcept {
    try {
        if (!std::filesystem::exists(workspace_)) {
            if (!createIfMissing) {
                return false;
            }
            std::filesystem::create_directories(workspace_);
        }

        std::filesystem::create_directories(workspace_ / contract::kWritingDir);
        std::filesystem::create_directories(workspace_ / contract::kReadyDir);
        std::filesystem::create_directories(workspace_ / contract::kProcessingDir);
        std::filesystem::create_directories(workspace_ / contract::kOutputDir);
        std::filesystem::create_directories(workspace_ / contract::kFailedDir);

        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to create workspace: " + std::string(e.what()));
        return false;
    } catch (...) {
        LOG_ERROR("Unknown error creating workspace");
        return false;
    }
}

JobId Work::generateId() {
    static std::atomic<uint64_t> counter{0};

    auto now = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    uint64_t unique_counter = counter.fetch_add(1);

    std::stringstream ss;
    ss << std::setw(20) << std::setfill('0') << now
       << "_" << getpid()
       << "_" << std::setw(6) << std::setfill('0') << unique_counter;
    return ss.str();
}

bool Work::isValidPrompt(const std::string& prompt) const noexcept {
    return !prompt.empty() && prompt.size() <= maxBytes_;
}

bool Work::createJobDirectory(const JobId& jobId) const noexcept {
    try {
        std::filesystem::create_directories(workspace_ / contract::kWritingDir);
        auto jobPath = workspace_ / contract::kWritingDir / jobId;
        return std::filesystem::create_directory(jobPath);
    } catch (...) {
        return false;
    }
}

bool Work::writePromptFile(const JobId& jobId, const std::string& prompt) const noexcept {
    try {
        auto promptPath = workspace_ / contract::kWritingDir / jobId / contract::kPromptFile;
        std::ofstream file(promptPath, std::ios::binary);
        if (!file) return false;

        file << prompt;
        file.flush();
        return file.good();
    } catch (...) {
        return false;
    }
}

bool Work::writeImageFiles(const JobId& jobId, const std::vector<std::filesystem::path>& imagePaths) const noexcept {
    try {
        auto jobPath = workspace_ / contract::kWritingDir / jobId;
        auto imagesDir = jobPath / contract::kImagesDir;
        std::filesystem::create_directories(imagesDir);

        int idx = 0;
        for (const auto& srcPath : imagePaths) {
            if (!std::filesystem::exists(srcPath)) {
                LOG_ERROR("Image file not found: " + srcPath.string());
                return false;
            }
            std::string ext = srcPath.extension().string();
            std::ostringstream filename;
            filename << "image_" << std::setw(6) << std::setfill('0') << idx << ext;
            std::string destFilename = filename.str();
            auto destPath = imagesDir / destFilename;
            std::error_code ec;
            // --keep-files requests a self-contained input snapshot.
            std::filesystem::copy_file(srcPath, destPath, std::filesystem::copy_options::overwrite_existing, ec);
            if (ec) {
                LOG_ERROR("Failed to write image file: " + srcPath.string());
                return false;
            }
            ++idx;
        }

        return true;
    } catch (...) {
        return false;
    }
}

bool Work::writeAudioFiles(const JobId& jobId, const std::vector<std::filesystem::path>& audioPaths) const noexcept {
    try {
        auto jobPath = workspace_ / contract::kWritingDir / jobId;
        auto audioDir = jobPath / contract::kAudioInputDir;
        std::filesystem::create_directories(audioDir);

        int idx = 0;
        for (const auto& srcPath : audioPaths) {
            if (!std::filesystem::exists(srcPath)) {
                LOG_ERROR("Audio file not found: " + srcPath.string());
                return false;
            }
            std::string ext = srcPath.extension().string();
            std::ostringstream filename;
            filename << "audio_" << std::setw(6) << std::setfill('0') << idx << ext;
            auto destPath = audioDir / filename.str();
            std::error_code ec;
            // --keep-files requests a self-contained input snapshot.
            std::filesystem::copy_file(srcPath, destPath, std::filesystem::copy_options::overwrite_existing, ec);
            if (ec) {
                LOG_ERROR("Failed to write audio file: " + srcPath.string());
                return false;
            }
            ++idx;
        }

        return true;
    } catch (...) {
        return false;
    }
}

bool Work::writeTypeFile(const JobId& jobId, JobType type) const noexcept {
    try {
        auto typePath = workspace_ / contract::kWritingDir / jobId / contract::kTypeFile;
        std::ofstream file(typePath, std::ios::binary);
        if (!file) return false;

        file << contract::toString(type);
        file.flush();
        return file.good();
    } catch (...) {
        return false;
    }
}

bool Work::writeStructuredOutputFiles(const JobId& jobId, const SubmitOptions& opts) const noexcept {
    try {
        const auto dir = workspace_ / contract::kWritingDir / jobId;
        if (!opts.schema.empty()) {
            std::ofstream schemaFile(dir / contract::kSchemaFile, std::ios::binary);
            if (!schemaFile) return false;
            schemaFile << opts.schema;
            schemaFile.flush();
            if (!schemaFile.good()) return false;
        }
        if (!opts.grammar.empty()) {
            std::ofstream grammarFile(dir / contract::kGrammarFile, std::ios::binary);
            if (!grammarFile) return false;
            grammarFile << opts.grammar;
            grammarFile.flush();
            if (!grammarFile.good()) return false;
        }
        return true;
    } catch (...) {
        return false;
    }
}

bool Work::writeMetaFile(const JobId& jobId, JobType type, const SubmitOptions& opts,
                         const std::vector<std::filesystem::path>& inputPaths) const noexcept {
    try {
        JobMeta meta;
        meta.submitted_at = formatTimestamp();
        meta.mode = contract::toString(type);
        meta.reference_inputs = !inputPaths.empty() && !opts.keep_files;
        if (meta.reference_inputs) {
            for (const auto& path : inputPaths) {
                meta.input_paths.push_back(std::filesystem::canonical(path).string());
            }
        }
        meta.parent = opts.parent;
        meta.output_format = opts.output_format;
        meta.keep_files = opts.keep_files;
        for (const auto& tag : opts.tags) {
            if (isValidTag(tag)) {
                meta.tags.push_back(tag);
            } else {
                LOG_WARN("Ignoring invalid tag for job " + jobId + ": " + tag);
            }
        }
        return writeMetaJson(workspace_ / contract::kWritingDir / jobId, meta);
    } catch (...) {
        return false;
    }
}

bool Work::atomicPublish(const JobId& stagingId, const JobId& jobId) const noexcept {
    try {
        auto writingPath = workspace_ / contract::kWritingDir / stagingId;
        auto readyPath = workspace_ / contract::kReadyDir / jobId;

        std::filesystem::rename(writingPath, readyPath);
        return true;
    } catch (...) {
        return false;
    }
}

void Work::cleanupFailedJob(const JobId& jobId) const noexcept {
    std::error_code ec;
    std::filesystem::remove_all(workspace_ / contract::kWritingDir / jobId, ec);
}

}
