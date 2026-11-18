// Known address gap, with 64-bit addressing: splitting the constant off a
// signed index regroups its variable part, which can wrap where the original
// index doesn't, and the parts are sign extended and added in 64 bits.
// ImproveMemoryOps splits off such a constant without a proof only if it is
// small (kUnprovenSplitBound): then the variable part can only wrap if the
// index is within that constant of the i32 limits.
//
// split_edge_load_i8: p[(x + 1) + y + programIndex] with x = INT32_MIN and
// y = -1 reads p[INT32_MIN + programIndex]. The constant 1 + programIndex is
// small, but x + y wraps to INT32_MAX. With 32-bit addressing, a load built
// from the access computes the sum of the parts at 32 bits, which is the
// index (address-gaps-ok.ispc). With 64-bit addressing the offsets are 64-bit
// throughout.
//
// XFAIL until such a split is proven or rebuilt as the 32-bit sum with
// 64-bit addressing too. address-gap-regrouped-field.ispc has the same gap
// for a struct field. See address-gaps.inc for the kernel.

// RUN: %{ispc} --pic --target=sse4-i32x4 -O2 --woff --addressing=64 -h %t-sse4-a64.h %S/address-gaps.inc -o %t-sse4-a64.o
// RUN: %{cc} -O2 -x c -c %S/address-gaps.inc -o %t-sse4-a64.c.o --include %t-sse4-a64.h
// RUN: %{cc} %t-sse4-a64.o %t-sse4-a64.c.o -o %t-sse4-a64.bin
// RUN: %t-sse4-a64.bin split_edge_load_i8

// REQUIRES: LINUX_HOST && X86_ENABLED && X86_64_HOST
// XFAIL: *
