#include <string>
#include <vector>

#include "doctest.h"
#include "profile_summary.h"

using sintence::MatchDetail;
using sintence::MatchParticipant;
using sintence::SummarizeProfile;

namespace {

MatchDetail Game(const std::string& champion, const std::string& role, bool win, int kills,
                 int deaths, int assists) {
    MatchParticipant self;
    self.puuid = "me";
    self.champion_id = static_cast<int>(champion.size());
    self.champion = champion;
    self.role = role;
    self.win = win;
    self.kills = kills;
    self.deaths = deaths;
    self.assists = assists;
    self.cs = 200;

    MatchParticipant other;
    other.puuid = "them";
    other.champion = "Zed";
    other.role = role;
    other.win = !win;

    MatchDetail match;
    match.duration_seconds = 1800;
    match.participants = {self, other};
    return match;
}

}  // namespace

TEST_CASE("SummarizeProfile: итог, роли и чемпионы") {
    const std::vector<MatchDetail> matches = {
        Game("Vladimir", "MIDDLE", true, 10, 2, 5),
        Game("Vladimir", "MIDDLE", false, 3, 6, 4),
        Game("Ahri", "MIDDLE", true, 7, 1, 9),
        Game("Lux", "UTILITY", false, 1, 4, 12),
    };
    const auto summary = SummarizeProfile(matches, "me");
    CHECK(summary.games == 4);
    CHECK(summary.wins == 2);

    REQUIRE(summary.roles.size() == 2);
    CHECK(summary.roles[0].role == "MIDDLE");
    CHECK(summary.roles[0].games == 3);
    CHECK(summary.roles[0].wins == 2);

    REQUIRE(summary.champions.size() == 3);
    CHECK(summary.champions[0].champion == "Vladimir");
    CHECK(summary.champions[0].games == 2);
    CHECK(summary.champions[0].wins == 1);
    CHECK(summary.champions[0].Kda() == doctest::Approx(22.0 / 8.0));
    CHECK(summary.champions[0].cs == 400);
    CHECK(summary.champions[0].duration_seconds == 3600);
    // Поровну игр — по имени, чтобы список не прыгал.
    CHECK(summary.champions[1].champion == "Ahri");
    CHECK(summary.champions[2].champion == "Lux");
}

TEST_CASE("SummarizeProfile: ноль смертей делится на единицу") {
    const auto summary = SummarizeProfile({Game("Ahri", "MIDDLE", true, 7, 0, 9)}, "me");
    REQUIRE(summary.champions.size() == 1);
    CHECK(summary.champions[0].Kda() == doctest::Approx(16.0));
}

TEST_CASE("SummarizeProfile: чужие матчи и пустой список") {
    CHECK(SummarizeProfile({}, "me").games == 0);
    const auto summary = SummarizeProfile({Game("Ahri", "MIDDLE", true, 1, 1, 1)}, "stranger");
    CHECK(summary.games == 0);
    CHECK(summary.roles.empty());
    CHECK(summary.champions.empty());
}
