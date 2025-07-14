#
#  Copyright (c) 2025, Intel Corporation
#
#  SPDX-License-Identifier: BSD-3-Clause

cmake_minimum_required(VERSION 3.16)
project(simple_lib VERSION 1.0.0 LANGUAGES C CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set C standard
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Find ISPC using our package configuration
find_package(ispc REQUIRED)

# Create the C++ executable
add_executable(simple_lib simple.cpp)

# Create the C executable
add_executable(simple_c simple_c.c)

# Link against ISPC library
target_link_libraries(simple_lib ispc::lib)
target_link_libraries(simple_c ispc::lib)

# Copy simple.ispc to build directory so the example can find it
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/simple.ispc ${CMAKE_CURRENT_BINARY_DIR}/simple.ispc COPYONLY)