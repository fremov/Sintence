#ifndef SINTENCE_ANALYSIS_GAMES_BY_CHAMPION_H
#define SINTENCE_ANALYSIS_GAMES_BY_CHAMPION_H

#include <map>
#include <string>
#include <vector>

#include "match_source.h"  // MatchEntry

// analysis/games_by_champion — таблица «чемпион -> сколько записей».
//
// Базовая агрегация, с которой начинается любой экран статистики.
// Записи с пустым именем чемпиона в таблицу не попадают.

namespace sintence {

// Сколько записей пришлось на каждого чемпиона.
//
// Ключ — имя чемпиона как оно пришло, значение — число записей с этим именем.
// Регистр не нормализуется: "ahri" и "Ahri" — разные ключи, нормализацией
// занимается Champion, а не этот слой.
//
// Здесь не проверяется валидность самих строк матча: это работа
// ChampionReport::Add. Считаются все записи, включая те, что позже будут
// отброшены как битые.
std::map<std::string, int> CountGamesByChampion(const std::vector<MatchEntry>& entries);

// Сколько игр у конкретного чемпиона. Нет такого чемпиона — 0.
//
// Таблица передана по const-ссылке специально: так операция чтения
// физически не может изменить её. Попытка написать games[name] здесь
// не скомпилируется, и это правильный результат, а не препятствие.
int GamesOf(const std::map<std::string, int>& games, const std::string& champion_name);

// Имя чемпиона с наибольшим числом записей.
// Пустая таблица — пустая строка.
// Ничья: побеждает тот, кто меньше лексикографически ("Ahri" раньше "Zed").
std::string MostPlayedChampion(const std::map<std::string, int>& games);

}  // namespace sintence

#endif  // SINTENCE_ANALYSIS_GAMES_BY_CHAMPION_H
