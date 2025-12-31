# cmake_minimum_required(VERSION 3.12)
# project(student_agent_module)

# set(CMAKE_CXX_STANDARD 17)

# # Set Python policy to avoid warnings
# # cmake_policy(SET CMP0148 NEW)

# # Find Python first
# find_package(Python COMPONENTS Interpreter Development REQUIRED)

# # Find pybind11
# find_package(pybind11 REQUIRED)

# # Build the original student_agent_module
# pybind11_add_module(student_agent_module student_agent.cpp)

# # Build the complete s1 module
cmake_minimum_required(VERSION 3.12)
project(student_agent_module)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ------------------------------------------------------------------
# Find dependencies
# ------------------------------------------------------------------
find_package(Python COMPONENTS Interpreter Development REQUIRED)
find_package(pybind11 REQUIRED)

# ------------------------------------------------------------------
# Build the student_agent_module (with agent.cpp)
# ------------------------------------------------------------------
pybind11_add_module(student_agent_module
    student_agent.cpp
    agent.cpp           # ✅ include your new agent implementation
)


# ------------------------------------------------------------------
# Include current directory for headers
# ------------------------------------------------------------------
target_include_directories(student_agent_module PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# ------------------------------------------------------------------
# Optional: link time optimization flags for speed
# ------------------------------------------------------------------
if (CMAKE_BUILD_TYPE STREQUAL "Release")
    message(STATUS "Building in Release mode with optimization flags")
    add_compile_options(-O3)
endif()
