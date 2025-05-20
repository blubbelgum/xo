cmake_minimum_required(VERSION 3.10)
project(xo-c VERSION 0.1.0 LANGUAGES C)

# Set C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Output directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Find packages
# Add any external package dependencies here
# find_package(...)

# Add subdirectories
add_subdirectory(src)

# Define the executable
add_executable(xo-c src/main.c)

# Link libraries
target_link_libraries(xo-c
    xo_core
)

# Installation rules
install(TARGETS xo-c
    RUNTIME DESTINATION bin
) 