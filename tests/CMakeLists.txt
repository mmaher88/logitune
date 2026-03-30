find_package(GTest REQUIRED)
include(GoogleTest)

add_executable(logitune-tests
    test_smoke.cpp
    test_transport.cpp
    test_feature_dispatcher.cpp
    test_scroll_features.cpp
    test_features.cpp
    test_button_features.cpp
    test_profile_engine.cpp
    test_device_discovery.cpp
    test_action_executor.cpp
    test_device_registry.cpp
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
)
target_include_directories(logitune-tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(logitune-tests PRIVATE logitune-core logitune-app-lib Qt6::Test GTest::gtest_main)
gtest_discover_tests(logitune-tests)
