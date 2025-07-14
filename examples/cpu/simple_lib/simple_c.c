/*
  Copyright (c) 2025, Intel Corporation
  SPDX-License-Identifier: BSD-3-Clause
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>

#include "ispc/ispc.h"

/* Simple file existence check in C */
int file_exists(const char *filename) {
    struct stat buffer;
    return (stat(filename, &buffer) == 0);
}

int main() {
    /* Initialize ISPC using C API */
    printf("Initializing ISPC using C API...\n");
    if (!ispc_initialize()) {
        fprintf(stderr, "Error: Failed to initialize ISPC\n");
        return 1;
    }

    printf("Compiling simple.ispc using C API...\n");

    /* Set up compilation arguments */
    char *args1[] = {"ispc", "simple.ispc", "--target=host", "-O2", "-o", "simple_ispc_c.o", "-h", "simple_ispc_c.h"};
    int argc1 = sizeof(args1) / sizeof(args1[0]);

    int result = ispc_compile_from_args(argc1, args1);

    if (result == 0) {
        printf("ISPC compilation successful!\n");
    } else {
        fprintf(stderr, "ISPC compilation failed with code: %d\n", result);
        ispc_shutdown();
        return result;
    }

    printf("Compiling simple.ispc with different options using C API...\n");

    /* Set up a second compilation with different options */
    char *args2[] = {"ispc", "simple.ispc", "--target=host", "-O0", "--emit-asm", "-o", "simple_debug_c.s", "-h", "simple_debug_c.h", "-g"};
    int argc2 = sizeof(args2) / sizeof(args2[0]);

    /* Execute second compilation */
    int result2 = ispc_compile_from_args(argc2, args2);

    if (result2 == 0) {
        printf("Second compilation successful with different options\n");

        /* Verify that files with different extensions were created */
        if (file_exists("simple_ispc_c.o") && file_exists("simple_debug_c.s")) {
            printf("SUCCESS: Both compilations produced different output files:\n");
            printf("  - First compilation:  simple_ispc_c.o\n");
            printf("  - Second compilation: simple_debug_c.s\n");
        } else {
            printf("Note: Output files may not be visible in current directory\n");
        }
    } else {
        fprintf(stderr, "Second ISPC compilation failed with code: %d\n", result2);
    }

    /* Test ISPCEngine API from C */
    printf("\nTesting ISPCEngine C API...\n");

    char *engine_args[] = {"ispc", "simple.ispc", "--target=host", "-O2", "-o", "simple_engine_c.o", "-h", "simple_engine_c.h"};
    int engine_argc = sizeof(engine_args) / sizeof(engine_args[0]);

    ispc_engine_t *engine = ispc_engine_create_from_args(engine_argc, engine_args);
    if (!engine) {
        fprintf(stderr, "Failed to create ISPC engine\n");
    } else {
        printf("ISPC engine created successfully\n");

        int engine_result = ispc_engine_execute(engine);
        if (engine_result == 0) {
            printf("Engine execution successful!\n");
            
            /* Check if it's in JIT mode */
            int is_jit = ispc_engine_is_jit_mode(engine);
            printf("Engine JIT mode: %s\n", is_jit ? "enabled" : "disabled");
        } else {
            fprintf(stderr, "Engine execution failed with code: %d\n", engine_result);
        }

        /* Clean up engine */
        ispc_engine_destroy(engine);
        printf("Engine destroyed\n");
    }

    printf("\nCleaning up...\n");
    ispc_shutdown();
    printf("ISPC shutdown complete\n");

    return 0;
}