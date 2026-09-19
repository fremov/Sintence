#include "champion_pool.h"

#include "champion.h"  // тема 1 — нормализация роли уже написана там

namespace course {
void ChampionPool::Add(const std::string& champion_name,
                       const std::string& role) {
    if (champion_name.empty()) {
        return;
    }
    Champion champion(champion_name, role);
    champions_.insert(champion.Name());
    roles_[champion.Name()].insert(champion.Role());
}

const std::set<std::string>& ChampionPool::Champions() const {
    return champions_;
}

std::set<std::string> ChampionPool::RolesOf(
    const std::string& champion_name) const {
    
    if (const auto found = roles_.find(champion_name); found == roles_.end()) {
        return {};    
    } else {
        return found->second;
    }
}

bool ChampionPool::Contains(const std::string& champion_name) const {
    return champions_.contains(champion_name);
}

std::size_t ChampionPool::Size() const {
    return champions_.size();
}
} // namespace course