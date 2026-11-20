; Known compiler crash of GatherCoalescePass: a group of gathers with more
; lanes than the target's (8 on sse4-i32x4) is planned with the target's
; vector types and fails with "Unexpected gather type in lPlanCoalesce"; the
; slot computation would also write past its part of the lane buffer.
;
; XFAIL until such a group is left alone or planned at its own width.
;
; RUN: %{ispc-opt} --target=sse4-i32x4 --passes=gather-coalesce %s -o - | FileCheck %s
; REQUIRES: X86_ENABLED
; XFAIL: *

declare <8 x i32> @__pseudo_gather_factored_base_offsets32_i32(ptr, <8 x i32>, i32, <8 x i32>, <8 x i32>) nounwind memory(read)

; CHECK-LABEL: define <8 x i32> @more_lanes(
; CHECK: ret
define <8 x i32> @more_lanes(ptr %base, i32 %u) {
  %ui = insertelement <8 x i32> poison, i32 %u, i32 0
  %us = shufflevector <8 x i32> %ui, <8 x i32> poison, <8 x i32> zeroinitializer
  %a = call <8 x i32> @__pseudo_gather_factored_base_offsets32_i32(ptr %base, <8 x i32> %us, i32 4, <8 x i32> <i32 0, i32 8, i32 16, i32 24, i32 32, i32 40, i32 48, i32 56>, <8 x i32> splat (i32 -1))
  %b = call <8 x i32> @__pseudo_gather_factored_base_offsets32_i32(ptr %base, <8 x i32> %us, i32 4, <8 x i32> <i32 4, i32 12, i32 20, i32 28, i32 36, i32 44, i32 52, i32 60>, <8 x i32> splat (i32 -1))
  %r = add <8 x i32> %a, %b
  ret <8 x i32> %r
}
