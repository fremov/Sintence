#include "lcu_actions.h"

#include <format>

#include "app_log.h"
#include "json.hpp"
#include "json_helpers.h"
#include "lcu_json.h"

namespace sintence {

namespace {

constexpr std::string_view kOwnPrefix = "Sintence";

long long PageId(const nlohmann::json& page) {
    if (!page.contains("id") || !page["id"].is_number_integer()) {
        return -1;
    }
    return page["id"].get<long long>();
}

bool Deletable(const nlohmann::json& page) {
    return GetBoolean(page, "isDeletable").value_or(false);
}

bool Current(const nlohmann::json& page) {
    return GetBoolean(page, "current").value_or(false) ||
           GetBoolean(page, "isActive").value_or(false);
}

// Короткое пояснение из ответа клиента: {"message": "..."} или тело как есть.
std::string ClientMessage(const LcuClient::Response& response) {
    const auto doc = nlohmann::json::parse(response.body, nullptr, false);
    if (!doc.is_discarded() && doc.is_object()) {
        if (auto message = GetString(doc, "message"); message && !message->empty()) {
            return *message;
        }
    }
    return std::format("клиент ответил {}", response.status);
}

}  // namespace

bool IsCompleteRunePage(const RunePageSpec& page) {
    if (page.primary_style_id <= 0 || page.sub_style_id <= 0 ||
        page.primary_style_id == page.sub_style_id || page.perk_ids.size() != 9) {
        return false;
    }
    for (const int id : page.perk_ids) {
        if (id <= 0) {
            return false;
        }
    }
    return true;
}

std::string RunePageJson(const RunePageSpec& page) {
    const nlohmann::json doc = {
        {"name", page.name},
        {"primaryStyleId", page.primary_style_id},
        {"subStyleId", page.sub_style_id},
        {"selectedPerkIds", page.perk_ids},
        {"current", true},
    };
    return doc.dump();
}

std::optional<PagePlan> PlanRunePage(std::string_view pages_json,
                                     std::string_view inventory_json, bool replace_current) {
    const auto pages = nlohmann::json::parse(pages_json, nullptr, false);
    const auto inventory = nlohmann::json::parse(inventory_json, nullptr, false);
    if (pages.is_discarded() || !pages.is_array() || inventory.is_discarded() ||
        !inventory.is_object()) {
        return std::nullopt;
    }

    PagePlan plan;
    int own_pages = 0;
    const nlohmann::json* current_deletable = nullptr;
    const nlohmann::json* first_deletable = nullptr;
    for (const auto& page : pages) {
        if (!page.is_object() || !Deletable(page)) {
            continue;
        }
        ++own_pages;
        if (first_deletable == nullptr) {
            first_deletable = &page;
        }
        if (Current(page)) {
            current_deletable = &page;
        }
        const auto name = GetString(page, "name").value_or("");
        if (!plan.delete_id && name.starts_with(kOwnPrefix)) {
            plan.delete_id = PageId(page);
        }
    }
    if (plan.delete_id) {
        return plan;
    }

    const int allowed = GetInt(inventory, "ownedPageCount").value_or(0);
    if (own_pages < allowed) {
        return plan;
    }

    const nlohmann::json* target = current_deletable ? current_deletable : first_deletable;
    if (target == nullptr) {
        plan.impossible = true;
        return plan;
    }
    if (replace_current) {
        plan.delete_id = PageId(*target);
        return plan;
    }
    plan.need_replace = true;
    plan.replace_name = GetString(*target, "name").value_or("");
    return plan;
}

std::pair<int, int> ChooseSpellSlots(int current1, int current2, int want_a, int want_b) {
    if (current1 == want_a || current1 == want_b) {
        return {current1, current1 == want_a ? want_b : want_a};
    }
    if (current2 == want_a || current2 == want_b) {
        return {current2 == want_a ? want_b : want_a, current2};
    }
    return {want_a, want_b};
}

ActionResult LcuActions::ApplyRunePage(const RunePageSpec& page, bool replace_current) {
    const std::lock_guard<std::mutex> lock(mutex_);
    ActionResult result;

    if (!IsCompleteRunePage(page)) {
        result.status = ActionResult::Status::Invalid;
        result.message = "страница рун неполная — в паке нет id (нужна схема 3)";
        return result;
    }

    const auto pages = lcu_.Get("/lol-perks/v1/pages");
    const auto inventory = lcu_.Get("/lol-perks/v1/inventory");
    if (!pages || !inventory) {
        result.status = ActionResult::Status::ClientUnavailable;
        result.message = "клиент League не отвечает";
        return result;
    }

    const auto plan = PlanRunePage(*pages, *inventory, replace_current);
    if (!plan) {
        result.message = "не разобрал список страниц рун клиента";
        return result;
    }
    if (plan->impossible) {
        result.message = "нет ни одной своей страницы рун, которую можно заменить";
        return result;
    }
    if (plan->need_replace) {
        result.status = ActionResult::Status::NeedReplace;
        result.replace_name = plan->replace_name;
        result.message = "свободной страницы рун нет";
        return result;
    }

    if (plan->delete_id) {
        const auto deleted =
            lcu_.Send("DELETE", std::format("/lol-perks/v1/pages/{}", *plan->delete_id));
        if (deleted.status < 200 || deleted.status >= 300) {
            result.message = "не удалось освободить страницу: " + ClientMessage(deleted);
            return result;
        }
    }

    const auto created = lcu_.Send("POST", "/lol-perks/v1/pages", RunePageJson(page));
    if (created.status < 200 || created.status >= 300) {
        result.message = "клиент не принял страницу: " + ClientMessage(created);
        return result;
    }
    Log("руны: страница «{}» записана и выбрана", page.name);
    result.status = ActionResult::Status::Ok;
    result.message = "руны применены";
    return result;
}

ActionResult LcuActions::ApplySummonerSpells(int spell_a, int spell_b) {
    const std::lock_guard<std::mutex> lock(mutex_);
    ActionResult result;

    if (spell_a <= 0 || spell_b <= 0 || spell_a == spell_b) {
        result.status = ActionResult::Status::Invalid;
        result.message = "нужны два разных заклинания";
        return result;
    }

    const auto phase_body = lcu_.Get("/lol-gameflow/v1/gameflow-phase");
    if (!phase_body) {
        result.status = ActionResult::Status::ClientUnavailable;
        result.message = "клиент League не отвечает";
        return result;
    }
    if (ParseGameflowPhase(*phase_body).value_or("") != "ChampSelect") {
        result.status = ActionResult::Status::NotInChampSelect;
        result.message = "заклинания меняются только в выборе чемпиона";
        return result;
    }

    int current1 = 0;
    int current2 = 0;
    if (const auto session_body = lcu_.Get("/lol-champ-select/v1/session")) {
        if (const auto session = ParseChampSelectSession(*session_body)) {
            for (const LobbyMember& member : session->members) {
                if (member.is_self) {
                    current1 = member.spell1_id;
                    current2 = member.spell2_id;
                }
            }
        }
    }

    const auto [slot1, slot2] = ChooseSpellSlots(current1, current2, spell_a, spell_b);
    const nlohmann::json body = {{"spell1Id", slot1}, {"spell2Id", slot2}};
    const auto patched =
        lcu_.Send("PATCH", "/lol-champ-select/v1/session/my-selection", body.dump());
    if (patched.status < 200 || patched.status >= 300) {
        result.message = "клиент не принял заклинания: " + ClientMessage(patched);
        return result;
    }
    Log("заклинания: {} и {} выбраны", slot1, slot2);
    result.status = ActionResult::Status::Ok;
    result.message = "заклинания применены";
    return result;
}

}  // namespace sintence
