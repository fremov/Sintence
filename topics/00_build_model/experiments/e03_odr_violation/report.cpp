// Вторая единица трансляции. Больше ей ничего делать не надо —
// достаточно включить тот же заголовок.

#include "kda.h"

double BestKdaOfTwo(int k1, int d1, int a1, int k2, int d2, int a2) {
    const double first = ComputeKda(k1, d1, a1);
    const double second = ComputeKda(k2, d2, a2);
    return first > second ? first : second;
}
