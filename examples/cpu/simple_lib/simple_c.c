/*
  Copyright (c) 2025, Intel Corporation

  SPDX-License-Identifier: BSD-3-Clause
*/

#include <stdio.h>
#include <stdlib.h>

/* Note: We can't include ispc/ispc.h in pure C mode since it's C++ only on this branch */

int main() {
    printf("Note: This example requires the C API interface.\n");
    printf("Please use the c-interface-only branch to test the C API.\n");
    printf("This global-state-management branch focuses on RAII fixes for C++ interface.\n");
    
    return 0;
}