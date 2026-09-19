// Это единица трансляции (translation unit): kda.cpp плюс всё, что
// втягивают его #include. Компилятор превращает её в один объектный файл
// kda.obj, ничего не зная про остальные файлы проекта.

#include "kda.h"

namespace course {

// Определение. Сигнатура обязана совпасть с объявлением в заголовке
// до последнего символа: имя, namespace, типы параметров, const.
// Если разойдётся хоть в одном — компилятор промолчит, а линкер скажет
// unresolved external symbol / undefined reference.
double ComputeKda(const Kda& kda) {
    const int deaths = kda.deaths == 0 ? 1 : kda.deaths;
    return static_cast<double>(kda.kills + kda.assists) / deaths;
}

}  // namespace course
