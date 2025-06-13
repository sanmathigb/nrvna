/*
* nrvna - Local AI Orchestration Engine
* Copyright (c) 2025 Sanmathi Bharamgouda
* SPDX-License-Identifier: MIT
*/

#pragma once
#include <string>

struct llama_model;
struct llama_context;
namespace nrvna {
    class Runner {
    public:
        explicit Runner(const std::string& modelLocation);
        std::string run(const std::string& content);
        ~Runner();
    private:
        llama_model* model_ = nullptr;
        llama_context* ctx_ = nullptr;

        std::string formatPrompt(const std::string& content);
    };
} // namespace nrvna