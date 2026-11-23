# SPDX-License-Identifier: GPL-3.0-only
# Installs the build into a scratch prefix and runs the installed program's
# self check, so packaging mistakes (missing files, wrong paths) are caught.
# Arguments: -DBUILD_DIR=... -DPREFIX=... -DPROGRAM=<the program, relative to
# the prefix> -DCONFIG=<configuration, for multi-configuration generators>
file(REMOVE_RECURSE "${PREFIX}")
set(install_config)
if(CONFIG)
    set(install_config --config "${CONFIG}")
endif()
execute_process(
    COMMAND "${CMAKE_COMMAND}" --install "${BUILD_DIR}" --prefix "${PREFIX}" ${install_config}
    RESULT_VARIABLE install_result
    OUTPUT_QUIET)
if(NOT install_result EQUAL 0)
    message(FATAL_ERROR "cmake --install failed: ${install_result}")
endif()
foreach(expected
        "${PROGRAM}"
        share/applications/eggbert-editor.desktop
        share/icons/hicolor/scalable/apps/eggbert-editor.svg
        share/doc/eggbert-editor/COPYING
        share/doc/eggbert-editor/NOTICE.md
        share/doc/eggbert-editor/user-guide.md
        share/doc/eggbert-editor/format-notes.md)
    if(NOT EXISTS "${PREFIX}/${expected}")
        message(FATAL_ERROR "installed tree lacks ${expected}")
    endif()
endforeach()
set(ENV{QT_QPA_PLATFORM} offscreen)
execute_process(
    COMMAND "${PREFIX}/${PROGRAM}" --smoke-test
    RESULT_VARIABLE smoke_result
    OUTPUT_VARIABLE smoke_output
    ERROR_VARIABLE smoke_error)
if(NOT smoke_result EQUAL 0)
    message(FATAL_ERROR "installed smoke test failed (${smoke_result}): ${smoke_output}${smoke_error}")
endif()
message(STATUS "${smoke_output}")
