cmake_minimum_required(VERSION 3.17)

project(msglib-cpp)

set(VERSION "0.1.0")
#---------------------------------------------------------------------------------------
# Build Types including support for sanitizers
#---------------------------------------------------------------------------------------

set(CMAKE_BUILD_TYPE ${CMAKE_BUILD_TYPE}
    CACHE STRING "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel tsan asan lsan msan ubsan"
    FORCE)

# ThreadSanitizer
set(CMAKE_C_FLAGS_TSAN
    "-fsanitize=thread -g -O1"
    CACHE STRING "Flags used by the C compiler during ThreadSanitizer builds."
    FORCE)
set(CMAKE_CXX_FLAGS_TSAN
    "-fsanitize=thread -g -O1"
    CACHE STRING "Flags used by the C++ compiler during ThreadSanitizer builds."
    FORCE)

# AddressSanitize
set(CMAKE_C_FLAGS_ASAN
    "-fsanitize=address -fno-optimize-sibling-calls -fsanitize-address-use-after-scope -fno-omit-frame-pointer -g -O1"
    CACHE STRING "Flags used by the C compiler during AddressSanitizer builds."
    FORCE)
set(CMAKE_CXX_FLAGS_ASAN
    "-fsanitize=address -fno-optimize-sibling-calls -fsanitize-address-use-after-scope -fno-omit-frame-pointer -g -O1"
    CACHE STRING "Flags used by the C++ compiler during AddressSanitizer builds."
    FORCE)

# LeakSanitizer
set(CMAKE_C_FLAGS_LSAN
    "-fsanitize=leak -fno-omit-frame-pointer -g -O1"
    CACHE STRING "Flags used by the C compiler during LeakSanitizer builds."
    FORCE)
set(CMAKE_CXX_FLAGS_LSAN
    "-fsanitize=leak -fno-omit-frame-pointer -g -O1"
    CACHE STRING "Flags used by the C++ compiler during LeakSanitizer builds."
    FORCE)

# MemorySanitizer (clang only)
set(CMAKE_C_FLAGS_MSAN
    "-fsanitize=memory -fno-optimize-sibling-calls -fsanitize-memory-track-origins=2 -fno-omit-frame-pointer -g -O2"
    CACHE STRING "Flags used by the C compiler during MemorySanitizer builds."
    FORCE)
set(CMAKE_CXX_FLAGS_MSAN
    "-fsanitize=memory -fno-optimize-sibling-calls -fsanitize-memory-track-origins=2 -fno-omit-frame-pointer -g -O2"
    CACHE STRING "Flags used by the C++ compiler during MemorySanitizer builds."
    FORCE)

# UndefinedBehaviour
set(CMAKE_C_FLAGS_UBSAN
    "-fsanitize=undefined"
    CACHE STRING "Flags used by the C compiler during UndefinedBehaviourSanitizer builds."
    FORCE)
set(CMAKE_CXX_FLAGS_UBSAN
    "-fsanitize=undefined"
    CACHE STRING "Flags used by the C++ compiler during UndefinedBehaviourSanitizer builds."
    FORCE)
# MemorySanitizer only supported by clang
if ("${CMAKE_BUILD_TYPE}" STREQUAL "msan" )
  if (CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    message( FATAL_ERROR "msan not supported for g++")
  endif ()
endif ()

if(NOT CMAKE_BUILD_TYPE)
  set (CMAKE_BUILD_TYPE Debug)
endif(NOT CMAKE_BUILD_TYPE)

#---------------------------------------------------------------------------------------
# Project options
#---------------------------------------------------------------------------------------

option(BUILD_EXAMPLES "Build examples" ON)
option(BUILD_TESTS "Build unit tests." ON)
option(BUILD_SANDBOX "Build sandbox code" OFF)
option(BUILD_COVERAGE "Build for code coverage." OFF)
option(BUILD_SHARED_LIBS "Build Shared Libraries" ON)
option(BUILD_STATIC_LIBS "Build Static Libraries" OFF)

option(ENABLE_CLANG_TIDY "Enable testing with clang-tidy" FALSE)
option(ENABLE_CPPCHECK "Enable testing with cppcheck" FALSE)

set (CMAKE_MODULE_PATH  ${CMAKE_MODULE_PATH}
                        ${CMAKE_CURRENT_SOURCE_DIR}/CMake)


#---------------------------------------------------------------------------------------
# compiler config
#---------------------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" OR "${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
    add_compile_options("-Wall")
    add_compile_options("-Wextra")
    add_compile_options("-Wconversion")
    add_compile_options("-pedantic")
    add_compile_options("-Wfatal-errors")

endif()

#---------------------------------------------------------------------------------------
# Dependencies
#---------------------------------------------------------------------------------------

# Threads
find_package(Threads)

if (BUILD_TESTS)
  # Google Test
  find_package(GTest REQUIRED)
endif()

if (BUILD_EXAMPLES OR BUILD_SANDBOX)
# fmt library dependency
find_package(fmt CONFIG REQUIRED)

# spdlog library dependency
find_package(spdlog CONFIG REQUIRED)
endif()

# clang-tidy
if(ENABLE_CLANG_TIDY)
  find_program(CLANGTIDY clang-tidy)
  if(CLANGTIDY)
    message(STATUS "Using ${CLANGTIDY} for clang-tidy")
    set(CMAKE_CXX_CLANG_TIDY "${CLANGTIDY}")
  else()
    message(SEND_ERROR "clang-tidy requested but executable not found")
  endif()
endif()
configure_file(.clang-tidy .clang-tidy COPYONLY)

# cppcheck
if(ENABLE_CPPCHECK)
  find_program(CPPCHECK cppcheck)
  if(CPPCHECK)
    message(STATUS "Enabling cppcheck")
    set(CMAKE_CXX_CPPCHECK ${CPPCHECK})
    list(
      APPEND CMAKE_CXX_CPPCHECK 
          "--enable=warning,style"
          "--suppressions-list=${CMAKE_SOURCE_DIR}/suppressions.txt"
    )
  else()
    message(SEND_ERROR "cppcheck requested but executable not found")
  endif()
endif()

#---------------------------------------------------------------------------------------
# Sources, headers, directories and libs
#---------------------------------------------------------------------------------------
# From http://www.cmake.org/pipermail/cmake/2010-March/035992.html:
# function to collect all the sources from sub-directories
# into a single list
function(add_sources)
  get_property(is_defined GLOBAL PROPERTY SRCS_LIST DEFINED)
  if(NOT is_defined)
    define_property(GLOBAL PROPERTY SRCS_LIST
      BRIEF_DOCS "List of source files"
      FULL_DOCS "List of all source files in the entire project")
  endif()
  # make absolute paths
  set(SRCS)
  foreach(s IN LISTS ARGN)
    if(NOT IS_ABSOLUTE "${s}")
      get_filename_component(s "${s}" ABSOLUTE)
    endif()
    list(APPEND SRCS "${s}")
  endforeach()
  # append to global list
  set_property(GLOBAL APPEND PROPERTY SRCS_LIST "${SRCS}")
endfunction(add_sources)

file(GLOB sources "src/[a-zA-Z]*.cpp")
file(GLOB_RECURSE public_headers "include/msglib-cpp/[a-zA-Z]*.h")
file(GLOB private_headers "src/[a-zA-Z]*.h")
file(GLOB examples "examples/[a-zA-Z]*.[ch]*")
file(GLOB tests "test/[a-zA-Z]*.cpp")

set(library_sources
    ${sourcse}
    ${public_headers}
    ${private_headers}
    ${examples}
    ${tests}
)
add_sources(${library_sources})

if(VERBOSE)
    message(STATUS "sources: ${sources}")
    message(STATUS "public_headers: ${public_headers}")
    message(STATUS "private_headers: ${private_headers}")
    message(STATUS "examples: ${examples}")
    message(STATUS "tests: ${tests}")
endif(VERBOSE)

if (BUILD_EXAMPLES)
    add_subdirectory(example)    
endif(BUILD_EXAMPLES)
if (BUILD_SANDBOX)
    add_subdirectory(sandbox)
endif(BUILD_SANDBOX)
if (BUILD_TESTS)
    include(CTest)
    if (BUILD_COVERAGE)
        include (CodeCoverage)
        setup_target_for_coverage_gcovr_html( NAME MsgLibCoverage EXECUTABLE test/msglibTests DEPENDENCIES msglibTests BASE_DIRECTORY "." EXCLUDE "include/msglib/detail/DiagResource.h" "test/*")
    endif (BUILD_COVERAGE)
    add_subdirectory(test)
endif(BUILD_TESTS)

#---------------------------------------------------------------------------------------
# Addons
#---------------------------------------------------------------------------------------

# Formatting
get_property(all_sources GLOBAL PROPERTY SRCS_LIST)
add_custom_target(format
    COMMAND clang-format --style=file -i ${all_sources}
    COMMENT "Running clang-format"
    VERBATIM)





