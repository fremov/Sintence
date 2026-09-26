#include "lcu_json.h"

#include <cctype>
#include <charconv>

#include "json.hpp"
#include "json_helpers.h"

namespace sintence {

namespace {

// "middle" -> "MIDDLE". Роли пака те же пять, что у match-v5.
std::string PackRole(std::string_view position) {
    std::string upper;
    for (const char symbol : position) {
        upper.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(symbol))));
    }
    if (upper == "TOP" || upper == "JUNGLE" || upper == "MIDDLE" || upper == "BOTTOM" ||
        upper == "UTILITY") {
        return upper;
    }
    return {};
}

// Id заклинания или чемпиона. Клиент кладёт туда 64-битные числа
// (2^64-1 значит «не выбрано»), поэтому читаем как беззнаковое и
// отбрасываем всё, что не похоже на настоящий id.
int SmallId(const nlohmann::json& obj, std::string_view key) {
    if (!obj.contains(key)) {
        return 0;
    }
    const auto& value = obj[key];
    if (value.is_number_unsigned()) {
        const auto raw = value.get<unsigned long long>();
        return raw < 100000 ? static_cast<int>(raw) : 0;
    }
    if (value.is_number_integer()) {
        const auto raw = value.get<long long>();
        return raw > 0 && raw < 100000 ? static_cast<int>(raw) : 0;
    }
    return 0;
}

std::string RiotIdOf(const nlohmann::json& obj) {
    const auto name = GetString(obj, "gameName").value_or("");
    const auto tag = GetString(obj, "tagLine").value_or("");
    if (name.empty() || tag.empty()) {
        return {};
    }
    return name + "#" + tag;
}

void ParseTeam(const nlohmann::json& session, std::string_view key, LobbySide side,
               int local_cell, Lobby& lobby) {
    if (!session.contains(key) || !session[key].is_array()) {
        return;
    }
    for (const auto& raw : session[key]) {
        if (!raw.is_object()) {
            continue;
        }
        LobbyMember member;
        member.side = side;
        member.cell_id = GetInt(raw, "cellId").value_or(-1);
        member.champion_id = SmallId(raw, "championId");
        member.pick_intent_id = SmallId(raw, "championPickIntent");
        member.position = PackRole(GetString(raw, "assignedPosition").value_or(""));
        member.spell1_id = SmallId(raw, "spell1Id");
        member.spell2_id = SmallId(raw, "spell2Id");
        member.is_self = side == LobbySide::Ally && member.cell_id == local_cell;

        // Противника в выборе чемпиона игрок не видит — и мы не берём.
        if (side == LobbySide::Ally) {
            member.hidden = GetString(raw, "nameVisibilityType").value_or("") == "HIDDEN";
            if (!member.hidden || member.is_self) {
                member.puuid = GetString(raw, "puuid").value_or("");
                member.riot_id = RiotIdOf(raw);
            }
        }
        lobby.members.push_back(std::move(member));
    }
}

void ParseBans(const nlohmann::json& bans, std::string_view key, std::vector<int>& out) {
    if (!bans.contains(key) || !bans[key].is_array()) {
        return;
    }
    for (const auto& raw : bans[key]) {
        if (raw.is_number_integer() && raw.get<long long>() > 0) {
            out.push_back(static_cast<int>(raw.get<long long>()));
        }
    }
}

}  // namespace

std::optional<LcuCredentials> ParseLockfile(std::string_view text) {
    // Хвостовой перевод строки бывает, если файл пересохранили руками.
    while (!text.empty() && (text.back() == '\n' || text.back() == '\r' || text.back() == ' ')) {
        text.remove_suffix(1);
    }

    std::vector<std::string_view> fields;
    std::size_t start = 0;
    for (std::size_t i = 0; i <= text.size(); ++i) {
        if (i == text.size() || text[i] == ':') {
            fields.push_back(text.substr(start, i - start));
            start = i + 1;
        }
    }
    if (fields.size() != 5) {
        return std::nullopt;
    }

    LcuCredentials credentials;
    const auto port_text = fields[2];
    const auto result =
        std::from_chars(port_text.data(), port_text.data() + port_text.size(), credentials.port);
    if (result.ec != std::errc{} || result.ptr != port_text.data() + port_text.size() ||
        credentials.port <= 0 || credentials.port > 65535) {
        return std::nullopt;
    }
    credentials.password = std::string(fields[3]);
    credentials.protocol = std::string(fields[4]);
    if (credentials.password.empty()) {
        return std::nullopt;
    }
    return credentials;
}

std::optional<std::string> ParseGameflowPhase(std::string_view json_text) {
    const auto doc = nlohmann::json::parse(json_text, nullptr, false);
    if (doc.is_discarded() || !doc.is_string()) {
        return std::nullopt;
    }
    return doc.get<std::string>();
}

std::optional<LcuSummoner> ParseCurrentSummoner(std::string_view json_text) {
    const auto doc = nlohmann::json::parse(json_text, nullptr, false);
    if (doc.is_discarded() || !doc.is_object()) {
        return std::nullopt;
    }
    LcuSummoner summoner;
    summoner.puuid = GetString(doc, "puuid").value_or("");
    summoner.riot_id = RiotIdOf(doc);
    if (summoner.puuid.empty()) {
        return std::nullopt;
    }
    return summoner;
}

std::optional<std::vector<LobbyMember>> ParseGameflowSession(std::string_view json_text,
                                                             std::string_view self_puuid) {
    const auto doc = nlohmann::json::parse(json_text, nullptr, false);
    if (doc.is_discarded() || !doc.is_object() || !doc.contains("gameData") ||
        !doc["gameData"].is_object() || self_puuid.empty()) {
        return std::nullopt;
    }
    const auto& game = doc["gameData"];

    const auto team_has_self = [&](std::string_view key) {
        if (!game.contains(key) || !game[key].is_array()) {
            return false;
        }
        for (const auto& raw : game[key]) {
            if (raw.is_object() && GetString(raw, "puuid").value_or("") == self_puuid) {
                return true;
            }
        }
        return false;
    };
    const bool self_in_one = team_has_self("teamOne");
    const bool self_in_two = team_has_self("teamTwo");
    if (!self_in_one && !self_in_two) {
        return std::nullopt;
    }

    // Заклинания лежат отдельно: playerChampionSelections, по puuid.
    std::vector<std::pair<std::string, std::pair<int, int>>> spells;
    if (game.contains("playerChampionSelections") &&
        game["playerChampionSelections"].is_array()) {
        for (const auto& raw : game["playerChampionSelections"]) {
            if (raw.is_object()) {
                spells.emplace_back(GetString(raw, "puuid").value_or(""),
                                    std::pair{SmallId(raw, "spell1Id"), SmallId(raw, "spell2Id")});
            }
        }
    }

    std::vector<LobbyMember> members;
    int cell = 0;
    for (const std::string_view key : {"teamOne", "teamTwo"}) {
        if (!game.contains(key) || !game[key].is_array()) {
            continue;
        }
        const bool ally = (key == "teamOne") == self_in_one;
        for (const auto& raw : game[key]) {
            if (!raw.is_object()) {
                continue;
            }
            LobbyMember member;
            member.side = ally ? LobbySide::Ally : LobbySide::Enemy;
            member.cell_id = cell++;
            member.puuid = GetString(raw, "puuid").value_or("");
            member.riot_id = RiotIdOf(raw);
            if (member.riot_id.empty()) {
                // Старые версии клиента: имя целиком в summonerName.
                const auto name = GetString(raw, "summonerName").value_or("");
                if (name.find('#') != std::string::npos) {
                    member.riot_id = name;
                }
            }
            member.champion_id = SmallId(raw, "championId");
            member.position = PackRole(GetString(raw, "selectedPosition").value_or(""));
            member.is_self = member.puuid == self_puuid;
            for (const auto& [puuid, pair] : spells) {
                if (!puuid.empty() && puuid == member.puuid) {
                    member.spell1_id = pair.first;
                    member.spell2_id = pair.second;
                }
            }
            if (member.puuid.empty() && member.riot_id.empty() && member.champion_id == 0) {
                continue;
            }
            members.push_back(std::move(member));
        }
    }
    return members;
}

std::optional<Lobby> ParseChampSelectSession(std::string_view json_text) {
    const auto doc = nlohmann::json::parse(json_text, nullptr, false);
    if (doc.is_discarded() || !doc.is_object()) {
        return std::nullopt;
    }

    Lobby lobby;
    lobby.source = "lcu";
    const int local_cell = GetInt(doc, "localPlayerCellId").value_or(-1);

    ParseTeam(doc, "myTeam", LobbySide::Ally, local_cell, lobby);
    ParseTeam(doc, "theirTeam", LobbySide::Enemy, local_cell, lobby);

    if (doc.contains("timer") && doc["timer"].is_object()) {
        const auto& timer = doc["timer"];
        lobby.timer_phase = GetString(timer, "phase").value_or("");
        // Целое или дробное — зависит от версии клиента, поэтому как double.
        lobby.time_left_ms =
            static_cast<long long>(GetDouble(timer, "adjustedTimeLeftInPhase").value_or(0.0));
        // internalNowInEpochMs — когда клиент выставил этот остаток. Вместе
        // они дают момент окончания этапа, до которого можно считать самим.
        const auto set_at = GetDouble(timer, "internalNowInEpochMs");
        const bool infinite = GetBoolean(timer, "isInfinite").value_or(false);
        if (set_at && *set_at > 0 && !infinite) {
            lobby.timer_ends_at_ms = static_cast<long long>(*set_at) + lobby.time_left_ms;
        }
    }
    if (doc.contains("bans") && doc["bans"].is_object()) {
        ParseBans(doc["bans"], "myTeamBans", lobby.bans);
        ParseBans(doc["bans"], "theirTeamBans", lobby.bans);
    }
    return lobby;
}

}  // namespace sintence
