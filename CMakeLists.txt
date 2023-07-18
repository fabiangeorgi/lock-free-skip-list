cmake_minimum_required(VERSION 3.16)
project(skip_list)
set(CMAKE_CXX_STANDARD 20)

include(FetchContent)

option(CI_BUILD "Set to ON for complete build in CI." OFF)

set(SANITIZER_FLAGS -O2 -g -fno-omit-frame-pointer)

# We override the default flags here so you can use `assert()` in RelWithDebInfo builds.
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g3" CACHE STRING "" FORCE)

function(add_sanitizer_flags target)
  if (CMAKE_BUILD_TYPE MATCHES Asan)
    add_compile_options(${SANITIZER_FLAGS})
    target_compile_options(${target} PRIVATE -fsanitize=address -fsanitize=undefined)
    target_link_options(${target} PRIVATE -fsanitize=address -fsanitize=undefined)
  elseif (CMAKE_BUILD_TYPE MATCHES Tsan)
    add_compile_options(${SANITIZER_FLAGS})
    target_compile_options(${target} PRIVATE -fsanitize=thread)
    target_link_options(${target} PRIVATE -fsanitize=thread)
    target_compile_options(${target} PUBLIC -DTSAN_BUILD)
  endif ()
endfunction()

if (NOT CMAKE_BUILD_TYPE)
  message(STATUS "No build type specified. Defaulting to Debug.
                    Use cmake `-DCMAKE_BUILD_TYPE=Release ..` to build an optimized version.")
endif ()

if (CMAKE_BUILD_TYPE MATCHES Release)
  message(STATUS "Building an optimized release version.")
  add_compile_options(-O3)
endif ()

if (CI_BUILD)
  # We cannot set this generally, as it does not work on all architectures.
  message(STATUS "Adding -march/tune=native in CI")
  add_compile_options(-march=native -mtune=native)
endif ()

set(TASK_SOURCES src/skip_list.cpp src/skip_list.hpp)
add_library(skip_list ${TASK_SOURCES})
target_include_directories(skip_list INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)
add_sanitizer_flags(skip_list)

enable_testing()
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.13.0
)

set(BUILD_GMOCK ON CACHE BOOL "" FORCE)
set(BUILD_GTEST ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(basic_test test/basic.cpp)
add_test(basic_test basic_test)
target_link_libraries(basic_test skip_list gtest gmock)
add_sanitizer_flags(basic_test)

if (${CI_BUILD})
  # Build advanced tests in CI only
  add_executable(advanced_test test/advanced.cpp)
  add_test(advanced_test advanced_test)
  target_link_libraries(advanced_test skip_list gtest gmock)
  add_sanitizer_flags(advanced_test)

  add_executable(hdp_benchmark test/benchmark.cpp)
  target_link_libraries(hdp_benchmark skip_list)
endif ()
