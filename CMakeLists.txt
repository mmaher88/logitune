cmake_minimum_required(VERSION 3.22)
project(logitune VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Quick Svg DBus Widgets Concurrent Test QuickTest)
find_package(PkgConfig REQUIRED)
pkg_check_modules(UDEV REQUIRED libudev)

option(BUILD_TESTING "Build tests" ON)
option(BUILD_HW_TESTING "Build hardware integration tests (requires connected device)" OFF)

add_subdirectory(src/core)
add_subdirectory(src/app)

if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()

install(FILES data/71-logitune.rules DESTINATION lib/udev/rules.d)
install(FILES data/com.logitune.Logitune.desktop DESTINATION share/applications)
install(FILES data/com.logitune.Logitune.desktop DESTINATION etc/xdg/autostart)
install(FILES data/com.logitune.Logitune.metainfo.xml DESTINATION share/metainfo)
install(FILES data/com.logitune.Logitune.svg DESTINATION share/icons/hicolor/scalable/apps)
