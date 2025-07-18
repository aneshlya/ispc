#  Copyright (c) 2025, Intel Corporation
#  SPDX-License-Identifier: BSD-3-Clause

set(example_name attention)
project(${example_name})

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Python
find_package(Python 3.7 COMPONENTS Interpreter Development REQUIRED)

# Try to find nanobind
set(NANOBIND_FOUND FALSE)
execute_process(
    COMMAND "${Python_EXECUTABLE}" -m nanobind --cmake_dir
    OUTPUT_VARIABLE NB_DIR
    RESULT_VARIABLE NB_RESULT
    ERROR_VARIABLE NB_ERROR
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(NB_RESULT EQUAL 0 AND NB_DIR)
    list(APPEND CMAKE_PREFIX_PATH "${NB_DIR}")
    find_package(nanobind CONFIG QUIET)
    if(nanobind_FOUND)
        set(NANOBIND_FOUND TRUE)
    endif()
endif()

# Check if nanobind was found, exit early if not
if(NOT NANOBIND_FOUND)
    message(WARNING "nanobind not found. Skipping the attention example.")
    message(WARNING "To build this example, please install nanobind with: pip install -r requirements.txt")
    return()  # Exit early, stopping processing of this CMakeLists.txt file
endif()

# Define ISPC compilation settings
set(ISPC_SRC ${CMAKE_CURRENT_SOURCE_DIR}/attention.ispc)
set(ISPC_OBJ ${CMAKE_CURRENT_BINARY_DIR}/attention.o)
set(ISPC_HEADER ${CMAKE_CURRENT_BINARY_DIR}/attention_ispc.h)
set(ISPC_NANOBIND_WRAPPER ${CMAKE_CURRENT_BINARY_DIR}/attention.cpp)

# Set ISPC flags
set(ISPC_FLAGS -O2 --target=host -DISPC_TASKED_LOOP3 -DGOTO_IMPL)
if (WIN32)
    set(ISPC_FLAGS ${ISPC_FLAGS} --dllexport)
else()
    set(ISPC_FLAGS ${ISPC_FLAGS} --pic)
endif()

# Add custom command to compile ISPC file
add_custom_command(
    OUTPUT ${ISPC_OBJ} ${ISPC_NANOBIND_WRAPPER}
    COMMAND ${ISPC_EXECUTABLE} ${ISPC_FLAGS} ${ISPC_SRC} -o ${ISPC_OBJ} -h ${ISPC_HEADER} --nanobind-wrapper=${ISPC_NANOBIND_WRAPPER}
    DEPENDS ${ISPC_SRC}
    COMMENT "Compiling ISPC file: ${ISPC_SRC}"
)

# Make sure the ISPC header file is created before it's needed
add_custom_target(generate_attention
    DEPENDS ${ISPC_OBJ} ${ISPC_NANOBIND_WRAPPER}
)

# Add nanobind extension module
nanobind_add_module(
    attention
    ${ISPC_NANOBIND_WRAPPER}
)

# Make sure the ISPC files are generated before building the module
add_dependencies(attention generate_attention)

# Link with the ISPC object file
target_link_libraries(attention PRIVATE ${ISPC_OBJ})

# Add include directories to find the ISPC header
target_include_directories(attention PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

target_sources(attention PRIVATE ${EXAMPLES_ROOT}/common/tasksys.cpp)
find_package(TBB QUIET COMPONENTS tbb)
if (TBB_FOUND)
    # TBB package does not produce any info when found, so print it here.
    message(STATUS "Found TBB: ${TBB_VERSION} at ${TBB_DIR}")
    target_link_libraries(attention PRIVATE TBB::tbb)
else()
    # On some OS (Ubuntu 18.04, CentOS) TBB is installed without CMake support
    # Try to find TBB library and header files
    find_library(TBB_LIB NAMES libtbb.so.2 tbb)
    get_filename_component(TBB_LIB_DIR "${TBB_LIB}" DIRECTORY)
    find_path(TBB_INCLUDE_DIR NAMES ${TBB_HEADER} PATHS ${TBB_LIB_DIR}/../include)
    # If TBB library and header files are found, use them.
    if (TBB_LIB AND TBB_INCLUDE_DIR)
        message(STATUS "Found TBB: ${TBB_LIB}")
        message(STATUS "TBB_INCLUDE_DIR is ${TBB_INCLUDE_DIR}")
        target_link_libraries(attention PRIVATE ${TBB_LIB})
        target_include_directories(attention PRIVATE ${TBB_INCLUDE_DIR})
        set (TBB_FOUND TRUE)
    endif()
endif()
if (NOT TBB_FOUND)
message(FATAL_ERROR "TBB is not found! Please install TBB or set the TBB_ROOT pointing to TBB location")
endif()
target_compile_definitions(attention PRIVATE ISPC_USE_TBB_PARALLEL_FOR)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/attention)
# On Windows, link with appropriate libraries to provide DLL entry point
if(WIN32)
    target_link_libraries(attention PRIVATE msvcrt)
    set_target_properties(attention PROPERTIES
        LINKER_LANGUAGE CXX
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    )
    if (MSVC)
        add_custom_command(TARGET attention POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy
                $<TARGET_FILE:attention>
                ${CMAKE_BINARY_DIR}/point_transform_nanobind/$<TARGET_FILE_NAME:attention>
            COMMENT "Copying attention DLL to build directory"
    )
    endif()
endif()

# Add a target to copy Python file
add_custom_target(attention_py
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_SOURCE_DIR}/attention.py
        ${CMAKE_BINARY_DIR}/attention/attention.py
    COMMENT "Copying attention.py example to build directory"
)
add_dependencies(attention attention_py)

# Installation targets
if (NOT ISPC_PREPARE_PACKAGE)
    install(TARGETS attention LIBRARY DESTINATION examples/attention)
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/attention.py DESTINATION examples/attention)
endif()
