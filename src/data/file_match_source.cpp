#include "file_match_source.h"

#include <fstream>
#include <sstream>
#include <system_error>
#include <utility>

#include "match_json.h"

namespace course {
namespace  {
bool IsMatchCandidate(const fs::path& path) {
    return (path.extension() == ".json" && path.filename() != "index.json");
}
}
FileMatchSource::FileMatchSource(std::filesystem::path dir, std::string puuid)
    : dir_(std::move(dir)), puuid_(std::move(puuid)) {
}

std::string FileMatchSource::Name() const {
    return "files:" + dir_.filename().string();
}

bool FileMatchSource::IsAvailable() const {
    return JsonFileCount() > 0;
}

std::vector<MatchEntry> FileMatchSource::LoadMatches() const {
    if (!IsAvailable()) {
        return {};
    }
    std::vector<MatchEntry> result;
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(dir_, ec)) {
        if (IsMatchCandidate(entry.path())) {
            std::ifstream file(entry.path().string());
            if (!file) {
                continue;
            }
            std::ostringstream buffer;
            buffer << file.rdbuf();
            auto parsed = ParseMatchEntry(buffer.str(), puuid_);
            if (!parsed) {
                continue;
            }
            result.push_back(std::move(*parsed));
        }
    }

    return result;
}

std::size_t FileMatchSource::JsonFileCount() const {
    std::error_code ec;
    if (!fs::is_directory(dir_, ec)) {
        return 0;
    }
    size_t counter = 0;
    for (const auto& entry : fs::directory_iterator(dir_, ec)) {
        if (IsMatchCandidate(entry.path())) {
            ++counter;
        }
    }
    return counter;
}
} // namespace course