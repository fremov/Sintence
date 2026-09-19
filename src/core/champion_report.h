#ifndef COURSE_TOPIC01_CHAMPION_REPORT_H
#define COURSE_TOPIC01_CHAMPION_REPORT_H

#include <string>
#include <vector>

// ============================================================================
// Задача 1.3 — ChampionReport: KDA, winrate, CS/min
//
// Критерии приёмки (что значит «готово» сверх зелёных тестов):
//   1. Все метрики вычисляются в const-методах на лету. Ни одно вычисленное
//      значение не хранится в поле: два источника правды рано или поздно разойдутся.
//   2. Нигде нет деления int на int там, где нужен дробный результат,
//      и нигде нет деления на ноль.
//   3. Пустой отчёт отвечает нулями, а не падает и не возвращает мусор.
//   4. HasEnoughData честно говорит, что выборки мало. Анализатор, который
//      печатает winrate 70% по семи играм, врёт красивым числом.
//   5. Заглушек и TODO в файле не осталось.
//
// Одна идея этой задачи: const-методы, которые вычисляют, а не хранят.
// Возврат к спринту 1: данные лежат в std::vector, и по нему надо пройти.
// ============================================================================

namespace course {

// Одна строка статистики за матч. Это структура данных, а не объект:
// у неё нет инвариантов и нет поведения, только поля. Поэтому struct,
// а не class («Чистый код», глава 6).
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

}  // namespace course

#endif  // COURSE_TOPIC01_CHAMPION_REPORT_H
