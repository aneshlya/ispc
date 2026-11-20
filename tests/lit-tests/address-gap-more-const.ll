; Known address gap of the late constant split (more-const) of
; ImproveMemoryOps: it moves the constant of the offsets x + 0x40000000 of a
; factored gather with scale 4 into the delta as 0x40000000 * 4, computed at
; 32 bits, which is 0. The factored gather of builtins/util.m4 sign extends
; the offsets before it scales them, so the call reads 2^32 bytes from the
; base for x = 0, and after the split it reads at the base.
;
; XFAIL until c * scale is moved to the delta only where it fits.
;
; RUN: %{ispc-opt} --target=sse4-i32x4 --addressing=32 --passes=improve-memory-ops %s -o - | FileCheck %s
; REQUIRES: X86_ENABLED
; XFAIL: *

declare <4 x float> @__pseudo_gather_factored_base_offsets32_float(ptr, <4 x i32>, i32, <4 x i32>, <4 x i32>)
declare <4 x float> @__pseudo_gather_factored_base_offsets64_float(ptr, <4 x i64>, i32, <4 x i64>, <4 x i32>)

; CHECK-LABEL: define <4 x float> @wrapped_delta(
; CHECK-NOT: @__pseudo_gather_factored_base_offsets32_float(ptr %base, <4 x i32> %x, i32 4,
; CHECK: ret
define <4 x float> @wrapped_delta(ptr %base, <4 x i32> %x, <4 x i32> %mask) {
  %o = add nsw <4 x i32> %x, splat (i32 1073741824)
  %v = call <4 x float> @__pseudo_gather_factored_base_offsets32_float(ptr %base, <4 x i32> %o, i32 4, <4 x i32> zeroinitializer, <4 x i32> %mask)
  ret <4 x float> %v
}
