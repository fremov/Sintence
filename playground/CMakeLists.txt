# Playground — одна программа, собранная из всех реализованных задач.
# Не тест: в ctest не регистрируется, на закрытие тем не влияет.
# По мере прохождения тем сюда добавляются новые .cpp и новые секции в main.cpp.

set(T01_CHAMPION_DIR "${CMAKE_SOURCE_DIR}/topics/01_classes/tasks/t01_champion")
set(T01_STATS_DIR "${CMAKE_SOURCE_DIR}/topics/01_classes/tasks/t02_player_stats")
set(T01_REPORT_DIR "${CMAKE_SOURCE_DIR}/topics/01_classes/tasks/t03_champion_report")
set(T02_FILE_DIR "${CMAKE_SOURCE_DIR}/topics/02_raii/tasks/t01_match_file")
set(T02_PAYLOAD_DIR "${CMAKE_SOURCE_DIR}/topics/02_raii/tasks/t02_match_payload")
set(T02_INDEX_DIR "${CMAKE_SOURCE_DIR}/topics/02_raii/tasks/t03_champion_index")
set(T03_SOURCE_DIR "${CMAKE_SOURCE_DIR}/topics/03_polymorphism/tasks/t01_match_source")
set(T03_FALLBACK_DIR "${CMAKE_SOURCE_DIR}/topics/03_polymorphism/tasks/t02_fallback_source")
set(T03_BUILDER_DIR "${CMAKE_SOURCE_DIR}/topics/03_polymorphism/tasks/t03_index_builder")

add_executable(playground
    main.cpp
    "${T01_CHAMPION_DIR}/champion.cpp"
    "${T01_STATS_DIR}/player_stats.cpp"
    "${T01_REPORT_DIR}/champion_report.cpp"
    "${T02_FILE_DIR}/match_file.cpp"
    "${T02_PAYLOAD_DIR}/match_payload.cpp"
    "${T02_INDEX_DIR}/champion_index.cpp"
    "${T03_SOURCE_DIR}/match_source.cpp"
    "${T03_FALLBACK_DIR}/fallback_source.cpp"
    "${T03_BUILDER_DIR}/index_builder.cpp")

target_include_directories(playground PRIVATE
    "${T01_CHAMPION_DIR}"
    "${T01_STATS_DIR}"
    "${T01_REPORT_DIR}"
    "${T02_FILE_DIR}"
    "${T02_PAYLOAD_DIR}"
    "${T02_INDEX_DIR}"
    "${T03_SOURCE_DIR}"
    "${T03_FALLBACK_DIR}"
    "${T03_BUILDER_DIR}")

target_link_libraries(playground PRIVATE course_common)
set_target_properties(playground PROPERTIES FOLDER "playground")
set_target_properties(playground PROPERTIES VS_GLOBAL_VcpkgEnabled false)

target_compile_definitions(playground PRIVATE
    COURSE_FIXTURES_DIR="${CMAKE_SOURCE_DIR}/project/data/fixtures")

if(MSVC)
    target_compile_definitions(playground PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

if(COURSE_ASAN_DLLS)
    add_custom_command(TARGET playground POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${COURSE_ASAN_DLLS} "$<TARGET_FILE_DIR:playground>"
        VERBATIM)
endif()
