#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using json = nlohmann::json;

struct Move {
    std::string file;
    std::string category;
};

static bool directName(const std::string &name) {
    return !name.empty() && name != "." && name != ".." &&
           name.find('/') == std::string::npos &&
           name.find('\\') == std::string::npos;
}

static std::string entryName(const fs::path &path) {
    return path.filename().string();
}

static void requireDirectName(const std::string &name, const std::string &kind) {
    if (!directName(name)) {
        throw std::runtime_error(kind + " must be a direct name: " + name);
    }
}

int main(int argc, char **argv) {
    try {
        bool apply = false;
        int first = 1;
        if (argc > 1 && std::string(argv[1]) == "--apply") {
            apply = true;
            first++;
        }
        if (argc - first < 3) {
            std::cerr << "usage: file-organizer-validator [--apply] INBOX PLAN CATEGORY...\n";
            return 2;
        }

        const fs::path inbox = argv[first++];
        const fs::path planPath = argv[first++];
        const auto inboxStatus = fs::symlink_status(inbox);
        if (fs::is_symlink(inboxStatus) || !fs::is_directory(inboxStatus)) {
            throw std::runtime_error("inbox must be a directory and not a symlink");
        }

        std::set<std::string> categories;
        for (; first < argc; first++) {
            const std::string category = argv[first];
            requireDirectName(category, "category");
            if (!categories.insert(category).second) {
                throw std::runtime_error("duplicate category: " + category);
            }
        }

        std::ifstream input(planPath);
        if (!input) {
            throw std::runtime_error("cannot read plan: " + planPath.string());
        }
        const json plan = json::parse(input);
        if (!plan.is_object() || !plan.contains("moves") ||
            !plan.contains("review") || !plan["moves"].is_array() ||
            !plan["review"].is_array()) {
            throw std::runtime_error("plan must contain moves and review arrays");
        }

        std::vector<Move> moves;
        std::set<std::string> plannedFiles;
        for (const auto &item : plan["moves"]) {
            if (!item.is_object() || !item.contains("file") ||
                !item.contains("category") || !item["file"].is_string() ||
                !item["category"].is_string()) {
                throw std::runtime_error("each move needs string file and category values");
            }
            const Move move{item["file"].get<std::string>(),
                            item["category"].get<std::string>()};
            requireDirectName(move.file, "file");
            if (!categories.count(move.category)) {
                throw std::runtime_error("category is not allowed: " + move.category);
            }
            if (!plannedFiles.insert(move.file).second) {
                throw std::runtime_error("duplicate file in plan: " + move.file);
            }
            moves.push_back(move);
        }

        std::vector<std::string> review;
        for (const auto &item : plan["review"]) {
            if (!item.is_string()) {
                throw std::runtime_error("review entries must be strings");
            }
            const std::string file = item.get<std::string>();
            requireDirectName(file, "file");
            if (!plannedFiles.insert(file).second) {
                throw std::runtime_error("duplicate file in plan: " + file);
            }
            review.push_back(file);
        }

        for (const auto &category : categories) {
            if (plannedFiles.count(category)) {
                throw std::runtime_error("category conflicts with a file name: " + category);
            }
        }

        std::map<std::string, std::string> locations;
        const auto recordFile = [&](const fs::path &path, const std::string &location) {
            const auto status = fs::symlink_status(path);
            if (fs::is_symlink(status)) {
                throw std::runtime_error("symlinks are not allowed: " + path.string());
            }
            if (!fs::is_regular_file(status)) {
                throw std::runtime_error("unsupported inbox entry: " + path.string());
            }
            const std::string file = entryName(path);
            if (!locations.emplace(file, location).second) {
                throw std::runtime_error("file exists in multiple locations: " + file);
            }
        };

        for (const auto &entry : fs::directory_iterator(inbox)) {
            const auto status = entry.symlink_status();
            const std::string name = entryName(entry.path());
            if (fs::is_symlink(status)) {
                throw std::runtime_error("symlinks are not allowed: " + entry.path().string());
            }
            if (fs::is_regular_file(status)) {
                recordFile(entry.path(), "");
                continue;
            }
            if (!fs::is_directory(status) || !categories.count(name)) {
                throw std::runtime_error("unknown inbox entry: " + name);
            }
            for (const auto &child : fs::directory_iterator(entry.path())) {
                recordFile(child.path(), name);
            }
        }

        for (const auto &[file, location] : locations) {
            (void)location;
            if (!plannedFiles.count(file)) {
                throw std::runtime_error("unknown file: " + file);
            }
        }
        for (const auto &file : plannedFiles) {
            if (!locations.count(file)) {
                throw std::runtime_error("planned file is missing: " + file);
            }
        }

        std::vector<Move> pending;
        for (const auto &move : moves) {
            const std::string &location = locations.at(move.file);
            if (location.empty()) {
                pending.push_back(move);
            } else if (location != move.category) {
                throw std::runtime_error("file is in the wrong category: " + move.file);
            }
        }
        for (const auto &file : review) {
            if (!locations.at(file).empty()) {
                throw std::runtime_error("review file was moved: " + file);
            }
        }

        for (const auto &move : pending) {
            std::cout << (apply ? "MOVE " : "WOULD MOVE ") << move.file << " -> "
                      << move.category << '/' << move.file << '\n';
        }
        for (const auto &file : review) {
            std::cout << "REVIEW " << file << '\n';
        }

        if (apply) {
            for (const auto &move : pending) {
                fs::create_directory(inbox / move.category);
            }
            for (const auto &move : pending) {
                fs::rename(inbox / move.file,
                           inbox / move.category / move.file);
            }
        }
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "file-organizer-validator: " << error.what() << '\n';
        return 1;
    }
}
