#ifndef SINTENCE_DATA_FALLBACK_SOURCE_H
#define SINTENCE_DATA_FALLBACK_SOURCE_H

#include <string>
#include <vector>

#include "match_source.h"  // интерфейс и FixtureMatchSource

// data/fallback_source — источник, выбирающий между двумя другими.
//
// Хранит два источника по ссылке на базовый интерфейс и ни разу не упоминает
// конкретную реализацию: живой источник, если он доступен, иначе запасной.

namespace sintence {

// Источник с запасным вариантом: работает через основной, пока тот доступен,
// иначе переключается на резервный.
//
// Оба источника переданы снаружи и живут дольше этого объекта — владения нет,
// поэтому хранить их надо ссылкой или указателем на MatchSource, а не копией
// (интерфейс копировать запрещено).
class FallbackMatchSource : public MatchSource {
public:
    FallbackMatchSource(const MatchSource& primary, const MatchSource& backup);

    // Источник, который будет использован прямо сейчас:
    //   основной, если он доступен;
    //   иначе резервный, если доступен он;
    //   иначе nullptr — данных сейчас нет ниоткуда.
    //
    // Метод не виртуальный: он не часть интерфейса MatchSource, а деталь
    // именно этой реализации. Возвращает указатель на базу — вызывающий
    // по-прежнему не знает, какая реализация ему досталась.
    const MatchSource* Active() const;
    
    std::string Name() const override;
    bool IsAvailable() const override;
    std::vector<MatchEntry> LoadMatches() const override;

private:
    const MatchSource* primary_ = nullptr;
    const MatchSource* backup_ = nullptr;
};

}  // namespace sintence

#endif  // SINTENCE_DATA_FALLBACK_SOURCE_H
