#ifndef SINTENCE_ANALYSIS_CHAMPION_POOL_H
#define SINTENCE_ANALYSIS_CHAMPION_POOL_H

#include <cstddef>
#include <map>
#include <set>
#include <string>

// analysis/champion_pool — пул чемпионов игрока.
//
// На ком игрок вообще играет и на каких позициях. Уникальность имён держит
// std::set, роли нормализуются через Champion — второго места, где роль
// приводится к верхнему регистру, в проекте нет.

namespace sintence {

class ChampionPool {
public:
    // Записывает, что на этом чемпионе сыграли на этой роли.
    //
    // Повтор той же пары — не ошибка и не дубликат: пул хранит уникальные
    // значения, второй вызов с теми же аргументами ничего не меняет.
    //
    // Пустое имя чемпиона отбрасывается целиком: чемпиона без имени
    // в пуле быть не может. Пустая роль — нормальный вход, так бывает,
    // когда игрок ушёл с линии; во что её превращать, решает Champion.
    void Add(const std::string& champion_name, const std::string& role);

    // Все уникальные чемпионы пула, по возрастанию имени.
    //
    // Возвращается ссылка, а не копия: множество живёт ровно столько же,
    // сколько пул, и печатается часто. Оно const — снаружи его не испортить.
    const std::set<std::string>& Champions() const;

    // Роли, на которых играли этого чемпиона. Неизвестный чемпион —
    // пустое множество, а не исключение.
    //
    // Здесь возвращается копия, а не ссылка: для неизвестного чемпиона
    // внутри просто нет объекта, на который можно сослаться.
    std::set<std::string> RolesOf(const std::string& champion_name) const;

    bool Contains(const std::string& champion_name) const;

    // Сколько уникальных чемпионов в пуле.
    std::size_t Size() const;

private:
    std::set<std::string> champions_;
    std::map<std::string, std::set<std::string>> roles_;
};

}  // namespace sintence

#endif  // SINTENCE_ANALYSIS_CHAMPION_POOL_H
