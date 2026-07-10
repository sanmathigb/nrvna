// Unit tests for the job contract. Plain asserts, no framework, no llama.
#include "nrvna/contract.hpp"
#include <cstdio>
#include <fstream>
#include <string>

using namespace nrvnaai;
namespace fs = std::filesystem;

static int failures = 0;
#define CHECK(cond) do { \
    if (!(cond)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++failures; } \
} while (0)

int main() {
    // State directories
    CHECK(contract::stateDir("/ws", Status::Queued)  == fs::path("/ws/input/ready"));
    CHECK(contract::stateDir("/ws", Status::Running) == fs::path("/ws/processing"));
    CHECK(contract::stateDir("/ws", Status::Done)    == fs::path("/ws/output"));
    CHECK(contract::stateDir("/ws", Status::Failed)  == fs::path("/ws/failed"));
    CHECK(contract::stateDir("/ws", Status::Missing).empty());
    CHECK(contract::jobDir("/ws", Status::Done, "42") == fs::path("/ws/output/42"));
    CHECK(contract::jobDir("/ws", Status::Missing, "42").empty());

    // Status names (as written to meta.json and --json)
    CHECK(std::string(contract::toString(Status::Queued))  == "queued");
    CHECK(std::string(contract::toString(Status::Running)) == "running");
    CHECK(std::string(contract::toString(Status::Done))    == "done");
    CHECK(std::string(contract::toString(Status::Failed))  == "failed");
    CHECK(std::string(contract::toString(Status::Missing)) == "missing");

    // JobType round-trip through type.txt spelling
    for (JobType t : {JobType::Text, JobType::Embed, JobType::Vision, JobType::Tts, JobType::Stt}) {
        CHECK(contract::parseJobType(contract::toString(t)) == t);
    }
    CHECK(contract::parseJobType("") == JobType::Text);        // absent type.txt
    CHECK(contract::parseJobType("garbage") == JobType::Text); // unknown = text (today's behavior)

    // Job ID grammar (moved from Flow::isValidJobId)
    CHECK(contract::isValidJobId("00001781482179019396_4090_000000"));
    CHECK(!contract::isValidJobId(""));
    CHECK(!contract::isValidJobId("_123"));
    CHECK(!contract::isValidJobId("123_"));
    CHECK(!contract::isValidJobId("12__34"));
    CHECK(!contract::isValidJobId("abc"));
    CHECK(!contract::isValidJobId(std::string(129, '1')));

    // Output artifact rule: result > transcript > audio > embedding
    {
        auto tmp = fs::temp_directory_path() / "contract_test_job";
        fs::remove_all(tmp);
        fs::create_directories(tmp);

        CHECK(!contract::findOutputArtifact(tmp).has_value());

        std::ofstream(tmp / contract::kEmbeddingFile) << "{}";
        auto a = contract::findOutputArtifact(tmp);
        CHECK(a && a->kind == contract::ArtifactKind::Embedding);
        CHECK(a && a->path == tmp / contract::kEmbeddingFile);

        std::ofstream(tmp / contract::kAudioFile) << "x";
        a = contract::findOutputArtifact(tmp);
        CHECK(a && a->kind == contract::ArtifactKind::Audio);

        std::ofstream(tmp / contract::kTranscriptFile) << "x";
        a = contract::findOutputArtifact(tmp);
        CHECK(a && a->kind == contract::ArtifactKind::Transcript);

        std::ofstream(tmp / contract::kResultFile) << "x";
        a = contract::findOutputArtifact(tmp);
        CHECK(a && a->kind == contract::ArtifactKind::Result);
        CHECK(std::string(contract::toString(a->kind)) == "result");

        fs::remove_all(tmp);
    }

    if (failures) { std::printf("%d failure(s)\n", failures); return 1; }
    std::printf("contract_test: all checks passed\n");
    return 0;
}
