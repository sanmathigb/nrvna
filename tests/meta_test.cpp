#include "nrvna/meta.hpp"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <string>

using namespace nrvna;
namespace fs = std::filesystem;
using json = nlohmann::json;

namespace {

bool writeText(const fs::path& path, const std::string& text) {
    std::ofstream file(path, std::ios::binary);
    file << text;
    return file.good();
}

bool rejects(const fs::path& dir, const std::string& text) {
    return writeText(dir / "meta.json", text) && !readMetaJson(dir);
}

} // namespace

int main() {
    auto dir = fs::temp_directory_path() / "nrvna_meta_test";
    fs::remove_all(dir);
    fs::create_directories(dir);

    JobMeta in;
    in.submitted_at = "2026-07-11T00:00:00.000000Z";
    in.mode = "text";
    in.parent = "123_456";
    in.tags = {"night", "quote\"slash\\line\n", "caf\u00e9"};
    in.output_format = "json_schema";
    in.recovery_attempts = 2;
    in.completed_at = "2026-07-11T00:00:01.000000Z";
    in.duration_s = 1.234;
    in.artifacts = {"result.txt"};
    in.status = "done";

    if (!writeMetaJson(dir, in)) return 1;
    auto out = readMetaJson(dir);
    if (!out || out->submitted_at != in.submitted_at || out->mode != in.mode ||
        out->parent != in.parent || out->tags != in.tags ||
        out->output_format != in.output_format ||
        out->recovery_attempts != in.recovery_attempts ||
        out->completed_at != in.completed_at || out->duration_s != 1.23 ||
        out->artifacts != in.artifacts || out->status != in.status) return 2;

    JobMeta minimal;
    minimal.submitted_at = in.submitted_at;
    minimal.mode = "embed";
    if (!writeMetaJson(dir, minimal)) return 3;

    std::ifstream persistedFile(dir / "meta.json", std::ios::binary);
    json persisted = json::parse(persistedFile);
    if (persisted.size() != 2 || persisted.contains("parent") ||
        persisted.contains("recovery_attempts") || persisted.contains("status")) return 4;

    auto minimalOut = readMetaJson(dir);
    if (!minimalOut || !minimalOut->parent.empty() || !minimalOut->tags.empty() ||
        !minimalOut->output_format.empty() || minimalOut->recovery_attempts != 0 ||
        !minimalOut->completed_at.empty() || minimalOut->duration_s != -1.0 ||
        !minimalOut->artifacts.empty() || !minimalOut->status.empty()) return 5;

    if (!writeText(dir / "meta.json",
                   R"({"submitted_at":"time","mode":"text","future":{"value":1}})")) return 6;
    auto withUnknown = readMetaJson(dir);
    if (!withUnknown || withUnknown->submitted_at != "time" ||
        withUnknown->mode != "text") return 7;

    if (!rejects(dir, R"({"submitted_at":"time","mode":"text")")) return 8;
    if (!rejects(dir, R"({"mode":"text"})")) return 9;
    if (!rejects(dir, R"({"submitted_at":"","mode":"text"})")) return 10;
    if (!rejects(dir, R"({"submitted_at":"time","mode":1})")) return 11;
    if (!rejects(dir, R"({"submitted_at":"time","mode":"text","tags":"tag"})")) return 12;
    if (!rejects(dir, R"({"submitted_at":"time","mode":"text","tags":[1]})")) return 13;
    if (!rejects(dir, R"({"submitted_at":"time","mode":"text","recovery_attempts":-1})")) return 14;
    if (!rejects(dir, R"({"submitted_at":"time","mode":"text","recovery_attempts":4294967296})")) return 15;
    if (!rejects(dir, R"({"submitted_at":"time","mode":"text","duration_s":"fast"})")) return 16;
    if (!rejects(dir, R"({"submitted_at":"time","mode":"text","artifacts":[1]})")) return 17;
    if (!rejects(dir, R"(["not","metadata"])")) return 18;

    fs::remove_all(dir);
    std::puts("meta_test: all checks passed");
    return 0;
}
