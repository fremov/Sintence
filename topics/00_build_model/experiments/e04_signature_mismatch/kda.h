#ifndef E04_KDA_H
#define E04_KDA_H

// Объявление говорит: третий параметр — int.
// В kda.cpp определение говорит: третий параметр — double.
// Для C++ это две разные функции (перегрузка), а не опечатка.
//
// Почини: приведи объявление и определение к одной сигнатуре.

double ComputeKda(int kills, int deaths, int assists);

#endif  // E04_KDA_H
