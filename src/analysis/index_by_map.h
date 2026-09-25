#ifndef SINTENCE_ANALYSIS_INDEX_BY_MAP_H
#define SINTENCE_ANALYSIS_INDEX_BY_MAP_H

#include "champion_index.h"  // ChampionIndex
#include "match_source.h"  // интерфейс источника

// analysis/index_by_map — сборка индекса чемпионов через std::map.
//
// Быстрый вариант BuildChampionIndex: поиск накопленного отчёта стоит
// O(log N) вместо линейного прохода.
//
// ВАЖНО — отличие в наблюдаемом контракте, а не только в скорости:
// std::map хранит ключи отсортированными, поэтому ChampionNames() здесь
// возвращает имена по алфавиту, а не в порядке первого появления в записях.
// Порядок зафиксирован в контракте ниже и проверяется тестом.

namespace sintence {

// Собирает индекс отчётов по чемпионам из любого источника матчей.
//
// Контракт (отличается от BuildChampionIndex ровно одним пунктом — порядком):
//   - недоступный источник (IsAvailable() == false) даёт пустой индекс;
//   - на каждое имя чемпиона — ровно один отчёт со всеми его строками;
//   - порядок чемпионов в индексе — ПО ВОЗРАСТАНИЮ ИМЕНИ, а не порядок
//     первого появления;
//   - записи с пустым champion_name отбрасываются целиком;
//   - валидность самих строк матча не проверяется здесь — это работа
//     ChampionReport::Add. Чемпион, у которого все строки
//     оказались негодными, остаётся в индексе с нулём игр.
ChampionIndex BuildChampionIndexFast(const MatchSource& source);

}  // namespace sintence

#endif  // SINTENCE_ANALYSIS_INDEX_BY_MAP_H
