cmake_minimum_required (VERSION 3.1)

project (msglibTests)

enable_testing()

# Disable clang-tidy checks for unit test code
set(CMAKE_CXX_CLANG_TIDY "")

include_directories( 
    .
    ${gtest_SOURCE_DIR}/include ${gtest_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

set ( msglibTests_SRC test_Pool.cpp
)

add_executable ( msglibTests ${msglibTests_SRC} )


target_link_libraries( msglibTests gtest gtest_main )

target_link_libraries( msglibTests msglib-cpp )

add_test (msglibTests msglibTests )
