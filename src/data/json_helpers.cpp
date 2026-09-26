#include "json_helpers.h"

namespace sintence {

std::optional<int> GetInt(const nlohmann::json& obj, std::string_view key) {
    if (!obj.contains(key) || !obj[key].is_number_integer()) {
        return std::nullopt;
    }
    return obj[key].get<int>();
}

std::optional<double> GetDouble(const nlohmann::json& obj,
                                std::string_view key) {
    if (!obj.contains(key) || !obj[key].is_number()) {
        return std::nullopt;
    }
    return obj[key].get<double>();
}

std::optional<long long> GetInt64(const nlohmann::json& obj,
                                  std::string_view key) {
    if (!obj.contains(key) || !obj[key].is_number_integer()) {
        return std::nullopt;
    }
    return obj[key].get<long long>();
}

std::optional<std::string> GetString(const nlohmann::json& obj,
                                     std::string_view key) {
    if (!obj.contains(key) || !obj[key].is_string()) {
        return std::nullopt;
    }
    return obj[key].get<std::string>();
}

std::optional<bool> GetBoolean(const nlohmann::json& obj,
                               std::string_view key) {
    if (!obj.contains(key) || !obj[key].is_boolean()) {
        return std::nullopt;
    }
    return obj[key].get<bool>();
}

}  // namespace sintence
