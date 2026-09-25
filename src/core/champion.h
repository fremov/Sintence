#ifndef SINTENCE_CORE_CHAMPION_H
#define SINTENCE_CORE_CHAMPION_H

#include <string>

// core/champion — чемпион и его роль.
//
// Инвариант держит конструктор: после его работы Name() и Role() не пустые,
// а Role() всегда в верхнем регистре. Геттеры ничего не перепроверяют.

namespace sintence {

class Champion {
public:
    // Создаёт чемпиона.
    //   name — как в поле participant.championName из данных Riot: "Ahri", "LeeSin".
    //   role — как в participant.teamPosition: "TOP", "JUNGLE", "MIDDLE",
    //          "BOTTOM", "UTILITY".
    //
    // Должен принять "middle", "Middle" и "MIDDLE" как одно и то же.
    // Должен отвергнуть пустые строки: пустое имя превращается в "Unknown",
    // пустая роль — в "UNKNOWN". В реальных матчах teamPosition приходит пустым,
    // когда игрок ушёл с линии на ранней стадии, и падать из-за этого нельзя.
    Champion(std::string name, std::string role);

    // Имя чемпиона как есть. Регистр не трогаем: "Ahri" — имя собственное,
    // и в отчёте оно должно выглядеть человечески.
    const std::string& Name() const;

    // Роль в верхнем регистре. Никогда не пустая.
    const std::string& Role() const;

    // Строка для отчёта: "Ahri (MIDDLE)".
    std::string DisplayName() const;

    // true, если роль совпадает с переданной без учёта регистра.
    // Нужен для фильтра «покажи только мид».
    // Пустой аргумент — не роль: должен вернуть false.
    bool HasRole(const std::string& role) const;

private:
    std::string name_;
    std::string role_;
};

}  // namespace sintence

#endif  // SINTENCE_CORE_CHAMPION_H
