/*
 * nrvna ai - Work submission tool (wrk)
 * Copyright (c) 2025 Sanmathi Bharamgouda
 * SPDX-License-Identifier: MIT
 */

#include "nrvna/work.hpp"
#include "nrvna/flow.hpp"
#include "nrvna/logger.hpp"
#include <filesystem>
#include <iostream>
#include <sstream>
#include <iterator>
#include <unistd.h>

using namespace nrvnaai;

constexpr const char* VERSION = NRVNA_VERSION;

void printUsage() {
    std::cout << "Submit work to an nrvna workspace.\n\n";
    std::cout << "Usage:\n";
    std::cout << "  wrk <workspace> [prompt...] [options]\n";
    std::cout << "  wrk <workspace> - [options]\n\n";
    std::cout << "Options:\n";
    std::cout << "  -i, --image <path>   Attach an image (repeatable)\n";
    std::cout << "      --audio <path>   Attach audio (repeatable)\n";
    std::cout << "      --embed          Create an embedding\n";
    std::cout << "      --tts            Generate speech\n";
    std::cout << "      --stt            Transcribe audio\n";
    std::cout << "      --parent <id>    Set the parent job\n";
    std::cout << "      --tag <tag>      Add a tag (repeatable)\n";
    std::cout << "  -h, --help           Show help\n";
    std::cout << "  -v, --version        Show version\n";
    std::cout << "\n";
    std::cout << "Examples:\n";
    std::cout << "  cat report.txt | wrk ./ws -\n";
    std::cout << "  { echo \"Summarize:\"; cat notes.md; } | wrk ./ws -\n";
    std::cout << "  wrk ./ws \"What is this screenshot about?\" --image shot.png\n";
    std::cout << "\n";
    std::cout << "The workspace is created if missing. Prints the job ID on stdout;\n";
    std::cout << "collect the result later with: flw <workspace> -w <job-id>\n";
}

int main(int argc, char* argv[]) {
    // Default to WARN for clean piping; NRVNA_LOG_LEVEL overrides
    if (!std::getenv("NRVNA_LOG_LEVEL"))
        Logger::setLevel(LogLevel::WARN);

    // Handle --help and --version before anything else
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            printUsage();
            return 0;
        }
        if (arg == "-v" || arg == "--version") {
            std::cout << VERSION << "\n";
            return 0;
        }
    }

    if (argc < 2) {
        printUsage();
        return 1;
    }

    std::string workspace = argv[1];
    std::string prompt;
    std::vector<std::filesystem::path> imagePaths;
    std::vector<std::filesystem::path> audioPaths;
    std::vector<std::string> promptParts;
    bool useEmbed = false;
    std::string mode;
    SubmitOptions submitOptions;

    // Detect stdin input: `wrk ws` with piped stdin, or `wrk ws - ...`
    bool readStdin = false;
    if (argc == 2 && !isatty(fileno(stdin))) {
        readStdin = true;
    } else if (argc >= 3 && std::string(argv[2]) == "-") {
        readStdin = true;
    }

    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--") {
            while (++i < argc) promptParts.push_back(argv[i]);
            break;
        } else if (arg == "--image" || arg == "-i") {
            if (i + 1 >= argc) {
                std::cerr << "Error: --image requires a path\n";
                return 1;
            }
            imagePaths.emplace_back(argv[++i]);
        } else if (arg == "--audio") {
            if (i + 1 >= argc) {
                std::cerr << "Error: --audio requires a path\n";
                return 1;
            }
            audioPaths.emplace_back(argv[++i]);
        } else if (arg == "--parent") {
            if (i + 1 >= argc) {
                std::cerr << "Error: --parent requires a job ID\n";
                return 1;
            }
            submitOptions.parent = argv[++i];
            if (!Flow::isValidJobId(submitOptions.parent)) {
                std::cerr << "Error: invalid parent job ID\n";
                return 1;
            }
        } else if (arg == "--tag") {
            if (i + 1 >= argc) {
                std::cerr << "Error: --tag requires a value\n";
                return 1;
            }
            std::string tag = argv[++i];
            if (!Work::isValidTag(tag)) {
                std::cerr << "Error: invalid tag '" << tag << "'\n";
                return 1;
            }
            submitOptions.tags.push_back(tag);
        } else if (arg == "--embed") {
            useEmbed = true;
        } else if (arg == "--tts") {
            mode = "tts";
        } else if (arg == "--stt") {
            mode = "stt";
        } else if (arg.size() > 1 && arg[0] == '-') {
            std::cerr << "Error: unknown option: " << arg << "\n";
            return 1;
        } else {
            promptParts.push_back(arg);
        }
    }

    if (readStdin) {
        prompt.assign((std::istreambuf_iterator<char>(std::cin)),
                       std::istreambuf_iterator<char>());
        // Remove strictly trailing newline if prompt is just a one-liner
        if (!prompt.empty() && prompt.back() == '\n') {
            prompt.pop_back();
        }
    } else {
        for (size_t i = 0; i < promptParts.size(); ++i) {
            if (i > 0) prompt += ' ';
            prompt += promptParts[i];
        }
    }

    if (!mode.empty() && mode != "tts" && mode != "stt") {
        std::cerr << "Error: Unknown mode '" << mode << "'. Supported: tts, stt\n";
        return 1;
    }

    if (!audioPaths.empty() && mode != "stt") {
        std::cerr << "Error: --audio requires --stt\n";
        return 1;
    }

    if (mode == "stt" && audioPaths.empty()) {
        std::cerr << "Error: --stt requires --audio <path>\n";
        return 1;
    }

    if (prompt.empty() && !(useEmbed && !imagePaths.empty()) && !(mode == "stt" && !audioPaths.empty())) {
        std::cerr << "Error: Empty prompt provided\n";
        return 1;
    }

    if (useEmbed && !mode.empty()) {
        std::cerr << "Error: --embed and --tts/--stt are mutually exclusive\n";
        return 1;
    }

    if (mode == "tts" && (!imagePaths.empty() || !audioPaths.empty())) {
        std::cerr << "Error: --tts cannot be combined with --image or --audio\n";
        return 1;
    }

    if (mode == "stt" && !imagePaths.empty()) {
        std::cerr << "Error: --stt and --image are mutually exclusive\n";
        return 1;
    }

    try {
        Work work(workspace, true); // Create workspace if missing

        SubmitResult result;
        if (mode == "tts") {
            result = work.submit(prompt, JobType::Tts, {}, submitOptions);
        } else if (mode == "stt") {
            result = work.submitAudio(prompt, audioPaths, submitOptions);
        } else if (useEmbed && !imagePaths.empty()) {
            result = work.submit(prompt, JobType::Embed, imagePaths, submitOptions);
        } else if (useEmbed) {
            result = work.submit(prompt, JobType::Embed, {}, submitOptions);
        } else if (!imagePaths.empty()) {
            result = work.submit(prompt, JobType::Vision, imagePaths, submitOptions);
        } else {
            result = work.submit(prompt, JobType::Text, {}, submitOptions);
        }

        if (result.ok) {
            // Just the job ID - clean for piping, no noise
            std::cout << result.id << std::endl;
            return 0;
        } else {
            std::cerr << "Error: " << result.message << std::endl;
            return 1;
        }

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Error: Unknown error submitting job\n";
        return 1;
    }
}
