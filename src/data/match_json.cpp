#include "match_json.h"

#include "json.hpp"  // nlohmann/json, вендорный заголовок из third_party/

namespace course {

std::optional<MatchEntry> ParseMatchEntry(std::string_view json_text, std::string_view puuid) {
    // TODO: достать из матча строку игрока с этим puuid.
    //
    // ПОТОК ДАННЫХ
    //   вход:   текст одного файла матча и puuid игрока
    //   выход:  MatchEntry (имя чемпиона + MatchLine) либо std::nullopt
    //
    // API nlohmann, которого хватит на всю задачу:
    //   nlohmann::json::parse(text, nullptr, false)
    //       третий аргумент false = «не бросать исключение на битом JSON»;
    //       вместо этого вернётся объект, у которого is_discarded() == true
    //   doc.is_discarded()              JSON не разобрался
    //   doc.contains("info")            есть ли ключ
    //   doc["info"]["participants"]     вложенный доступ
    //   for (const auto& p : array)     обход массива
    //   p.value("championName", std::string{})
    //       достать поле со значением по умолчанию, если его нет —
    //       именно то, что нужно, чтобы не падать на битых данных
    //   p["kills"].is_number_integer()  проверка типа перед чтением
    //   p["kills"].get<int>()           собственно чтение
    //
    // ПОРЯДОК ДЕЙСТВИЙ
    //   1. Разобрать текст. Не разобрался (is_discarded) — nullopt.
    //   2. Дойти до info.participants, проверяя по дороге, что ключи есть
    //      и что participants — массив. Отсутствие любого — nullopt.
    //   3. Найти участника, у которого поле puuid совпадает с аргументом.
    //      Не нашёлся — nullopt, и это нормальный случай, а не ошибка:
    //      в каталоге могут лежать чужие матчи.
    //   4. Заполнить MatchLine. Каждое числовое поле проверять на тип ПЕРЕД
    //      чтением: в fixture_malformed.json у участника deaths равно null,
    //      а gameDuration отсутствует вовсе. get<int>() на таком поле бросит
    //      исключение — тест на этот файл и проверяет, что ты этого не сделал.
    //   5. minions — это СУММА totalMinionsKilled и neutralMinionsKilled:
    //      лесные монстры в CS считаются, и без них у любого леснка
    //      получится CS/min втрое меньше настоящего.
    //   6. duration_seconds — из info.gameDuration, не из участника.
    //   7. champion_name — championName как есть, регистр не трогаем
    //      (за нормализацию отвечает Champion из core/, а не адаптер).
    //
    // Почему nullopt, а не исключение: битый файл в выгрузке из 193 матчей —
    // рабочая ситуация, а не сбой программы. Правильную работу с ошибками
    // (std::expected с причиной) разберём отдельно, когда станет ясно,
    // что «почему пропустили» нужно показывать пользователю.
    (void)json_text;
    (void)puuid;
    return std::nullopt;
}

}  // namespace course
