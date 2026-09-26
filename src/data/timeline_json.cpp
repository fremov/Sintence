#include "timeline_json.h"

#include <cstdlib>
#include <iterator>
#include <string>

#include "json.hpp"
#include "json_helpers.h"

namespace sintence {

std::optional<MatchTimeline> ParseMatchTimeline(std::string_view json_text) {
    const auto doc = nlohmann::json::parse(json_text, nullptr, false);
    if (doc.is_discarded() || !doc.is_object() || !doc.contains("metadata") ||
        !doc.contains("info") || !doc["metadata"].is_object() || !doc["info"].is_object()) {
        return std::nullopt;
    }
    const auto& metadata = doc["metadata"];
    const auto& info = doc["info"];
    if (!metadata.contains("participants") || !metadata["participants"].is_array() ||
        !info.contains("frames") || !info["frames"].is_array()) {
        return std::nullopt;
    }

    MatchTimeline timeline;
    for (const auto& puuid : metadata["participants"]) {
        TimelineParticipant participant;
        participant.puuid = puuid.is_string() ? puuid.get<std::string>() : std::string();
        timeline.participants.push_back(std::move(participant));
    }
    const auto count = static_cast<int>(timeline.participants.size());
    const auto at = [&](int participant_id) -> TimelineParticipant* {
        return participant_id >= 1 && participant_id <= count
                   ? &timeline.participants[static_cast<std::size_t>(participant_id - 1)]
                   : nullptr;
    };

    for (const auto& frame : info["frames"]) {
        if (!frame.is_object()) {
            continue;
        }
        const int minute =
            static_cast<int>(GetInt64(frame, "timestamp").value_or(0) / 60000);

        // Золото на кадр: participantFrames — объект "1".."10".
        if (frame.contains("participantFrames") && frame["participantFrames"].is_object()) {
            int blue = 0;
            int red = 0;
            for (const auto& [key, value] : frame["participantFrames"].items()) {
                if (!value.is_object()) {
                    continue;
                }
                const int id = GetInt(value, "participantId").value_or(std::atoi(key.c_str()));
                const int gold = GetInt(value, "totalGold").value_or(0);
                (id <= count / 2 ? blue : red) += gold;
            }
            timeline.gold_diff.push_back(blue - red);
        }

        if (!frame.contains("events") || !frame["events"].is_array()) {
            continue;
        }
        for (const auto& event : frame["events"]) {
            if (!event.is_object()) {
                continue;
            }
            const std::string type = GetString(event, "type").value_or("");
            TimelineParticipant* who = at(GetInt(event, "participantId").value_or(0));
            if (who == nullptr) {
                continue;
            }
            if (type == "SKILL_LEVEL_UP") {
                static constexpr char kSlots[] = {'?', 'Q', 'W', 'E', 'R'};
                const int slot = GetInt(event, "skillSlot").value_or(0);
                if (slot >= 1 && slot <= 4) {
                    who->skills.push_back(kSlots[slot]);
                }
            } else if (type == "ITEM_PURCHASED") {
                who->purchases.push_back({GetInt(event, "itemId").value_or(0), minute});
            } else if (type == "ITEM_UNDO") {
                const int undone = GetInt(event, "beforeId").value_or(0);
                for (auto it = who->purchases.rbegin(); it != who->purchases.rend(); ++it) {
                    if (it->item_id == undone) {
                        who->purchases.erase(std::next(it).base());
                        break;
                    }
                }
            }
        }
    }
    return timeline;
}

}  // namespace sintence
