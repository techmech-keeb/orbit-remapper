cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# NimBLE's security manager, on "key missing" from a bonded device, starts
# a new pairing by itself (ble_sm.c, BLE_RESTART_PAIR) without ever
# posting the failure to ble.c. That let a device that had dropped its
# bond pair again outside pairing mode (1fd6c6a report). The option has no
# Kconfig entry, only an #ifndef default of 1 in esp_nimble_cfg.h. With it
# off, the failure reaches ble.c, which re-pairs only in pairing mode.
idf_build_set_property(COMPILE_DEFINITIONS "MYNEWT_VAL_BLE_RESTART_PAIR=0" APPEND)

# Version. version.txt holds the product version (X.Y.Z, optionally
# -rc.N). The release workflow builds with ORBIT_RELEASE=1, which shows it as
# v<version> on the screen and in the logs; every other build shows the
# commit, so a test build is never mistaken for a release. The USB
# bcdDevice carries the version in BCD the way QMK's device_version does
# (2.0.1 -> 0x0201), which caps minor and patch at 9.
file(STRINGS "${CMAKE_CURRENT_LIST_DIR}/version.txt" ORBIT_VERSION LIMIT_COUNT 1)
if(NOT ORBIT_VERSION MATCHES "^([0-9]?[0-9])\\.([0-9])\\.([0-9])(-[0-9A-Za-z.-]+)?$")
    message(FATAL_ERROR "version.txt: \"${ORBIT_VERSION}\" is not X.Y.Z[-pre] with X <= 99 and Y, Z <= 9")
endif()
math(EXPR ORBIT_VERSION_BCD "((${CMAKE_MATCH_1} / 10) << 12) | ((${CMAKE_MATCH_1} % 10) << 8) | (${CMAKE_MATCH_2} << 4) | ${CMAKE_MATCH_3}"
     OUTPUT_FORMAT HEXADECIMAL)
if("$ENV{ORBIT_RELEASE}" STREQUAL "1")
    set(PROJECT_VER "v${ORBIT_VERSION}")
else()
    # As ESP-IDF's default (git describe), but never a tag: the commit only.
    execute_process(COMMAND git describe --always --dirty --abbrev=7 --match=no-tag-wanted
                    WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}"
                    OUTPUT_VARIABLE PROJECT_VER OUTPUT_STRIP_TRAILING_WHITESPACE
                    RESULT_VARIABLE git_result ERROR_QUIET)
    if(NOT git_result EQUAL 0)
        set(PROJECT_VER "${ORBIT_VERSION}-nogit")
    endif()
endif()
idf_build_set_property(COMPILE_DEFINITIONS "ORBIT_VERSION=\"${ORBIT_VERSION}\"" APPEND)
idf_build_set_property(COMPILE_DEFINITIONS "ORBIT_VERSION_BCD=${ORBIT_VERSION_BCD}" APPEND)

project(orbit)
