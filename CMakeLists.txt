cmake_minimum_required(VERSION 3.20)
project(Sintence LANGUAGES CXX)

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

option(SINTENCE_SANITIZERS "Собирать Debug с address/undefined sanitizer" ON)

# MSVC: /RTC1 несовместим с /fsanitize=address, а инкрементальная линковка
# ломает ASan. Убираем обоих из стандартных Debug-флагов.
if(MSVC AND SINTENCE_SANITIZERS)
    string(REPLACE "/RTC1" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# ---------------------------------------------------------------------------
# sintence_common — общие настройки для всех задач: doctest, предупреждения,
# санитайзеры. Каждая задача просто линкуется с этой INTERFACE-библиотекой.
# ---------------------------------------------------------------------------

add_library(sintence_common INTERFACE)
# SYSTEM: doctest — чужой заголовок, его предупреждения не наши.
target_include_directories(sintence_common SYSTEM INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/third_party")
target_compile_features(sintence_common INTERFACE cxx_std_23)

if(MSVC)
    target_compile_options(sintence_common INTERFACE /W4 /permissive- /utf-8 /EHsc /external:W0)
    if(SINTENCE_SANITIZERS)
        # Неопределённое поведение не падает — оно работает, пока не перестанет.
        target_compile_options(sintence_common INTERFACE $<$<CONFIG:Debug>:/fsanitize=address;/Zi;/Od>)
        target_link_options(sintence_common INTERFACE $<$<CONFIG:Debug>:/INCREMENTAL:NO>)
        # ASan в MSVC линкуется динамически: рядом с exe нужен clang_rt.asan*.dll,
        # иначе тест падает с 0xc0000135 (DLL not found) ещё до первой проверки.
        get_filename_component(SINTENCE_MSVC_BIN "${CMAKE_CXX_COMPILER}" DIRECTORY)
        file(GLOB SINTENCE_ASAN_DLLS "${SINTENCE_MSVC_BIN}/clang_rt.asan*dynamic-*.dll")
        if(NOT SINTENCE_ASAN_DLLS)
            message(WARNING "Не нашёл clang_rt.asan*.dll рядом с cl.exe — "
                            "перенастрой с -DCOURSE_SANITIZERS=OFF, если тесты не запускаются")
        endif()
    endif()
else()
    target_compile_options(sintence_common INTERFACE -Wall -Wextra -Wpedantic)
    if(SINTENCE_SANITIZERS)
        target_compile_options(sintence_common INTERFACE
            $<$<CONFIG:Debug>:-fsanitize=address,undefined;-fno-omit-frame-pointer;-g;-O0>)
        target_link_options(sintence_common INTERFACE
            $<$<CONFIG:Debug>:-fsanitize=address,undefined>)
    endif()
endif()

# ---------------------------------------------------------------------------
# В сборке ровно две мишени:
#   sintence        приложение, src/app/main.cpp
#   sintence_tests  весь набор тестов одним бинарником
# плюс sintence_lib — статическая библиотека с кодом src/, на которую
# они обе линкуются. Ничего больше здесь не собирается: разовая практика
# (practice_code) и demo темы 0 собираются руками, командами из их README.
#
# Тесты можно отключить: cmake -S . -B build -DSINTENCE_TESTS=OFF
# ---------------------------------------------------------------------------

option(SINTENCE_TESTS "Собирать sintence_tests" ON)

add_subdirectory(src)

if(SINTENCE_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
