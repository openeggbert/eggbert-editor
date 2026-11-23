# SPDX-License-Identifier: GPL-3.0-only
cmake_minimum_required(VERSION 3.21)
project(EggbertEditor VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 6.2 REQUIRED COMPONENTS Widgets)
if(BUILD_TESTING OR NOT DEFINED BUILD_TESTING)
    find_package(Qt6 6.2 REQUIRED COMPONENTS Test)
endif()

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
    src/world/adaptive.cpp
    src/world/adaptive.hpp
    src/world/animation.cpp
    src/world/animation.hpp
    src/world/atlas_table.cpp
    src/world/atlas_table.hpp
    src/world/doctor.cpp
    src/world/doctor.hpp
    src/world/hex_model.cpp
    src/world/hex_model.hpp
    src/world/layout.hpp
    src/world/le_reader.hpp
    src/world/metadata.cpp
    src/world/metadata.hpp
    src/world/editor_groups.cpp
    src/world/editor_groups.hpp
    src/world/object_animation_data.cpp
    src/world/object_animation_data.hpp
    src/world/object_catalog.cpp
    src/world/object_catalog.hpp
    src/world/object_ops.cpp
    src/world/object_ops.hpp
    src/world/offset_map.cpp
    src/world/offset_map.hpp
    src/world/profile.hpp
    src/world/revision_convert.cpp
    src/world/game_convert.cpp
    src/world/installation.cpp
    src/world/campaign.cpp
    src/world/stamp.cpp
    src/world/text_world.cpp
    src/world/merge.cpp
    src/world/campaign_doors.cpp
    src/world/reachability.cpp
    src/world/new_world.cpp
    src/world/encyclopedia.cpp
    src/world/custom_border.cpp
    src/world/revision_convert.hpp
    src/world/tile_tools.cpp
    src/world/tile_tools.hpp
    src/world/world_document.cpp
    src/world/world_document.hpp
    src/world/world_probe.cpp
    src/world/world_probe.hpp
    src/world/world_stats.cpp
    src/world/world_stats.hpp
    src/world/world_view.cpp
    src/world/world_view.hpp
)
target_include_directories(eggbert_world PUBLIC src)

# Everything of the application except main(), so tests can link it.
qt_add_library(eggbert_app STATIC
    src/app/appearance.cpp
    src/app/appearance.hpp
    src/app/diagnostics_panel.cpp
    src/app/diagnostics_panel.hpp
    src/app/cli.cpp
    src/app/cli.hpp
    src/app/document_file.cpp
    src/app/document_file.hpp
    src/app/document_tab.cpp
    src/app/document_tab.hpp
    src/app/game_assets.cpp
    src/app/game_assets.hpp
    src/app/hex_view.cpp
    src/app/hex_view.hpp
    src/app/inspector_panel.cpp
    src/app/inspector_panel.hpp
    src/app/main_window.cpp
    src/app/metadata_dialog.cpp
    src/app/metadata_dialog.hpp
    src/app/minimap_widget.cpp
    src/app/minimap_widget.hpp
    src/app/statistics_panel.cpp
    src/app/recovery_store.cpp
    src/app/deploy_store.cpp
    src/app/catalog_translator.cpp
    src/app/campaign_pack.cpp
    src/app/recovery_store.hpp
    src/app/smoke_test.cpp
    src/app/smoke_test.hpp
    src/app/sprite_icons.cpp
    src/app/tool_icons.cpp
    src/app/tool_icons.hpp
    src/app/sprite_icons.hpp
    src/app/statistics_panel.hpp
    src/app/main_window.hpp
    src/app/music_player.cpp
    src/app/music_player.hpp
    src/app/objects_panel.cpp
    src/app/objects_panel.hpp
    src/app/palette_panel.cpp
    src/app/installation_panel.cpp
    src/app/encyclopedia_dialog.cpp
    src/app/palette_panel.hpp
    src/app/png_export.cpp
    src/app/png_export.hpp
    src/app/png_export_dialog.cpp
    src/app/png_export_dialog.hpp
    src/app/probe_report.cpp
    src/app/probe_report.hpp
    src/app/view_transform.hpp
    src/app/world_canvas.cpp
    src/app/world_canvas.hpp
    src/app/world_renderer.cpp
    src/app/world_renderer.hpp
)
target_include_directories(eggbert_app PUBLIC src)
target_link_libraries(eggbert_app PUBLIC Qt6::Widgets eggbert_world)
target_compile_definitions(eggbert_app PUBLIC EGGBERT_EDITOR_VERSION="${PROJECT_VERSION}")
set_target_properties(eggbert_app PROPERTIES AUTOMOC ON)

# Documents shown by the Help menu, embedded so they are always available.
set(eggbert_documents docs/user-guide.md docs/format-notes.md NOTICE.md COPYING)
set_source_files_properties(docs/user-guide.md PROPERTIES QT_RESOURCE_ALIAS user-guide.md)
set_source_files_properties(docs/format-notes.md PROPERTIES QT_RESOURCE_ALIAS format-notes.md)
qt_add_resources(eggbert_app "documents" PREFIX "/docs" FILES ${eggbert_documents})
# Catalogues are read as `.ts` at run time: no Linguist tools are needed.
qt_add_resources(eggbert_app "translations" PREFIX "/translations"
    FILES translations/eggbert-editor_cs.ts)

qt_add_executable(eggbert-editor src/app/main.cpp)
target_link_libraries(eggbert-editor PRIVATE eggbert_app)
set_target_properties(eggbert-editor PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_BUNDLE_NAME "Eggbert Editor"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
)

# Installation: the program, desktop integration and documents. Qt comes from
# the system; nothing proprietary is installed.
include(GNUInstallDirs)
set(CMAKE_INSTALL_DOCDIR "${CMAKE_INSTALL_DATAROOTDIR}/doc/eggbert-editor")
install(TARGETS eggbert-editor
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    BUNDLE DESTINATION .)
install(FILES resources/eggbert-editor.desktop
    DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/applications")
install(FILES resources/eggbert-editor.svg
    DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/scalable/apps")
install(FILES COPYING NOTICE.md README.md docs/user-guide.md docs/format-notes.md
    DESTINATION "${CMAKE_INSTALL_DOCDIR}")

set(CPACK_PACKAGE_NAME "eggbert-editor")
set(CPACK_PACKAGE_VENDOR "Open Eggbert")
set(CPACK_PACKAGE_CONTACT "Open Eggbert <https://github.com/openeggbert/eggbert-editor>")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
    "Lossless editor for Speedy Blupi and Speedy Eggbert world files")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/COPYING")
set(CPACK_GENERATOR "TGZ;DEB")
set(CPACK_SOURCE_GENERATOR "TGZ")
set(CPACK_SOURCE_IGNORE_FILES "/build[^/]*/;/\\.git/;/CMakeUserPresets\\.json$")
set(CPACK_DEBIAN_PACKAGE_SECTION "games")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/openeggbert/eggbert-editor")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
# Listening to a game's music uses an installed MIDI player; none is required.
set(CPACK_DEBIAN_PACKAGE_SUGGESTS "fluidsynth | timidity | wildmidi")
find_program(EGGBERT_DPKG_SHLIBDEPS dpkg-shlibdeps)
if(EGGBERT_DPKG_SHLIBDEPS)
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
else()
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libqt6widgets6 (>= 6.2)")
endif()
include(CPack)

include(CTest)
if(BUILD_TESTING)
    # Headless core tests: no Qt, no game assets.
    foreach(name layout le_reader world_probe world_view world_document world_stats offset_map tile_tools metadata object_ops hex_model doctor adaptive animation atlas_table editor_groups revision_convert new_world encyclopedia expert_edit custom_border game_convert installation campaign campaign_doors reachability stamp text_world merge view_transform)
        add_executable(${name}-test tests/${name}_test.cpp)
        target_link_libraries(${name}-test PRIVATE eggbert_world)
        add_test(NAME ${name} COMMAND ${name}-test)
    endforeach()

    # Application tests run without a display.
    foreach(name probe_report document_file recovery_store deploy_store campaign_pack cli world_renderer png_export game_assets main_window docks tools metadata_ui objects_ui inspector doctor_ui adaptive_ui workspace artwork_ui palette music_player animation_ui interface_ui recovery_ui convert_ui new_world_ui encyclopedia_ui experimental_ui convert_game_ui installation_ui campaign_ui stamps_ui merge_ui translation)
        add_executable(${name}-test tests/${name}_test.cpp)
        target_link_libraries(${name}-test PRIVATE eggbert_app Qt6::Test)
        add_test(NAME ${name} COMMAND ${name}-test)
        set_tests_properties(${name} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    endforeach()

    # The built program's own self check, then the same from an installed tree.
    # The headless commands of the built program, without any display.
    add_test(NAME cli_usage COMMAND eggbert-editor info)
    set_tests_properties(cli_usage PROPERTIES WILL_FAIL TRUE TIMEOUT 60)
    add_test(NAME smoke COMMAND eggbert-editor --smoke-test)
    set_tests_properties(smoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    # Informational options must work without any display.
    add_test(NAME version COMMAND eggbert-editor --version)
    set_tests_properties(version PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=xcb;DISPLAY=;WAYLAND_DISPLAY="
        PASS_REGULAR_EXPRESSION "Eggbert Editor ${PROJECT_VERSION}")
    add_test(NAME install_smoke
        COMMAND "${CMAKE_COMMAND}" -DBUILD_DIR=${CMAKE_BINARY_DIR}
                -DPREFIX=${CMAKE_BINARY_DIR}/install-smoke -P
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/install_smoke.cmake")

    # A test that waits for input (for example a dialog nobody answers) must
    # fail instead of blocking the run.
    get_property(eggbert_tests DIRECTORY PROPERTY TESTS)
    set_tests_properties(${eggbert_tests} PROPERTIES TIMEOUT 120)

    # Opt-in check against real world files kept outside the repository.
    add_executable(sample_matrix-test tests/sample_matrix_test.cpp)
    target_link_libraries(sample_matrix-test PRIVATE eggbert_app Qt6::Test)
    add_test(NAME sample_matrix COMMAND sample_matrix-test)
    set_tests_properties(sample_matrix PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen" SKIP_RETURN_CODE 77 TIMEOUT 600)
endif()
