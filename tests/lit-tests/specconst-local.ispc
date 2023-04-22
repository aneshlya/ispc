// RUN: not %{ispc} %s -o %t.o --nostdlib --target=host --nowrap --enable-specconst --specconst-map /dev/null 2>&1 | FileCheck %s

// CHECK: Specialization constants must be declared globally (for variable "A")

uniform int foo() {
    specconst uniform int A = 10;
    return A;
}
