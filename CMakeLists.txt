# SPDX-License-Identifier: GPL-3.0-only
cmake_minimum_required(VERSION 3.21)
project(EggbertEditor VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 6.2 REQUIRED COMPONENTS Widgets)

option(EGGBERT_SANITIZE "Build with address and undefined-behavior sanitizers" OFF)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
    if(EGGBERT_SANITIZE)
        add_compile_options(-fsanitize=address,undefined -fno-sanitize-recover=undefined
                            -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address,undefined)
    endif()
endif()

add_library(eggbert_world STATIC
    src/world/layout.hpp
    src/world/le_reader.hpp
    src/world/world_document.cpp
    src/world/world_document.hpp
    src/world/world_file.cpp
    src/world/world_file.hpp
    src/world/world_probe.cpp
    src/world/world_probe.hpp
    src/world/world_view.cpp
    src/world/world_view.hpp
)
target_include_directories(eggbert_world PUBLIC src)

# Everything of the application except main(), so tests can link it.
qt_add_library(eggbert_app STATIC
    src/app/main_window.cpp
    src/app/main_window.hpp
    src/app/probe_report.cpp
    src/app/probe_report.hpp
)
target_include_directories(eggbert_app PUBLIC src)
target_link_libraries(eggbert_app PUBLIC Qt6::Widgets eggbert_world)

qt_add_executable(eggbert-editor src/app/main.cpp)
target_link_libraries(eggbert-editor PRIVATE eggbert_app)
set_target_properties(eggbert-editor PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_BUNDLE_NAME "Eggbert Editor"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
)

include(CTest)
if(BUILD_TESTING)
    # Headless core tests: no Qt, no game assets.
    foreach(name world_file layout le_reader world_probe world_view world_document)
        add_executable(${name}-test tests/${name}_test.cpp)
        target_link_libraries(${name}-test PRIVATE eggbert_world)
        add_test(NAME ${name} COMMAND ${name}-test)
    endforeach()

    # Application tests run without a display.
    foreach(name probe_report)
        add_executable(${name}-test tests/${name}_test.cpp)
        target_link_libraries(${name}-test PRIVATE eggbert_app)
        add_test(NAME ${name} COMMAND ${name}-test)
        set_tests_properties(${name} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    endforeach()
endif()
