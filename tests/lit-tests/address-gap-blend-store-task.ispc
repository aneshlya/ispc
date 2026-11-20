// Known wrong code: a masked store to a stack object is lowered to a load,
// blend and store of the whole vector even after the object's address was
// passed to a launched task. The store writes back the lanes that are off,
// which the task may be writing at the same time: the program has no race
// (the task writes the lanes the caller leaves off), the code has one.
//
// XFAIL until the blend is used only for objects no other task can access.

// RUN: %{ispc} %s --target=avx2-i32x8 -O2 --woff --emit-llvm-text -o - | FileCheck %s

// REQUIRES: X86_ENABLED
// XFAIL: *

// CHECK-LABEL: define void @shared_with_task(
// CHECK: call void @ISPCLaunch(
// CHECK-NOT: load <8 x {{i32|float}}>, ptr %a
// CHECK: @ISPCSync

task void writer(uniform int *uniform p, uniform int n) {
    for (uniform int i = n; i < programCount; i++)
        p[i] = 7;
}

export void shared_with_task(uniform int n, uniform int out[]) {
    varying int a = 0;
    launch writer((uniform int *)&a, n);
    if (programIndex < n)
        a = 1;
    sync;
    out[programIndex] = a;
}
