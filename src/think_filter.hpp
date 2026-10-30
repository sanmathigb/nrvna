#pragma once

#include <string>

namespace nrvna::detail {

inline bool promptEndsWithOpenThinkBlock(const std::string& prompt) {
    const auto start = prompt.rfind("<think>");
    const auto end = prompt.rfind("</think>");
    return start != std::string::npos && (end == std::string::npos || start > end);
}

inline std::string trimThinkWhitespace(const std::string& text) {
    const auto start = text.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) return "";
    const auto end = text.find_last_not_of(" \t\n\r");
    return text.substr(start, end - start + 1);
}

inline void eraseThinkRange(std::string& text, size_t begin, size_t end) {
    const bool needs_separator = begin > 0 && end < text.size() &&
        text[begin - 1] != ' ' && text[begin - 1] != '\t' && text[begin - 1] != '\n' && text[begin - 1] != '\r' &&
        text[end] != ' ' && text[end] != '\t' && text[end] != '\n' && text[end] != '\r';
    text.erase(begin, end - begin);
    if (needs_separator) text.insert(begin, " ");
}

// Remove reasoning markers, including an opening marker carried by the prompt.
inline std::string stripThinkBlocks(const std::string& text, bool starts_inside_think = false) {
    std::string result = text;

    if (starts_inside_think) {
        const auto end = result.find("</think>");
        if (end == std::string::npos) return "";
        result.erase(0, end + 8);
    }

    size_t pos = 0;
    while ((pos = result.find("<think>", pos)) != std::string::npos) {
        const auto end = result.find("</think>", pos);
        if (end == std::string::npos) {
            result.erase(pos);
        } else {
            size_t after = end + 8;
            while (after < result.size() && (result[after] == ' ' || result[after] == '\t' ||
                                              result[after] == '\n' || result[after] == '\r')) ++after;
            eraseThinkRange(result, pos, after);
        }
    }

    pos = 0;
    while ((pos = result.find("<|channel>thought", pos)) != std::string::npos) {
        const auto end = result.find("<channel|>", pos);
        if (end == std::string::npos) {
            result.erase(pos);
        } else {
            size_t after = end + 10;
            while (after < result.size() && (result[after] == ' ' || result[after] == '\t' ||
                                              result[after] == '\n' || result[after] == '\r')) ++after;
            eraseThinkRange(result, pos, after);
        }
    }

    pos = 0;
    while ((pos = result.find("[Start thinking]", pos)) != std::string::npos) {
        auto end = result.find("[/End thinking]", pos);
        if (end == std::string::npos) end = result.find("<channel|>", pos);
        if (end == std::string::npos) {
            result.erase(pos);
        } else {
            const size_t marker_size = result.compare(end, 15, "[/End thinking]") == 0 ? 15 : 10;
            size_t after = end + marker_size;
            while (after < result.size() && (result[after] == ' ' || result[after] == '\t' ||
                                              result[after] == '\n' || result[after] == '\r')) ++after;
            eraseThinkRange(result, pos, after);
        }
    }

    return trimThinkWhitespace(result);
}

} // namespace nrvna::detail
