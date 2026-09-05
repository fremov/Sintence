# Песочница для практики: pr.cpp будет постоянно перезаписываться,
# поэтому здесь только обычный исполняемый файл — без регистрации в ctest.
add_executable(practice_code pr.cpp)
target_link_libraries(practice_code PRIVATE course_common)
set_target_properties(practice_code PROPERTIES FOLDER "practice")
set_target_properties(practice_code PROPERTIES VS_GLOBAL_VcpkgEnabled false)
if(COURSE_ASAN_DLLS)
    add_custom_command(TARGET practice_code POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${COURSE_ASAN_DLLS} "$<TARGET_FILE_DIR:practice_code>"
        VERBATIM)
endif()
