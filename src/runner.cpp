/*
* nrvna - Local AI Orchestration Engine
* Copyright (c) 2025 Sanmathi Bharamgouda
* SPDX-License-Identifier: MIT
*/

#include "nrvna/runner.hpp"
#include "llama.h"
#include <vector>
#include <stdexcept>

namespace nrvna {
Runner::Runner(const std::string& modelLocation) {
    // Exactly from simple.cpp
    ggml_backend_load_all();

    llama_model_params model_params = llama_model_default_params();
    model_params.n_gpu_layers = 99;

    model_ = llama_model_load_from_file(modelLocation.c_str(), model_params);
    if (!model_) {
        throw std::runtime_error("Failed to load model: " + modelLocation);
    }
}
Runner::~Runner() {
    if (ctx_) llama_free(ctx_);
    if (model_) llama_model_free(model_);
}
std::string Runner::run(const std::string& content) {
    std::string prompt = formatPrompt(content);
    const llama_vocab* vocab = llama_model_get_vocab(model_);
    const int n_prompt = -llama_tokenize(vocab, prompt.c_str(), prompt.size(), NULL, 0, true, true);
    std::vector<llama_token> prompt_tokens(n_prompt);
    if (llama_tokenize(vocab, prompt.c_str(), prompt.size(), prompt_tokens.data(), prompt_tokens.size(), true, true) < 0) {
        throw std::runtime_error("Failed to tokenize prompt");
    }
    int n_predict = 1500;
    llama_context_params ctx_params = llama_context_default_params();
    ctx_params.n_ctx = n_prompt + n_predict - 1;
    ctx_params.n_batch = n_prompt;
    ctx_params.no_perf = false;
    ctx_ = llama_init_from_model(model_, ctx_params);
    if (!ctx_) {
        throw std::runtime_error("Failed to create context");
    }
    auto sparams = llama_sampler_chain_default_params();
    sparams.no_perf = false;
    llama_sampler* smpl = llama_sampler_chain_init(sparams);
    llama_sampler_chain_add(smpl, llama_sampler_init_greedy());
    llama_batch batch = llama_batch_get_one(prompt_tokens.data(), prompt_tokens.size());
    // Generate response
    std::string output;
    llama_token new_token_id;
    for (int n_pos = 0; n_pos + batch.n_tokens < n_prompt + n_predict; ) {
        if (llama_decode(ctx_, batch)) {
            break;
        }
        n_pos += batch.n_tokens;
        new_token_id = llama_sampler_sample(smpl, ctx_, -1);
        if (llama_vocab_is_eog(vocab, new_token_id)) {
            break;
        }
        char buf[128];
        int n = llama_token_to_piece(vocab, new_token_id, buf, sizeof(buf), 0, true);
        if (n > 0) {
            output.append(buf, n);
        }
        batch = llama_batch_get_one(&new_token_id, 1);
    }
    // Cleanup
    llama_sampler_free(smpl);
    llama_free(ctx_);
    ctx_ = nullptr;

    return output;
}
std::string Runner::formatPrompt(const std::string& content) {
    return "Technical Request: " + content + "\n\nProvide a detailed technical response:\n\n";
}
} // namespace nrvna