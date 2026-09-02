// Эксперимент 1: объявление есть, определения нет.
//
// Собрать:
//   cl /nologo /std:c++17 /EHsc main.cpp        (Developer PowerShell)
//   g++ -std=c++17 main.cpp -o main             (Linux/macOS)
//
// Ожидаемая ошибка: LNK2019 / undefined reference to `ComputeKda(int, int,
// int)`.
//
// Обрати внимание: компилятор НЕ ругается. Он видит объявление и верит,
// что функция где-то есть. Ошибку находит линкер, когда собирает
// исполняемый файл и не может подставить адрес.
//
// Почини: добавь определение ComputeKda прямо в этот файл и пересобери.

#include <iostream>

// Объявление: обещание, что такая функция существует.
double ComputeKda(int kills, int deaths, int assists);

double ComputeKda(int kills, int deaths, int assists) {
  return kills + assists + deaths;
}

int main() {
  std::cout << ComputeKda(10, 5, 5) << '\n';
  return 0;
}
