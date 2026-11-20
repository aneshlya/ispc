; Known compiler crash of the broadcast of ImproveMemoryOps: offsets whose
; lanes 1 to 3 are one value and whose lane 0 is undefined (an insert chain
; without lane 0) count as all equal, and the broadcast takes the offset of
; lane 0, which isn't there: an assertion of llvmutil.cpp fails. The lane is
; off, so its offset may be anything.
;
; XFAIL until the broadcast takes its address from a defined lane that is on.
;
; RUN: %{ispc-opt} --target=sse4-i32x4 --passes=improve-memory-ops %s -o - | FileCheck %s
; REQUIRES: X86_ENABLED
; XFAIL: *

declare <4 x float> @__pseudo_gather_factored_base_offsets32_float(ptr, <4 x i32>, i32, <4 x i32>, <4 x i32>)

; CHECK-LABEL: define <4 x float> @undef_lane0(
; CHECK: ret
define <4 x float> @undef_lane0(ptr %base, i32 %o) {
  %i1 = insertelement <4 x i32> poison, i32 %o, i32 1
  %i2 = insertelement <4 x i32> %i1, i32 %o, i32 2
  %i3 = insertelement <4 x i32> %i2, i32 %o, i32 3
  %v = call <4 x float> @__pseudo_gather_factored_base_offsets32_float(ptr %base, <4 x i32> %i3, i32 1, <4 x i32> zeroinitializer, <4 x i32> <i32 0, i32 -1, i32 -1, i32 -1>)
  ret <4 x float> %v
}
