// RUN: not %{ispc} %s -o %t.o --nostdlib --target=host --nowrap --enable-specconst --specconst-map /dev/null 2>&1 | FileCheck %s

// CHECK: Initializer for global variable "B" must be a constant.

const uniform int A = 10;
specconst uniform uint64 B = (uint64)&A;
