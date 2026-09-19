# ---------------------------------------------------------------------------
# analyzer_tests — один бинарник со всеми тестами.
# Новый tests/test_*.cpp подхватывается автоматически, править здесь нечего.
# ---------------------------------------------------------------------------

file(GLOB ANALYZER_TEST_SOURCES CONFIGURE_DEPENDS "test_*.cpp")

add_executable(analyzer_tests main.cpp ${ANALYZER_TEST_SOURCES})
target_link_libraries(analyzer_tests PRIVATE analyzer_lib)
set_target_properties(analyzer_tests PROPERTIES VS_GLOBAL_VcpkgEnabled false)

# Фикстуры и реальная выгрузка: тесты находят данные без относительных путей
# вида ../../.., которые ломаются от смены рабочего каталога.
target_compile_definitions(analyzer_tests PRIVATE
    COURSE_FIXTURES_DIR="${CMAKE_SOURCE_DIR}/project/data/fixtures"
    COURSE_MATCHES_DIR="${CMAKE_SOURCE_DIR}/project/data/matches")

if(COURSE_ASAN_DLLS)
    add_custom_command(TARGET analyzer_tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${COURSE_ASAN_DLLS} "$<TARGET_FILE_DIR:analyzer_tests>"
        VERBATIM)
endif()

# Один тест в ctest — весь набор. Отдельные случаи запускаются флагами doctest:
#   analyzer_tests.exe --source-file="*match_json*"
add_test(NAME analyzer_tests COMMAND analyzer_tests)

list(LENGTH ANALYZER_TEST_SOURCES ANALYZER_TEST_COUNT)
message(STATUS "Анализатор: тестовых файлов — ${ANALYZER_TEST_COUNT}")
