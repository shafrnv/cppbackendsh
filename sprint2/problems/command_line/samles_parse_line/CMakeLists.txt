cmake_minimum_required(VERSION 3.11)

project(merge CXX)
set(CMAKE_CXX_STANDARD 20)

include(${CMAKE_BINARY_DIR}/conanbuildinfo_multi.cmake)
conan_basic_setup(TARGETS)
find_package(Boost 1.78.0 REQUIRED)
if(Boost_FOUND)
  include_directories(${Boost_INCLUDE_DIRS})
endif()
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_executable(merge src/merge.cpp)

target_include_directories(merge PRIVATE CONAN_PKG::boost)
target_link_libraries(merge PRIVATE PRIVATE Threads::Threads CONAN_PKG::boost)
