cmake_minimum_required(VERSION 3.12)
project(nonrec LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)

include_directories(include/)

find_package(doctest REQUIRED)
find_package(benchmark REQUIRED)
find_package(Boost REQUIRED)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
endif ()

if (MSVC)
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Zc:__cplusplus")
endif ()

add_executable(nonrec-test test/main.cpp)
target_include_directories(nonrec-test PRIVATE ${Boost_INCLUDE_DIR})
target_link_libraries(nonrec-test PRIVATE doctest::doctest  )

if (APPLE)
    target_compile_definitions(nonrec-test PRIVATE _GNU_SOURCE)
endif ()

add_executable(nonrec-bench bench/main.cpp)
target_link_libraries(nonrec-bench PRIVATE benchmark::benchmark)

# The benchmark also runs recursive DFS which does not fit into default stack sizes.
set (BENCHMARK_STACK_SIZE "0x4000000") # 64 MiB
if (MSVC)
    set_target_properties(nonrec-bench PROPERTIES LINK_FLAGS "/STACK:${BENCHMARK_STACK_SIZE}")
elseif (APPLE)
    set_target_properties(nonrec-bench PROPERTIES LINK_FLAGS "-Wl,-stack_size,${BENCHMARK_STACK_SIZE}")
endif ()
