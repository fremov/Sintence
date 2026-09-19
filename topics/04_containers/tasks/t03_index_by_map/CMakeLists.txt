# Задача 4.3 — тот же индекс на std::map.
# Собирается вместе с задачей 3.3: тест сравнивает две реализации между собой,
# поэтому сломанная 3.3 уронит и эту сборку.
set(TOPIC01_REPORT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../01_classes/tasks/t03_champion_report")
set(TOPIC02_INDEX_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../02_raii/tasks/t03_champion_index")
set(TOPIC03_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../03_polymorphism/tasks/t01_match_source")
set(TOPIC03_BUILDER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../03_polymorphism/tasks/t03_index_builder")

course_add_task(topic04_t03_index_by_map
    index_by_map.cpp
    "${TOPIC03_BUILDER_DIR}/index_builder.cpp"
    "${TOPIC03_SOURCE_DIR}/match_source.cpp"
    "${TOPIC02_INDEX_DIR}/champion_index.cpp"
    "${TOPIC01_REPORT_DIR}/champion_report.cpp"
    tests.cpp)

target_include_directories(topic04_t03_index_by_map PRIVATE
    "${TOPIC03_BUILDER_DIR}"
    "${TOPIC03_SOURCE_DIR}"
    "${TOPIC02_INDEX_DIR}"
    "${TOPIC01_REPORT_DIR}")
