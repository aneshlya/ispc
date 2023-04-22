// RUN: %{ispc} %s -DFOO -o %t-foo.bc --nostdlib --target=xehpg-x8 --nowrap --enable-specconst --specconst-map %t-foo.specconst 2>&1 | FileCheck %s
// RUN: %{ispc} %s -DBAR -o %t-bar.bc --nostdlib --target=xehpg-x8 --nowrap --enable-specconst --specconst-map %t-bar.specconst 2>&1 | FileCheck %s
// RUN: ${ispc} link -o - %t-foo.bc %t-bar.bc | llvm-dis -o - --show-annotations | FileCheck %s

// REQUIRES: XE_ENABLED
// XFAIL: *

// CHECK-NOT: %A.specconsti32 ={{.*}}call i32 @_Z20__spirv_SpecConstantii(i32 100, i32 10)
// CHECK-NOT: %B.specconsti32 ={{.*}}call i32 @_Z20__spirv_SpecConstantii(i32 100, i32 20)


#ifdef FOO
specconst uniform int A = 10;

task uniform int foo(uniform int C) {
    return A * C;
}
#endif

#ifdef BAR
specconst uniform int B = 20;

task uniform int bar(uniform int C) {
    return B * C;
}
#endif
