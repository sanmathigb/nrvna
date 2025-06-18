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

// Static members
std::shared_ptr<llama_model> Runner::shared_model_ = nullptr;
std::string Runner::current_model_path_ = "";

Runner::Runner(const std::string& modelLocation) {
    ggml_backend_load_all();

    // Load model only if different path or not loaded
    if (!shared_model_ || current_model_path_ != modelLocation) {
        llama_model_params model_params = llama_model_default_params();
        model_params.n_gpu_layers = 99;

        llama_model* model = llama_model_load_from_file(modelLocation.c_str(), model_params);
        if (!model) {
            throw std::runtime_error("Failed to load model: " + modelLocation);
        }

        shared_model_ = std::shared_ptr<llama_model>(model, llama_model_free);
        current_model_path_ = modelLocation;
    }
}

Runner::~Runner() {
    if (ctx_) llama_free(ctx_);
}

std::string Runner::run(const std::string& content) {
    std::string prompt = formatPrompt(content);
    const llama_vocab* vocab = llama_model_get_vocab(shared_model_.get());

    const int n_prompt = -llama_tokenize(vocab, prompt.c_str(), prompt.size(), NULL, 0, true, true);
    if (n_prompt <= 0) return "";

    std::vector<llama_token> prompt_tokens(n_prompt);
    if (llama_tokenize(vocab, prompt.c_str(), prompt.size(), prompt_tokens.data(), prompt_tokens.size(), true, true) < 0) {
        return "";
    }

    int n_predict = 1500;
    int required_context = n_prompt + n_predict + 100;
    int max_context = 2048;

    if (required_context > max_context) {
        n_predict = max_context - n_prompt - 100;
        if (n_predict < 200) return "";
    }

    llama_context_params ctx_params = llama_context_default_params();
    ctx_params.n_ctx = n_prompt + n_predict + 100;
    ctx_params.n_batch = n_prompt;
    ctx_params.no_perf = false;

    ctx_ = llama_init_from_model(shared_model_.get(), ctx_params);
    if (!ctx_) return "";

    auto sparams = llama_sampler_chain_default_params();
    sparams.no_perf = false;
    llama_sampler* smpl = llama_sampler_chain_init(sparams);
    llama_sampler_chain_add(smpl, llama_sampler_init_penalties(64, 1.1f, 0.0f, 0.0f));
    llama_sampler_chain_add(smpl, llama_sampler_init_greedy());

    llama_batch batch = llama_batch_get_one(prompt_tokens.data(), prompt_tokens.size());

    std::string output;
    llama_token new_token_id;

    for (int n_pos = 0; n_pos + batch.n_tokens < n_prompt + n_predict; ) {
        if (llama_decode(ctx_, batch)) break;
        n_pos += batch.n_tokens;
        new_token_id = llama_sampler_sample(smpl, ctx_, -1);
        if (llama_vocab_is_eog(vocab, new_token_id)) break;
        char buf[128];
        int n = llama_token_to_piece(vocab, new_token_id, buf, sizeof(buf), 0, true);
        if (n > 0) output.append(buf, n);
        batch = llama_batch_get_one(&new_token_id, 1);
    }

    llama_sampler_free(smpl);
    llama_free(ctx_);
    ctx_ = nullptr;
    return output;
}

std::string Runner::formatPrompt(const std::string& content) {
    return "<|user|>\n" + content + "\n<|assistant|>\n";
}

} // namespace nrvna