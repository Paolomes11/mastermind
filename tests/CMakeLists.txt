include(FetchContent)

FetchContent_Declare(
    doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG        v2.4.11
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(doctest)

add_executable(mastermind_tests
    main.cpp
    test_code.cpp
    test_feedback.cpp
    test_solver.cpp
)
target_link_libraries(mastermind_tests PRIVATE mastermind_core doctest::doctest)

add_test(NAME mastermind_tests COMMAND mastermind_tests)
