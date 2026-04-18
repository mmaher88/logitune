find_package(GTest REQUIRED)
include(GoogleTest)

add_executable(logitune-tests
    test_main.cpp
    test_smoke.cpp
    test_transport.cpp
    test_feature_dispatcher.cpp
    test_capability_dispatch.cpp
    test_scroll_features.cpp
    test_features.cpp
    test_button_features.cpp
    test_profile_engine.cpp
    test_device_discovery.cpp
    test_action_executor.cpp
    test_device_registry.cpp
    test_descriptor_writer.cpp
    test_editor_model.cpp
    test_smoke_mocks.cpp
    mocks/MockDesktop.cpp
    mocks/MockInjector.cpp
    mocks/MockTransport.cpp
    test_keystroke_parser.cpp
    test_button_action.cpp
    test_button_model.cpp
    test_profile_model.cpp
    test_action_model.cpp
    test_device_model.cpp
    test_wmclass_resolution.cpp
    test_app_controller.cpp
    test_profile_switching.cpp
    test_profile_persistence.cpp
    test_device_reconnect.cpp
    test_thumb_wheel_behavior.cpp
    test_profile_switch_behavior.cpp
    test_notification_filtering.cpp
    test_device_session.cpp
    test_physical_device.cpp
    test_settings_change_behavior.cpp
    test_desktop_factory.cpp
    test_profile_apply_behavior.cpp
    test_settings_model.cpp
    test_json_device.cpp
    test_device_fetcher.cpp
    test_dpi_cycle_ring.cpp
)
target_include_directories(logitune-tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(logitune-tests PRIVATE logitune-core logitune-app-lib Qt6::Test Qt6::Widgets GTest::gtest)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/logitune)
file(CREATE_LINK ${CMAKE_SOURCE_DIR}/devices ${CMAKE_BINARY_DIR}/logitune/devices SYMBOLIC)

gtest_discover_tests(logitune-tests
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;XDG_DATA_DIRS=${CMAKE_BINARY_DIR}"
    DISCOVERY_MODE PRE_TEST)

# Tray tests need a clean QApplication — separate binary to avoid fixture conflicts
add_executable(logitune-tray-tests test_main.cpp test_tray_manager.cpp)
target_include_directories(logitune-tray-tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(logitune-tray-tests PRIVATE logitune-app-lib Qt6::Test Qt6::Widgets GTest::gtest)
gtest_discover_tests(logitune-tray-tests
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;XDG_DATA_DIRS=${CMAKE_BINARY_DIR}"
    DISCOVERY_MODE PRE_TEST)

add_subdirectory(qml)

if(BUILD_HW_TESTING)
    add_subdirectory(hw)
endif()
