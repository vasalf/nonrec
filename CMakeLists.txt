cmake_minimum_required(VERSION 3.12)
project(nonrec LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)

include_directories(include/)

find_package(doctest REQUIRED)
find_package(libunwind REQUIRED)
find_package(benchmark REQUIRED)
find_package(elfutils REQUIRED)
find_package(zstd REQUIRED)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
endif ()

add_executable(nonrec-test test/main.cpp)
target_link_libraries(nonrec-test PRIVATE doctest::doctest libunwind::libunwind elfutils::elfutils zstd::libzstd_static)

add_executable(nonrec-bench bench/main.cpp)
target_link_libraries(nonrec-bench PRIVATE benchmark::benchmark)
