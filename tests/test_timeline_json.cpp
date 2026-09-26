#include "doctest.h"
#include "timeline_json.h"

using sintence::ParseMatchTimeline;

namespace {

// match-v5 timeline на двоих (1 — синие, 2 — красные), три кадра.
// Во втором кадре игрок 1 купил и сразу отменил покупку.
constexpr const char* kTimeline = R"({
  "metadata": {"matchId": "RU_1", "participants": ["p-blue", "p-red"]},
  "info": {"frames": [
    {"timestamp": 0,
     "participantFrames": {"1": {"participantId": 1, "totalGold": 500},
                           "2": {"participantId": 2, "totalGold": 500}},
     "events": [
       {"type": "ITEM_PURCHASED", "participantId": 1, "itemId": 1056, "timestamp": 1000},
       {"type": "SKILL_LEVEL_UP", "participantId": 1, "skillSlot": 1},
       {"type": "SKILL_LEVEL_UP", "participantId": 2, "skillSlot": 3}
     ]},
    {"timestamp": 60000,
     "participantFrames": {"1": {"participantId": 1, "totalGold": 900},
                           "2": {"participantId": 2, "totalGold": 700}},
     "events": [
       {"type": "SKILL_LEVEL_UP", "participantId": 1, "skillSlot": 2},
       {"type": "ITEM_PURCHASED", "participantId": 1, "itemId": 2003, "timestamp": 61000},
       {"type": "ITEM_UNDO", "participantId": 1, "beforeId": 2003, "afterId": 0}
     ]},
    {"timestamp": 120000,
     "participantFrames": {"1": {"participantId": 1, "totalGold": 1200},
                           "2": {"participantId": 2, "totalGold": 1600}},
     "events": [
       {"type": "SKILL_LEVEL_UP", "participantId": 1, "skillSlot": 4},
       {"type": "ITEM_PURCHASED", "participantId": 2, "itemId": 3134, "timestamp": 130000},
       {"type": "CHAMPION_KILL", "killerId": 2, "victimId": 1, "timestamp": 125000},
       {"type": "SKILL_LEVEL_UP", "participantId": 7, "skillSlot": 1}
     ]}
  ]}
})";

}  // namespace

TEST_CASE("ParseMatchTimeline: золото синие минус красные по минутам") {
    const auto timeline = ParseMatchTimeline(kTimeline);
    REQUIRE(timeline.has_value());
    REQUIRE(timeline->gold_diff.size() == 3);
    CHECK(timeline->gold_diff[0] == 0);
    CHECK(timeline->gold_diff[1] == 200);
    CHECK(timeline->gold_diff[2] == -400);
}

TEST_CASE("ParseMatchTimeline: порядок прокачки") {
    const auto timeline = ParseMatchTimeline(kTimeline);
    REQUIRE(timeline.has_value());
    REQUIRE(timeline->participants.size() == 2);
    CHECK(timeline->participants[0].puuid == "p-blue");
    CHECK(timeline->participants[0].skills == "QWR");
    CHECK(timeline->participants[1].skills == "E");
}

TEST_CASE("ParseMatchTimeline: покупки с минутой, отменённые убраны") {
    const auto timeline = ParseMatchTimeline(kTimeline);
    REQUIRE(timeline.has_value());
    const auto& blue = timeline->participants[0].purchases;
    REQUIRE(blue.size() == 1);
    CHECK(blue[0].item_id == 1056);
    CHECK(blue[0].minute == 0);
    const auto& red = timeline->participants[1].purchases;
    REQUIRE(red.size() == 1);
    CHECK(red[0].item_id == 3134);
    CHECK(red[0].minute == 2);
}

TEST_CASE("ParseMatchTimeline: мусор — nullopt") {
    CHECK_FALSE(ParseMatchTimeline("").has_value());
    CHECK_FALSE(ParseMatchTimeline(R"({"metadata": {}, "info": {}})").has_value());
    CHECK_FALSE(
        ParseMatchTimeline(R"({"metadata": {"participants": []}, "info": {"frames": 1}})")
            .has_value());
}

TEST_CASE("ParseMatchTimeline: время смертей") {
    const auto timeline = ParseMatchTimeline(kTimeline);
    REQUIRE(timeline.has_value());
    REQUIRE(timeline->participants[0].death_seconds.size() == 1);
    CHECK(timeline->participants[0].death_seconds[0] == 125);
    CHECK(timeline->participants[1].death_seconds.empty());
}
