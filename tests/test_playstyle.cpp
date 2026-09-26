#include <algorithm>
#include <string>
#include <unordered_map>
#include <vector>

#include "doctest.h"
#include "playstyle.h"

using sintence::AnalyzePlayStyle;
using sintence::MatchDetail;
using sintence::MatchParticipant;
using sintence::PlayStyle;
using sintence::StyleFinding;
using sintence::StyleTag;

namespace {

// Игрок на роли в получасовой игре и девять типичных игроков вокруг.
// Типичный — ровно медианы из playstyle.cpp для MIDDLE, чтобы плашки
// появлялись только от того, что задано в тесте.
struct Game {
    std::string role = "MIDDLE";
    bool win = true;
    int champion_id = 8;
    int kills = 5;
    int deaths = 7;
    int assists = 5;
    int cs = 198;               // 6.6 в минуту за 30 минут
    int buildings = 6300;       // 210 в минуту
    int early_takedowns = 4;
    int vision = 19;            // ~0.62 в минуту
    int lane_lead = 0;
    int team_kills = 23;        // вместе с игроком: участие ~0.43
    int duration = 1800;
};

MatchDetail Make(const Game& game, int index) {
    MatchDetail match;
    match.match_id = "RU_" + std::to_string(index);
    match.duration_seconds = game.duration;
    MatchParticipant self;
    self.puuid = "me";
    self.team_id = 100;
    self.role = game.role;
    self.win = game.win;
    self.champion_id = game.champion_id;
    self.kills = game.kills;
    self.deaths = game.deaths;
    self.assists = game.assists;
    self.cs = game.cs;
    self.damage_to_buildings = game.buildings;
    self.early_takedowns = game.early_takedowns;
    self.vision_score = game.vision;
    self.lane_lead = game.lane_lead;
    match.participants.push_back(self);

    MatchParticipant ally;
    ally.puuid = "ally";
    ally.team_id = 100;
    ally.kills = game.team_kills - game.kills;
    match.participants.push_back(ally);
    return match;
}

std::vector<MatchDetail> Many(const Game& game, int count) {
    std::vector<MatchDetail> matches;
    for (int i = 0; i < count; ++i) {
        matches.push_back(Make(game, i));
    }
    return matches;
}

bool Has(const PlayStyle& style, StyleTag tag) {
    return std::ranges::any_of(style.tags, [tag](const StyleFinding& f) { return f.tag == tag; });
}

const StyleFinding* Find(const PlayStyle& style, StyleTag tag) {
    const auto found = std::ranges::find_if(style.tags, [tag](const StyleFinding& f) { return f.tag == tag; });
    return found == style.tags.end() ? nullptr : &*found;
}

}  // namespace

TEST_CASE("AnalyzePlayStyle: типичный игрок — без плашек стиля") {
    Game typical;
    typical.champion_id = 0;  // разные чемпионы ниже, чтобы не было «одного чемпиона»
    std::vector<MatchDetail> matches;
    for (int i = 0; i < 8; ++i) {
        typical.champion_id = i + 1;
        typical.win = i % 2 == 0;
        matches.push_back(Make(typical, i));
    }
    const PlayStyle style = AnalyzePlayStyle(matches, "me");
    CHECK(style.games == 8);
    CHECK(style.role_games == 8);
    CHECK(style.tags.empty());
}

TEST_CASE("AnalyzePlayStyle: пуш, фарм, ранняя агрессия, линия") {
    Game game;
    game.buildings = 12000;     // 400 в минуту при типичных 210
    game.cs = 250;              // 8.3 в минуту
    game.early_takedowns = 7;   // при типичных 4
    game.lane_lead = 1;
    const PlayStyle style = AnalyzePlayStyle(Many(game, 6), "me");
    CHECK(Has(style, StyleTag::kPusher));
    CHECK(Has(style, StyleTag::kFarmer));
    CHECK(Has(style, StyleTag::kEarlyAggression));
    CHECK(Has(style, StyleTag::kLaneWinner));
    const StyleFinding* push = Find(style, StyleTag::kPusher);
    REQUIRE(push != nullptr);
    CHECK(push->value == doctest::Approx(400));
    CHECK(push->typical == doctest::Approx(210));
    CHECK(push->games == 6);
}

TEST_CASE("AnalyzePlayStyle: смерти, в том числе ранние по timeline") {
    Game game;
    game.deaths = 11;
    std::vector<MatchDetail> matches = Many(game, 6);
    std::unordered_map<std::string, int> early;
    for (const MatchDetail& match : matches) {
        early[match.match_id] = 3;  // при типичных 1.45 у центра
    }
    const PlayStyle style = AnalyzePlayStyle(matches, "me", early);
    CHECK(Has(style, StyleTag::kDiesOften));
    const StyleFinding* dying = Find(style, StyleTag::kEarlyDeaths);
    REQUIRE(dying != nullptr);
    CHECK(dying->value == doctest::Approx(3.0));
    CHECK(dying->typical == doctest::Approx(1.45));

    Game careful;
    careful.deaths = 3;
    CHECK(Has(AnalyzePlayStyle(Many(careful, 6), "me"), StyleTag::kRarelyDies));
}

TEST_CASE("AnalyzePlayStyle: мало игр — плашек по роли нет") {
    Game game;
    game.buildings = 20000;
    const PlayStyle style = AnalyzePlayStyle(Many(game, sintence::kMinStyleGames - 1), "me");
    CHECK_FALSE(Has(style, StyleTag::kPusher));
}

TEST_CASE("AnalyzePlayStyle: серии — от свежей игры") {
    std::vector<MatchDetail> matches;
    Game loss;
    loss.win = false;
    Game win;
    for (int i = 0; i < 4; ++i) {
        matches.push_back(Make(loss, i));
    }
    matches.push_back(Make(win, 10));
    const PlayStyle losing = AnalyzePlayStyle(matches, "me");
    const StyleFinding* streak = Find(losing, StyleTag::kLossStreak);
    REQUIRE(streak != nullptr);
    CHECK(streak->value == doctest::Approx(4));

    // Свежая победа прерывает серию поражений.
    matches.insert(matches.begin(), Make(win, 11));
    CHECK_FALSE(Has(AnalyzePlayStyle(matches, "me"), StyleTag::kLossStreak));
}

TEST_CASE("AnalyzePlayStyle: один чемпион, ремейки и ARAM") {
    Game game;
    std::vector<MatchDetail> matches = Many(game, 7);
    Game other = game;
    other.champion_id = 103;
    matches.push_back(Make(other, 50));
    Game remake = game;
    remake.duration = 200;
    matches.push_back(Make(remake, 60));
    Game aram = game;
    aram.role = "";
    aram.champion_id = 22;
    matches.push_back(Make(aram, 70));

    const PlayStyle style = AnalyzePlayStyle(matches, "me");
    CHECK(style.games == 9);        // ремейк не считается
    CHECK(style.role_games == 8);   // ARAM — без роли
    const StyleFinding* one_trick = Find(style, StyleTag::kOneTrick);
    REQUIRE(one_trick != nullptr);
    CHECK(one_trick->champion_id == 8);
    CHECK(one_trick->games == 7);
    REQUIRE(!style.champion_games.empty());
    CHECK(style.champion_games.front() == std::pair{8, 7});
}

TEST_CASE("AnalyzePlayStyle: поддержке фарм и пуш не считаются") {
    Game support;
    support.role = "UTILITY";
    support.cs = 20;
    support.buildings = 0;
    support.vision = 100;  // 3.3 в минуту при типичных 2.35
    const PlayStyle style = AnalyzePlayStyle(Many(support, 6), "me");
    CHECK_FALSE(Has(style, StyleTag::kLowFarm));
    CHECK_FALSE(Has(style, StyleTag::kPusher));
    CHECK(Has(style, StyleTag::kVisionControl));
}
