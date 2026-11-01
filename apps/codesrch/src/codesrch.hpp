#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace codesrch {

struct Chunk {
    std::string path;
    std::uint32_t first_line = 0;
    std::uint32_t last_line = 0;
    std::string text;
    std::string key;
};

struct Record : Chunk {
    std::vector<float> vector;
    std::uint64_t vector_offset = 0;
    double score = 0;
};

std::vector<Chunk> splitText(const std::string& path, const std::string& text,
                             std::size_t window = 40, std::size_t overlap = 8,
                             std::size_t maxBytes = 1500);
std::uint64_t fingerprint(const std::string& value);
void appendU40(std::vector<unsigned char>& out, std::uint64_t value);
std::uint64_t readU40(const unsigned char* data);
double cosine(const std::vector<float>& a, const std::vector<float>& b);
void rankHybrid(std::vector<Record>& records, const std::string& query,
                std::size_t topK);

int run(int argc, char** argv);

} // namespace codesrch
