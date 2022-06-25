cmake_minimum_required(VERSION 3.12)
project(nonrec LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)

include_directories(include/)

find_package(doctest REQUIRED)
find_package(libunwind REQUIRED)
find_package(benchmark REQUIRED)
find_package(elfutils REQUIRED)
find_package(zstd REQUIRED)

add_executable(nonrec-test test/main.cpp)
target_link_libraries(nonrec-test PRIVATE doctest::doctest libunwind::libunwind elfutils::elfutils zstd::libzstd_static)

add_executable(nonrec-bench bench/main.cpp)
target_link_libraries(nonrec-bench PRIVATE benchmark::benchmark)
