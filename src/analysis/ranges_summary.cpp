#include "ranges_summary.h"

#include <algorithm>
#include <ranges>

namespace course {

std::vector<ChampionSummary> BuildSummaries(const ChampionIndex& index) {
    // TODO: собрать сводку по всем чемпионам индекса.
    //
    // ПОТОК ДАННЫХ
    //   вход:      ChampionIndex (тема 2)
    //   конвейер:  имена -> отчёты -> строки сводки
    //   выход:     std::vector<ChampionSummary>
    //
    // API, который нужен:
    //   index.ChampionNames()             -> std::vector<std::string>
    //   index.Find(name)                  -> const ChampionReport*
    //   report->Games() / Winrate() / Kda() / HasEnoughData()
    //   std::views::transform(f)          превратить каждый элемент
    //   std::ranges::to<std::vector>()    материализовать конвейер (C++23)
    //
    // ПОРЯДОК ДЕЙСТВИЙ
    //   1. Взять имена из индекса.
    //   2. Пропустить их через views::transform: лямбда получает имя,
    //      возвращает готовый ChampionSummary. Захват — [&index] поимённо.
    //   3. Замкнуть конвейер на std::ranges::to<std::vector>().
    //      Без него у тебя на руках view, а не вектор, и вернуть его
    //      наружу нельзя: он ссылается на локальные данные и умрёт
    //      вместе с ними (ASan покажет heap-use-after-free).
    //   4. ChampionSummary — агрегат: заполняется фигурными скобками
    //      в порядке объявления полей (имя, games, winrate, kda, enough_data).
    //
    // Про nullptr от Find: имена взяты из самого индекса, поэтому отчёт
    // найдётся всегда. Но проверку всё равно напиши — по той же причине,
    // что и в 5.2: контракт Find допускает nullptr, и полагаться на то,
    // что «здесь не может быть», — значит оставить UB на будущее.
    // Что вернуть для ненайденного имени, реши сам и объясни на ревью:
    // пустая строка сводки и пропуск элемента — разные решения
    // с разными последствиями для таблицы на экране.
    (void)index;
    return {};
}

std::vector<ChampionSummary> TopByWinrate(const std::vector<ChampionSummary>& summaries,
                                          std::size_t limit) {
    // TODO: топ по винрейту среди тех, у кого достаточно данных.
    //
    // ПОТОК ДАННЫХ
    //   вход:      готовая сводка и лимит
    //   конвейер:  отфильтровать -> материализовать -> отсортировать -> обрезать
    //   выход:     std::vector<ChampionSummary>
    //
    // API, который нужен:
    //   std::views::filter(pred)                     отобрать
    //   std::ranges::to<std::vector>()               материализовать
    //   std::ranges::sort(v, comp, proj)             отсортировать
    //   std::ranges::greater{}                       компаратор «по убыванию»
    //   &ChampionSummary::winrate                    проекция на поле
    //   std::views::take(n)                          взять первые n
    //
    // ПОРЯДОК ДЕЙСТВИЙ
    //   1. Отфильтровать строки с enough_data. Предикат можно записать
    //      проекцией на само поле: filter(&ChampionSummary::enough_data)
    //      читается как «оставить те, у кого поле истинно».
    //   2. Материализовать в вектор: сортировать view нельзя, sort меняет
    //      элементы местами, а view — это описание, а не хранилище.
    //   3. Отсортировать по убыванию winrate. Здесь критерий — ровно одно
    //      поле, поэтому нужна ПРОЕКЦИЯ, а не лямбда:
    //      std::ranges::sort(rows, std::ranges::greater{}, &ChampionSummary::winrate).
    //      Сравни с задачей 5.1, где критерий был составной и проекция
    //      не годилась.
    //   4. Обрезать до limit. Способ на выбор: resize (как в 5.1) или
    //      views::take + ranges::to. take безопасен при limit больше
    //      размера — он просто отдаст всё, что есть.
    //   5. Вернуть вектор, не view.
    (void)summaries;
    (void)limit;
    return {};
}

int TotalGames(const std::vector<ChampionSummary>& summaries) {
    // TODO: сумма games по всей сводке.
    //
    // ПОТОК ДАННЫХ
    //   вход:   сводка
    //   выход:  int
    //
    // API, который нужен:
    //   std::ranges::fold_left(диапазон, начальное_значение, функция)
    //
    // fold_left — это C++23-замена std::accumulate: берёт начальное значение
    // и функцию «накопитель + элемент -> новый накопитель», проходит диапазон
    // слева направо и возвращает итог. Здесь начальное значение 0,
    // а функция прибавляет к накопителю поле games очередной строки.
    //
    // Одна строка. Если получился цикл — перечитай раздел 6 теории.
    (void)summaries;
    return 0;
}

}  // namespace course
