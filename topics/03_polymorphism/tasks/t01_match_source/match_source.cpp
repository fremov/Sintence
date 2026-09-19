#include "match_source.h"

namespace course {
FixtureMatchSource::FixtureMatchSource(std::string name,
                                       std::vector<MatchEntry> entries,
                                       bool available) : name_(std::move(name)),
    entries_(std::move(entries)), available_(available) {
}

std::string FixtureMatchSource::Name() const {
    return name_;
}

bool FixtureMatchSource::IsAvailable() const {
    return available_;
}

std::vector<MatchEntry> FixtureMatchSource::LoadMatches() const {
    if (!available_) {
        return std::vector<MatchEntry>();
    }
    return entries_;
}
} // namespace course