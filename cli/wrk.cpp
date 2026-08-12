/*
 * nrvna - Work submission tool (wrk)
 * Copyright (c) 2025 Sanmathi Bharamgouda
 * SPDX-License-Identifier: MIT
 */

#include "nrvna/work.hpp"
#include "nrvna/flow.hpp"
#include "nrvna/contract.hpp"
#include "nrvna/logger.hpp"
#include "json-schema-to-grammar.h"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <iterator>
#include <unistd.h>

using namespace nrvna;

constexpr const char* VERSION = NRVNA_VERSION;

namespace {

bool readStructuredFile(const std::filesystem::path& path, std::string& content,
                        std::string& error) {
    std::error_code ec;
    const auto status = std::filesystem::symlink_status(path, ec);
    if (ec || !std::filesystem::exists(status)) {
        error = "file does not exist";
        return false;
    }
    if (std::filesystem::is_symlink(status) || !std::filesystem::is_regular_file(status)) {
        error = "path is not a regular file";
        return false;
    }
    const auto bytes = std::filesystem::file_size(path, ec);
    if (ec) {
        error = "cannot determine file size";
        return false;
    }
    if (bytes == 0) {
        error = "file is empty";
        return false;
    }
    if (bytes > contract::kMaxStructuredOutputBytes) {
        error = "file exceeds the 1000000-byte limit";
        return false;
    }
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        error = "cannot open file";
        return false;
    }
    content.assign(std::istreambuf_iterator<char>(file), {});
    if (content.size() != bytes) {
        error = "cannot read file";
        return false;
    }
    return true;
}

} // namespace

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
    std::cout << "      --json-schema <path>  Constrain text or vision output with JSON Schema\n";
    std::cout << "      --grammar <path>      Constrain text or vision output with GBNF\n";
    std::cout << "  -h, --help           Show help\n";
    std::cout << "  -v, --version        Show version\n";
    std::cout << "\n";
    std::cout << "Examples:\n";
    std::cout << "  cat report.txt | wrk ./ws -\n";
    std::cout << "  { echo \"Summarize:\"; cat notes.md; } | wrk ./ws -\n";
    std::cout << "  wrk ./ws \"What is this screenshot about?\" --image shot.png\n";
    std::cout << "  wrk ./ws \"Extract the fields\" --json-schema fields.schema.json\n";
    std::cout << "\n";
    std::cout << "wrk creates the workspace when it is missing.\n";
    std::cout << "It prints only the job ID on stdout. Collect the result with:\n";
    std::cout << "  flw <workspace> -w <job-id>\n";
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
    bool sawTts = false, sawStt = false;
    SubmitOptions submitOptions;
    std::filesystem::path schemaPath;
    std::filesystem::path grammarPath;

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
            if (!contract::isValidJobId(submitOptions.parent)) {
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
        } else if (arg == "--json-schema") {
            if (i + 1 >= argc) {
                std::cerr << "Error: --json-schema requires a path\n";
                return 1;
            }
            schemaPath = argv[++i];
        } else if (arg == "--grammar") {
            if (i + 1 >= argc) {
                std::cerr << "Error: --grammar requires a path\n";
                return 1;
            }
            grammarPath = argv[++i];
        } else if (arg == "--embed") {
            useEmbed = true;
        } else if (arg == "--tts") {
            sawTts = true;
            mode = "tts";
        } else if (arg == "--stt") {
            sawStt = true;
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
        // Remove one trailing newline from stdin input.
        if (!prompt.empty() && prompt.back() == '\n') {
            prompt.pop_back();
        }
    } else {
        for (size_t i = 0; i < promptParts.size(); ++i) {
            if (i > 0) prompt += ' ';
            prompt += promptParts[i];
        }
    }

    if (sawTts && sawStt) {
        std::cerr << "Error: --tts and --stt are mutually exclusive\n";
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

    if (!schemaPath.empty() && !grammarPath.empty()) {
        std::cerr << "Error: --json-schema and --grammar are mutually exclusive\n";
        return 1;
    }

    if ((!schemaPath.empty() || !grammarPath.empty()) && (useEmbed || !mode.empty())) {
        std::cerr << "Error: structured output requires a text or vision job\n";
        return 1;
    }

    if (!schemaPath.empty()) {
        try {
            std::string error;
            if (!readStructuredFile(schemaPath, submitOptions.schema, error)) {
                std::cerr << "Error: cannot read JSON Schema " << schemaPath << ": " << error << "\n";
                return 1;
            }
            auto schema = nlohmann::ordered_json::parse(submitOptions.schema);
            submitOptions.grammar = json_schema_to_grammar(schema, true);
            submitOptions.output_format = "json_schema";
        } catch (const std::exception& e) {
            std::cerr << "Error: invalid JSON Schema: " << e.what() << "\n";
            return 1;
        }
    } else if (!grammarPath.empty()) {
        std::string error;
        if (!readStructuredFile(grammarPath, submitOptions.grammar, error)) {
            std::cerr << "Error: cannot read GBNF grammar " << grammarPath << ": " << error << "\n";
            return 1;
        }
        submitOptions.output_format = "gbnf";
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
