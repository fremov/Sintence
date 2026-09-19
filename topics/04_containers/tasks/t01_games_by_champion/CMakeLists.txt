# Задача 4.1 — таблица «чемпион → игры» на std::map.
# Тянет MatchEntry из темы 3: тип записи не меняется от смены контейнера.
set(TOPIC01_REPORT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../01_classes/tasks/t03_champion_report")
set(TOPIC03_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../03_polymorphism/tasks/t01_match_source")

course_add_task(topic04_t01_games_by_champion
    games_by_champion.cpp
    "${TOPIC03_SOURCE_DIR}/match_source.cpp"
    tests.cpp)

target_include_directories(topic04_t01_games_by_champion PRIVATE
    "${TOPIC03_SOURCE_DIR}"
    "${TOPIC01_REPORT_DIR}")
