// Known address gap of GatherCoalescePass: it moves the loads of a group to
// the first gather, and only an instruction that may write memory stops the
// group. memory_barrier() calls a builtin declared readnone, so the reads of
// y, after the barrier, are made before it: another thread's stores that the
// barrier orders before them may not be seen.
//
// XFAIL until reads aren't moved across a barrier.

// RUN: %{ispc} %s --target=sse4-i32x4 -O2 --woff --emit-llvm-text -o - | FileCheck %s

// REQUIRES: X86_ENABLED
// XFAIL: *

// The coalesced load of the x and y elements, <8 x i32>, comes before the
// fence today; the x elements alone are 4 elements apart, too far for one load.
// CHECK-LABEL: define void @barrier_between(
// CHECK-NOT: load <8 x i32>
// CHECK: fence seq_cst
struct P {
    float x, y;
};
export void barrier_between(uniform P a[], uniform float out[]) {
    foreach (i = 0 ... programCount) {
        float x = a[2 * i].x;
        memory_barrier();
        float y = a[2 * i].y;
        out[i] = x + y;
    }
}
