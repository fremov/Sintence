# ---------------------------------------------------------------------------
# analyzer_lib — весь код анализатора, кроме точки входа.
# Слои по project/ARCHITECTURE.md:
#   core/     доменные типы, ничего про файлы, JSON и сеть
#   data/     адаптеры источников
#   analysis/ метрики и агрегация над типами core/
# Приложение и тесты линкуются с этой библиотекой, а не пересобирают исходники.
# ---------------------------------------------------------------------------

file(GLOB ANALYZER_CORE_SOURCES CONFIGURE_DEPENDS "core/*.cpp")
file(GLOB ANALYZER_DATA_SOURCES CONFIGURE_DEPENDS "data/*.cpp")
file(GLOB ANALYZER_ANALYSIS_SOURCES CONFIGURE_DEPENDS "analysis/*.cpp")

add_library(analyzer_lib STATIC
    ${ANALYZER_CORE_SOURCES}
    ${ANALYZER_DATA_SOURCES}
    ${ANALYZER_ANALYSIS_SOURCES})

target_include_directories(analyzer_lib PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/core"
    "${CMAKE_CURRENT_SOURCE_DIR}/data"
    "${CMAKE_CURRENT_SOURCE_DIR}/analysis")

# MatchFile сознательно работает с fopen (тема 2 — RAII на сыром ресурсе).
# MSVC считает fopen устаревшим и требует fopen_s, которого нет в стандарте;
# это определение было в сборке задачи 2.1 и переехало сюда вместе с кодом.
if(MSVC)
    target_compile_definitions(analyzer_lib PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

target_link_libraries(analyzer_lib PUBLIC course_common)
set_target_properties(analyzer_lib PROPERTIES FOLDER "analyzer")
set_target_properties(analyzer_lib PROPERTIES VS_GLOBAL_VcpkgEnabled false)

# ---------------------------------------------------------------------------
# analyzer — само приложение.
# ---------------------------------------------------------------------------

add_executable(analyzer app/main.cpp)
target_link_libraries(analyzer PRIVATE analyzer_lib)
set_target_properties(analyzer PROPERTIES FOLDER "analyzer")
set_target_properties(analyzer PROPERTIES VS_GLOBAL_VcpkgEnabled false)

# Путь к данным по умолчанию — чтобы запускать без аргументов.
target_compile_definitions(analyzer PRIVATE
    ANALYZER_DEFAULT_DATA_DIR="${CMAKE_SOURCE_DIR}/project/data/matches")

if(COURSE_ASAN_DLLS)
    add_custom_command(TARGET analyzer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${COURSE_ASAN_DLLS} "$<TARGET_FILE_DIR:analyzer>"
        VERBATIM)
endif()
