cmake_minimum_required(VERSION 3.20)
project(cpp_mentor_course LANGUAGES CXX)

# ---------------------------------------------------------------------------
# Базовые настройки курса.
# Этот файл трогать не нужно: новые задачи подхватываются автоматически.
# ---------------------------------------------------------------------------

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Стандарт — C++23. У MSVC он включается флагом /std:c++latest: /std:c++23 cl
# не понимает и молча игнорирует (D9002), поэтому стандарт задаётся только
# через CMake, а не руками в командной строке.

option(COURSE_SANITIZERS "Собирать Debug с address/undefined sanitizer" ON)

# MSVC: /RTC1 несовместим с /fsanitize=address, а инкрементальная линковка
# ломает ASan. Убираем обоих из стандартных Debug-флагов.
if(MSVC AND COURSE_SANITIZERS)
    string(REPLACE "/RTC1" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# ---------------------------------------------------------------------------
# course_common — общие настройки для всех задач: doctest, предупреждения,
# санитайзеры. Каждая задача просто линкуется с этой INTERFACE-библиотекой.
# ---------------------------------------------------------------------------

add_library(course_common INTERFACE)
# SYSTEM: doctest — чужой заголовок, его предупреждения не наши.
target_include_directories(course_common SYSTEM INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/third_party")
target_compile_features(course_common INTERFACE cxx_std_23)

if(MSVC)
    target_compile_options(course_common INTERFACE /W4 /permissive- /utf-8 /EHsc /external:W0)
    if(COURSE_SANITIZERS)
        # Неопределённое поведение не падает — оно работает, пока не перестанет.
        target_compile_options(course_common INTERFACE $<$<CONFIG:Debug>:/fsanitize=address;/Zi;/Od>)
        target_link_options(course_common INTERFACE $<$<CONFIG:Debug>:/INCREMENTAL:NO>)
        # ASan в MSVC линкуется динамически: рядом с exe нужен clang_rt.asan*.dll,
        # иначе тест падает с 0xc0000135 (DLL not found) ещё до первой проверки.
        get_filename_component(COURSE_MSVC_BIN "${CMAKE_CXX_COMPILER}" DIRECTORY)
        file(GLOB COURSE_ASAN_DLLS "${COURSE_MSVC_BIN}/clang_rt.asan*dynamic-*.dll")
        if(NOT COURSE_ASAN_DLLS)
            message(WARNING "Не нашёл clang_rt.asan*.dll рядом с cl.exe — "
                            "перенастрой с -DCOURSE_SANITIZERS=OFF, если тесты не запускаются")
        endif()
    endif()
else()
    target_compile_options(course_common INTERFACE -Wall -Wextra -Wpedantic)
    if(COURSE_SANITIZERS)
        target_compile_options(course_common INTERFACE
            $<$<CONFIG:Debug>:-fsanitize=address,undefined;-fno-omit-frame-pointer;-g;-O0>)
        target_link_options(course_common INTERFACE
            $<$<CONFIG:Debug>:-fsanitize=address,undefined>)
    endif()
endif()

# ---------------------------------------------------------------------------
# course_add_task(<имя> <исходники...>) — один исполняемый файл с тестами
# плюс регистрация в ctest.
# ---------------------------------------------------------------------------

enable_testing()

function(course_add_task target)
    add_executable(${target} ${ARGN})
    target_link_libraries(${target} PRIVATE course_common)
    set_target_properties(${target} PROPERTIES FOLDER "course")
    # Глобальная интеграция vcpkg подмешивается в любой проект MSBuild на машине
    # и тянет свои include/lib. Курсу это не нужно: он самодостаточный.
    set_target_properties(${target} PROPERTIES VS_GLOBAL_VcpkgEnabled false)
    if(COURSE_ASAN_DLLS)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    ${COURSE_ASAN_DLLS} "$<TARGET_FILE_DIR:${target}>"
            VERBATIM)
    endif()
    add_test(NAME ${target} COMMAND ${target})
endfunction()

# ---------------------------------------------------------------------------
# Автообнаружение задач. Новая папка с CMakeLists.txt внутри
# topics/<тема>/tasks/<задача>/ появляется в сборке сама, без правок здесь.
# Отдельно подхватываются эталонные разборы topics/<тема>/demo/.
# ---------------------------------------------------------------------------

# practice_code — папка для разовой практики (файл там постоянно
# перезаписывается), поэтому подключаем её явно, а не автообнаружением.
add_subdirectory(practice_code)

# playground — одна программа из всех реализованных задач: видно, как классы
# работают вместе на настоящих данных. Не тест, в ctest не регистрируется.
add_subdirectory(playground)

file(GLOB_RECURSE COURSE_TASK_LISTS CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/topics/*/tasks/*/CMakeLists.txt")
file(GLOB COURSE_DEMO_LISTS CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/topics/*/demo/CMakeLists.txt")

foreach(list_file IN LISTS COURSE_DEMO_LISTS COURSE_TASK_LISTS)
    get_filename_component(task_dir "${list_file}" DIRECTORY)
    file(RELATIVE_PATH task_rel "${CMAKE_CURRENT_SOURCE_DIR}" "${task_dir}")
    add_subdirectory("${task_rel}")
endforeach()

list(LENGTH COURSE_TASK_LISTS COURSE_TASK_COUNT)
message(STATUS "Курс: найдено задач — ${COURSE_TASK_COUNT}")
