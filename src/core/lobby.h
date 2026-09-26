#ifndef SINTENCE_CORE_LOBBY_H
#define SINTENCE_CORE_LOBBY_H

#include <string>
#include <vector>

// core/lobby — состав игры ДО начала матча: выбор чемпиона и экран загрузки.
//
// Отдельно от LiveGame, потому что знания здесь другие. В выборе чемпиона
// противники безымянны, у союзников может быть скрыто имя, чемпион ещё
// не выбран, а есть только намерение. Табло живого матча ничего этого
// не знает, и поля «может быть, потом» ему не нужны.
//
// Слой core: ни HTTP, ни JSON. См. project/CHAMP_SELECT.md.

namespace sintence {

// Своя сторона или чужая. Не ORDER/CHAOS: в выборе чемпиона клиент
// говорит «моя команда» и «их команда», а синяя она или красная, решает
// уже матч.
enum class LobbySide {
    Ally,
    Enemy,
};

struct LobbyMember {
    LobbySide side = LobbySide::Ally;
    int cell_id = -1;          // место в выборе чемпиона, 0..9

    // Пусто, если клиент имя не показывает (анонимность в ранговых
    // очередях) или это противник в выборе чемпиона. Восстанавливать
    // скрытое запрещает POLICY.md — пустое так и остаётся пустым.
    std::string puuid;
    std::string riot_id;       // "Riot Tuxedo#TXC1"
    bool hidden = false;       // клиент явно сказал «имя скрыто»

    int champion_id = 0;       // выбран и зафиксирован (или 0)
    int pick_intent_id = 0;    // наведён, но ещё не выбран (или 0)
    std::string position;      // "MIDDLE" — назначенная роль, если есть
    int spell1_id = 0;         // заклинания призывателя, числовые id
    int spell2_id = 0;
    bool is_self = false;
};

// Снимок лобби.
struct Lobby {
    // Этап клиента как есть: "None", "Lobby", "Matchmaking", "ReadyCheck",
    // "ChampSelect", "GameStart", "InProgress", "EndOfGame"... Пусто —
    // клиент League не найден.
    std::string phase;

    // Откуда состав: "lcu" — выбор чемпиона, "lcu-game" — экран загрузки
    // и матч из сессии клиента, "spectator" — то же из spectator-v5,
    // "" — состава нет.
    std::string source;

    // Внутренний этап выбора чемпиона ("BAN_PICK", "FINALIZATION"...)
    // и сколько в нём осталось.
    //
    // time_left_ms — остаток НА МОМЕНТ последнего изменения сессии: клиент
    // обновляет сессию только по событиям (пик, бан), и между ними это число
    // стоит на месте. Поэтому есть timer_ends_at_ms — момент окончания
    // этапа по часам Unix (мс); интерфейс сам считает до него обратный
    // отсчёт. 0 — неизвестно.
    std::string timer_phase;
    long long time_left_ms = 0;
    long long timer_ends_at_ms = 0;

    // Почему состава нет или откуда он — для интерфейса и для отладки:
    // «жду spectator-v5: попытка 3, ответ 404».
    std::string note;

    std::vector<LobbyMember> members;
    std::vector<int> bans;     // id забаненных чемпионов, обе команды

    // Свой Riot ID из клиента League: окно профиля по умолчанию
    // показывает историю того, кто сидит за компьютером.
    std::string self_riot_id;
};

}  // namespace sintence

#endif  // SINTENCE_CORE_LOBBY_H
