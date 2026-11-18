// Known address gap, with 64-bit addressing, like
// address-gap-regrouped-wrap.ispc but for a struct field read through a
// varying pointer: split_field_load_i8 reads the field at offset 1 of
// (x + 1) + y + programIndex with x = INT32_MIN and y = -1. The constant
// 1 + programIndex is split off under the sign extension, and the field offset
// is added above it. x + y wraps to INT32_MAX, so the 64-bit sum of the parts
// is 4 GB off. With 32-bit addressing, a load built from the access computes
// the 32-bit sum (address-gaps-ok.ispc).
//
// XFAIL until such a split is proven or rebuilt as the 32-bit sum with
// 64-bit addressing too. See address-gaps.inc for the kernel.

// RUN: %{ispc} --pic --target=sse4-i32x4 -O2 --woff --addressing=64 -h %t-sse4-a64.h %S/address-gaps.inc -o %t-sse4-a64.o
// RUN: %{cc} -O2 -x c -c %S/address-gaps.inc -o %t-sse4-a64.c.o --include %t-sse4-a64.h
// RUN: %{cc} %t-sse4-a64.o %t-sse4-a64.c.o -o %t-sse4-a64.bin
// RUN: %t-sse4-a64.bin split_field_load_i8

// REQUIRES: LINUX_HOST && X86_ENABLED && X86_64_HOST
// XFAIL: *
