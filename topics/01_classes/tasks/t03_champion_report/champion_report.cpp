#include "champion_report.h"

namespace course {
namespace {

// Заглушка, чтобы файл собирался до того, как ты напишешь тела.
// Удали её, когда закончишь.
const std::string kStub = "<не реализовано>";

}  // namespace

ChampionReport::ChampionReport(std::string /*champion_name*/) {
    // TODO
}

void ChampionReport::Add(const MatchLine& /*line*/) {
    // TODO: проверка строки — отдельный вопрос от её добавления.
    // Если условие проверки не помещается в одну читаемую строку,
    // это намёк, что ему нужна своя функция с именем.
}

const std::string& ChampionReport::ChampionName() const {
    // TODO
    return kStub;
}

int ChampionReport::Games() const {
    // TODO
    return -1;
}

double ChampionReport::Kda() const {
    // TODO: сумму собирать по всему вектору, а не считать среднее средних.
    return -1.0;
}

double ChampionReport::Winrate() const {
    // TODO
    return -1.0;
}

double ChampionReport::CsPerMinute() const {
    // TODO
    return -1.0;
}

bool ChampionReport::HasEnoughData() const {
    // TODO
    return true;
}

}  // namespace course
