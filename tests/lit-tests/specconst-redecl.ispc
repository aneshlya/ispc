// RUN: not %{ispc} %s -o %t.o --nostdlib --target=host --nowrap --enable-specconst --specconst-map /dev/null 2>&1 | FileCheck %s

// CHECK: Redefinition of variable "A" is illegal.

specconst uniform uint32 A = 30;
specconst uniform uint32 A = 10;
