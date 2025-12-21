cmake_minimum_required(VERSION 3.20)
project(ts0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Single source file with everything embedded
add_executable(ts0 src/main.cpp)

# Enable all warnings
target_compile_options(ts0 PRIVATE -Wall -Wextra -Os)

# Static linking
target_link_options(ts0 PRIVATE -static)
