#include "codesrch.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {
void check(bool ok, const char* message) {
    if (!ok) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}
}

int main() {
    const auto chunks = codesrch::splitText("a.cpp", "one\ntwo\nthree\nfour\n", 3, 1);
    check(chunks.size() == 2, "overlapping line chunks");
    check(chunks[0].first_line == 1 && chunks[0].last_line == 3,
          "first chunk line range");
    check(chunks[1].first_line == 3 && chunks[1].last_line == 4,
          "second chunk line range");
    check(chunks[1].text == "three\nfour", "chunk text");
    const auto longLine = codesrch::splitText("a.cpp", "abcdefghij", 3, 0, 4);
    check(longLine.size() == 3, "long code line splits into bounded chunks");
    check(longLine[0].text == "abcd" && longLine[1].text == "efgh" &&
          longLine[2].text == "ij", "long line fragments preserve source text");
    const auto boundedChunks = codesrch::splitText("large.cpp", std::string(5000, 'x'));
    check(boundedChunks.size() == 4, "large line splits at the default byte limit");
    for (const auto& chunk : boundedChunks)
        check(chunk.text.size() <= 1500, "default chunk stays under the byte limit");

    std::vector<unsigned char> packed;
    codesrch::appendU40(packed, 0xFEDCBA9876ULL);
    check(packed.size() == 5, "five-byte offset size");
    check(codesrch::readU40(packed.data()) == 0xFEDCBA9876ULL,
          "five-byte offset round trip");
    bool rejectedWideOffset = false;
    try { codesrch::appendU40(packed, 1ULL << 40); }
    catch (const std::out_of_range&) { rejectedWideOffset = true; }
    check(rejectedWideOffset, "five-byte offset rejects overflow");
    check(codesrch::fingerprint("same") == codesrch::fingerprint("same"),
          "stable fingerprint");
    check(codesrch::fingerprint("same") != codesrch::fingerprint("other"),
          "different fingerprints");
    check(std::abs(codesrch::cosine({1, 0}, {1, 0}) - 1.0) < 1e-9,
          "cosine identical vectors");
    check(std::abs(codesrch::cosine({1, 0}, {0, 1})) < 1e-9,
          "cosine orthogonal vectors");
    check(codesrch::cosine({1}, {1, 0}) == -1, "cosine rejects dimension mismatch");

    codesrch::Record semanticOnly;
    semanticOnly.path = "semantic.cpp";
    semanticOnly.text = "the handler validates user credentials";
    semanticOnly.vector = {1.0f, 0.0f};
    semanticOnly.score = 1.0;
    codesrch::Record exactCode;
    exactCode.path = "auth.go";
    exactCode.text = "func Register() { require POST authentication }";
    exactCode.vector = {0.9f, 0.1f};
    exactCode.score = 0.9;
    std::vector<codesrch::Record> ranked{semanticOnly, exactCode};
    codesrch::rankHybrid(ranked, "where does POST require authentication", 2);
    check(ranked.front().path == "auth.go", "hybrid ranking promotes exact code terms");

    std::cout << "codesrch unit checks passed\n";
}
