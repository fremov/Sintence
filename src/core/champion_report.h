#ifndef SINTENCE_CORE_CHAMPION_REPORT_H
#define SINTENCE_CORE_CHAMPION_REPORT_H

#include <string>
#include <vector>

// core/champion_report — метрики по одному чемпиону: KDA, winrate, CS/min.
//
// Метрики вычисляются в const-методах на лету и не хранятся в полях:
// два источника правды рано или поздно разойдутся.
// Пустой отчёт отвечает нулями, а не мусором.

namespace sintence {

// Одна строка статистики за матч. Это структура данных, а не объект:
// у неё нет инвариантов и нет поведения, только поля. Поэтому struct,
// а не class 
//
// Поля соответствуют данным Riot один в один:
//   kills / deaths / assists — participant.kills / deaths / assists
//   win                      — participant.win
//   minions                  — participant.totalMinionsKilled
//                              + participant.neutralMinionsKilled
//   duration_seconds         — info.gameDuration
struct MatchLine {
    int kills = 0;
    int deaths = 0;
    int assists = 0;
    bool win = false;
    int minions = 0;
    int duration_seconds = 0;
};

class ChampionReport {
public:
    // Порог, ниже которого выводы по статистике не имеют смысла.
    // 30 игр — не магия, а грубая граница, за которой winrate перестаёт
    // прыгать на десять процентов от одного матча.
    static constexpr int kMinGamesForConclusion = 30;

    explicit ChampionReport(std::string champion_name);

    // Добавляет строку матча.
    // Отбрасывает целиком строку, в которой duration_seconds <= 0
    // или любое из kills / deaths / assists / minions отрицательно.
    void Add(const MatchLine& line);

    const std::string& ChampionName() const;
    int Games() const;

    // (kills + assists) / deaths по всем матчам сразу, а не среднее
    // от KDA отдельных матчей — это разные числа, и первое честнее.
    // Ноль смертей за всю выборку: делим на 1, это принятое соглашение
    // (иначе получится бесконечность, которую нечем показать в таблице).
    // Нет матчей — 0.0.
    double Kda() const;

    // Доля побед от 0.0 до 1.0. Нет матчей — 0.0.
    double Winrate() const;

    // Суммарные миньоны, делённые на суммарное время в минутах.
    // Нет матчей — 0.0.
    double CsPerMinute() const;

    // true, если матчей набралось хотя бы kMinGamesForConclusion.
    bool HasEnoughData() const;

private:
    std::string champion_name_;
    std::vector<MatchLine> lines_;
};

}  // namespace sintence

#endif  // SINTENCE_CORE_CHAMPION_REPORT_H
