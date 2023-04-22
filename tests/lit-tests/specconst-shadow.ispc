// RUN: %{ispc} %s -o %t.o --nostdlib --target=host --nowrap --enable-specconst --specconst-map /dev/null 2>&1 | FileCheck %s
// RUN: %{ispc} %s -o - --emit-llvm-text --nostdlib --target=host --nowrap --enable-specconst --specconst-map /dev/null | FileCheck %s --check-prefix=CHECK_LLVMIR

// Is warning only
// CHECK-COUNT-1: Symbol "A" shadows symbol declared in outer scope.

// CHECK_LLVMIR-LABEL:  @foo
// CHECK_LLVMIR-NOT:    __spirv_SpecConstant

specconst uniform int A = 10;

uniform int foo(uniform int B) {
    uniform int A = B << 4;
    return A;
}

uniform int64 bar(uniform int64 B) {
#pragma ignore warning
    uniform int A = B << 10;
    return A;
}
