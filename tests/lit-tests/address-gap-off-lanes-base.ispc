// Known address gap: the factored gathers of util.m4 read the element at
// their base for the lanes that are off, and the base may not be readable
// (here one past the end of the array). Faults on every target whose gather
// is emulated for the element type.
//
// XFAIL until those lanes read only memory known to be readable.
// XFAIL: *
// The kernels and the oracle are in address-gap-off-lanes.inc.

// RUN: %{ispc} --pic --target=sse2-i32x4 -O2 --woff  -h %t-sse2-i32x4.h %S/address-gap-off-lanes.inc -o %t-sse2-i32x4.o
// RUN: %{cc} -O2 -x c -c %S/address-gap-off-lanes.inc -o %t-sse2-i32x4.c.o --include %t-sse2-i32x4.h
// RUN: %{cc} %t-sse2-i32x4.o %t-sse2-i32x4.c.o -o %t-sse2-i32x4.bin
// RUN: %t-sse2-i32x4.bin gather_from_end

// RUN: %{ispc} --pic --target=sse4-i32x4 -O2 --woff  -h %t-sse4-i32x4.h %S/address-gap-off-lanes.inc -o %t-sse4-i32x4.o
// RUN: %{cc} -O2 -x c -c %S/address-gap-off-lanes.inc -o %t-sse4-i32x4.c.o --include %t-sse4-i32x4.h
// RUN: %{cc} %t-sse4-i32x4.o %t-sse4-i32x4.c.o -o %t-sse4-i32x4.bin
// RUN: %t-sse4-i32x4.bin gather_from_end

// RUN: %{ispc} --pic --target=avx2-i8x32 -O2 --woff  -h %t-avx2-i8x32.h %S/address-gap-off-lanes.inc -o %t-avx2-i8x32.o
// RUN: %{cc} -O2 -x c -c %S/address-gap-off-lanes.inc -o %t-avx2-i8x32.c.o --include %t-avx2-i8x32.h
// RUN: %{cc} %t-avx2-i8x32.o %t-avx2-i8x32.c.o -o %t-avx2-i8x32.bin
// RUN: %t-avx2-i8x32.bin gather_from_end

// REQUIRES: LINUX_HOST && X86_ENABLED && X86_64_HOST
