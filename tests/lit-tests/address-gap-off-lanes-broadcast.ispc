// Known address gap: a gather whose lanes all read one address becomes a
// scalar load that is made even when no lane is on: after a case label that
// no lane reaches, and in a function called from there with every lane off.
//
// XFAIL until the load is made only if a lane is on.
// XFAIL: *
// The kernels and the oracle are in address-gap-off-lanes.inc.

// RUN: %{ispc} --pic --target=sse4-i32x4 -O2 --woff  -h %t-sse4-i32x4.h %S/address-gap-off-lanes.inc -o %t-sse4-i32x4.o
// RUN: %{cc} -O2 -x c -c %S/address-gap-off-lanes.inc -o %t-sse4-i32x4.c.o --include %t-sse4-i32x4.h
// RUN: %{cc} %t-sse4-i32x4.o %t-sse4-i32x4.c.o -o %t-sse4-i32x4.bin
// RUN: %t-sse4-i32x4.bin broadcast_after_case broadcast_in_callee

// REQUIRES: LINUX_HOST && X86_ENABLED && X86_64_HOST
