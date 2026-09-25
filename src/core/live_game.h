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

// Одна строка табло живого матча: то, что игрок и так видит на экране (Tab).
//
// Скрытого состояния здесь нет и быть не может — Live Client Data API его
// не отдаёт, и project/POLICY.md запрещает на нём строить.
struct LivePlayer {
    std::string champion_name;  // "Annie"
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
