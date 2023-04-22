// RUN: not %{ispc} %s -o %t.o --nostdlib --target=host --nowrap --enable-specconst --specconst-map /dev/null 2>&1 | FileCheck %s

// CHECK: Can't assign to type "specconst uniform unsigned int32" on left-hand side of expression

specconst uniform uint32 A = 30;
void assign() { A = 10; }
