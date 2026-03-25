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
    # Uses direct include of source files to avoid complex linking
    add_executable(test_system_checker
        test_system_checker.cpp
    )
    target_include_directories(test_system_checker PRIVATE
        ${CMAKE_SOURCE_DIR}/src
    )
    target_link_libraries(test_system_checker PRIVATE
        GTest::gtest
        GTest::gtest_main
        Qt6::Core
        ole32
        oleaut32
        mmdevapi
    )
    if(MSVC)
        target_compile_options(test_system_checker PRIVATE /utf-8 /W4 /wd4100)
    endif()

    include(GoogleTest)
    gtest_discover_tests(BlueDrop_tests)
    gtest_discover_tests(test_system_checker)
endif()
