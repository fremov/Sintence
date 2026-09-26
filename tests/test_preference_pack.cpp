#include "doctest.h"
#include "preference_pack.h"

using sintence::PreferenceBucket;
using sintence::PreferencePack;

namespace {

// Форма файла, которую пишет scripts/build_pack.py. Если она разъедется,
// приложение молча покажет пустоту — этот тест и есть договор между
// Python-агрегатором и C++.
constexpr const char* kPack = R"({
  "schemaVersion": 2,
  "region": "RU",
  "patch": "16.19",
  "generatedAt": 1758830400,
  "minGames": 20,
  "buckets": {
    "Vladimir|MIDDLE|Yasuo|EMERALD": {
      "champion": "Vladimir", "role": "MIDDLE", "opponent": "Yasuo",
      "tier": "EMERALD", "patch": "16.19", "games": 74, "winrate": 0.541,
      "runePages": [{
        "name": "Conqueror", "games": 51, "share": 0.689, "winrate": 0.56,
        "winrateLow": 0.4231,
        "page": {
          "keystone": "Conqueror", "primaryTree": "Precision",
          "primary": ["Presence of Mind", "Legend: Haste", "Coup de Grace"],
          "secondaryTree": "Sorcery",
          "secondary": ["Manaflow Band", "Transcendence"],
          "shards": ["Adaptive Force", "Adaptive Force", "Health"]
        }
      }],
      "skillOrders": [{
        "name": "Q>E>Q>W>Q", "steps": ["Q", "E", "Q", "W", "Q"],
        "games": 44, "share": 0.594, "winrate": 0.57, "winrateLow": 0.4222
      }],
      "itemChains": [{
        "name": "Riftmaker → Rylai's Crystal Scepter → Zhonya's Hourglass",
        "steps": ["Riftmaker", "Rylai's Crystal Scepter", "Zhonya's Hourglass"],
        "games": 29, "share": 0.392, "winrate": 0.62, "winrateLow": 0.4383
      }]
    },
    "Vladimir|MIDDLE|ANY|EMERALD": {
      "champion": "Vladimir", "role": "MIDDLE", "opponent": "ANY",
      "tier": "EMERALD", "patch": "16.19", "games": 810, "winrate": 0.503,
      "runePages": [], "itemChains": [],
      "skillOrders": [{
        "name": "Q>E>Q>W>Q", "steps": ["Q", "E", "Q", "W", "Q"],
        "games": 402, "share": 0.496, "winrate": 0.51, "winrateLow": 0.4761
      }]
    },
    "Vladimir|MIDDLE|ANY|ALL": {
      "champion": "Vladimir", "role": "MIDDLE", "opponent": "ANY",
      "tier": "ALL", "patch": "16.19", "games": 3400, "winrate": 0.498,
      "runePages": [], "itemChains": [],
      "skillOrders": [{
        "name": "Q>E>Q>W>Q", "steps": ["Q", "E", "Q", "W", "Q"],
        "games": 1700, "share": 0.5, "winrate": 0.5, "winrateLow": 0.4832
      }]
    }
  }
})";

}  // namespace

TEST_CASE("PreferencePack читает страницу рун целиком") {
    const auto pack = PreferencePack::LoadFromJson(kPack);

    REQUIRE(pack.has_value());
    CHECK(pack->Patch() == "16.19");
    CHECK(pack->Size() == 3);

    const PreferenceBucket* bucket =
        pack->Lookup("Vladimir", "MIDDLE", "Yasuo", "EMERALD");
    REQUIRE(bucket != nullptr);
    REQUIRE(bucket->rune_pages.size() == 1);

    const auto& page = bucket->rune_pages[0].page;
    REQUIRE(page.has_value());
    CHECK(page->keystone == "Conqueror");
    CHECK(page->primary_tree == "Precision");
    REQUIRE(page->primary.size() == 3);
    CHECK(page->primary[2] == "Coup de Grace");
    CHECK(page->secondary_tree == "Sorcery");
    REQUIRE(page->secondary.size() == 2);
    REQUIRE(page->shards.size() == 3);
    CHECK(page->shards[2] == "Health");
}

TEST_CASE("PreferencePack читает цепочки как последовательность, а не набор") {
    const auto pack = PreferencePack::LoadFromJson(kPack);
    REQUIRE(pack.has_value());

    const PreferenceBucket* bucket =
        pack->Lookup("Vladimir", "MIDDLE", "Yasuo", "EMERALD");
    REQUIRE(bucket != nullptr);

    REQUIRE(bucket->item_chains.size() == 1);
    const auto& chain = bucket->item_chains[0].steps;
    REQUIRE(chain.size() == 3);
    CHECK(chain[0] == "Riftmaker");
    CHECK(chain[2] == "Zhonya's Hourglass");

    REQUIRE(bucket->skill_orders.size() == 1);
    const auto& skills = bucket->skill_orders[0].steps;
    REQUIRE(skills.size() == 5);
    CHECK(skills[0] == "Q");
    CHECK(skills[1] == "E");
}

TEST_CASE("Lookup: точный матчап важнее ранговой корзины") {
    const auto pack = PreferencePack::LoadFromJson(kPack);
    REQUIRE(pack.has_value());

    const PreferenceBucket* bucket =
        pack->Lookup("Vladimir", "MIDDLE", "Yasuo", "EMERALD");
    REQUIRE(bucket != nullptr);
    CHECK(bucket->opponent == "Yasuo");
    CHECK(bucket->games == 74);
}

TEST_CASE("Lookup: нет матчапа — откат на «против всех», и это видно") {
    // Матчапов около сорока тысяч, пустой бакет здесь норма. Поле opponent
    // обязано сказать правду, иначе общая статистика будет выдана
    // за статистику против конкретного противника.
    const auto pack = PreferencePack::LoadFromJson(kPack);
    REQUIRE(pack.has_value());

    const PreferenceBucket* bucket =
        pack->Lookup("Vladimir", "MIDDLE", "Malzahar", "EMERALD");
    REQUIRE(bucket != nullptr);
    CHECK(bucket->opponent == "ANY");
    CHECK(bucket->tier == "EMERALD");
    CHECK(bucket->games == 810);
}

TEST_CASE("Lookup: нет корзины игрока — откат на все ранги") {
    const auto pack = PreferencePack::LoadFromJson(kPack);
    REQUIRE(pack.has_value());

    const PreferenceBucket* bronze =
        pack->Lookup("Vladimir", "MIDDLE", "Malzahar", "BRONZE");
    REQUIRE(bronze != nullptr);
    CHECK(bronze->tier == "ALL");
    CHECK(bronze->games == 3400);
}

TEST_CASE("Lookup: ранг неизвестен — общая корзина, чемпиона нет — nullptr") {
    const auto pack = PreferencePack::LoadFromJson(kPack);
    REQUIRE(pack.has_value());

    const PreferenceBucket* unknown_rank =
        pack->Lookup("Vladimir", "MIDDLE", "Yasuo", "");
    REQUIRE(unknown_rank != nullptr);
    CHECK(unknown_rank->tier == "ALL");

    CHECK(pack->Lookup("Zed", "MIDDLE", "Yasuo", "EMERALD") == nullptr);
    CHECK(pack->Lookup("Vladimir", "TOP", "Yasuo", "EMERALD") == nullptr);
    CHECK(pack->Lookup("", "MIDDLE", "Yasuo", "EMERALD") == nullptr);
}

TEST_CASE("PreferencePack: пак первой версии больше не принимается") {
    // В первой версии бакеты были без оппонента, а руны — списком
    // отдельных ключевых. Разобрать его наполовину значит показать
    // правдоподобную чушь.
    const auto old = PreferencePack::LoadFromJson(
        R"({"schemaVersion": 1, "patch": "16.18", "buckets": {}})");
    CHECK_FALSE(old.has_value());

    CHECK_FALSE(PreferencePack::LoadFromJson("{").has_value());
    CHECK_FALSE(PreferencePack::LoadFromJson("[]").has_value());
    CHECK_FALSE(PreferencePack::LoadFromJson(
                    R"({"schemaVersion": 2, "patch": "16.19"})").has_value());
}

TEST_CASE("PreferencePack: пустой пак живёт и ничего не находит") {
    const PreferencePack empty;

    CHECK(empty.Size() == 0);
    CHECK(empty.Lookup("Vladimir", "MIDDLE", "Yasuo", "EMERALD") == nullptr);
}

TEST_CASE("TierBucket укрупняет тиры и не теряет апекс") {
    CHECK(PreferencePack::TierBucket("IRON") == "BRONZE");
    CHECK(PreferencePack::TierBucket("GOLD") == "GOLD");
    CHECK(PreferencePack::TierBucket("EMERALD") == "EMERALD");
    CHECK(PreferencePack::TierBucket("CHALLENGER") == "DIAMOND");
    CHECK(PreferencePack::TierBucket("grandmaster") == "DIAMOND");
    CHECK(PreferencePack::TierBucket("") == "ALL");
    CHECK(PreferencePack::TierBucket("UNRANKED") == "ALL");
}

TEST_CASE("Lookup: точный матчап без вариантов уступает широкой выборке") {
    // Бакет, в котором игр набралось, а повторяющейся сборки нет,
    // бесполезен: показывать в нём нечего. Лестница обязана спуститься
    // к «против всех», где варианты есть.
    constexpr const char* kThinMatchup = R"({
      "schemaVersion": 2, "region": "RU", "patch": "16.19",
      "buckets": {
        "Caitlyn|BOTTOM|MissFortune|ALL": {
          "champion": "Caitlyn", "role": "BOTTOM", "opponent": "MissFortune",
          "tier": "ALL", "patch": "16.19", "games": 10, "winrate": 0.4,
          "runePages": [], "skillOrders": [], "itemChains": []
        },
        "Caitlyn|BOTTOM|ANY|ALL": {
          "champion": "Caitlyn", "role": "BOTTOM", "opponent": "ANY",
          "tier": "ALL", "patch": "16.19", "games": 88, "winrate": 0.51,
          "runePages": [], "skillOrders": [
            {"name": "Q>W>E>Q>Q", "steps": ["Q","W","E","Q","Q"],
             "games": 30, "share": 0.34, "winrate": 0.53, "winrateLow": 0.36}
          ], "itemChains": []
        }
      }
    })";

    const auto pack = PreferencePack::LoadFromJson(kThinMatchup);
    REQUIRE(pack.has_value());

    const PreferenceBucket* bucket =
        pack->Lookup("Caitlyn", "BOTTOM", "MissFortune", "ALL");
    REQUIRE(bucket != nullptr);
    CHECK(bucket->opponent == "ANY");
    CHECK(bucket->games == 88);
}

TEST_CASE("Lookup: пустые бакеты по всей лестнице — nullptr") {
    constexpr const char* kAllEmpty = R"({
      "schemaVersion": 2, "region": "RU", "patch": "16.19",
      "buckets": {
        "Caitlyn|BOTTOM|ANY|ALL": {
          "champion": "Caitlyn", "role": "BOTTOM", "opponent": "ANY",
          "tier": "ALL", "patch": "16.19", "games": 12, "winrate": 0.5,
          "runePages": [], "skillOrders": [], "itemChains": []
        }
      }
    })";

    const auto pack = PreferencePack::LoadFromJson(kAllEmpty);
    REQUIRE(pack.has_value());
    CHECK(pack->Lookup("Caitlyn", "BOTTOM", "MissFortune", "ALL") == nullptr);
}
