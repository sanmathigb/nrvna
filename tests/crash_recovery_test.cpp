#include "nrvna/meta.hpp"
#include "nrvna/server.hpp"

#include <csignal>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sys/wait.h>
#include <unistd.h>

using namespace nrvna;
namespace fs = std::filesystem;

static void writeOrphanedJob(const fs::path& ws, const JobId& job) {
    auto jobDir = ws / "processing" / job;
    fs::create_directories(jobDir);

    std::ofstream(jobDir / "prompt.txt") << "stuck\n";
    std::ofstream(jobDir / "meta.json")
        << "{\n"
        << "  \"submitted_at\": \"2026-08-06T00:00:00.000000Z\",\n"
        << "  \"mode\": \"text\",\n"
        << "  \"recovery_attempts\": 0\n"
        << "}\n";
}

int main() {
    auto ws = fs::temp_directory_path() / "nrvna_crash_recovery_test";
    fs::remove_all(ws);
    fs::create_directories(ws / "processing");
    fs::create_directories(ws / "input/ready");
    fs::create_directories(ws / "failed");

    int pipefd[2];
    if (pipe(pipefd) != 0) {
        std::perror("pipe");
        return 1;
    }

    const JobId job = "00001784899999999998_4090_000000";
    pid_t pid = fork();
    if (pid < 0) {
        std::perror("fork");
        return 2;
    }

    if (pid == 0) {
        close(pipefd[0]);
        writeOrphanedJob(ws, job);
        if (write(pipefd[1], "1", 1) != 1) {
            _exit(3);
        }
        pause();
        _exit(4);
    }

    close(pipefd[1]);
    char ready = 0;
    if (read(pipefd[0], &ready, 1) != 1) {
        std::perror("read");
        kill(pid, SIGKILL);
        waitpid(pid, nullptr, 0);
        return 5;
    }

    if (::kill(pid, SIGKILL) != 0) {
        std::perror("kill");
        waitpid(pid, nullptr, 0);
        return 6;
    }

    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        std::perror("waitpid");
        return 7;
    }
    if (!WIFSIGNALED(status) || WTERMSIG(status) != SIGKILL) {
        std::cerr << "worker was not killed by SIGKILL\n";
        return 8;
    }

    auto report = recoverOrphanedJobs(ws, 1);
    if (report.recovered != 1 || report.terminalized != 0) return 9;

    auto readyDir = ws / "input/ready" / job;
    auto failedDir = ws / "failed" / job;
    if (!fs::exists(readyDir)) return 10;
    if (fs::exists(failedDir)) return 11;

    auto meta = readMetaJson(readyDir);
    if (!meta || meta->recovery_attempts != 1) return 12;

    fs::remove_all(ws);
    std::puts("crash_recovery_test: all checks passed");
    return 0;
}
