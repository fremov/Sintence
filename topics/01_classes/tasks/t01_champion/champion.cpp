#include "champion.h"

namespace course {
namespace {

// Заглушка, чтобы файл собирался до того, как ты напишешь тела.
// Удали её, когда закончишь: в готовом коде её быть не должно.
    std::string CheckName(std::string name) {
      if (name.empty()) return "Unknown";

      return name;
    }

    std::string CheckRole(std::string role) {
      if (role.empty()) return "UNKNOWN";
        for (auto& index : role) {
          index = static_cast<char>(std::toupper(index));
        }
      return role;
    }
}  // namespace

Champion::Champion(std::string name, std::string role) : name_(CheckName(name)), role_(CheckRole(role)) {
    // TODO: инициализируй name_ и role_ через список инициализации,
    // а не присваиванием в теле. Роль перед этим надо нормализовать —
    // подумай, где должно жить приведение к верхнему регистру, чтобы
    // конструктор остался в одну строку.

}

const std::string& Champion::Name() const {
    // TODO
    return name_;
}

const std::string& Champion::Role() const {
    // TODO
    return role_;
}

std::string Champion::DisplayName() const {
    // TODO
    return name_ + " (" + role_ + ")";
}

bool Champion::HasRole(const std::string& role) const {
    // TODO
    if (role.empty()) return false;

    if (CheckRole(role) == role_) return true;

    return false;
}

}  // namespace course
