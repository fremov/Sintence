#ifndef SINTENCE_DATA_LCU_ACTIONS_H
#define SINTENCE_DATA_LCU_ACTIONS_H

#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "lcu_client.h"

// data/lcu_actions — единственные действия, которые Sintence совершает
// в клиенте League: страница рун и свои заклинания призывателя.
//
// Только по явному клику игрока. Политика Riot: продукт не должен
// принимать решения за игрока и действовать от его имени сам — импорт
// рун по нажатию кнопки допустим, автоматический нет. Поэтому здесь нет
// ни одного вызова «по событию»: только ответ на POST из интерфейса.
//
// Чужие страницы рун молча не трогаются. Своя — та, чьё имя начинается
// с «Sintence», — перезаписывается. Нет свободного места — отказ с именем
// страницы, которую можно заменить, и замена только повторным кликом
// с явным согласием (replace_current).

namespace sintence {

// Страница рун: два дерева и девять perk id в порядке клиента —
// ключевая, три малых основного древа, две дополнительного, три осколка.
struct RunePageSpec {
    std::string name;
    int primary_style_id = 0;
    int sub_style_id = 0;
    std::vector<int> perk_ids;
};

// Полная ли страница: оба дерева разные и ненулевые, девять ненулевых id.
bool IsCompleteRunePage(const RunePageSpec& page);

// Тело POST /lol-perks/v1/pages. current: true — сразу сделать активной.
std::string RunePageJson(const RunePageSpec& page);

// Что сделать со страницами, чтобы записать новую.
struct PagePlan {
    std::optional<long long> delete_id;  // удалить перед записью
    bool need_replace = false;           // места нет, нужно согласие
    std::string replace_name;            // какую страницу предлагаем заменить
    bool impossible = false;             // нет ни одной удаляемой страницы
};

// pages_json — ответ GET /lol-perks/v1/pages, inventory_json —
// GET /lol-perks/v1/inventory (ownedPageCount — сколько своих страниц можно).
//
// Порядок:
//   1. есть своя страница («Sintence…», удаляемая) — удалить её и записать;
//   2. своих страниц меньше, чем разрешено, — просто записать;
//   3. места нет: replace_current — удалить текущую (если она не встроенная,
//      иначе первую удаляемую) и записать; без него — need_replace с именем;
//   4. удаляемых страниц нет вовсе — impossible.
// Битый JSON — nullopt.
std::optional<PagePlan> PlanRunePage(std::string_view pages_json,
                                     std::string_view inventory_json, bool replace_current);

// Раскладка пары заклинаний по клавишам D и F.
//
// Игрок привыкает держать Скачок на своей клавише, и перестановка стоила бы
// ему Скачка в первой же драке. Поэтому заклинание, которое у него уже
// стоит, остаётся в своём слоте, а новое занимает другой.
//   сейчас (Скачок, Лечение), нужно {Воспламенение, Скачок}
//     -> (Скачок, Воспламенение)
//   сейчас (Призрак, Скачок), нужно {Скачок, Телепорт}
//     -> (Телепорт, Скачок)
std::pair<int, int> ChooseSpellSlots(int current1, int current2, int want_a, int want_b);

// Результат действия для интерфейса.
struct ActionResult {
    enum class Status {
        Ok,
        NeedReplace,        // нет свободной страницы рун, нужен повторный клик
        NotInChampSelect,   // заклинания меняются только в выборе чемпиона
        ClientUnavailable,  // клиент League не найден
        Invalid,            // неполная страница или неверные id
        Failed,             // клиент отказал — текст в message
    };
    Status status = Status::Failed;
    std::string message;
    std::string replace_name;
};

class LcuActions {
public:
    ActionResult ApplyRunePage(const RunePageSpec& page, bool replace_current);
    ActionResult ApplySummonerSpells(int spell_a, int spell_b);

private:
    // Два клика подряд не должны переплестись: удаление одной страницы
    // и запись другой — это два запроса.
    std::mutex mutex_;
    LcuClient lcu_;
};

}  // namespace sintence

#endif  // SINTENCE_DATA_LCU_ACTIONS_H
