#include "live_client_source.h"
#include <httplib.h>
#include <utility>
#include "live_client_json.h"

namespace sintence {

LiveClientSource::LiveClientSource(std::string host, int port)
    : host_(std::move(host)), port_(port) {}

std::string LiveClientSource::Name() const {
    return "live-client";
}

bool LiveClientSource::IsAvailable() const {
    return Fetch("/liveclientdata/gamestats").has_value();
}

std::optional<LiveGame> LiveClientSource::LoadGame() const {
    auto stats_body = Fetch("/liveclientdata/gamestats");
    if (!stats_body) {
        return std::nullopt;
    }
    auto players_body = Fetch("/liveclientdata/playerlist");
    if (!players_body) {
        return std::nullopt;
    }
    auto stats = ParseLiveGameStats(*stats_body);
    if (!stats) {
        return std::nullopt;
    }
    auto players = ParseLivePlayers(*players_body);
    if (!players) {
        return std::nullopt;
    }
    LiveGame game;
    game.stats = std::move(*stats);
    game.players = std::move(*players);

    return game;
}

std::optional<std::string> LiveClientSource::Fetch(
    std::string_view path) const {
    httplib::SSLClient cli(host_, port_);
    cli.enable_server_certificate_verification(false);
    cli.set_connection_timeout(2);
    cli.set_read_timeout(2);
    auto res = cli.Get(std::string(path));
    if (res && res->status == 200) {
        return res->body;
    }
    return std::nullopt;
}

}  // namespace sintence
