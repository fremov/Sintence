#ifndef SINTENCE_ANALYSIS_RANGES_SUMMARY_H
#define SINTENCE_ANALYSIS_RANGES_SUMMARY_H

#include <cstddef>
#include <string>
#include <vector>

#include "champion_index.h"  // ChampionIndex, внутри ChampionReport

// analysis/ranges_summary — сводка по индексу чемпионов.
//
// ChampionSummary — готовая строка таблицы для интерфейса: метрики посчитаны
// заранее, а не вычисляются при каждой отрисовке.
// Ни одна функция не возвращает view наружу, только std::vector.

namespace sintence {

// Строка сводки по одному чемпиону: всё, что нужно показать в таблице.
//
// Структура без инвариантов: её собирают из ChampionReport один раз
// и дальше только читают. Метрики хранятся уже вычисленными — в отличие
// от ChampionReport, который считает их на лету. Это осознанно: таблица
// на экране пересчитывает метрики на каждый кадр, если этого не сделать.
struct ChampionSummary {
    std::string champion_name;
    int games = 0;
    double winrate = 0.0;
    double kda = 0.0;
    bool enough_data = false;
};

// Сводка по всем чемпионам индекса.
//
// Контракт:
//   - по одной строке на каждого чемпиона из ChampionIndex::ChampionNames();
//   - порядок — тот же, что у ChampionNames() (порядок добавления);
//   - поля берутся у соответствующего ChampionReport: Games(), Winrate(),
//     Kda(), HasEnoughData();
//   - пустой индекс — пустой вектор.
std::vector<ChampionSummary> BuildSummaries(const ChampionIndex& index);

// Топ по винрейту среди тех, по кому есть данные.
//
// Контракт:
//   - строки с enough_data == false отбрасываются целиком, каким бы
//     ни был их винрейт: 100% по двум играм не попадает в топ никогда;
//   - порядок — по УБЫВАНИЮ winrate;
//   - ничья по винрейту контрактом не определена: порядок таких строк
//     считать стабильным нельзя;
//   - limit — сколько строк вернуть максимум; 0 даёт пустой вектор,
//     число больше размера — всё, что есть;
//   - пустой вход — пустой вектор.
std::vector<ChampionSummary> TopByWinrate(const std::vector<ChampionSummary>& summaries,
                                          std::size_t limit);

// Сколько всего игр во всей сводке.
//
// Складываются games всех строк, включая те, где данных мало:
// это ответ на вопрос «сколько матчей вообще разобрано», а не
// «сколько матчей пригодны для выводов».
//
// Пустая сводка — 0.
int TotalGames(const std::vector<ChampionSummary>& summaries);

}  // namespace sintence

#endif  // SINTENCE_ANALYSIS_RANGES_SUMMARY_H
