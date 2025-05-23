// RUN: %{ispc} --emit-spirv --target=xehpg-x8 --device=acm-g10 %s -o %t.spv
// RUN: ocloc compile -file %t.spv -spirv_input -options "-vc-codegen -Xfinalizer -output" -device acm-g10 -output %t.bin -output_no_suffix
// RUN: FileCheck %s -check-prefix=CHECK_FOO1 -input-file=foo1_kernel.asm
// RUN: FileCheck %s -check-prefix=CHECK_FOO2 -input-file=foo2_kernel.asm
// RUN: FileCheck %s -check-prefix=CHECK_FOO3 -input-file=foo3_kernel.asm

// REQUIRES: XE_ENABLED
// REQUIRES: OCLOC_INSTALLED
// REQUIRES: LINUX_HOST

// CHECK_FOO1-NOT: cmp

// Case 1 : assume() can be used to choose branch/remove
inline uniform int bar1(uniform int a, uniform int b){
    if (a < b)
        return 2;
    return 5;
}
uniform int foo1(uniform int a, uniform int b){
    assume(a < b);
    return bar1(a, b);
}

task void foo1_kernel(uniform int a[], uniform int res[]) {
    res[0] = foo1(a[0], a[1]);
}

// CHECK_FOO2-NOT: cmp
// CHECK_FOO2-NOT: jmpi

// Case 2 : pointer null check removed.
inline void bar2(uniform int * uniform a) {
    if (a != NULL) {
        a[2] = 9;
    }
}
task void foo2_kernel(uniform int a[]) {
    assume(a != NULL);
    bar2(a);
}

// CHECK_FOO3-COUNT-1: _foo3_kernel
// CHECK_FOO3-NOT: _foo3_kernel

// Case 3 : loop : remove remainder loop.
task void foo3_kernel(uniform int a[], uniform int res[], uniform int count) {
    assume(count % programCount == 0);
    foreach (i = 0 ... count) {
        res[i] += a[i];
    }
}
