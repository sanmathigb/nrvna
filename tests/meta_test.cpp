#include "nrvna/meta.hpp"
#include <cstdio>
#include <filesystem>
#include <fstream>

using namespace nrvna;
namespace fs = std::filesystem;

int main() {
    auto dir = fs::temp_directory_path() / "nrvna_meta_test";
    fs::remove_all(dir);
    fs::create_directories(dir);

    JobMeta in;
    in.submitted_at = "2026-07-11T00:00:00.000000Z";
    in.mode = "text";
    in.parent = "123_456";
    in.tags = {"night", "quoted-tag"};
    in.output_format = "json_schema";
    in.completed_at = "2026-07-11T00:00:01.000000Z";
    in.duration_s = 1.25;
    in.artifacts = {"result.txt"};
    in.status = "done";

    if (!writeMetaJson(dir, in)) return 1;
    auto out = readMetaJson(dir);
    if (!out || out->submitted_at != in.submitted_at || out->mode != in.mode ||
        out->parent != in.parent || out->tags != in.tags ||
        out->output_format != in.output_format ||
        out->artifacts != in.artifacts || out->status != in.status) return 2;

    std::ofstream(dir / "meta.json") << "{\"status\":\"done\"}\n";
    if (readMetaJson(dir)) return 3;

    fs::remove_all(dir);
    std::puts("meta_test: all checks passed");
    return 0;
}
