find_package(GTest REQUIRED)
include(GoogleTest)

add_executable(logitune-tests
    test_smoke.cpp
    test_transport.cpp
    test_feature_dispatcher.cpp
    test_scroll_features.cpp
    test_features.cpp
)
target_link_libraries(logitune-tests PRIVATE logitune-core GTest::gtest_main)
gtest_discover_tests(logitune-tests)
