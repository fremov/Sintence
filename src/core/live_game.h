#ifndef SINTENCE_CORE_LIVE_GAME_H
#define SINTENCE_CORE_LIVE_GAME_H

#include <optional>
#include <string>
#include <vector>

// core/live_game — снимок идущего прямо сейчас матча.
//
// Зачем отдельный тип, а не MatchEntry: MatchLine описывает ЗАКОНЧЕННЫЙ матч,
// у неё есть win и duration_seconds. В середине игры победителя не существует,
// и поле пришлось бы заполнять фиктивным false. Тип, у которого половина полей
// «не смотри сюда», отравляет всё, что его касается, поэтому у живого матча
// свои поля — те, которые в нём действительно есть.
//
// Слой core: ни HTTP, ни JSON, ни nlohmann. Здесь только доменные типы.

namespace sintence {

// Сторона карты. Riot называет команды ORDER (синие, низ слева)
// и CHAOS (красные, верх справа) — числовых id в Live Client API нет.
enum class Team {
    Order,
    Chaos,
};

// Предмет в инвентаре. Live Client отдаёт готовое название — справочник
// Data Dragon для этого не нужен.
struct LiveItem {
    int item_id = 0;
    std::string name;   // "Doran's Blade"
    int slot = 0;       // 0..6, где 6 — ячейка тринкета
    int count = 1;      // стак зелий
    int price = 0;      // цена покупки, не суммарная за стак
};

// Способность активного игрока. Уровень — то, сколько очков в неё вложено.
struct LiveAbility {
    std::string slot;  // "Q", "W", "E", "R", "Passive"
    std::string id;    // "AhriQ", "VladimirBloodGorged" — ключ Data Dragon
    std::string name;  // "Death Lotus"
    int level = 0;     // у Passive всегда 0
};

// Руны. Названия локализованы языком клиента, id — числовые perk id Riot:
// по ним интерфейс находит иконки и названия в Data Dragon.
//
// У активного игрока заполнено всё. У остальных Live Client отдаёт только
// ключевую руну и два дерева — ровно то, что видно на табло по Tab, —
// поэтому minor_* и shard_ids у них пустые.
struct LiveRunes {
    std::string keystone;        // "Electrocute"
    std::string primary_tree;    // "Domination"
    std::string secondary_tree;  // "Precision"
    std::vector<std::string> minor_runes;  // пять малых рун, порядок Riot

    int keystone_id = 0;        // 8112
    int primary_tree_id = 0;    // 8100
    int secondary_tree_id = 0;  // 8200
    std::vector<int> minor_rune_ids;  // параллельно minor_runes
    std::vector<int> shard_ids;       // осколки статов: 5008, 5008, 5001
};

// Заклинание призывателя. Видно у всех на табло.
struct LiveSummonerSpell {
    std::string key;   // "SummonerFlash" — ключ Data Dragon
    std::string name;  // "Скачок" — как показывает клиент
};

// Активный игрок — тот, за кем клиент. Про него API отдаёт то, чего нет
// про остальных: уровни способностей, руны целиком и текущее золото.
//
// Чужие способности и руны сюда не попадут никогда: Live Client их
// не отдаёт, а достраивать скрытое состояние запрещает POLICY.md.
struct LiveActivePlayer {
    std::string riot_id;
    int level = 0;
    double current_gold = 0.0;
    std::vector<LiveAbility> abilities;  // Q, W, E, R, Passive
    LiveRunes runes;
};

// Одна строка табло живого матча: то, что игрок и так видит на экране (Tab).
//
// Скрытого состояния здесь нет и быть не может — Live Client Data API его
// не отдаёт, и project/POLICY.md запрещает на нём строить.
struct LivePlayer {
    std::string champion_name;  // как показывает клиент: "Annie", "Владимир"

    // Каноническое имя из rawChampionName ("Zed", "MonkeyKing").
    // Нужно потому, что champion_name локализован языком клиента, и по нему
    // нельзя ни искать в паке предпочтений, ни ходить в Data Dragon.
    std::string champion_key;
    std::string riot_id;        // "Riot Tuxedo#TXC1"
    std::string position;       // "MIDDLE"; в ARAM приходит пустой строкой
    Team team = Team::Order;
    int level = 0;
    int kills = 0;
    int deaths = 0;
    int assists = 0;
    int creep_score = 0;
    bool is_bot = false;
    bool is_dead = false;

    // Инвентарь. Приходит в /playerlist и в /allgamedata, поэтому известен
    // про ВСЕХ участников, а не только про активного игрока.
    std::vector<LiveItem> items;

    // Ключевая руна и деревья (без малых рун) и два заклинания призывателя:
    // и то и другое показывает табло по Tab.
    LiveRunes runes;
    std::vector<LiveSummonerSpell> summoner_spells;
};

// Общие сведения о матче: ровно то, что отдаёт /liveclientdata/gamestats.
struct LiveGameStats {
    std::string game_mode;  // "CLASSIC", "ARAM", "PRACTICETOOL"
    std::string map_name;   // "Map11"
    double game_time_seconds = 0.0;  // Riot отдаёт дробное число, не int
};

// Матч целиком: статистика плюс табло.
struct LiveGame {
    LiveGameStats stats;
    std::vector<LivePlayer> players;

    // Есть только в ответе /allgamedata. Спектатор и реплей активного
    // игрока не имеют, поэтому optional, а не поле по умолчанию.
    std::optional<LiveActivePlayer> active_player;
};

// Интерфейс источника живого матча.
//
// Отдельный от MatchSource намеренно: MatchSource отвечает на вопрос
// «какие матчи сыграны», а этот — «что происходит прямо сейчас».
// Разные вопросы, разные времена, разная доступность.
class LiveGameSource {
public:
    virtual ~LiveGameSource() = default;

    // Имя источника для сообщений: "live-client", "fixture".
    virtual std::string Name() const = 0;

    // Идёт ли матч прямо сейчас. Вне игры порт 2999 не слушает,
    // и это нормальное состояние, а не сбой.
    virtual bool IsAvailable() const = 0;

    // Снимок матча. Матча нет или данные не разобрались — nullopt.
    // Исключений не бросает: «игра не запущена» — рабочий режим.
    virtual std::optional<LiveGame> LoadGame() const = 0;

    LiveGameSource(const LiveGameSource&) = delete;
    LiveGameSource& operator=(const LiveGameSource&) = delete;

protected:
    LiveGameSource() = default;
};

}  // namespace sintence

#endif  // SINTENCE_CORE_LIVE_GAME_H
