#include "match_detail.h"

namespace sintence {

const MatchParticipant* MatchDetail::Find(const std::string& puuid) const {
    for (const MatchParticipant& participant : participants) {
        if (participant.puuid == puuid) {
            return &participant;
        }
    }
    return nullptr;
}

MatchEntry ToMatchEntry(const MatchParticipant& participant, int duration_seconds) {
    MatchEntry entry;
    entry.champion_name = participant.champion;
    entry.line.kills = participant.kills;
    entry.line.deaths = participant.deaths;
    entry.line.assists = participant.assists;
    entry.line.win = participant.win;
    entry.line.minions = participant.cs;
    entry.line.duration_seconds = duration_seconds;
    return entry;
}

}  // namespace sintence
