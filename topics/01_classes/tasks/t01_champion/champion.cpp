#include "champion.h"

namespace course {
namespace {

// Заглушка, чтобы файл собирался до того, как ты напишешь тела.
// Удали её, когда закончишь: в готовом коде её быть не должно.
const std::string kStub = "<не реализовано>";

}  // namespace

Champion::Champion(std::string /*name*/, std::string /*role*/) {
    // TODO: инициализируй name_ и role_ через список инициализации,
    // а не присваиванием в теле. Роль перед этим надо нормализовать —
    // подумай, где должно жить приведение к верхнему регистру, чтобы
    // конструктор остался в одну строку.
}

const std::string& Champion::Name() const {
    // TODO
    return kStub;
}

const std::string& Champion::Role() const {
    // TODO
    return kStub;
}

std::string Champion::DisplayName() const {
    // TODO
    return kStub;
}

bool Champion::HasRole(const std::string& /*role*/) const {
    // TODO
    return false;
}

}  // namespace course
