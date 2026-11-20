// The cases of address-gap-off-lanes.inc on configurations that compile
// them correctly today: the hardware gathers of avx2-i32x8 don't take the
// rewrites that are wrong in address-gap-off-lanes-broadcast.ispc and
// address-gap-split-source.ispc, and the coalescer assembles 16 lanes right.
// This checks the oracle.
// The kernels and the oracle are in address-gap-off-lanes.inc.

// RUN: %{ispc} --pic --target=avx2-i32x8 -O2 --woff  -h %t-avx2-i32x8.h %S/address-gap-off-lanes.inc -o %t-avx2-i32x8.o
// RUN: %{cc} -O2 -x c -c %S/address-gap-off-lanes.inc -o %t-avx2-i32x8.c.o --include %t-avx2-i32x8.h
// RUN: %{cc} %t-avx2-i32x8.o %t-avx2-i32x8.c.o -o %t-avx2-i32x8.bin
// RUN: %t-avx2-i32x8.bin broadcast_after_case broadcast_in_callee int64_index late_split

// RUN: %{ispc} --pic --target=avx2-i16x16 -O2 --woff --opt=disable-gathers -h %t-x16.h %S/address-gap-off-lanes.inc -o %t-x16.o
// RUN: %{cc} -O2 -x c -c %S/address-gap-off-lanes.inc -o %t-x16.c.o --include %t-x16.h
// RUN: %{cc} %t-x16.o %t-x16.c.o -o %t-x16.bin
// RUN: %t-x16.bin coalesce_stride2

// RUN: %{ispc} --pic --target=avx2-i32x8 -O2 --woff --addressing=64 -h %t-a64.h %S/address-gap-off-lanes.inc -o %t-a64.o
// RUN: %{cc} -O2 -x c -c %S/address-gap-off-lanes.inc -o %t-a64.c.o --include %t-a64.h
// RUN: %{cc} %t-a64.o %t-a64.c.o -o %t-a64.bin
// RUN: %t-a64.bin wrapped_constants

// REQUIRES: LINUX_HOST && X86_ENABLED && X86_64_HOST
