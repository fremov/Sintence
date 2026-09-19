# MatchEntry несёт MatchLine из темы 1, поэтому нужен её заголовок.
# Тел функций отсюда не требуется: MatchLine — структура без методов.
set(TOPIC01_REPORT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../01_classes/tasks/t03_champion_report")

course_add_task(topic03_t01_match_source match_source.cpp tests.cpp)

target_include_directories(topic03_t01_match_source PRIVATE "${TOPIC01_REPORT_DIR}")
