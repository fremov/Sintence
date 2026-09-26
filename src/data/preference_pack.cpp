#include "preference_pack.h"

#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <utility>

#include "json.hpp"
#include "json_helpers.h"

namespace sintence {

namespace {
// Версия формата пака. Меняется вместе со схемой в build_pack.py.
// 2: матчапы, страницы рун, цепочки предметов.
constexpr int kSchemaVersion = 2;

std::vector<std::string> ParseStrings(const nlohmann::json& parent,
                                      std::string_view key) {
    std::vector<std::string> values;
    if (!parent.contains(key) || !parent[key].is_array()) {
        return values;
    }
    for (const auto& raw : parent[key]) {
        if (raw.is_string()) {
            values.push_back(raw.get<std::string>());
        }
    }
    return values;
}

std::optional<RunePage> ParsePage(const nlohmann::json& parent) {
    if (!parent.contains("page") || !parent["page"].is_object()) {
        return std::nullopt;
    }
    const auto& raw = parent["page"];

    RunePage page;
    page.keystone = GetString(raw, "keystone").value_or("");
    page.primary_tree = GetString(raw, "primaryTree").value_or("");
    page.secondary_tree = GetString(raw, "secondaryTree").value_or("");
    page.primary = ParseStrings(raw, "primary");
    page.secondary = ParseStrings(raw, "secondary");
    page.shards = ParseStrings(raw, "shards");

    // Страница без ключевой руны или без малых — не страница:
    // повторить её игрок не сможет.
    if (page.keystone.empty() || page.primary.empty()) {
        return std::nullopt;
    }
    return page;
}

std::vector<PreferenceVariant> ParseVariants(const nlohmann::json& parent,
                                             std::string_view key) {
    std::vector<PreferenceVariant> variants;
    if (!parent.contains(key) || !parent[key].is_array()) {
        return variants;
    }

    for (const auto& raw : parent[key]) {
        if (!raw.is_object()) {
            continue;
        }
        PreferenceVariant variant;
        variant.name = GetString(raw, "name").value_or("");
        if (variant.name.empty()) {
            continue;
        }
        variant.games = GetInt(raw, "games").value_or(0);
        variant.share = GetDouble(raw, "share").value_or(0.0);
        variant.winrate = GetDouble(raw, "winrate").value_or(0.0);
        variant.winrate_low = GetDouble(raw, "winrateLow").value_or(0.0);
        variant.steps = ParseStrings(raw, "steps");
        variant.page = ParsePage(raw);
        variants.push_back(std::move(variant));
    }
    return variants;
}

std::string Upper(std::string_view text) {
    std::string upper;
    upper.reserve(text.size());
    for (const char symbol : text) {
        upper.push_back(static_cast<char>(
            std::toupper(static_cast<unsigned char>(symbol))));
    }
    return upper;
}

}  // namespace

std::string PreferencePack::TierBucket(std::string_view tier) {
    const std::string upper = Upper(tier);
    if (upper == "IRON" || upper == "BRONZE") {
        return "BRONZE";
    }
    if (upper == "SILVER" || upper == "GOLD") {
        return "GOLD";
    }
    if (upper == "PLATINUM" || upper == "EMERALD") {
        return "EMERALD";
    }
    if (upper == "DIAMOND" || upper == "MASTER" || upper == "GRANDMASTER" ||
        upper == "CHALLENGER") {
        return "DIAMOND";
    }
    // Ранг неизвестен (нет профиля, не играл в соло) — общая корзина.
    return "ALL";
}

std::optional<PreferencePack> PreferencePack::LoadFromJson(std::string_view json_text) {
    const nlohmann::json doc = nlohmann::json::parse(json_text, nullptr, false);
    if (doc.is_discarded() || !doc.is_object()) {
        return std::nullopt;
    }

    // Чужая версия формата — отказ целиком. Пак, разобранный наполовину,
    // показал бы правдоподобную чушь, и это хуже пустого места.
    if (GetInt(doc, "schemaVersion").value_or(0) != kSchemaVersion) {
        return std::nullopt;
    }
    if (!doc.contains("buckets") || !doc["buckets"].is_object()) {
        return std::nullopt;
    }

    PreferencePack pack;
    pack.patch_ = GetString(doc, "patch").value_or("");
    pack.region_ = GetString(doc, "region").value_or("");

    for (const auto& [key, raw] : doc["buckets"].items()) {
        if (!raw.is_object()) {
            continue;
        }
        PreferenceBucket bucket;
        bucket.champion = GetString(raw, "champion").value_or("");
        bucket.role = GetString(raw, "role").value_or("");
        bucket.tier = GetString(raw, "tier").value_or("ALL");
        bucket.patch = GetString(raw, "patch").value_or(pack.patch_);
        bucket.games = GetInt(raw, "games").value_or(0);
        bucket.winrate = GetDouble(raw, "winrate").value_or(0.0);
        bucket.opponent = GetString(raw, "opponent").value_or("ANY");
        bucket.rune_pages = ParseVariants(raw, "runePages");
        bucket.skill_orders = ParseVariants(raw, "skillOrders");
        bucket.item_chains = ParseVariants(raw, "itemChains");

        if (bucket.champion.empty() || bucket.role.empty()) {
            continue;
        }
        pack.buckets_.emplace(key, std::move(bucket));
    }

    return pack;
}

std::optional<PreferencePack> PreferencePack::LoadFromFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return std::nullopt;
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return LoadFromJson(buffer.str());
}

std::optional<PreferencePack> PreferencePack::LoadFromDirectory(const std::string& dir) {
    std::ifstream index_file(std::filesystem::path(dir) / "index.json", std::ios::binary);
    if (!index_file) {
        return std::nullopt;
    }
    std::ostringstream index_buffer;
    index_buffer << index_file.rdbuf();

    const nlohmann::json index =
        nlohmann::json::parse(index_buffer.str(), nullptr, false);
    if (index.is_discarded() || !index.is_object()) {
        return std::nullopt;
    }

    const auto file = GetString(index, "file");
    if (!file || file->empty()) {
        return std::nullopt;
    }
    return LoadFromFile((std::filesystem::path(dir) / *file).string());
}

const PreferenceBucket* PreferencePack::Lookup(std::string_view champion,
                                               std::string_view role,
                                               std::string_view opponent,
                                               std::string_view tier) const {
    if (champion.empty() || role.empty()) {
        return nullptr;
    }

    const std::string bucket_tier = TierBucket(tier);
    const auto key = [&](std::string_view versus, std::string_view rank) {
        std::string result;
        result.reserve(champion.size() + role.size() + versus.size() + rank.size() + 3);
        result.append(champion).append("|").append(role).append("|");
        result.append(versus).append("|").append(rank);
        return result;
    };

    // Лестница отката: чем точнее бакет, тем реже в нём хватает выборки.
    // Матчапов около сорока тысяч, и пустой бакет здесь — норма, а не сбой.
    std::vector<std::string> candidates;
    if (!opponent.empty() && opponent != "ANY") {
        candidates.push_back(key(opponent, bucket_tier));
        if (bucket_tier != "ALL") {
            candidates.push_back(key(opponent, "ALL"));
        }
    }
    candidates.push_back(key("ANY", bucket_tier));
    if (bucket_tier != "ALL") {
        candidates.push_back(key("ANY", "ALL"));
    }

    // Бакет без единого варианта бесполезен: игр в нём набралось,
    // а повторяющейся сборки — нет. Такой пропускается, и лестница
    // спускается дальше, к более широкой выборке. Иначе точный матчап
    // на десяти играх вытеснял бы «против всех» на восьмидесяти,
    // где варианты как раз есть.
    for (const std::string& candidate : candidates) {
        const auto found = buckets_.find(candidate);
        if (found == buckets_.end()) {
            continue;
        }
        const PreferenceBucket& bucket = found->second;
        if (bucket.rune_pages.empty() && bucket.skill_orders.empty() &&
            bucket.item_chains.empty()) {
            continue;
        }
        return &bucket;
    }
    return nullptr;
}

}  // namespace sintence
