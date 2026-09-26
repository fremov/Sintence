#ifndef SINTENCE_DATA_PREFERENCE_PACK_H
#define SINTENCE_DATA_PREFERENCE_PACK_H

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

// data/preference_pack — предпочтения по чемпионам, посчитанные заранее.
//
// Приложение НЕ знает про базу матчей. Краулер (scripts/crawl.py) собирает
// сотни тысяч наблюдений в SQLite, агрегатор (scripts/build_pack.py) сводит
// их в один файл на пару мегабайт, и только этот файл сюда попадает.
// Сегодня он лежит на диске, завтра приедет по HTTP с сервера — формат
// и этот класс не изменятся.
//
// Почему не SQLite в C++: база на сотни мегабайт продукту не нужна, ей
// место у исследователя. Продукту нужен готовый ответ, а он помещается
// в хеш-таблицу целиком — поиск идёт без диска и без запросов.

namespace sintence {


// Страница рун целиком: основное древо с ключевой руной и тремя малыми,
// дополнительное с двумя, плюс три осколка статов.
//
// Половину страницы показывать бессмысленно: игрок не сможет её повторить.
// Поэтому вариант тут — вся страница как одно целое, а не распределение
// по каждой руне отдельно.
struct RunePage {
    std::string keystone;
    std::string primary_tree;
    std::vector<std::string> primary;    // три малых основного древа
    std::string secondary_tree;
    std::vector<std::string> secondary;  // две малых дополнительного
    std::vector<std::string> shards;     // атака, гибкий, защита

    // Те же руны числовыми perk id (схема пака 3+). По ним интерфейс берёт
    // иконки и названия на языке клиента: имена в паке английские.
    // В паке второй схемы id нет — векторы пустые, нули.
    int keystone_id = 0;
    int primary_tree_id = 0;
    std::vector<int> primary_ids;
    int secondary_tree_id = 0;
    std::vector<int> secondary_ids;
    std::vector<int> shard_ids;
};

// Один вариант: «так играют N раз, доля столько-то, винрейт такой-то».
struct PreferenceVariant {
    std::string name;
    int games = 0;
    double share = 0.0;    // доля среди всех игр бакета
    double winrate = 0.0;  // голая доля побед

    // Нижняя граница интервала Вильсона. Вариант с пятью играми из пяти
    // не обгонит проверенный вариант с 500 играми из 900 — именно поэтому
    // сортировать «по силе» нужно по ней, а не по winrate.
    double winrate_low = 0.0;

    // Цепочка: порядок повышения способностей ("Q","E","W"...) или
    // предметов в порядке покупки. Пусто у вариантов-страниц рун.
    std::vector<std::string> steps;

    // Id предметов цепочки, параллельно steps (схема пака 3+).
    // У порядка прокачки и у страниц рун пусто.
    std::vector<int> item_ids;

    // Заполнена только у страниц рун.
    std::optional<RunePage> page;
};

// Всё, что известно про пару «чемпион + роль» против конкретного
// оппонента по линии в одной ранговой корзине.
struct PreferenceBucket {
    std::string champion;  // каноническое "Vladimir"
    std::string role;      // "MIDDLE"
    std::string opponent;  // "Yasuo" или "ANY" — против всех
    std::string tier;      // "EMERALD", "DIAMOND", "ALL"
    std::string patch;     // "16.19"
    int games = 0;
    double winrate = 0.0;

    std::vector<PreferenceVariant> rune_pages;
    std::vector<PreferenceVariant> skill_orders;  // Q>E>W>Q>Q
    std::vector<PreferenceVariant> item_chains;   // Youmuu's → Eclipse → ...
};

class PreferencePack {
public:
    // Пустой пак: Lookup всегда возвращает nullptr. Это рабочее состояние —
    // пака может ещё не быть, и интерфейс обязан это пережить.
    PreferencePack() = default;

    // Читает JSON, собранный build_pack.py. Несовпадение schemaVersion —
    // отказ целиком: половина знакомых полей хуже, чем честное «нет данных».
    static std::optional<PreferencePack> LoadFromFile(const std::string& path);
    static std::optional<PreferencePack> LoadFromJson(std::string_view json_text);

    // Каталог с паками: читает index.json, берёт из него имя файла
    // и грузит его. Ровно так же будет устроен серверный вариант —
    // сначала индекс, потом пак, — поэтому формат индекса задан здесь.
    static std::optional<PreferencePack> LoadFromDirectory(const std::string& dir);

    // Поиск идёт по лестнице, сверху вниз, до первого попадания:
    //
    //   чемпион | роль | оппонент | корзина игрока   — самое точное
    //   чемпион | роль | оппонент | ALL              — матчап, любой ранг
    //   чемпион | роль | ANY      | корзина игрока   — против всех
    //   чемпион | роль | ANY      | ALL              — последнее прибежище
    //
    // Матчапов около сорока тысяч, и на редкие выборки не хватит никогда —
    // поэтому откат не аварийный путь, а нормальный режим работы. Что
    // именно нашлось, видно по полям opponent и tier у бакета: интерфейс
    // обязан их показывать, иначе «против всех» прочитается как
    // «против этого конкретного Ясуо».
    //
    // Указатель живёт, пока жив пак.
    const PreferenceBucket* Lookup(std::string_view champion, std::string_view role,
                                   std::string_view opponent,
                                   std::string_view tier) const;

    // Роль, на которой чемпиона играют чаще всего, по бакетам «против всех,
    // все ранги». Пустая строка — чемпиона в паке нет.
    //
    // Нужна там, где клиент роль не назначает: Practice Tool, пользовательские
    // игры, обычные игры без выбора линии приходят с position "NONE" или "".
    // Без неё Lookup искал бы бакет "Vladimir|NONE|..." и не находил ничего.
    std::string MainRole(std::string_view champion) const;

    // Тир игрока ("EMERALD", "GRANDMASTER") -> корзина пака.
    // Корзины укрупнённые: по отдельному дивизиону выборка расползается,
    // а сборки в Изумруде I и IV не различаются.
    static std::string TierBucket(std::string_view tier);

    const std::string& Patch() const { return patch_; }
    const std::string& Region() const { return region_; }
    std::size_t Size() const { return buckets_.size(); }

private:
    // Ключ — "Vladimir|MIDDLE|Yasuo|EMERALD", как его пишет build_pack.py.
    std::unordered_map<std::string, PreferenceBucket> buckets_;
    // Чемпион -> (роль, игр) самой частой роли. Строится при загрузке.
    std::unordered_map<std::string, std::pair<std::string, int>> main_roles_;
    std::string patch_;
    std::string region_;
};

}  // namespace sintence

#endif  // SINTENCE_DATA_PREFERENCE_PACK_H
