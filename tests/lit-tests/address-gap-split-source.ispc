// Known address gaps of the constant split of ImproveMemoryOps, from source
// (sse4-i32x4, where gathers take the factored form):
//
// int64_index:       an int64 index with 32-bit addressing is truncated by
//                    the frontend, so its adds lose nsw; the split gives
//                    shl nsw (trunc w), 1, which overflows, and the load
//                    reads 4 GiB below the array.
// wrapped_constants: with --addressing=64 LLVM folds INT32_MAX + 3 * lane to
//                    lane constants that wrap at 32 bits, and the split sign
//                    extends them: lane 1 reads 16 GiB below the array.
// late_split:        the same index as int64_index reaches a gather of an
//                    inlined function, and the late split (more-const)
//                    splits it after inlining.
//
// XFAIL until the split is exact.
// The kernels and the oracle are in address-gap-off-lanes.inc.

// RUN: %{ispc} --pic --target=sse4-i32x4 -O2 --woff -h %t-sse4.h %S/address-gap-off-lanes.inc -o %t-sse4.o
// RUN: %{cc} -O2 -x c -c %S/address-gap-off-lanes.inc -o %t-sse4.c.o --include %t-sse4.h
// RUN: %{cc} %t-sse4.o %t-sse4.c.o -o %t-sse4.bin
// RUN: %t-sse4.bin int64_index
// RUN: %t-sse4.bin late_split

// RUN: %{ispc} --pic --target=sse4-i32x4 -O2 --woff --addressing=64 -h %t-a64.h %S/address-gap-off-lanes.inc -o %t-a64.o
// RUN: %{cc} -O2 -x c -c %S/address-gap-off-lanes.inc -o %t-a64.c.o --include %t-a64.h
// RUN: %{cc} %t-a64.o %t-a64.c.o -o %t-a64.bin
// RUN: %t-a64.bin wrapped_constants

// REQUIRES: LINUX_HOST && X86_ENABLED && X86_64_HOST
// XFAIL: *
