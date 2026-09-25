#ifndef SINTENCE_CORE_MATCH_SOURCE_H
#define SINTENCE_CORE_MATCH_SOURCE_H

#include <string>
#include <vector>
#include <print>
#include "champion_report.h"  // MatchLine

// core/match_source — интерфейс источника матчей и реализация на фикстурах.
//
// Вызывающий код работает с MatchSource и не знает, какая реализация ему
// досталась: файлы, Match-V5, Live Client Data или заранее подготовленные
// записи.
//
// Недоступный источник — не ошибка: IsAvailable() == false, LoadMatches()
// возвращает пустой вектор, приложение работает дальше.

namespace sintence {

// Одна строка матча вместе с именем чемпиона, на котором её сыграли.
// Структура, а не класс: голые поля, никаких инвариантов 
//
// Источник отдаёт именно такие записи — уже разобранные, без JSON и без HTTP.
// Разбор появится, сеть —; интерфейс от этого не изменится.
struct MatchEntry {
    std::string champion_name;
    MatchLine line;
};

// Интерфейс любого источника матчей: фикстуры, Match-V5, Live Client Data, LCU.
// Три реализации из project/ARCHITECTURE.md появятся в темах 15 и 19,
// и ни одна из них не потребует менять этот заголовок.
class MatchSource {
public:
    virtual ~MatchSource() = default;

    // Имя источника для сообщений и логов: "fixtures", "match-v5", "live-client".
    virtual std::string Name() const = 0;

    // Доступен ли источник прямо сейчас. Live Client Data существует только
    // во время матча, LCU — только пока запущен клиент, HTTP — пока живёт ключ.
    virtual bool IsAvailable() const = 0;

    // Все матчи источника. Недоступный источник возвращает пустой вектор,
    // а не бросает исключение: офлайн — это рабочий режим, а не сбой.
    virtual std::vector<MatchEntry> LoadMatches() const = 0;

    // Интерфейс не копируется: копия через базовую ссылку скопировала бы
    // только базовую часть объекта (срезка).
    MatchSource(const MatchSource&) = delete;
    MatchSource& operator=(const MatchSource&) = delete;

protected:
    // Создавать может только наследник — снаружи создавать нечего.
    MatchSource() = default;
};

// Источник из заранее подготовленных записей: то, чем тестируется весь анализатор,
// пока нет ни сети, ни разбора JSON. Четвёртая реализация, которая по
// project/ARCHITECTURE.md достаётся бесплатно.
class FixtureMatchSource : public MatchSource {
public:
    // available задаётся явно, чтобы можно было проверить поведение
    // недоступного источника, не выключая интернет.
    FixtureMatchSource(std::string name, std::vector<MatchEntry> entries, bool available);

    std::string Name() const override;
    bool IsAvailable() const override;

    // Недоступный источник обязан вернуть пустой вектор, даже если записи
    // в нём лежат: смысл флага в том, что данные сейчас читать нельзя.
    std::vector<MatchEntry> LoadMatches() const override;

private:
    std::string name_;
    std::vector<MatchEntry> entries_;
    bool available_ = false;
};

}  // namespace sintence

#endif  // SINTENCE_CORE_MATCH_SOURCE_H
