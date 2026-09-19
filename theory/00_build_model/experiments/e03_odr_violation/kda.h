#ifndef E03_KDA_H
#define E03_KDA_H

// Include guard здесь есть, и он не помогает.
// Guard защищает от повторного включения в ОДНУ единицу трансляции.
// Здесь единиц трансляции две (main.cpp и report.cpp), и каждая получит
// свою копию определения функции — а имя у них одно на всю программу.
//
// Почини одним из двух способов:
//   1) убрать тело отсюда, оставить объявление, тело унести в kda.cpp
//   2) написать inline перед double — это разрешает одинаковые определения
//      в разных единицах трансляции

inline double ComputeKda(int kills, int deaths, int assists) {
  const int safe_deaths = deaths == 0 ? 1 : deaths;
  return static_cast<double>(kills + assists) / safe_deaths;
}

#endif  // E03_KDA_H
