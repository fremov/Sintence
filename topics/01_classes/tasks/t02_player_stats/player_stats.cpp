#include "player_stats.h"

namespace course {
namespace {

// Заглушка, чтобы файл собирался до того, как ты напишешь тела.
// Удали её, когда закончишь.
const std::string kStub = "<не реализовано>";

}  // namespace

PlayerStats::PlayerStats(std::string /*champion_name*/) {
    // TODO
}

void PlayerStats::AddMatch(int /*kills*/, int /*deaths*/, int /*assists*/, bool /*win*/) {
    // TODO: сначала решить, годятся ли данные, и только потом что-то менять.
    // Объект, который успел изменить два поля из пяти и передумал, —
    // это худший из возможных вариантов.
}

const std::string& PlayerStats::ChampionName() const {
    // TODO
    return kStub;
}

int PlayerStats::Games() const {
    // TODO
    return -1;
}

int PlayerStats::Wins() const {
    // TODO
    return -1;
}

int PlayerStats::Losses() const {
    // TODO
    return -1;
}

int PlayerStats::TotalKills() const {
    // TODO
    return -1;
}

int PlayerStats::TotalDeaths() const {
    // TODO
    return -1;
}

int PlayerStats::TotalAssists() const {
    // TODO
    return -1;
}

}  // namespace course
