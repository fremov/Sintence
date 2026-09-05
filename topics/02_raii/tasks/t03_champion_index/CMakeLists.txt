# Задача 2.3 собирается вместе с ChampionReport из темы 1 — это и есть callback:
# код прошлой темы продолжает жить, а не остаётся лежать в своей папке.
set(TOPIC01_REPORT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../01_classes/tasks/t03_champion_report")

course_add_task(topic02_t03_champion_index
    champion_index.cpp
    "${TOPIC01_REPORT_DIR}/champion_report.cpp"
    tests.cpp)

target_include_directories(topic02_t03_champion_index PRIVATE "${TOPIC01_REPORT_DIR}")
