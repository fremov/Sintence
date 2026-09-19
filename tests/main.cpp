// Единственная точка входа для всех тестов.
//
// Раньше каждый tests/test_*.cpp собирался в отдельный исполняемый файл,
// и в солюшене было два десятка проектов. Теперь один бинарник
// analyzer_tests: собирается быстрее, запускается одной командой,
// а doctest всё равно показывает, в каком файле упал тест.
//
// Запустить только часть:
//   analyzer_tests.exe --test-case="*winrate*"
//   analyzer_tests.exe --source-file="*match_json*"
//   analyzer_tests.exe --list-test-cases

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
