# SPDX-License-Identifier: GPL-3.0-only
cmake_minimum_required(VERSION 3.21)
project(EggbertEditor VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 6.2 REQUIRED COMPONENTS Widgets)

add_library(eggbert_world STATIC src/world/world_file.cpp)
target_include_directories(eggbert_world PUBLIC src)

qt_add_executable(eggbert-editor
    src/app/main.cpp
    src/app/main_window.cpp
    src/app/main_window.hpp
)
target_link_libraries(eggbert-editor PRIVATE Qt6::Widgets eggbert_world)
set_target_properties(eggbert-editor PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_BUNDLE_NAME "Eggbert Editor"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
)

include(CTest)
if(BUILD_TESTING)
    add_executable(world-file-test tests/world_file_test.cpp)
    target_link_libraries(world-file-test PRIVATE eggbert_world)
    add_test(NAME world-file COMMAND world-file-test)
endif()
