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
# В сборке ровно две мишени:
#   analyzer        приложение, src/app/main.cpp
#   analyzer_tests  весь набор тестов одним бинарником
# плюс analyzer_lib — статическая библиотека с кодом src/, на которую
# они обе линкуются. Ничего больше здесь не собирается: разовая практика
# (practice_code) и demo темы 0 собираются руками, командами из их README.
#
# Тесты можно отключить: cmake -S . -B build -DANALYZER_TESTS=OFF
# ---------------------------------------------------------------------------

option(ANALYZER_TESTS "Собирать analyzer_tests" ON)

add_subdirectory(src)

if(ANALYZER_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ---------------------------------------------------------------------------
# Теория. Исходник — theory/*/THEORY.md, читаемая версия — theory/index.html,
# собирается скриптом на каждой сборке (0.1 секунды), поэтому не устаревает.
# Нет Python — просто не собирается, Markdown никуда не девается.
# ---------------------------------------------------------------------------

find_package(Python3 COMPONENTS Interpreter QUIET)

if(Python3_Interpreter_FOUND)
    file(GLOB_RECURSE COURSE_THEORY_SOURCES CONFIGURE_DEPENDS
         "${CMAKE_CURRENT_SOURCE_DIR}/theory/*/THEORY.md")

    add_custom_command(
        OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/theory/index.html"
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/scripts/build_theory.py"
        DEPENDS ${COURSE_THEORY_SOURCES}
                "${CMAKE_CURRENT_SOURCE_DIR}/REVIEW.md"
                "${CMAKE_CURRENT_SOURCE_DIR}/scripts/build_theory.py"
        COMMENT "Собираю теорию в theory/index.html"
        VERBATIM)

    add_custom_target(theory ALL DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/theory/index.html")
    set_target_properties(theory PROPERTIES VS_GLOBAL_VcpkgEnabled false)
else()
    message(STATUS "Python не найден — theory/index.html собирать нечем; "
                   "теория остаётся в Markdown")
endif()
