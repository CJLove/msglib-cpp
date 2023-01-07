cmake_minimum_required(VERSION 3.1)

project(example)

# Disable clang-tidy checks for example code
set(CMAKE_CXX_CLANG_TIDY "")

include_directories( 
    ${CMAKE_SOURCE_DIR}/include
    .
)

set (pool_SRC pool.cpp)

add_executable( pool ${pool_SRC} )
target_link_libraries( pool pthread )

set (mbox_SRC mbox.cpp)

add_executable( mbox ${mbox_SRC})
target_link_libraries( mbox msglib-cpp pthread )

set (timer_SRC timer.cpp)

add_executable( timer ${timer_SRC})
target_link_libraries( timer msglib-cpp pthread )

set (queue_SRC queue.cpp)

add_executable( queue ${queue_SRC})
target_link_libraries( queue msglib-cpp pthread )
