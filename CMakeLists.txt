cmake_minimum_required(VERSION 3.22)
project(logitune VERSION 0.2.3 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Quick Svg DBus Widgets Concurrent Test QuickTest)
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
install(FILES data/logitune.desktop DESTINATION etc/xdg/autostart)
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
