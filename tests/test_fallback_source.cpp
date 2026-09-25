#include "doctest.h"

#include "fallback_source.h"

#include <string>
#include <utility>
#include <vector>

using sintence::FallbackMatchSource;
using sintence::FixtureMatchSource;
using sintence::MatchEntry;
using sintence::MatchSource;

namespace {

MatchEntry Entry(std::string champion, int kills) {
    MatchEntry entry;
    entry.champion_name = std::move(champion);
    entry.line.kills = kills;
    entry.line.deaths = 2;
    entry.line.assists = 5;
    entry.line.win = true;
    entry.line.minions = 200;
    entry.line.duration_seconds = 1800;
    return entry;
}

std::vector<MatchEntry> LiveEntries() {
    return {Entry("Ahri", 10), Entry("LeeSin", 4)};
}

std::vector<MatchEntry> CacheEntries() {
    return {Entry("Thresh", 1)};
}

// Ещё одна реализация того же интерфейса — с доступностью, которую можно
// переключать на ходу. Живая игра начинается и заканчивается, пока приложение
// работает, и FallbackMatchSource обязан это видеть.
//
// Заодно доказывает, что он работает с любой реализацией MatchSource,
// а не только с FixtureMatchSource.
class ToggleSource : public MatchSource {
public:
    ToggleSource(std::string name, std::vector<MatchEntry> entries)
        : name_(std::move(name)), entries_(std::move(entries)) {}

    void SetAvailable(bool available) { available_ = available; }

    std::string Name() const override { return name_; }
    bool IsAvailable() const override { return available_; }

    std::vector<MatchEntry> LoadMatches() const override {
        return available_ ? entries_ : std::vector<MatchEntry>();
    }

private:
    std::string name_;
    std::vector<MatchEntry> entries_;
    bool available_ = false;
};

}  // namespace

TEST_CASE("основной доступен — работает он") {
    const FixtureMatchSource live("live-client", LiveEntries(), true);
    const FixtureMatchSource cache("cache", CacheEntries(), true);
    const FallbackMatchSource source(live, cache);

    CHECK(source.Name() == "live-client");
    CHECK(source.IsAvailable());
    CHECK(source.LoadMatches().size() == 2);
    CHECK(source.Active() == &live);
}

TEST_CASE("основной недоступен — переключается на резервный") {
    const FixtureMatchSource live("live-client", LiveEntries(), false);
    const FixtureMatchSource cache("cache", CacheEntries(), true);
    const FallbackMatchSource source(live, cache);

    CHECK(source.Name() == "cache");
    CHECK(source.IsAvailable());

    const std::vector<MatchEntry> entries = source.LoadMatches();
    REQUIRE(entries.size() == 1);
    CHECK(entries[0].champion_name == "Thresh");
    CHECK(source.Active() == &cache);
}

TEST_CASE("оба недоступны — пусто, но не падение") {
    // Некорректный вход: данных нет ниоткуда. Офлайн и клиент закрыт —
    // это рабочее состояние анализатора, а не сбой.
    const FixtureMatchSource live("live-client", LiveEntries(), false);
    const FixtureMatchSource cache("cache", CacheEntries(), false);
    const FallbackMatchSource source(live, cache);

    CHECK_FALSE(source.IsAvailable());
    CHECK(source.Name() == "none");
    CHECK(source.LoadMatches().empty());
    CHECK(source.Active() == nullptr);
}

TEST_CASE("доступный, но пустой основной остаётся выбранным") {
    // Граничный случай, который легко перепутать: «нет записей» — это не
    // «недоступен». Если источник доступен, переключаться нельзя, иначе
    // анализатор подмешает старые данные к новому пустому матчу.
    const FixtureMatchSource live("live-client", {}, true);
    const FixtureMatchSource cache("cache", CacheEntries(), true);
    const FallbackMatchSource source(live, cache);

    CHECK(source.Name() == "live-client");
    CHECK(source.IsAvailable());
    CHECK(source.LoadMatches().empty());
    CHECK(source.Active() == &live);
}

TEST_CASE("FallbackMatchSource сам является MatchSource") {
    // Реализация интерфейса, состоящая из реализаций того же интерфейса.
    const FixtureMatchSource live("live-client", LiveEntries(), false);
    const FixtureMatchSource cache("cache", CacheEntries(), true);
    const FallbackMatchSource fallback(live, cache);

    const MatchSource& as_interface = fallback;
    CHECK(as_interface.Name() == "cache");
    CHECK(as_interface.IsAvailable());
    CHECK(as_interface.LoadMatches().size() == 1);
}

TEST_CASE("fallback можно вложить в fallback") {
    // Три источника цепочкой: живая игра → сеть → локальный кеш.
    // Ни один класс для этого не менялся — так и должно быть.
    const FixtureMatchSource live("live-client", LiveEntries(), false);
    const FixtureMatchSource remote("match-v5", {}, false);
    const FixtureMatchSource cache("cache", CacheEntries(), true);

    const FallbackMatchSource live_then_remote(live, remote);
    const FallbackMatchSource chain(live_then_remote, cache);

    CHECK(chain.Name() == "cache");
    CHECK(chain.IsAvailable());
    CHECK(chain.LoadMatches().size() == 1);
}

TEST_CASE("смена доступности видна сразу") {
    // Активный источник не запоминается в конструкторе: он выбирается
    // в момент вызова. Иначе анализатор застрянет на кеше до перезапуска.
    ToggleSource live("live-client", LiveEntries());
    const FixtureMatchSource cache("cache", CacheEntries(), true);
    const FallbackMatchSource source(live, cache);

    CHECK(source.Name() == "cache");
    CHECK(source.LoadMatches().size() == 1);

    live.SetAvailable(true);

    CHECK(source.Name() == "live-client");
    CHECK(source.LoadMatches().size() == 2);
    CHECK(source.Active() == &live);
}

TEST_CASE("работает с любой реализацией интерфейса") {
    // Ни FixtureMatchSource, ни ToggleSource в fallback_source.cpp
    // не упоминаются — иначе этот тест был бы невозможен.
    ToggleSource first("primary", LiveEntries());
    ToggleSource second("backup", CacheEntries());
    const FallbackMatchSource source(first, second);

    CHECK_FALSE(source.IsAvailable());

    second.SetAvailable(true);
    CHECK(source.Name() == "backup");

    first.SetAvailable(true);
    CHECK(source.Name() == "primary");
}
