#include "../src/think_filter.hpp"
#include "../src/vision_sampling.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

static int fail(const char* name) {
    std::cerr << "FAIL: " << name << '\n';
    return 1;
}

int main() {
    using nrvna::detail::promptEndsWithOpenThinkBlock;
    using nrvna::detail::resolveVisionTemperature;
    using nrvna::detail::stripThinkBlocks;

    if (!promptEndsWithOpenThinkBlock("<|im_start|>assistant\n<think>")) return fail("open think prompt");
    if (promptEndsWithOpenThinkBlock("<think>old</think> answer")) return fail("closed think prompt");
    if (stripThinkBlocks("analysis</think>answer", true) != "answer") return fail("prompt-open thought");
    if (!stripThinkBlocks("analysis without close", true).empty()) return fail("truncated thought");
    if (stripThinkBlocks("before<think>hidden</think> after") != "before after") return fail("inline thought");
    if (stripThinkBlocks("visible answer") != "visible answer") return fail("plain answer");

    if (resolveVisionTemperature(0.1f, true, false, false, 0.9f) != 0.1f) return fail("GGUF hint");
    if (resolveVisionTemperature(0.6f, true, true, false, 0.9f) != 0.6f) return fail("general override");
    if (resolveVisionTemperature(0.1f, true, true, true, 0.2f) != 0.2f) return fail("vision override");
    if (resolveVisionTemperature(0.8f, false, false, false, 0.9f) != 0.3f) return fail("vision fallback");
    return 0;
}
