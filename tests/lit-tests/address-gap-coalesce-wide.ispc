// Known wrong code: GatherCoalescePass assembles the result of every gather
// of a group after the first from the wrong loads at 32 and 64 lanes. Groups
// of that width appear with --opt=disable-gathers on avx2-i8x32.
//
// XFAIL until the assembly is right at every width.
// XFAIL: *
// The kernels and the oracle are in address-gap-off-lanes.inc.

// RUN: %{ispc} --pic --target=avx2-i8x32 -O2 --woff --opt=disable-gathers -h %t.h %S/address-gap-off-lanes.inc -o %t.o
// RUN: %{cc} -O2 -x c -c %S/address-gap-off-lanes.inc -o %t.c.o --include %t.h
// RUN: %{cc} %t.o %t.c.o -o %t.bin
// RUN: %t.bin coalesce_stride2

// REQUIRES: LINUX_HOST && X86_ENABLED && X86_64_HOST
