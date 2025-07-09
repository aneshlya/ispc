#
#  Copyright (c) 2025, Intel Corporation
#
#  SPDX-License-Identifier: BSD-3-Clause

cmake_minimum_required(VERSION 3.16)
project(simple_lib VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Option to choose between static and shared ISPC library
option(USE_STATIC_ISPC "Use static ISPC library instead of shared" OFF)

# Find ISPC using our package configuration
find_package(ispc REQUIRED)

# Create the original executable
add_executable(simple_lib simple.cpp)

# Create the JIT example executable
add_executable(simple_jit simple_jit.cpp)

# Link against ISPC library
# Choose between static and shared library based on option
if(USE_STATIC_ISPC)
    target_link_libraries(simple_lib ispc::lib_static)
    target_link_libraries(simple_jit ispc::lib_static)
    message(STATUS "Using static ISPC library")
else()
    target_link_libraries(simple_lib ispc::lib)
    target_link_libraries(simple_jit ispc::lib)
    message(STATUS "Using shared ISPC library")
endif()

# Copy simple.ispc to build directory so the example can find it
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/simple.ispc ${CMAKE_CURRENT_BINARY_DIR}/simple.ispc COPYONLY)