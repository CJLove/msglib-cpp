cmake_minimum_required(VERSION 3.1)

project(msglib-cpp)

include_directories( 
    ${CMAKE_SOURCE_DIR}/include 
    .
)

if (BUILD_COVERAGE)
    add_definitions( --coverage )
endif (BUILD_COVERAGE)

set (msglib-cpp_SRC
    Mailbox.cpp
#   TimerManager.cpp
)

if(BUILD_SHARED_LIBS)
    add_library( msglib-cpp SHARED ${msglib-cpp_SRC} )
endif(BUILD_SHARED_LIBS)

if(BUILD_STATIC_LIBS)
    add_library( msglib-cpp ${msglib-cpp_SRC} )
endif(BUILD_STATIC_LIBS)

target_link_libraries( msglib-cpp pthread rt )

if (BUILD_COVERAGE)
    target_link_libraries( msglib-cpp gcov )
endif (BUILD_COVERAGE)

if (BUILD_SHARED_LIBS)
    install(TARGETS ${PROJECT_NAME} LIBRARY DESTINATION lib)
endif(BUILD_SHARED_LIBS)
