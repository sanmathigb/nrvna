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

constexpr const char* VERSION = "0.1.0";

void printUsage(const char* progName) {
    std::cout << "nrvna-ai Work Submission Tool v" << VERSION << "\n\n";
    std::cout << "Usage: " << progName << " <workspace> <prompt...> [--image <path> ...]\n";
    std::cout << "       " << progName << " <workspace> <text> --embed\n";
    std::cout << "       " << progName << " <workspace> <text> --tts\n";
    std::cout << "       " << progName << " <workspace> --audio <path> --stt\n";
    std::cout << "       " << progName << " <workspace> -     (read prompt from stdin)\n";
    std::cout << "       " << progName << " --help | --version\n\n";
    std::cout << "Arguments:\n";
    std::cout << "  workspace     Directory for job storage\n";
    std::cout << "  prompt        Text prompt for inference (can be multiple words)\n";
    std::cout << "  -             Read prompt from stdin\n\n";
    std::cout << "Options:\n";
    std::cout << "  --image <path>   Attach image (repeatable)\n";
    std::cout << "  --audio <path>   Attach audio for speech-to-text (repeatable)\n";
    std::cout << "  --embed          Submit as embedding job (returns vector)\n";
    std::cout << "  --tts            Submit as text-to-speech job\n";
    std::cout << "  --stt            Submit as speech-to-text job\n";
    std::cout << "  --mode <type>    Job mode: tts or stt\n";
    std::cout << "  --               Treat remaining args as prompt (for prompts containing dashes)\n";
    std::cout << "  --parent <id>    Optional parent job ID\n";
    std::cout << "  --tag <tag>      Optional tag (repeatable)\n";
    std::cout << "  -h, --help       Show this help message\n";
    std::cout << "  -v, --version    Show version\n\n";
    std::cout << "Environment Variables:\n";
    std::cout << "  NRVNA_LOG_LEVEL    Log level (ERROR, WARN, INFO, DEBUG, TRACE)\n\n";
    std::cout << "Examples:\n";
    std::cout << "  " << progName << " ./workspace \"What is the capital of France?\"\n";
    std::cout << "  " << progName << " ./workspace Write a hello world program\n";
    std::cout << "  " << progName << " ./workspace \"Machine learning is...\" --embed\n";
    std::cout << "  " << progName << " ./workspace --audio note.wav --stt\n";
    std::cout << "  echo \"Hello\" | " << progName << " ./workspace -\n";
}

int main(int argc, char* argv[]) {
    // Default to WARN for clean piping; NRVNA_LOG_LEVEL overrides
    if (!std::getenv("NRVNA_LOG_LEVEL"))
        Logger::setLevel(LogLevel::WARN);

    // Handle --help and --version before anything else
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        }
        if (arg == "-v" || arg == "--version") {
            std::cout << VERSION << "\n";
            return 0;
        }
    }

    if (argc < 2) {
        printUsage(argv[0]);
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
        } else if (arg == "--mode") {
            if (i + 1 >= argc) {
                std::cerr << "Error: --mode requires a type (e.g. tts or stt)\n";
                return 1;
            }
            mode = argv[++i];
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
        std::cerr << "Error: --audio requires --stt or --mode stt\n";
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
