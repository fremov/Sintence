# Задача 3.3 — точка, где сходятся три темы: ChampionReport (тема 1),
# ChampionIndex (тема 2) и MatchSource (задача 3.1). Если сломается любая
# из них, отвалится и эта сборка — так и должно быть.
set(TOPIC01_REPORT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../01_classes/tasks/t03_champion_report")
set(TOPIC02_INDEX_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../02_raii/tasks/t03_champion_index")
set(TOPIC03_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../t01_match_source")

course_add_task(topic03_t03_index_builder
    index_builder.cpp
    "${TOPIC03_SOURCE_DIR}/match_source.cpp"
    "${TOPIC02_INDEX_DIR}/champion_index.cpp"
    "${TOPIC01_REPORT_DIR}/champion_report.cpp"
    tests.cpp)

target_include_directories(topic03_t03_index_builder PRIVATE
    "${TOPIC03_SOURCE_DIR}"
    "${TOPIC02_INDEX_DIR}"
    "${TOPIC01_REPORT_DIR}")
