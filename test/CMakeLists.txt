cmake_minimum_required (VERSION 3.17)

project (msglibTests)

enable_testing()

# Disable clang-tidy checks for unit test code
set(CMAKE_CXX_CLANG_TIDY "")

if (BUILD_COVERAGE)
    add_definitions( --coverage )
endif (BUILD_COVERAGE)

include_directories( 
    .
    ${gtest_SOURCE_DIR}/include ${gtest_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

set ( msglibTests_SRC 
    test_TimeConv.cpp
    test_Timers.cpp
    test_Queue.cpp
    test_Pool.cpp 
    test_Mailbox.cpp
)

add_executable ( msglibTests ${msglibTests_SRC} )


target_link_libraries( msglibTests gtest gtest_main )

target_link_libraries( msglibTests Threads::Threads pthread rt )

if (BUILD_COVERAGE)
    target_link_libraries( msglibTests gcov )
endif (BUILD_COVERAGE)

add_test (msglibTests msglibTests )
