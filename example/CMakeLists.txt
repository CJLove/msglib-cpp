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
target_link_libraries( pool Threads::Threads rt )

set (mbox_SRC mbox.cpp)

add_executable( mbox ${mbox_SRC})
target_link_libraries( mbox Threads::Threads rt )

set (timer_SRC timer.cpp)

add_executable( timer ${timer_SRC})
target_link_libraries( timer Threads::Threads rt )

set (queue_SRC queue.cpp)

add_executable( queue ${queue_SRC})
target_link_libraries( queue Threads::Threads rt )
