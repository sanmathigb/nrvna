#include "nrvna/meta.hpp"
#include "nrvna/scanner.hpp"
#include "nrvna/server.hpp"
#include "nrvna/work.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

using namespace nrvna;
namespace fs = std::filesystem;

namespace {
int failures = 0;

#define CHECK(condition) do { \
    if (!(condition)) { \
        std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        ++failures; \
    } \
} while (0)

std::string readText(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}
}

int main() {
    const auto root = fs::temp_directory_path() / "nrvna_filesystem_job_test";
    fs::remove_all(root);
    const auto workspace = root / "workspace";
    const auto outside = root / "outside";
    fs::create_directories(root);

    const auto image = root / "image.png";
    const auto audio = root / "audio.wav";
    std::ofstream(image, std::ios::binary) << "image";
    std::ofstream(audio, std::ios::binary) << "audio";

    Work work(workspace);
    const auto imageJob = work.submit("", JobType::Embed, {image});
    const auto audioJob = work.submitAudio("", {audio});
    CHECK(imageJob.ok);
    CHECK(audioJob.ok);
    CHECK(!fs::exists(workspace / "input/ready" / imageJob.id / "images"));
    CHECK(!fs::exists(workspace / "input/ready" / audioJob.id / "audio"));

    Scanner scanner(workspace);
    auto jobs = scanner.scan();
    CHECK(jobs.size() == 2);
    CHECK(scanner.readyJobCount() == 2);

    fs::create_directories(outside / "images");
    fs::create_directories(outside / "audio");
    std::ofstream(outside / "prompt.txt") << "outside prompt\n";
    const std::string outsideMeta =
        "{\"submitted_at\":\"time\",\"mode\":\"text\",\"keep_files\":false}\n";
    std::ofstream(outside / "meta.json") << outsideMeta;
    std::ofstream(outside / "images/keep.txt") << "keep image data";
    std::ofstream(outside / "audio/keep.txt") << "keep audio data";

    const auto readyLink = workspace / "input/ready/ready-escape";
    fs::create_directory_symlink(outside, readyLink);
    jobs = scanner.scan();
    CHECK(jobs.size() == 2);
    CHECK(scanner.readyJobCount() == 2);
    CHECK(readText(outside / "meta.json") == outsideMeta);
    CHECK(fs::exists(outside / "images/keep.txt"));
    CHECK(fs::exists(outside / "audio/keep.txt"));

    const auto processingLink = workspace / "processing/processing-escape";
    fs::create_directory_symlink(outside, processingLink);
    const auto report = recoverOrphanedJobs(workspace, 3);
    CHECK(report.recovered == 0);
    CHECK(report.terminalized == 0);
    CHECK(!fs::exists(workspace / "input/ready/processing-escape"));
    CHECK(!fs::exists(workspace / "failed/processing-escape"));
    CHECK(readText(outside / "meta.json") == outsideMeta);
    CHECK(fs::exists(outside / "images/keep.txt"));
    CHECK(fs::exists(outside / "audio/keep.txt"));

    const JobId linkedId = "linked-id";
    fs::create_directory_symlink(outside, workspace / "output" / linkedId);
    SubmitOptions linkedOptions;
    linkedOptions.id = linkedId;
    const auto linkedJob = work.submit("new work", JobType::Text, {}, linkedOptions);
    CHECK(!linkedJob.ok);
    CHECK(!fs::exists(workspace / "input/ready" / linkedId));
    CHECK(readText(outside / "meta.json") == outsideMeta);

    fs::remove_all(root);
    if (failures != 0) return 1;
    std::puts("filesystem-job-test: all checks passed");
    return 0;
}
