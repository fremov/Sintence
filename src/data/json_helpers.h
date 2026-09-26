#ifndef SINTENCE_DATA_JSON_HELPERS_H
#define SINTENCE_DATA_JSON_HELPERS_H

#include <optional>
#include <string>
#include <string_view>

#include "json.hpp"

// data/json_helpers — безопасное чтение полей JSON для разборщиков.
//
// ВАЖНО про границы слоёв: этот заголовок СОДЕРЖИТ nlohmann::json и включать
// его имеют право только data/*.cpp. Правило проекта запрещает чужие типы
// в core/ и analysis/ и в публичных заголовках адаптеров; здесь внутренняя
// кухня слоя data/. Если файл окажется включён из core/ или analysis/ —
// граница сломана.
//
// Все функции возвращают std::nullopt, если поля нет ИЛИ тип не тот:
// для вызывающего это одинаково означает «значения нет».

namespace sintence {

// Целое число. "kills": 4 -> 4; "kills": 4.5 или "kills": null -> nullopt.
std::optional<int> GetInt(const nlohmann::json& obj, std::string_view key);

// Любое число, целое или дробное, всегда как double.
// "gameTime": 1234.56 -> 1234.56; "gameTime": 60 -> 60.0 (целое тоже число).
// Нужен для полей, где Riot отдаёт дробь: время матча, respawnTimer, wardScore.
std::optional<double> GetDouble(const nlohmann::json& obj, std::string_view key);

// Большое целое. Нужно там, где int переполняется молча: championPoints
// и lastPlayTime у Riot имеют порядок 1e6 и 1.7e12 соответственно.
std::optional<long long> GetInt64(const nlohmann::json& obj, std::string_view key);

// Строка. "championName": "Annie" -> "Annie"; число или null -> nullopt.
std::optional<std::string> GetString(const nlohmann::json& obj, std::string_view key);

// Булево. "isDead": true -> true; "isDead": 1 -> nullopt (единица не bool).
std::optional<bool> GetBoolean(const nlohmann::json& obj, std::string_view key);

}  // namespace sintence

#endif  // SINTENCE_DATA_JSON_HELPERS_H
