cmake_minimum_required(VERSION 3.20)
project(ts0 VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Static linking configuration
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")
set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
set(BUILD_SHARED_LIBS OFF)

# Optimization flags for release
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -flto")
set(CMAKE_EXE_LINKER_FLAGS_RELEASE "-flto -s")

# Source files
set(SOURCES
    src/main.cpp
    src/cli.cpp
    src/config.cpp
    src/json.cpp
    src/glob.cpp
    src/process.cpp
    src/watcher.cpp
    src/commands/init.cpp
    src/commands/build.cpp
    src/commands/run.cpp
    src/commands/test.cpp
)

# Create executable
add_executable(ts0 ${SOURCES})

# Include directories
target_include_directories(ts0 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Link statically with pthread (needed for std::thread)
find_package(Threads REQUIRED)
target_link_libraries(ts0 PRIVATE Threads::Threads)

# For static linking on Linux
if(UNIX AND NOT APPLE)
    target_link_options(ts0 PRIVATE -static-libgcc -static-libstdc++)
endif()

# Installation
install(TARGETS ts0 RUNTIME DESTINATION bin)
