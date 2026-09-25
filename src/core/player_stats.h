#ifndef SINTENCE_CORE_PLAYER_STATS_H
#define SINTENCE_CORE_PLAYER_STATS_H

#include <string>

// core/player_stats — счётчики игрока: игры, победы, поражения.
//
// Состояние меняется ровно одним способом — через AddMatch, который отвечает
// за корректность. Инвариант: счётчики не отрицательные, Wins() <= Games(),
// Losses() + Wins() == Games().
//
// Испорченная строка матча не учитывается вообще: частично учтённый матч
// хуже пропущенного, он тихо портит статистику.

namespace sintence {

class PlayerStats {
public:
    // champion_name — имя чемпиона, по которому копится статистика.
    // explicit, чтобы PlayerStats не создавался случайно из строки.
    explicit PlayerStats(std::string champion_name);

    // Добавляет результат одного матча.
    //
    // Отрицательные kills / deaths / assists — это испорченные данные
    // (в JSON бывает null, который парсер превратит в -1). Такой матч
    // должен быть отброшен целиком: Games() не растёт, счётчики не меняются.
    //
    // Нули — нормальные данные: 0/0/0 бывает при ремейке на четвёртой минуте.
    void AddMatch(int kills, int deaths, int assists, bool win);

    const std::string& ChampionName() const;

    // Сколько матчей учтено.
    int Games() const;

    // Побед и поражений. Losses() не хранится отдельным полем —
    // хранить то, что выводится из уже имеющегося, значит завести
    // второй источник правды и рано или поздно их рассинхронизировать.
    int Wins() const;
    int Losses() const;

    // Суммы по всем учтённым матчам.
    int TotalKills() const;
    int TotalDeaths() const;
    int TotalAssists() const;

private:
    std::string champion_name_;
    int games_ = 0;
    int wins_ = 0;
    int total_kills_ = 0;
    int total_deaths_ = 0;
    int total_assists_ = 0;
};

}  // namespace sintence

#endif  // SINTENCE_CORE_PLAYER_STATS_H
