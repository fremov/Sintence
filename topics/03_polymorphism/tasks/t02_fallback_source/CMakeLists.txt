# Задача 3.2 собирается вместе с интерфейсом и FixtureMatchSource из задачи 3.1:
# если там что-то сломается, отвалится и она.
set(TOPIC01_REPORT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../01_classes/tasks/t03_champion_report")
set(TOPIC03_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../t01_match_source")

course_add_task(topic03_t02_fallback_source
    fallback_source.cpp
    "${TOPIC03_SOURCE_DIR}/match_source.cpp"
    tests.cpp)

target_include_directories(topic03_t02_fallback_source PRIVATE
    "${TOPIC03_SOURCE_DIR}"
    "${TOPIC01_REPORT_DIR}")
