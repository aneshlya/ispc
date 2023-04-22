// RUN: not %{ispc} %s -o %t.o --nostdlib --target=host --nowrap --enable-specconst --specconst-map /dev/null 2>&1 | FileCheck %s

// CHECK: Specialization constants with varying types or aggregates are not yet supported.

specconst varying int A = 10;
