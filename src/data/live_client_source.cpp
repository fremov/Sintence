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
    // Один запрос вместо трёх: /allgamedata содержит и статистику матча,
    // и табло с предметами, и активного игрока с рунами и способностями.
    // Тело около 40-80 КБ, но это локальный сокет.
    auto body = Fetch("/liveclientdata/allgamedata");
    if (!body) {
        return std::nullopt;
    }
    return ParseAllGameData(*body);
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
