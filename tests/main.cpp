// Единственная точка входа для всех тестов.
//
// Раньше каждый tests/test_*.cpp собирался в отдельный исполняемый файл,
// и в солюшене было два десятка проектов. Теперь один бинарник
// sintence_tests: собирается быстрее, запускается одной командой,
// а doctest всё равно показывает, в каком файле упал тест.
//
// Запустить только часть:
//   sintence_tests.exe --test-case="*winrate*"
//   sintence_tests.exe --source-file="*match_json*"
//   sintence_tests.exe --list-test-cases

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
