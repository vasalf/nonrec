cmake_minimum_required(VERSION 3.12)
project(nonrec LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)

include_directories(include/)

include(CTest)
find_package(doctest REQUIRED)
find_package(libunwind REQUIRED)

add_executable(nonrec-test test/main.cpp)
target_link_libraries(nonrec-test PRIVATE doctest::doctest libunwind::libunwind)
