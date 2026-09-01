#include "kda.h"

// Третий параметр здесь double, а в заголовке — int.
// Компилятор не ругается: он считает, что ты просто определил ещё одну
// перегрузку, а ту, что объявлена в заголовке, определишь где-то ещё.

double ComputeKda(int kills, int deaths, double assists) {
    const int safe_deaths = deaths == 0 ? 1 : deaths;
    return (kills + assists) / safe_deaths;
}
