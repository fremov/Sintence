#include "live_api_server.h"

#include <atomic>
#include <format>
#include <print>
#include <thread>
#include <unordered_map>
#include <utility>

#include <httplib.h>

#include "json.hpp"
#include "lcu_actions.h"

namespace sintence {

namespace {

// Версия JSON-контракта. Поднимается, когда ответ меняется несовместимо
// (поле удалено или сменило тип). Новые поля версию не поднимают: схемы
// интерфейса к лишним полям терпимы. Интерфейс читает её из /api/health
// и честно говорит «обнови sintence.exe», а не рисует пустое табло.
constexpr int kApiVersion = 2;

const char* TeamName(Team team) {
    return team == Team::Order ? "ORDER" : "CHAOS";
}

// Линия, которую понимает пак. "NONE" и "" приходят из Practice Tool,
// пользовательских игр и режимов без выбора линии.
bool IsLaneRole(std::string_view position) {
    return position == "TOP" || position == "JUNGLE" || position == "MIDDLE" ||
           position == "BOTTOM" || position == "UTILITY";
}

nlohmann::json RunesToJson(const LiveRunes& runes) {
    return {
        {"keystone", runes.keystone},
        {"keystoneId", runes.keystone_id},
        {"primaryTree", runes.primary_tree},
        {"primaryTreeId", runes.primary_tree_id},
        {"secondaryTree", runes.secondary_tree},
        {"secondaryTreeId", runes.secondary_tree_id},
        {"minorRunes", runes.minor_runes},
        {"minorRuneIds", runes.minor_rune_ids},
        {"shardIds", runes.shard_ids},
    };
}

nlohmann::json VariantsToJson(const std::vector<PreferenceVariant>& variants) {
    auto array = nlohmann::json::array();
    for (const PreferenceVariant& variant : variants) {
        nlohmann::json item = {
            {"name", variant.name},
            {"games", variant.games},
            {"share", variant.share},
            {"winrate", variant.winrate},
            {"winrateLow", variant.winrate_low},
        };
        if (!variant.steps.empty()) {
            item["steps"] = variant.steps;
        }
        if (!variant.item_ids.empty()) {
            item["itemIds"] = variant.item_ids;
        }
        if (!variant.spell_ids.empty()) {
            item["spellIds"] = variant.spell_ids;
        }
        if (variant.page) {
            item["page"] = {
                {"keystone", variant.page->keystone},
                {"keystoneId", variant.page->keystone_id},
                {"primaryTree", variant.page->primary_tree},
                {"primaryTreeId", variant.page->primary_tree_id},
                {"primary", variant.page->primary},
                {"primaryIds", variant.page->primary_ids},
                {"secondaryTree", variant.page->secondary_tree},
                {"secondaryTreeId", variant.page->secondary_tree_id},
                {"secondary", variant.page->secondary},
                {"secondaryIds", variant.page->secondary_ids},
                {"shards", variant.page->shards},
                {"shardIds", variant.page->shard_ids},
            };
        }
        array.push_back(std::move(item));
    }
    return array;
}

nlohmann::json BucketToJson(const std::string& riot_id, const PreferenceBucket& bucket) {
    return {
        {"riotId", riot_id},
        {"champion", bucket.champion},
        {"role", bucket.role},
        {"opponent", bucket.opponent},
        {"tier", bucket.tier},
        {"patch", bucket.patch},
        {"games", bucket.games},
        {"winrate", bucket.winrate},
        {"runePages", VariantsToJson(bucket.rune_pages)},
        {"skillOrders", VariantsToJson(bucket.skill_orders)},
        {"itemChains", VariantsToJson(bucket.item_chains)},
        {"summonerSpells", VariantsToJson(bucket.summoner_spells)},
    };
}

// Участник для подбора советов: каноническое имя, как его показывает
// клиент, и роль. Одинаково собирается из табло и из выбора чемпиона.
struct MatchSide {
    std::string key;       // "Ahri"
    std::string name;      // "Ари" или пусто, если клиент имени не дал
    std::string position;  // "MIDDLE", "NONE" или пусто
};

std::vector<std::string> SplitCsv(const std::string& text) {
    std::vector<std::string> parts;
    std::size_t start = 0;
    while (start <= text.size()) {
        const std::size_t comma = text.find(',', start);
        const std::size_t end = comma == std::string::npos ? text.size() : comma;
        if (end > start) {
            parts.push_back(text.substr(start, end - start));
        }
        if (comma == std::string::npos) {
            break;
        }
        start = comma + 1;
    }
    return parts;
}

// Советы активному игроку против каждого противника.
//
// Роль. Клиент назначает её только в матчах с выбором линии;
// в Practice Tool и пользовательских играх приходит "NONE". Тогда
// берём ту, на которой чемпиона играют чаще всего, — иначе поиск
// по паку не найдёт ничего, и советов не будет вовсе.
// Откуда роль, уезжает во фронт: угаданную нельзя выдавать за факт.
//
// Противники: первым тот, с кем стоишь на линии, — против него сборка
// и порядок прокачки решают больше всего. Роль противника известна из
// табло; в выборе чемпиона её нет, и тогда соперником по линии считается
// тот, чья самая частая роль по паку совпадает с твоей.
nlohmann::json BuildPreferences(const PreferencePack& pack, const std::string& me_id,
                                const MatchSide& me, const std::vector<MatchSide>& enemies,
                                const std::string& tier) {
    std::string role = me.position;
    std::string role_source = "client";
    if (!IsLaneRole(role)) {
        role = pack.MainRole(me.key);
        role_source = role.empty() ? "none" : "pack";
    }

    nlohmann::json doc;
    doc["patch"] = pack.Patch();
    doc["region"] = pack.Region();
    doc["you"] = {
        {"riotId", me_id},
        {"champion", me.key},
        {"championName", me.name},
        {"role", role.empty() ? me.position : role},
        {"roleSource", role_source},
    };

    const auto enemy_role = [&](const MatchSide& enemy) {
        return IsLaneRole(enemy.position) ? enemy.position : pack.MainRole(enemy.key);
    };

    const MatchSide* laner = nullptr;
    for (const MatchSide& enemy : enemies) {
        if (!role.empty() && enemy_role(enemy) == role) {
            laner = &enemy;
            break;
        }
    }

    auto matchups = nlohmann::json::array();
    const auto append = [&](const MatchSide& enemy, bool lane) {
        const PreferenceBucket* bucket = pack.Lookup(me.key, role, enemy.key, tier);
        if (bucket == nullptr) {
            return;
        }
        nlohmann::json entry = BucketToJson(me_id, *bucket);
        entry["versus"] = enemy.key;
        entry["versusName"] = enemy.name;
        entry["versusRole"] = enemy_role(enemy);
        entry["lane"] = lane;
        // exact=false означает «данных по этому матчапу не набралось,
        // показана статистика против всех» — без этого поля интерфейс
        // выдал бы общую картину за матчапную.
        entry["exact"] = bucket->opponent == enemy.key;
        matchups.push_back(std::move(entry));
    };

    if (laner != nullptr) {
        append(*laner, true);
    }
    for (const MatchSide& enemy : enemies) {
        if (&enemy != laner) {
            append(enemy, false);
        }
    }

    // Противники ещё не выбраны (начало выбора чемпиона) — хотя бы
    // «против всех», чтобы руны можно было готовить заранее.
    if (enemies.empty()) {
        append(MatchSide{"ANY", "", ""}, false);
    }
    doc["matchups"] = std::move(matchups);
    return doc;
}

// Можно ли этому запросу менять что-то в клиенте League.
//
// Сервер слушает 127.0.0.1, но браузер пользователя — тоже на этой машине,
// и любой открытый сайт может отправить запрос на 127.0.0.1:8777. Для GET
// это безвредно, для записи рун — нет. Три проверки:
//   1. заголовок X-Sintence-Action: чужой сайт может послать его только
//      через CORS-preflight (OPTIONS), а на него сервер не отвечает
//      разрешением — браузер запрос не отправит;
//   2. Host — 127.0.0.1 или localhost: защита от DNS-rebinding, когда
//      чужой домен временно указывает на 127.0.0.1;
//   3. Origin, если есть, — сам сервер или Vite в разработке.
bool TrustedWrite(const httplib::Request& request, int port) {
    if (request.get_header_value("X-Sintence-Action") != "1") {
        return false;
    }
    const std::string host = request.get_header_value("Host");
    const std::string port_suffix = std::format(":{}", port);
    if (host != "127.0.0.1" + port_suffix && host != "localhost" + port_suffix) {
        return false;
    }
    const std::string origin = request.get_header_value("Origin");
    if (origin.empty()) {
        return true;
    }
    return origin == "http://127.0.0.1" + port_suffix || origin == "http://localhost" + port_suffix ||
           origin == "http://localhost:5173" || origin == "http://127.0.0.1:5173";
}

std::string ActionStatusName(ActionResult::Status status) {
    switch (status) {
        case ActionResult::Status::Ok: return "ok";
        case ActionResult::Status::NeedReplace: return "need_replace";
        case ActionResult::Status::NotInChampSelect: return "not_in_champselect";
        case ActionResult::Status::ClientUnavailable: return "client_unavailable";
        case ActionResult::Status::Invalid: return "invalid";
        case ActionResult::Status::Failed: return "failed";
    }
    return "failed";
}

// Ответ на действие — всегда 200 с полем status: исход действия
// в клиенте League — это данные для интерфейса, а не ошибка HTTP.
std::string ActionToJson(const ActionResult& result) {
    return nlohmann::json{
        {"status", ActionStatusName(result.status)},
        {"message", result.message},
        {"replaceName", result.replace_name},
    }.dump();
}

std::string SideName(LobbySide side) {
    return side == LobbySide::Ally ? "ALLY" : "ENEMY";
}

}  // namespace

std::string LobbyToJson(const Lobby& lobby) {
    auto members = nlohmann::json::array();
    for (const LobbyMember& member : lobby.members) {
        // puuid наружу не отдаём: интерфейсу он не нужен, а лишний
        // идентификатор в JS — лишний.
        members.push_back({
            {"side", SideName(member.side)},
            {"cellId", member.cell_id},
            {"riotId", member.riot_id},
            {"hidden", member.hidden},
            {"championId", member.champion_id},
            {"pickIntentId", member.pick_intent_id},
            {"position", member.position},
            {"spell1Id", member.spell1_id},
            {"spell2Id", member.spell2_id},
            {"isSelf", member.is_self},
        });
    }
    nlohmann::json doc = {
        {"phase", lobby.phase},
        {"source", lobby.source},
        {"timerPhase", lobby.timer_phase},
        {"timeLeftMs", lobby.time_left_ms},
        {"timerEndsAtMs", lobby.timer_ends_at_ms},
        {"note", lobby.note},
        {"bans", lobby.bans},
        {"members", std::move(members)},
    };
    return doc.dump();
}

std::string LiveGameToJson(const LiveGame& game) {
    nlohmann::json doc;
    doc["stats"] = {
        {"gameMode", game.stats.game_mode},
        {"mapName", game.stats.map_name},
        {"gameTimeSeconds", game.stats.game_time_seconds},
    };

    auto players = nlohmann::json::array();
    for (const LivePlayer& player : game.players) {
        auto items = nlohmann::json::array();
        for (const LiveItem& item : player.items) {
            items.push_back({
                {"itemId", item.item_id},
                {"name", item.name},
                {"slot", item.slot},
                {"count", item.count},
                {"price", item.price},
            });
        }

        auto spells = nlohmann::json::array();
        for (const LiveSummonerSpell& spell : player.summoner_spells) {
            spells.push_back({{"key", spell.key}, {"name", spell.name}});
        }

        players.push_back({
            {"championName", player.champion_name},
            {"championKey", player.champion_key},
            {"riotId", player.riot_id},
            {"position", player.position},
            {"team", TeamName(player.team)},
            {"level", player.level},
            {"kills", player.kills},
            {"deaths", player.deaths},
            {"assists", player.assists},
            {"creepScore", player.creep_score},
            {"isBot", player.is_bot},
            {"isDead", player.is_dead},
            {"items", std::move(items)},
            {"runes", RunesToJson(player.runes)},
            {"summonerSpells", std::move(spells)},
        });
    }
    doc["players"] = std::move(players);

    if (game.active_player) {
        const LiveActivePlayer& active = *game.active_player;

        auto abilities = nlohmann::json::array();
        for (const LiveAbility& ability : active.abilities) {
            abilities.push_back({
                {"slot", ability.slot},
                {"id", ability.id},
                {"name", ability.name},
                {"level", ability.level},
            });
        }

        doc["activePlayer"] = {
            {"riotId", active.riot_id},
            {"level", active.level},
            {"currentGold", active.current_gold},
            {"abilities", std::move(abilities)},
            {"runes", RunesToJson(active.runes)},
        };
    } else {
        // null, а не пропуск ключа: наблюдатель и реплей — рабочие режимы,
        // и фронт должен уметь их показать.
        doc["activePlayer"] = nullptr;
    }

    return doc.dump();
}

std::string ProfilesToJson(const std::vector<PlayerProfile>& profiles,
                           const ProfileService::Progress& progress) {
    nlohmann::json doc;
    doc["progress"] = {
        {"done", progress.done},
        {"total", progress.total},
        {"running", progress.running},
    };

    auto items = nlohmann::json::array();
    for (const PlayerProfile& profile : profiles) {
        nlohmann::json item;
        item["riotId"] = profile.riot_id;

        if (profile.solo_queue) {
            item["solo"] = {
                {"tier", profile.solo_queue->tier},
                {"division", profile.solo_queue->division},
                {"leaguePoints", profile.solo_queue->league_points},
                {"wins", profile.solo_queue->wins},
                {"losses", profile.solo_queue->losses},
            };
        } else {
            // null, а не пропуск поля: «не играл в соло» — это ответ,
            // и фронт должен уметь его показать, а не гадать.
            item["solo"] = nullptr;
        }

        auto masteries = nlohmann::json::array();
        for (const ChampionMastery& mastery : profile.top_masteries) {
            masteries.push_back({
                {"championId", mastery.champion_id},
                {"level", mastery.level},
                {"points", mastery.points},
                {"lastPlayTimeMs", mastery.last_play_time_ms},
            });
        }
        item["masteries"] = std::move(masteries);
        items.push_back(std::move(item));
    }
    doc["profiles"] = std::move(items);

    return doc.dump();
}

struct LiveApiServer::Impl {
    const LiveGameSource& source;
    std::string web_root;
    int port;
    ProfileService* profiles;
    const PreferencePack* pack;
    const LobbyService* lobby;
    LcuActions actions;
    httplib::Server server;
    std::thread thread;

    Impl(const LiveGameSource& source_in, std::string web_root_in, int port_in,
         ProfileService* profiles_in, const PreferencePack* pack_in,
         const LobbyService* lobby_in)
        : source(source_in),
          web_root(std::move(web_root_in)),
          port(port_in),
          profiles(profiles_in),
          pack(pack_in),
          lobby(lobby_in) {}
};

LiveApiServer::LiveApiServer(const LiveGameSource& source, std::string web_root, int port,
                             ProfileService* profiles, const PreferencePack* pack,
                             const LobbyService* lobby)
    : impl_(std::make_unique<Impl>(source, std::move(web_root), port, profiles, pack,
                                   lobby)) {}

LiveApiServer::~LiveApiServer() {
    Stop();
}

int LiveApiServer::Port() const {
    return impl_->port;
}

bool LiveApiServer::Start() {
    // Что умеет этот запуск. Интерфейс спрашивает один раз при старте:
    // версия контракта и какие функции включены, чтобы не выяснять это
    // по кодам 501 на каждом эндпоинте.
    impl_->server.Get("/api/health", [this](const httplib::Request&,
                                            httplib::Response& response) {
        response.set_header("Access-Control-Allow-Origin", "*");
        nlohmann::json doc = {
            {"apiVersion", kApiVersion},
            {"profiles", impl_->profiles != nullptr},
            {"preferences", impl_->pack != nullptr},
            {"game", impl_->source.IsAvailable()},
        };
        if (impl_->pack != nullptr) {
            doc["pack"] = {{"patch", impl_->pack->Patch()}, {"region", impl_->pack->Region()}};
        } else {
            doc["pack"] = nullptr;
        }
        response.set_content(doc.dump(), "application/json");
    });

    impl_->server.Get("/api/live", [this](const httplib::Request&, httplib::Response& response) {
        // Заголовок нужен только режиму разработки (vite на :5173 — другой
        // origin). В собранном виде фронт раздаётся этим же сервером,
        // и origin совпадает.
        response.set_header("Access-Control-Allow-Origin", "*");

        const std::optional<LiveGame> game = impl_->source.LoadGame();
        if (!game) {
            response.status = 503;
            response.set_content(R"({"error":"no active game"})", "application/json");
            return;
        }
        response.set_content(LiveGameToJson(*game), "application/json");
    });

    impl_->server.Get("/api/profiles", [this](const httplib::Request&, httplib::Response& response) {
        response.set_header("Access-Control-Allow-Origin", "*");

        if (impl_->profiles == nullptr) {
            // Ключа Riot нет — функция не реализуема в этом запуске,
            // и это 501, а не ошибка запроса.
            response.status = 501;
            response.set_content(R"({"error":"no riot api key"})", "application/json");
            return;
        }

        // Матч идёт — заказываем тех, кто на табло. Матча нет — всё равно
        // отдаём готовое: профили заказывает и выбор чемпиона (LobbyService),
        // и интерфейс показывает их до начала игры.
        if (const std::optional<LiveGame> game = impl_->source.LoadGame()) {
            // Ботов в Riot API нет: аккаунта у них не существует, и запрос
            // по их «Riot ID» гарантированно вернул бы 404. Riot ID без тега —
            // тоже бот или заглушка клиента: у живого игрока тег есть всегда.
            std::vector<std::string> riot_ids;
            riot_ids.reserve(game->players.size());
            int bots = 0;
            int nameless = 0;
            for (const LivePlayer& player : game->players) {
                if (player.is_bot) {
                    ++bots;
                    continue;
                }
                if (player.riot_id.empty() || player.riot_id.find('#') == std::string::npos) {
                    ++nameless;
                    continue;
                }
                riot_ids.push_back(player.riot_id);
            }

            const ProfileService::Progress before = impl_->profiles->Status();
            impl_->profiles->Request(riot_ids);
            const ProfileService::Progress after = impl_->profiles->Status();
            if (after.total != before.total) {
                std::println("профили: заказано игроков — {} (ботов пропущено {}, "
                             "без тега {}), всего в очереди {}",
                             riot_ids.size(), bots, nameless, after.total);
            }
        }

        response.set_content(
            ProfilesToJson(impl_->profiles->Ready(), impl_->profiles->Status()),
            "application/json");
    });

    // Состав до начала матча: выбор чемпиона (LCU) и экран загрузки
    // (spectator-v5). Клиент League не запущен — 200 с пустым phase:
    // это состояние, а не ошибка.
    impl_->server.Get("/api/lobby", [this](const httplib::Request&, httplib::Response& response) {
        response.set_header("Access-Control-Allow-Origin", "*");
        if (impl_->lobby == nullptr) {
            response.status = 501;
            response.set_content(R"({"error":"lobby service disabled"})", "application/json");
            return;
        }
        response.set_content(LobbyToJson(impl_->lobby->Snapshot()), "application/json");
    });

    impl_->server.Get("/api/preferences", [this](const httplib::Request& request,
                                                 httplib::Response& response) {
        response.set_header("Access-Control-Allow-Origin", "*");

        if (impl_->pack == nullptr) {
            response.status = 501;
            response.set_content(R"({"error":"no preference pack"})", "application/json");
            return;
        }

        // Ранг берём из уже выкачанного профиля: мета в Изумруде и мета
        // в Бронзе — разные меты. Профиля ещё нет — общая корзина,
        // и это видно по полю tier у бакета.
        const auto tier_of = [this](const std::string& riot_id) {
            if (impl_->profiles != nullptr) {
                for (const PlayerProfile& profile : impl_->profiles->Ready()) {
                    if (profile.riot_id == riot_id && profile.solo_queue) {
                        return profile.solo_queue->tier;
                    }
                }
            }
            return std::string("ALL");
        };

        const std::optional<LiveGame> game = impl_->source.LoadGame();
        if (!game) {
            // До матча: чемпион, роль и пики противников приходят параметрами
            // от интерфейса, который знает их из /api/lobby и переводит
            // числовые id в ключи через Data Dragon:
            //   /api/preferences?champion=Ahri&role=MIDDLE&enemies=Zed,LeeSin
            const std::string champion = request.get_param_value("champion");
            if (champion.empty()) {
                response.status = 503;
                response.set_content(R"({"error":"no active game"})", "application/json");
                return;
            }
            std::string self_id;
            if (impl_->lobby != nullptr) {
                for (const LobbyMember& member : impl_->lobby->Snapshot().members) {
                    if (member.is_self) {
                        self_id = member.riot_id;
                    }
                }
            }
            MatchSide me{champion, request.get_param_value("name"),
                         request.get_param_value("role")};
            std::vector<MatchSide> enemies;
            for (const std::string& key : SplitCsv(request.get_param_value("enemies"))) {
                enemies.push_back({key, "", ""});
            }
            response.set_content(
                BuildPreferences(*impl_->pack, self_id, me, enemies, tier_of(self_id)).dump(),
                "application/json");
            return;
        }

        // Советы строятся ТОЛЬКО для активного игрока: чужие руны и
        // порядок прокачки Live Client не отдаёт никому, а показывать
        // «как обычно играют на Ясуо» на карточке противника — значит
        // подсказывать, чего игрок видеть не должен (project/POLICY.md).
        if (!game->active_player) {
            response.status = 503;
            response.set_content(R"({"error":"no active player"})", "application/json");
            return;
        }
        const std::string& me_id = game->active_player->riot_id;

        const LivePlayer* me = nullptr;
        for (const LivePlayer& player : game->players) {
            if (player.riot_id == me_id) {
                me = &player;
                break;
            }
        }
        if (me == nullptr || me->champion_key.empty()) {
            response.status = 503;
            response.set_content(R"({"error":"active player not on scoreboard"})",
                                 "application/json");
            return;
        }

        std::vector<MatchSide> enemies;
        for (const LivePlayer& player : game->players) {
            if (player.team != me->team && !player.champion_key.empty()) {
                enemies.push_back({player.champion_key, player.champion_name, player.position});
            }
        }
        const MatchSide self{me->champion_key, me->champion_name, me->position};
        response.set_content(
            BuildPreferences(*impl_->pack, me_id, self, enemies, tier_of(me_id)).dump(),
            "application/json");
    });

    // Применить страницу рун. Только по клику в интерфейсе — см. lcu_actions.h.
    //   {"name": "...", "primaryStyleId": 8100, "subStyleId": 8200,
    //    "perkIds": [9 id], "replace": false}
    impl_->server.Post("/api/champselect/runes", [this](const httplib::Request& request,
                                                        httplib::Response& response) {
        if (!TrustedWrite(request, impl_->port)) {
            response.status = 403;
            response.set_content(R"({"error":"forbidden"})", "application/json");
            return;
        }
        const auto body = nlohmann::json::parse(request.body, nullptr, false);
        if (body.is_discarded() || !body.is_object()) {
            response.status = 400;
            response.set_content(R"({"error":"bad json"})", "application/json");
            return;
        }
        RunePageSpec page;
        page.name = body.value("name", std::string("Sintence")).substr(0, 25);
        page.primary_style_id = body.value("primaryStyleId", 0);
        page.sub_style_id = body.value("subStyleId", 0);
        if (body.contains("perkIds") && body["perkIds"].is_array()) {
            for (const auto& id : body["perkIds"]) {
                page.perk_ids.push_back(id.is_number_integer() ? id.get<int>() : 0);
            }
        }
        const bool replace = body.value("replace", false);
        response.set_content(ActionToJson(impl_->actions.ApplyRunePage(page, replace)),
                             "application/json");
    });

    // Применить пару заклинаний призывателя: {"spell1Id": 4, "spell2Id": 14}.
    // Раскладку по D/F сервер подбирает сам, не сдвигая уже стоящие.
    impl_->server.Post("/api/champselect/spells", [this](const httplib::Request& request,
                                                         httplib::Response& response) {
        if (!TrustedWrite(request, impl_->port)) {
            response.status = 403;
            response.set_content(R"({"error":"forbidden"})", "application/json");
            return;
        }
        const auto body = nlohmann::json::parse(request.body, nullptr, false);
        if (body.is_discarded() || !body.is_object()) {
            response.status = 400;
            response.set_content(R"({"error":"bad json"})", "application/json");
            return;
        }
        const auto result = impl_->actions.ApplySummonerSpells(body.value("spell1Id", 0),
                                                               body.value("spell2Id", 0));
        response.set_content(ActionToJson(result), "application/json");
    });

    if (!impl_->server.set_mount_point("/", impl_->web_root)) {
        return false;
    }

    // bind_to_port до запуска потока: так ошибка «порт занят» видна сразу,
    // а не молча теряется внутри фонового потока.
    if (!impl_->server.bind_to_port("127.0.0.1", impl_->port)) {
        return false;
    }

    impl_->thread = std::thread([this] { impl_->server.listen_after_bind(); });
    return true;
}

void LiveApiServer::Stop() {
    if (!impl_) {
        return;
    }
    impl_->server.stop();
    if (impl_->thread.joinable()) {
        impl_->thread.join();
    }
}

}  // namespace sintence
