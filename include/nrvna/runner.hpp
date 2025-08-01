/*
* nrvna - Local AI Orchestration Engine
* Copyright (c) 2025 Sanmathi Bharamgouda
* SPDX-License-Identifier: MIT
*/

#pragma once
#include <string>
#include <memory>
#include <mutex>

struct llama_model;
struct llama_context;

namespace nrvna {
    class Runner {
    public:
        explicit Runner(const std::string& modelPath);
        ~Runner();
        std::string run(const std::string& content);

    private:
        static std::shared_ptr<llama_model> shared_model_;
        static std::string current_model_path_;
        static std::mutex model_mutex_;  // Added for thread safety

        llama_context* ctx_ = nullptr;

        std::string formatPrompt(const std::string& content);
    };
} // namespace nrvna