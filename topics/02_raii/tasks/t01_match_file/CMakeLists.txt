course_add_task(topic02_t01_match_file match_file.cpp tests.cpp)

# Тесты открывают настоящие фикстуры анализатора: путь к ним подставляется
# на этапе сборки, чтобы тест не зависел от того, откуда его запустили.
target_compile_definitions(topic02_t01_match_file PRIVATE
    COURSE_FIXTURES_DIR="${CMAKE_SOURCE_DIR}/project/data/fixtures")

# MSVC считает std::fopen устаревшим и предлагает fopen_s, которого нет
# в стандарте. Здесь нужен именно портируемый вариант.
if(MSVC)
    target_compile_definitions(topic02_t01_match_file PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
