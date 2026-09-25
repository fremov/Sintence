#ifndef SINTENCE_CORE_CHAMPION_INDEX_H
#define SINTENCE_CORE_CHAMPION_INDEX_H

#include <string>
#include <vector>

#include "champion_report.h"

// core/champion_index — отчёты по чемпионам, сложенные в одно место.
//
// Правило нуля: все поля управляют собой сами, поэтому ни одна из шести
// специальных функций-членов здесь не объявлена, а копирование, перемещение
// и разрушение работают корректно.
//
// Find возвращает указатель на хранимый отчёт, а не копию.

namespace sintence {

// Все отчёты по чемпионам в одном месте: то, что анализатор показывает
// на главном экране. Поиск линейный — std::map появится,
// и там же станет видно, зачем он тут нужен.
class ChampionIndex {
public:
    // Добавляет отчёт в индекс.
    //
    // Принимает по значению сознательно: вызывающий сам решает, отдать копию
    // (index.AddReport(report)) или временный объект без копирования
    // (index.AddReport(std::move(report))). Внутри отчёт перемещается в вектор.
    //
    // Если отчёт по этому чемпиону уже есть — он заменяется целиком.
    // Иначе Find стал бы неоднозначным: два отчёта на одно имя.
    void AddReport(ChampionReport report);

    // Отчёт по имени чемпиона или nullptr, если такого нет.
    // Имя сравнивается точно, как оно пришло: нормализацией занимается Champion
    //, а не индекс.
    const ChampionReport* Find(const std::string& champion_name) const;

    int Size() const;
    bool Empty() const;

    // Имена всех чемпионов в порядке добавления.
    std::vector<std::string> ChampionNames() const;

private:
    std::vector<ChampionReport> reports_;
};

}  // namespace sintence

#endif  // SINTENCE_CORE_CHAMPION_INDEX_H
