#include "nrvna/server.hpp"
#include "nrvna/meta.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>

using namespace nrvna;
namespace fs = std::filesystem;

int main() {
    auto ws = fs::temp_directory_path() / "nrvna_recovery_test";
    fs::remove_all(ws);
    fs::create_directories(ws / "processing");
    fs::create_directories(ws / "input/ready");
    fs::create_directories(ws / "failed");

    const JobId job = "00001784899999999999_4090_000000";
    auto jobDir = ws / "processing" / job;
    fs::create_directories(jobDir);

    {
        std::ofstream(jobDir / "prompt.txt") << "stuck\n";
        std::ofstream(jobDir / "meta.json")
            << "{\n"
            << "  \"submitted_at\": \"2026-08-06T00:00:00.000000Z\",\n"
            << "  \"mode\": \"text\",\n"
            << "  \"recovery_attempts\": 1\n"
            << "}\n";
    }

    auto report = recoverOrphanedJobs(ws, 1);
    if (report.recovered != 0 || report.terminalized != 1) return 1;

    auto readyDir = ws / "input/ready" / job;
    auto failedDir = ws / "failed" / job;
    if (fs::exists(readyDir)) return 2;
    if (!fs::exists(failedDir)) return 3;

    std::ifstream errorFile(failedDir / "error.txt");
    std::string error((std::istreambuf_iterator<char>(errorFile)),
                      std::istreambuf_iterator<char>());
    if (error.find("recovery ceiling") == std::string::npos) return 4;

    auto meta = readMetaJson(failedDir);
    if (!meta || meta->recovery_attempts != 1 || meta->status != "failed") return 5;

    fs::remove_all(ws);
    std::puts("recovery_test: all checks passed");
    return 0;
}
