/*
  Copyright (c) 2025, Intel Corporation

  SPDX-License-Identifier: BSD-3-Clause
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ispc/ispc.h"

int main() {
    printf("Testing ISPC C API...\n");

    // Test initialization
    printf("Initializing ISPC library...\n");
    if (!ispc_initialize()) {
        printf("ERROR: Failed to initialize ISPC library\n");
        return 1;
    }
    printf("ISPC library initialized successfully\n");

    // Test compile from args function
    printf("\nTesting ispc_compile_from_args...\n");
    char *compile_args[] = {"simple_c", "-o", "simple_c_output.o", "simple.ispc"};
    int argc = sizeof(compile_args) / sizeof(compile_args[0]);

    int result = ispc_compile_from_args(argc, compile_args);
    if (result == 0) {
        printf("Compilation completed successfully\n");
    } else {
        printf("Compilation failed with return code: %d\n", result);
    }

    // Test engine creation and execution
    printf("\nTesting ISPCEngine C API...\n");
    char *engine_args[] = {"simple_c", "-o", "simple_c_engine.o", "simple.ispc"};
    int engine_argc = sizeof(engine_args) / sizeof(engine_args[0]);

    ispc_engine_t *engine = ispc_engine_create_from_args(engine_argc, engine_args);
    if (engine == NULL) {
        printf("ERROR: Failed to create ISPC engine\n");
        ispc_shutdown();
        return 1;
    }
    printf("ISPC engine created successfully\n");

    printf("Executing engine...\n");
    int engine_result = ispc_engine_execute(engine);
    if (engine_result == 0) {
        printf("Engine execution completed successfully\n");
    } else {
        printf("Engine execution failed with return code: %d\n", engine_result);
    }

    // Clean up engine
    printf("Destroying engine...\n");
    ispc_engine_destroy(engine);
    printf("Engine destroyed\n");

    // Test shutdown
    printf("\nShutting down ISPC library...\n");
    ispc_shutdown();
    printf("ISPC library shut down successfully\n");

    printf("\nC API test completed!\n");
    return 0;
}