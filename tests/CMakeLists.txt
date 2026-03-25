if(BUILD_TESTS)
    find_package(GTest CONFIG REQUIRED)

    # Placeholder test
    add_executable(BlueDrop_tests
        test_placeholder.cpp
    )
    target_link_libraries(BlueDrop_tests PRIVATE
        GTest::gtest
        GTest::gtest_main
    )

    # SystemChecker tests
    add_executable(test_system_checker
        test_system_checker.cpp
    )
    target_link_libraries(test_system_checker PRIVATE
        GTest::gtest
        GTest::gtest_main
        BlueDrop_core
    )
    if(MSVC)
        target_compile_options(test_system_checker PRIVATE /utf-8 /W4 /wd4100)
    endif()

    # DeviceEnumerator tests
    add_executable(test_device_enumerator
        test_device_enumerator.cpp
    )
    target_link_libraries(test_device_enumerator PRIVATE
        GTest::gtest
        GTest::gtest_main
        BlueDrop_core
    )
    if(MSVC)
        target_compile_options(test_device_enumerator PRIVATE /utf-8 /W4 /wd4100)
    endif()

    include(GoogleTest)
    gtest_discover_tests(BlueDrop_tests)
    gtest_discover_tests(test_system_checker)
    gtest_discover_tests(test_device_enumerator)
endif()
