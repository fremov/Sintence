#include "live_api_server.h"

#include <atomic>
#include <print>
#include <thread>
#include <unordered_map>
#include <utility>

#include <httplib.h>

#include "json.hpp"

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
    };
}

}  // namespace

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
    httplib::Server server;
    std::thread thread;

    Impl(const LiveGameSource& source_in, std::string web_root_in, int port_in,
         ProfileService* profiles_in, const PreferencePack* pack_in)
        : source(source_in),
          web_root(std::move(web_root_in)),
          port(port_in),
          profiles(profiles_in),
          pack(pack_in) {}
};

LiveApiServer::LiveApiServer(const LiveGameSource& source, std::string web_root, int port,
                             ProfileService* profiles, const PreferencePack* pack)
    : impl_(std::make_unique<Impl>(source, std::move(web_root), port, profiles, pack)) {}

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

        const std::optional<LiveGame> game = impl_->source.LoadGame();
        if (!game) {
            response.status = 503;
            response.set_content(R"({"error":"no active game"})", "application/json");
            return;
        }

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

        response.set_content(
            ProfilesToJson(impl_->profiles->Ready(), impl_->profiles->Status()),
            "application/json");
    });

    impl_->server.Get("/api/preferences", [this](const httplib::Request&,
                                                 httplib::Response& response) {
        response.set_header("Access-Control-Allow-Origin", "*");

        if (impl_->pack == nullptr) {
            response.status = 501;
            response.set_content(R"({"error":"no preference pack"})", "application/json");
            return;
        }

        const std::optional<LiveGame> game = impl_->source.LoadGame();
        if (!game) {
            response.status = 503;
            response.set_content(R"({"error":"no active game"})", "application/json");
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

        // Ранг берём из уже выкачанного профиля: мета в Изумруде и мета
        // в Бронзе — разные меты. Профиля ещё нет — общая корзина,
        // и это видно по полю tier у бакета.
        std::string tier = "ALL";
        if (impl_->profiles != nullptr) {
            for (const PlayerProfile& profile : impl_->profiles->Ready()) {
                if (profile.riot_id == me_id && profile.solo_queue) {
                    tier = profile.solo_queue->tier;
                    break;
                }
            }
        }

        // Роль. Клиент назначает её только в матчах с выбором линии;
        // в Practice Tool и пользовательских играх приходит "NONE". Тогда
        // берём ту, на которой чемпиона играют чаще всего, — иначе поиск
        // по паку не найдёт ничего, и советов не будет вовсе.
        // Откуда роль, уезжает во фронт: угаданную нельзя выдавать за факт.
        std::string role = me->position;
        std::string role_source = "client";
        if (!IsLaneRole(role)) {
            role = impl_->pack->MainRole(me->champion_key);
            role_source = role.empty() ? "none" : "pack";
        }

        nlohmann::json doc;
        doc["patch"] = impl_->pack->Patch();
        doc["region"] = impl_->pack->Region();
        doc["you"] = {
            {"riotId", me_id},
            {"champion", me->champion_key},
            {"championName", me->champion_name},
            {"role", role.empty() ? me->position : role},
            {"roleSource", role_source},
        };

        // Противники: первым тот, с кем стоишь на линии, — против него
        // сборка и порядок прокачки решают больше всего. Остальные идут
        // следом в порядке табло.
        auto matchups = nlohmann::json::array();
        const LivePlayer* laner = nullptr;
        for (const LivePlayer& player : game->players) {
            if (player.team != me->team && !player.champion_key.empty() &&
                !role.empty() && player.position == role) {
                laner = &player;
                break;
            }
        }

        const auto append = [&](const LivePlayer& enemy, bool lane) {
            const PreferenceBucket* bucket = impl_->pack->Lookup(
                me->champion_key, role, enemy.champion_key, tier);
            if (bucket == nullptr) {
                return;
            }
            nlohmann::json entry = BucketToJson(me_id, *bucket);
            entry["versus"] = enemy.champion_key;
            entry["versusName"] = enemy.champion_name;
            entry["versusRole"] = enemy.position;
            entry["lane"] = lane;
            // exact=false означает «данных по этому матчапу не набралось,
            // показана статистика против всех» — без этого поля интерфейс
            // выдал бы общую картину за матчапную.
            entry["exact"] = bucket->opponent == enemy.champion_key;
            matchups.push_back(std::move(entry));
        };

        if (laner != nullptr) {
            append(*laner, true);
        }
        for (const LivePlayer& enemy : game->players) {
            if (enemy.team == me->team || enemy.champion_key.empty()) {
                continue;
            }
            if (laner != nullptr && &enemy == laner) {
                continue;
            }
            append(enemy, false);
        }
        doc["matchups"] = std::move(matchups);

        response.set_content(doc.dump(), "application/json");
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
