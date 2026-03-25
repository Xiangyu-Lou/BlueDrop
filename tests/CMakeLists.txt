if(BUILD_TESTS)
    find_package(GTest CONFIG REQUIRED)

    # Placeholder test to verify test infrastructure works
    add_executable(BlueDrop_tests
        test_placeholder.cpp
    )

    target_link_libraries(BlueDrop_tests PRIVATE
        GTest::gtest
        GTest::gtest_main
    )

    include(GoogleTest)
    gtest_discover_tests(BlueDrop_tests)
endif()
