#ifndef SINTENCE_ANALYSIS_TOP_CHAMPIONS_H
#define SINTENCE_ANALYSIS_TOP_CHAMPIONS_H

#include <cstddef>
#include <map>
#include <string>
#include <vector>

// analysis/top_champions — топ чемпионов по числу игр.
//
// На входе таблица от CountGamesByChampion. Порядок при равном числе игр
// задан компаратором явно, а не зависит от данных; limit больше размера
// таблицы — не ошибка.

namespace sintence {

// Строка таблицы «чемпион — сколько игр».
//
// Структура, а не класс: голые поля, никаких инвариантов
//  Нужна потому, что std::map отдаёт пары
// std::pair<const std::string, int>, а наружу удобнее отдавать
// именованные поля, а не .first / .second.
struct ChampionGames {
    std::string champion_name;
    int games = 0;
};

// Топ чемпионов по числу игр.
//
// Контракт:
//   - порядок: по УБЫВАНИЮ числа игр;
//   - при равном числе игр — по ВОЗРАСТАНИЮ имени ("Ahri" раньше "Zed");
//     это часть контракта, а не то, как получилось: тест проверяет;
//   - limit — сколько строк вернуть максимум. limit == 0 даёт пустой вектор,
//     limit больше размера таблицы — всю таблицу, без выхода за границы;
//   - пустая таблица даёт пустой вектор.
//
// Чемпионы с нулём игр в таблицу не попадают по построению (CountGamesByChampion
// создаёт ключ только на непустой записи), поэтому отдельно их отбрасывать
// здесь не нужно.
std::vector<ChampionGames> TopChampionsByGames(const std::map<std::string, int>& games,
                                               std::size_t limit);

}  // namespace sintence

#endif  // SINTENCE_ANALYSIS_TOP_CHAMPIONS_H
