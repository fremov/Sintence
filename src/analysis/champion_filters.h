#ifndef SINTENCE_ANALYSIS_CHAMPION_FILTERS_H
#define SINTENCE_ANALYSIS_CHAMPION_FILTERS_H

#include <string>
#include <vector>

#include "champion_index.h"  // ChampionIndex, внутри ChampionReport

// analysis/champion_filters — выборки по индексу чемпионов.
//
// Три вопроса к одному индексу: сколько чемпионов набрали достаточную
// выборку, какие из них прошли порог по числу игр, есть ли хотя бы один
// с винрейтом выше заданного.

namespace sintence {

// Сколько чемпионов в индексе набрали достаточно игр для выводов.
//
// «Достаточно» определяет сам отчёт — ChampionReport::HasEnoughData().
// Своего порога здесь заводить не нужно: он уже есть
// (kMinGamesForConclusion), и второй его экземпляр разойдётся с первым.
//
// Пустой индекс — 0.
int CountChampionsWithEnoughData(const ChampionIndex& index);

// Имена чемпионов, у которых игр не меньше min_games.
//
// Контракт:
//   - сравнение нестрогое: ровно min_games игр — попадает;
//   - порядок имён — тот же, что у ChampionIndex::ChampionNames(),
//     то есть порядок добавления. Сортировка — дело вызывающего кода, а не эта функция;
//   - min_games <= 0 — вернуть всех, кто есть в индексе;
//   - пустой индекс — пустой вектор.
std::vector<std::string> ChampionsWithMinGames(const ChampionIndex& index, int min_games);

// Есть ли в индексе хотя бы один чемпион с винрейтом строго выше порога,
// по которому при этом набрано достаточно данных.
//
// Оба условия обязательны, и порядок важен именно такой: winrate 100%
// по двум играм — это не повод что-то советовать игроку. Проверять
// достаточность данных — половина смысла этой функции.
//
// Пустой индекс — false.
bool HasChampionAboveWinrate(const ChampionIndex& index, double min_winrate);

}  // namespace sintence

#endif  // SINTENCE_ANALYSIS_CHAMPION_FILTERS_H
