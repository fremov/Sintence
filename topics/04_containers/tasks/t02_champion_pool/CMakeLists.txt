# Задача 4.2 — множества и словарь множеств.
# Callback к теме 1: нормализацию роли делает Champion, поэтому его .cpp
# собирается вместе с задачей. Сломается тема 1 — отвалится и эта.
set(TOPIC01_CHAMPION_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../01_classes/tasks/t01_champion")

course_add_task(topic04_t02_champion_pool
    champion_pool.cpp
    "${TOPIC01_CHAMPION_DIR}/champion.cpp"
    tests.cpp)

target_include_directories(topic04_t02_champion_pool PRIVATE
    "${TOPIC01_CHAMPION_DIR}")
