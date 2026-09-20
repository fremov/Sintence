#ifndef COURSE_TOPIC03_MATCH_SOURCE_H
#define COURSE_TOPIC03_MATCH_SOURCE_H

#include <string>
#include <vector>
#include <print>
#include "champion_report.h"  // тема 1 — MatchLine

// ============================================================================
// Задача 3.1 — MatchSource: интерфейс и первая реализация
//
// Критерии приёмки (что значит «готово» сверх зелёных тестов):
//   1. MatchSource остаётся абстрактным: ни одного тела в нём не появилось.
//      Создать его объект невозможно, и тест это проверяет.
//   2. FixtureMatchSource переопределяет все три метода, каждое переопределение
//      помечено override. Без него опечатка в сигнатуре не будет замечена.
//   3. LoadMatches не отдаёт ссылку на внутренний вектор и не меняет объект:
//      метод const, возвращается копия.
//   4. Недоступный источник — не ошибка: IsAvailable() == false, а LoadMatches()
//      возвращает пустой вектор. Живая игра выключена — анализатор работает дальше.
//   5. Заглушек и TODO в файле не осталось.
//
// Одна идея этой задачи: вызывающий код работает с MatchSource и никогда
// не знает, какая реализация ему досталась.
//
// Callback к теме 1: MatchEntry несёт MatchLine — ту самую структуру, которой
// ты кормил ChampionReport неделю назад.
// ============================================================================

namespace course {

// Одна строка матча вместе с именем чемпиона, на котором её сыграли.
// Структура, а не класс: голые поля, никаких инвариантов («Чистый код», глава 6).
//
// Источник отдаёт именно такие записи — уже разобранные, без JSON и без HTTP.
// Разбор появится в теме 8, сеть — в теме 15; интерфейс от этого не изменится.
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
    // только базовую часть объекта (срезка). Тема 2, правило 3/5.
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

}  // namespace course

#endif  // COURSE_TOPIC03_MATCH_SOURCE_H
