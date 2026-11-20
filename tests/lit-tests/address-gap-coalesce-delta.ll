; Known compiler crash of GatherCoalescePass: a delta that is neither a
; constant data vector nor zero (here an instruction) reaches an assertion
; when the slots are computed, even for a group of one gather.
;
; XFAIL until such a gather is left alone or its delta is handled.
;
; RUN: %{ispc-opt} --target=sse4-i32x4 --passes=gather-coalesce %s -o - | FileCheck %s
; REQUIRES: X86_ENABLED
; XFAIL: *

declare <4 x i32> @__pseudo_gather_factored_base_offsets32_i32(ptr, <4 x i32>, i32, <4 x i32>, <4 x i32>) nounwind memory(read)

; CHECK-LABEL: define <4 x i32> @delta_expr(
; CHECK: ret
define <4 x i32> @delta_expr(ptr %base, i32 %u, <4 x i32> %d) {
  %ui = insertelement <4 x i32> poison, i32 %u, i32 0
  %us = shufflevector <4 x i32> %ui, <4 x i32> poison, <4 x i32> zeroinitializer
  %dd = add <4 x i32> %d, <i32 0, i32 8, i32 16, i32 24>
  %a = call <4 x i32> @__pseudo_gather_factored_base_offsets32_i32(ptr %base, <4 x i32> %us, i32 4, <4 x i32> %dd, <4 x i32> splat (i32 -1))
  ret <4 x i32> %a
}
