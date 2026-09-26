#ifndef SINTENCE_CORE_PLAYER_PROFILE_H
#define SINTENCE_CORE_PLAYER_PROFILE_H

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// core/player_profile — то, что известно про игрока ВНЕ текущего матча.
//
// LivePlayer отвечает на вопрос «что он делает прямо сейчас» (KDA, уровень,
// жив ли). PlayerProfile отвечает на другой вопрос — «кто это вообще такой»:
// ранг, сколько игр за сезон, на ком играет. Данные приходят из Riot API
// по ключу, а не из локального клиента, и живут дольше одного матча.
//
// Слой core: ни HTTP, ни JSON, ни nlohmann. Только доменные типы.

namespace sintence {

// Ранговая запись в одной очереди. Riot отдаёт по записи на каждую очередь,
// в которой игрок сыграл хотя бы одну калибровку.
//
// Почему нет поля winrate: это производное значение, а не данные. Метрики
// живут в analysis/ и считаются из wins и losses, иначе одно и то же число
// начнёт храниться в двух местах и разъедется.
struct RankedStats {
    std::string queue;     // "RANKED_SOLO_5x5", "RANKED_FLEX_SR"
    std::string tier;      // "EMERALD", "CHALLENGER"
    std::string division;  // "II"; у Master и выше приходит "I"
    int league_points = 0;
    int wins = 0;
    int losses = 0;
};

// Мастери на одном чемпионе.
//
// champion_id, а не имя: champion-mastery-v4 имён не отдаёт вовсе, только
// числовой id. Имя и картинка берутся из Data Dragon — это статика без ключа
// и без лимитов, и тянет её фронтенд, а не C++. Граница слоёв от этого
// только выигрывает: в data/ не появляется ещё один справочник.
struct ChampionMastery {
    int champion_id = 0;
    int level = 0;
    long long points = 0;

    // Unix-время в МИЛЛИСЕКУНДАХ (Riot отдаёт именно так, не в секундах).
    // Хранится как есть, без перевода в календарную дату: перевод зависит
    // от часового пояса, а это забота интерфейса, не домена.
    long long last_play_time_ms = 0;
};

// Профиль одного участника лобби.
//
// solo_queue пустой — это НЕ ошибка: в соло-очереди может не быть ни одной
// игры за сезон, и такой игрок существует. Пустой optional означает
// «не играл», а не «не смогли узнать».
struct PlayerProfile {
    std::string riot_id;  // "Riot Tuxedo#TXC1" — как отдаёт Live Client
    std::string puuid;
    std::optional<RankedStats> solo_queue;

    // До трёх записей, по убыванию очков. Порядок задаёт Riot,
    // и его надо сохранять, а не пересортировывать.
    std::vector<ChampionMastery> top_masteries;
};

}  // namespace sintence

#endif  // SINTENCE_CORE_PLAYER_PROFILE_H
