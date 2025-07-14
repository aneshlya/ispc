#
#  Copyright (c) 2025, Intel Corporation
#
#  SPDX-License-Identifier: BSD-3-Clause

cmake_minimum_required(VERSION 3.16)
project(simple_lib VERSION 1.0.0 LANGUAGES C CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find ISPC using our package configuration
find_package(ispc REQUIRED)

# Create the original executable
add_executable(simple_lib simple.cpp)

# Create the JIT example executable
add_executable(simple_jit simple_jit.cpp)

# Create the C API example executable
add_executable(simple_c simple_c.c)

# Ensure simple_c.c is compiled as C (not C++)
set_target_properties(simple_c PROPERTIES 
    LINKER_LANGUAGE C
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
)

# Link against ISPC library
target_link_libraries(simple_lib ispc::lib)
target_link_libraries(simple_jit ispc::lib)
target_link_libraries(simple_c ispc::lib)

# Copy simple.ispc to build directory so the example can find it
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/simple.ispc ${CMAKE_CURRENT_BINARY_DIR}/simple.ispc COPYONLY)