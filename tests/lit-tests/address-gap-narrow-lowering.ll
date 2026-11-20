; Known address gap of the narrowing of ImproveMemoryOps: it narrows the
; offsets sext(x) * 4 - 4 of a factored gather to the 32-bit form with offsets
; x, scale 4 and delta -4 without checking that x * 4 fits in 32 bits. The
; address of that form then depends on the lowering: for x = 0x20000000 the
; factored gather of builtins/util.m4 sign extends first and computes
; 2^31 - 4, the factored builtin of builtins/generic.ispc multiplies with nsw
; at 32 bits and gets poison.
;
; XFAIL until the offsets are narrowed only where every lowering computes the
; same address.
;
; RUN: %{ispc-opt} --target=sse4-i32x4 --addressing=32 --passes=improve-memory-ops %s -o - | FileCheck %s
; REQUIRES: X86_ENABLED
; XFAIL: *

declare <4 x float> @__pseudo_gather64_float(<4 x i64>, <4 x i32>)
declare <4 x float> @__pseudo_gather_factored_base_offsets32_float(ptr, <4 x i32>, i32, <4 x i32>, <4 x i32>)
declare <4 x float> @__pseudo_gather_factored_base_offsets64_float(ptr, <4 x i64>, i32, <4 x i64>, <4 x i32>)

; CHECK-LABEL: define <4 x float> @lowering_dependent(
; CHECK-NOT: @__pseudo_gather_factored_base_offsets32_float(ptr {{[^,]+}}, <4 x i32> %x, i32 4,
; CHECK: ret
define <4 x float> @lowering_dependent(ptr %base, <4 x i32> %x, <4 x i32> %mask) {
  %b = ptrtoint ptr %base to i64
  %s = insertelement <4 x i64> poison, i64 %b, i32 0
  %bases = shufflevector <4 x i64> %s, <4 x i64> poison, <4 x i32> zeroinitializer
  %xe = sext <4 x i32> %x to <4 x i64>
  %m = mul nsw <4 x i64> %xe, splat (i64 4)
  %off = add nsw <4 x i64> %m, splat (i64 -4)
  %addresses = add <4 x i64> %bases, %off
  %v = call <4 x float> @__pseudo_gather64_float(<4 x i64> %addresses, <4 x i32> %mask)
  ret <4 x float> %v
}
