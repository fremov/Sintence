#include "fallback_source.h"

namespace course {
FallbackMatchSource::FallbackMatchSource(const MatchSource& primary,
                                         const MatchSource& backup) {
    primary_ = &primary;
    backup_ = &backup;
}

const MatchSource* FallbackMatchSource::Active() const {
    if (primary_->IsAvailable()) {
        return primary_;
    } else if (backup_->IsAvailable()) {
        return backup_;
    }
    return nullptr;
}

std::string FallbackMatchSource::Name() const {
    if (Active() != nullptr) {
        return Active()->Name();
    }
    return "none";
}

bool FallbackMatchSource::IsAvailable() const {
    if (Active() != nullptr) {
        return Active()->IsAvailable();
    }
    return false;
}

std::vector<MatchEntry> FallbackMatchSource::LoadMatches() const {
    if (Active() != nullptr) {
        return Active()->LoadMatches();
    }
    return {};
}
} // namespace course