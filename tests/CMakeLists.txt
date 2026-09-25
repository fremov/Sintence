# ---------------------------------------------------------------------------
# sintence_tests — один бинарник со всеми тестами.
# Новый tests/test_*.cpp подхватывается автоматически, править здесь нечего.
# ---------------------------------------------------------------------------

file(GLOB SINTENCE_TEST_SOURCES CONFIGURE_DEPENDS "test_*.cpp")

add_executable(sintence_tests main.cpp ${SINTENCE_TEST_SOURCES})
target_link_libraries(sintence_tests PRIVATE sintence_lib)
set_target_properties(sintence_tests PROPERTIES VS_GLOBAL_VcpkgEnabled false)

# Фикстуры и реальная выгрузка: тесты находят данные без относительных путей
# вида ../../.., которые ломаются от смены рабочего каталога.
target_compile_definitions(sintence_tests PRIVATE
    SINTENCE_FIXTURES_DIR="${CMAKE_SOURCE_DIR}/project/data/fixtures"
    SINTENCE_MATCHES_DIR="${CMAKE_SOURCE_DIR}/project/data/matches")

if(SINTENCE_ASAN_DLLS)
    add_custom_command(TARGET sintence_tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${SINTENCE_ASAN_DLLS} "$<TARGET_FILE_DIR:sintence_tests>"
        VERBATIM)
endif()

# Один тест в ctest — весь набор. Отдельные случаи запускаются флагами doctest:
#   sintence_tests.exe --source-file="*match_json*"
add_test(NAME sintence_tests COMMAND sintence_tests)

list(LENGTH SINTENCE_TEST_SOURCES SINTENCE_TEST_COUNT)
message(STATUS "Анализатор: тестовых файлов — ${SINTENCE_TEST_COUNT}")
