; Known address gaps of GatherCoalescePass. Each case has its own prefix:
;
; SLOTS:   the slots are absolute byte offsets divided by the element size,
;          not distances: with the deltas 3, 11, 19, 27 and 7, 15, 23, 31 of
;          two i32 gathers, the loads start at base + 4 * u, 3 bytes before
;          the first element read, instead of at base + 4 * u + 3.
; PRODUCT: the shared pointer is base + u * scale computed at the offset
;          width: for u = 0x20000000 and scale 4 the gathers read
;          base + 2^31 (the factored gather of builtins/util.m4 sign extends
;          u first), the loads base - 2^31.
; CALL:    the loads are made at the first gather, before a call that doesn't
;          write memory but may not return (no willreturn), so they may be
;          made when the program doesn't make them.
;
; XFAIL until the loads read what the gathers read, where they read it.
;
; RUN: %{ispc-opt} --target=sse4-i32x4 --passes=gather-coalesce %s -o - | FileCheck %s --check-prefix=SLOTS
; RUN: %{ispc-opt} --target=sse4-i32x4 --passes=gather-coalesce %s -o - | FileCheck %s --check-prefix=PRODUCT
; RUN: %{ispc-opt} --target=sse4-i32x4 --passes=gather-coalesce %s -o - | FileCheck %s --check-prefix=CALL
; REQUIRES: X86_ENABLED
; XFAIL: *

declare <4 x i32> @__pseudo_gather_factored_base_offsets32_i32(ptr, <4 x i32>, i32, <4 x i32>, <4 x i32>) nounwind memory(read)
declare void @opaque() nounwind memory(none)

; SLOTS-LABEL: define <4 x i32> @slots(
; SLOTS: getelementptr i8, ptr {{%[^,]+}}, i64 3
; SLOTS: ret
define <4 x i32> @slots(ptr %base, i32 %u) {
  %ui = insertelement <4 x i32> poison, i32 %u, i32 0
  %us = shufflevector <4 x i32> %ui, <4 x i32> poison, <4 x i32> zeroinitializer
  %a = call <4 x i32> @__pseudo_gather_factored_base_offsets32_i32(ptr %base, <4 x i32> %us, i32 4, <4 x i32> <i32 3, i32 11, i32 19, i32 27>, <4 x i32> splat (i32 -1))
  %b = call <4 x i32> @__pseudo_gather_factored_base_offsets32_i32(ptr %base, <4 x i32> %us, i32 4, <4 x i32> <i32 7, i32 15, i32 23, i32 31>, <4 x i32> splat (i32 -1))
  %r = add <4 x i32> %a, %b
  ret <4 x i32> %r
}

; PRODUCT-LABEL: define <4 x i32> @product(
; PRODUCT-NOT: mul i32 %u, 4
; PRODUCT: ret
define <4 x i32> @product(ptr %base, i32 %u) {
  %ui = insertelement <4 x i32> poison, i32 %u, i32 0
  %us = shufflevector <4 x i32> %ui, <4 x i32> poison, <4 x i32> zeroinitializer
  %a = call <4 x i32> @__pseudo_gather_factored_base_offsets32_i32(ptr %base, <4 x i32> %us, i32 4, <4 x i32> <i32 0, i32 8, i32 16, i32 24>, <4 x i32> splat (i32 -1))
  %b = call <4 x i32> @__pseudo_gather_factored_base_offsets32_i32(ptr %base, <4 x i32> %us, i32 4, <4 x i32> <i32 4, i32 12, i32 20, i32 28>, <4 x i32> splat (i32 -1))
  %r = add <4 x i32> %a, %b
  ret <4 x i32> %r
}

; CALL-LABEL: define <4 x i32> @call_between(
; CALL: call void @opaque()
; CALL: {{load|call <4 x i32> @__pseudo_gather}}
; CALL: ret
define <4 x i32> @call_between(ptr %base, i32 %u) {
  %ui = insertelement <4 x i32> poison, i32 %u, i32 0
  %us = shufflevector <4 x i32> %ui, <4 x i32> poison, <4 x i32> zeroinitializer
  %a = call <4 x i32> @__pseudo_gather_factored_base_offsets32_i32(ptr %base, <4 x i32> %us, i32 4, <4 x i32> <i32 0, i32 8, i32 16, i32 24>, <4 x i32> splat (i32 -1))
  call void @opaque()
  %b = call <4 x i32> @__pseudo_gather_factored_base_offsets32_i32(ptr %base, <4 x i32> %us, i32 4, <4 x i32> <i32 4, i32 12, i32 20, i32 28>, <4 x i32> splat (i32 -1))
  %r = add <4 x i32> %a, %b
  ret <4 x i32> %r
}
