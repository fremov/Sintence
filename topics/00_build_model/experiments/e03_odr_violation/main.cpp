// Эксперимент 3: тело функции лежит в заголовке, заголовок включён
// в две единицы трансляции. Нарушено ODR — One Definition Rule.
//
// Собрать (обязательно ОБА файла, иначе ошибки не будет):
//   cl /nologo /std:c++17 /EHsc main.cpp report.cpp
//   g++ -std=c++17 main.cpp report.cpp -o main
//
// Ожидаемая ошибка: LNK2005 "ComputeKda" уже определён в report.obj
//                   / multiple definition of `ComputeKda(int, int, int)'.
//
// Это ошибка ЛИНКЕРА. Каждый файл по отдельности компилируется нормально —
// проблема видна только когда объектные файлы кладут рядом.
//
// Почини: см. комментарий в kda.h.
#include <iostream>

#include "report.h"

int main() {
  std::cout << BestKdaOfTwo(10, 5, 5, 3, 7, 12) << '\n';
  return 0;
}
