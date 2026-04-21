# SPDX-License-Identifier: GPL-3.0-only
cmake_minimum_required(VERSION 3.22)

# Derive version from the latest v*-prefixed git tag so local/dev builds
# report the same version users see on the last release. project()'s
# VERSION field only accepts numeric major.minor.patch, so pre-release
# suffixes (-beta.N) are stripped into LOGITUNE_DISPLAY_VERSION for the
# About page label; the numeric core stays the authoritative Qt version.
find_package(Git QUIET)
set(LOGITUNE_VERSION "0.2.3")
set(LOGITUNE_DISPLAY_VERSION "${LOGITUNE_VERSION}")

if(Git_FOUND AND EXISTS "${CMAKE_SOURCE_DIR}/.git")
    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --tags --abbrev=0 --match "v[0-9]*"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE _git_tag
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE _git_rc
    )
    if(_git_rc EQUAL 0 AND _git_tag MATCHES "^v([0-9]+)\\.([0-9]+)\\.([0-9]+)(.*)$")
        set(LOGITUNE_VERSION "${CMAKE_MATCH_1}.${CMAKE_MATCH_2}.${CMAKE_MATCH_3}")
        string(REGEX REPLACE "^v" "" LOGITUNE_DISPLAY_VERSION "${_git_tag}")
    endif()
endif()

project(logitune VERSION ${LOGITUNE_VERSION} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Quick Svg DBus Widgets Concurrent Test QuickTest Network)
find_package(PkgConfig REQUIRED)
pkg_check_modules(UDEV REQUIRED libudev)

include(GNUInstallDirs)

option(BUILD_TESTING "Build tests" ON)
option(BUILD_HW_TESTING "Build hardware integration tests (requires connected device)" OFF)

add_subdirectory(src/core)
add_subdirectory(src/app)

if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()

install(FILES data/71-logitune.rules DESTINATION lib/udev/rules.d)
install(FILES data/logitune.desktop DESTINATION share/applications)
install(FILES data/logitune-autostart.desktop
        DESTINATION /etc/xdg/autostart
        RENAME logitune.desktop)
install(FILES data/com.logitune.Logitune.svg DESTINATION share/icons/hicolor/scalable/apps)

install(DIRECTORY ${CMAKE_SOURCE_DIR}/devices/
        DESTINATION ${CMAKE_INSTALL_DATADIR}/logitune/devices)

# GNOME Shell extension (both API versions — app selects correct one at runtime)
# Root extension.js = v45 (GNOME 45+, most common). Shell loads from root.
# v42/v45 subdirs kept for the app's runtime copy to user dir on older GNOME.
set(GNOME_EXT_DIR share/gnome-shell/extensions/logitune-focus@logitune.com)
install(FILES data/gnome-extension/metadata.json DESTINATION ${GNOME_EXT_DIR})
install(FILES data/gnome-extension/v45/extension.js DESTINATION ${GNOME_EXT_DIR})
install(FILES data/gnome-extension/v42/extension.js DESTINATION ${GNOME_EXT_DIR}/v42)
install(FILES data/gnome-extension/v45/extension.js DESTINATION ${GNOME_EXT_DIR}/v45)
