// Заголовок БЕЗ include guard. Так писать нельзя — в этом и эксперимент.
//
// Почини одним из двух способов:
//   1) #ifndef KDA_H / #define KDA_H ... #endif   — работает везде
//   2) #pragma once                               — короче, поддерживают все
//                                                   актуальные компиляторы
#pragma once

struct Kda {
  int kills;
  int deaths;
  int assists;
};
