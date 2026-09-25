#include "live_api_server.h"

#include <atomic>
#include <thread>
#include <utility>

#include <httplib.h>

#include "json.hpp"

namespace sintence {

namespace {

const char* TeamName(Team team) {
    return team == Team::Order ? "ORDER" : "CHAOS";
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
        players.push_back({
            {"championName", player.champion_name},
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
        });
    }
    doc["players"] = std::move(players);

    return doc.dump();
}

struct LiveApiServer::Impl {
    const LiveGameSource& source;
    std::string web_root;
    int port;
    httplib::Server server;
    std::thread thread;

    Impl(const LiveGameSource& source_in, std::string web_root_in, int port_in)
        : source(source_in), web_root(std::move(web_root_in)), port(port_in) {}
};

LiveApiServer::LiveApiServer(const LiveGameSource& source, std::string web_root, int port)
    : impl_(std::make_unique<Impl>(source, std::move(web_root), port)) {}

LiveApiServer::~LiveApiServer() {
    Stop();
}

int LiveApiServer::Port() const {
    return impl_->port;
}

bool LiveApiServer::Start() {
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
