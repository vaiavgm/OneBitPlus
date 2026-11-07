cmake_minimum_required(VERSION 3.16)
project(OneBitPlusTests CXX)

# Unit tests for the iPlug2-independent helpers in src/.
#   cmake -S tests -B build-tests && cmake --build build-tests && ./build-tests/OneBitPlusTests
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(OneBitPlusTests TestMain.cpp)
target_include_directories(OneBitPlusTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
find_package(Threads REQUIRED)
target_link_libraries(OneBitPlusTests PRIVATE Threads::Threads)

if(MSVC)
  target_compile_options(OneBitPlusTests PRIVATE /W4 /permissive-)
else()
  target_compile_options(OneBitPlusTests PRIVATE -Wall -Wextra)
endif()

enable_testing()
add_test(NAME OneBitPlusTests COMMAND OneBitPlusTests)
