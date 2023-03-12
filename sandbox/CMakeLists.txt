cmake_minimum_required(VERSION 3.1)

project(sandbox)

# Disable clang-tidy checks for example code
set(CMAKE_CXX_CLANG_TIDY "")

include_directories( 
    ${CMAKE_SOURCE_DIR}/include
    .
)

set (pool_SRC pool.cpp)

add_executable( pool ${pool_SRC} )
target_link_libraries( pool Threads::Threads rt )

set (queue_SRC queue.cpp)

add_executable( queue ${queue_SRC})
target_link_libraries( queue Threads::Threads rt )
