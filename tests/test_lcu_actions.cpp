#include "doctest.h"
#include "lcu_actions.h"

#include <string>

using sintence::ChooseSpellSlots;
using sintence::IsCompleteRunePage;
using sintence::PlanRunePage;
using sintence::RunePageJson;
using sintence::RunePageSpec;

namespace {

RunePageSpec Electrocute() {
    return {"Sintence: Ари", 8100, 8200, {8112, 8139, 8140, 8106, 8224, 8236, 5008, 5008, 5001}};
}

// Страницы клиента: две встроенные (удалить нельзя), две свои.
constexpr const char* kPagesFull = R"([
  {"id": 1, "name": "Рекомендуемая", "isDeletable": false, "current": false},
  {"id": 2, "name": "Мид-маги", "isDeletable": true, "current": true},
  {"id": 3, "name": "Лес", "isDeletable": true, "current": false},
  {"id": 50, "name": "Точность", "isDeletable": false, "current": false}
])";

}  // namespace

TEST_CASE("IsCompleteRunePage: два разных дерева и девять ненулевых id") {
    CHECK(IsCompleteRunePage(Electrocute()));

    auto same_tree = Electrocute();
    same_tree.sub_style_id = 8100;
    CHECK_FALSE(IsCompleteRunePage(same_tree));

    auto short_page = Electrocute();
    short_page.perk_ids.pop_back();
    CHECK_FALSE(IsCompleteRunePage(short_page));

    // Пак второй схемы: id нет — страницу не собрать.
    auto no_ids = Electrocute();
    no_ids.perk_ids[3] = 0;
    CHECK_FALSE(IsCompleteRunePage(no_ids));
}

TEST_CASE("RunePageJson: тело, которое ждёт клиент") {
    const std::string json = RunePageJson(Electrocute());
    CHECK(json.find(R"("primaryStyleId":8100)") != std::string::npos);
    CHECK(json.find(R"("subStyleId":8200)") != std::string::npos);
    CHECK(json.find(R"("selectedPerkIds":[8112,8139,8140,8106,8224,8236,5008,5008,5001])") !=
          std::string::npos);
    CHECK(json.find(R"("current":true)") != std::string::npos);
}

TEST_CASE("PlanRunePage: своя страница перезаписывается") {
    constexpr const char* kWithOwn = R"([
      {"id": 2, "name": "Мид-маги", "isDeletable": true, "current": true},
      {"id": 7, "name": "Sintence: Зед", "isDeletable": true, "current": false}
    ])";
    const auto plan = PlanRunePage(kWithOwn, R"({"ownedPageCount": 2})", false);
    REQUIRE(plan.has_value());
    REQUIRE(plan->delete_id.has_value());
    CHECK(*plan->delete_id == 7);
    CHECK_FALSE(plan->need_replace);
}

TEST_CASE("PlanRunePage: есть свободное место — ничего не удаляется") {
    const auto plan = PlanRunePage(kPagesFull, R"({"ownedPageCount": 5})", false);
    REQUIRE(plan.has_value());
    CHECK_FALSE(plan->delete_id.has_value());
    CHECK_FALSE(plan->need_replace);
    CHECK_FALSE(plan->impossible);
}

TEST_CASE("PlanRunePage: места нет — без согласия чужая страница не трогается") {
    const auto plan = PlanRunePage(kPagesFull, R"({"ownedPageCount": 2})", false);
    REQUIRE(plan.has_value());
    CHECK_FALSE(plan->delete_id.has_value());
    CHECK(plan->need_replace);
    // Предлагается текущая: её игрок сейчас и заменил бы руками.
    CHECK(plan->replace_name == "Мид-маги");

    const auto agreed = PlanRunePage(kPagesFull, R"({"ownedPageCount": 2})", true);
    REQUIRE(agreed.has_value());
    REQUIRE(agreed->delete_id.has_value());
    CHECK(*agreed->delete_id == 2);
}

TEST_CASE("PlanRunePage: текущая встроенная — предлагается первая своя") {
    constexpr const char* kPresetCurrent = R"([
      {"id": 1, "name": "Рекомендуемая", "isDeletable": false, "current": true},
      {"id": 3, "name": "Лес", "isDeletable": true, "current": false}
    ])";
    const auto plan = PlanRunePage(kPresetCurrent, R"({"ownedPageCount": 1})", false);
    REQUIRE(plan.has_value());
    CHECK(plan->need_replace);
    CHECK(plan->replace_name == "Лес");
}

TEST_CASE("PlanRunePage: удалять нечего — impossible; битый ответ — nullopt") {
    constexpr const char* kOnlyPresets = R"([{"id": 1, "name": "x", "isDeletable": false}])";
    const auto plan = PlanRunePage(kOnlyPresets, R"({"ownedPageCount": 0})", true);
    REQUIRE(plan.has_value());
    CHECK(plan->impossible);

    CHECK_FALSE(PlanRunePage("{", "{}", false).has_value());
    CHECK_FALSE(PlanRunePage("[]", "[]", false).has_value());
}

TEST_CASE("ChooseSpellSlots: уже стоящее заклинание не сдвигается") {
    constexpr int kFlash = 4;
    constexpr int kIgnite = 14;
    constexpr int kHeal = 7;
    constexpr int kTeleport = 12;
    constexpr int kGhost = 6;

    // Скачок на D остаётся на D.
    CHECK(ChooseSpellSlots(kFlash, kHeal, kIgnite, kFlash) == std::pair{kFlash, kIgnite});
    // Скачок на F остаётся на F.
    CHECK(ChooseSpellSlots(kGhost, kFlash, kFlash, kTeleport) == std::pair{kTeleport, kFlash});
    // Обе уже стоят — ничего не меняется.
    CHECK(ChooseSpellSlots(kIgnite, kFlash, kFlash, kIgnite) == std::pair{kIgnite, kFlash});
    // Ничего общего — как рекомендовано.
    CHECK(ChooseSpellSlots(kGhost, kHeal, kFlash, kIgnite) == std::pair{kFlash, kIgnite});
    // Заклинаний ещё нет (0) — как рекомендовано.
    CHECK(ChooseSpellSlots(0, 0, kFlash, kIgnite) == std::pair{kFlash, kIgnite});
}
