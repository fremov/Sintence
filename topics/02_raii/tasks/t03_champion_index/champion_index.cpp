#include "champion_index.h"

namespace course {

void ChampionIndex::AddReport(ChampionReport /*report*/) {
    // TODO: найти отчёт с таким же именем чемпиона и заменить его,
    // либо добавить новый в конец.
    //
    // Отчёт пришёл по значению — он уже твой. Класть его в вектор копированием
    // после этого бессмысленно: подумай, что здесь должно стоять вместо копии.
}

const ChampionReport* ChampionIndex::Find(const std::string& /*champion_name*/) const {
    // TODO: линейный проход. Возвращать адрес хранимого отчёта, а не копию.
    return nullptr;
}

int ChampionIndex::Size() const {
    // TODO
    return -1;
}

bool ChampionIndex::Empty() const {
    // TODO
    return true;
}

std::vector<std::string> ChampionIndex::ChampionNames() const {
    // TODO
    return std::vector<std::string>();
}

}  // namespace course
