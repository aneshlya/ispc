; ImproveMemoryOps narrows an offset decomposed under the sign extension it
; removes to the Wrapped 32-bit forms, whose offset is the i32 sum of the
; parts, the operand of that extension, so that a load rebuilt from the
; access computes that sum. The declarations of the Wrapped forms are
; created from those of the Extended forms.
;
; RUN: %{ispc-opt} --target=sse4-i32x4 --passes=improve-memory-ops %s -o - | FileCheck %s --check-prefix=SSE4
; RUN: %{ispc-opt} --target=avx2-i32x8 --passes=improve-memory-ops %s -o - | FileCheck %s --check-prefix=AVX2
; REQUIRES: X86_ENABLED

declare <4 x i8> @__pseudo_gather64_i8(<4 x i64>, <4 x i32>)
declare <4 x i8> @__pseudo_gather_factored_base_offsets32_i8(ptr, <4 x i32>, i32, <4 x i32>, <4 x i32>)
declare <4 x i8> @__pseudo_gather_factored_base_offsets64_i8(ptr, <4 x i64>, i32, <4 x i64>, <4 x i32>)
declare <8 x float> @__pseudo_gather64_float(<8 x i64>, <8 x i32>)
declare <8 x float> @__pseudo_gather_base_offsets32_float(ptr, i32, <8 x i32>, <8 x i32>)
declare <8 x float> @__pseudo_gather_base_offsets64_float(ptr, i32, <8 x i64>, <8 x i32>)

; (x + y) + 16 without nsw: the constant is split off under the sext.
; SSE4-LABEL: define <4 x i8> @split_under_sext(
; SSE4: call <4 x i8> @__pseudo_gather_wrapped_factored_base_offsets32_i8(ptr %{{.*}}, <4 x i32> %{{.*}}, i32 1, <4 x i32> %{{.*}}, <4 x i32> %mask)
; SSE4: ret
define <4 x i8> @split_under_sext(ptr %base, <4 x i32> %x, <4 x i32> %y, <4 x i32> %mask) {
  %a = add <4 x i32> %x, %y
  %b = add <4 x i32> %a, splat (i32 16)
  %off = sext <4 x i32> %b to <4 x i64>
  %bi0 = ptrtoint ptr %base to i64
  %bi = insertelement <4 x i64> poison, i64 %bi0, i32 0
  %bs = shufflevector <4 x i64> %bi, <4 x i64> poison, <4 x i32> zeroinitializer
  %addr = add <4 x i64> %bs, %off
  %v = call <4 x i8> @__pseudo_gather64_i8(<4 x i64> %addr, <4 x i32> %mask)
  ret <4 x i8> %v
}

; The scale 8 is taken out of sext(x * 24 + y * 24), whose add may wrap.
; AVX2-LABEL: define <8 x float> @scale_under_sext(
; AVX2: call <8 x float> @__pseudo_gather_wrapped_base_offsets32_float(ptr %{{.*}}, i32 8, <8 x i32> %{{.*}}, <8 x i32> %mask)
; AVX2: ret
define <8 x float> @scale_under_sext(ptr %base, <8 x i32> %x, <8 x i32> %y, <8 x i32> %mask) {
  %a = mul nsw <8 x i32> %x, splat (i32 24)
  %b = mul nsw <8 x i32> %y, splat (i32 24)
  %c = add <8 x i32> %a, %b
  %off = sext <8 x i32> %c to <8 x i64>
  %bi0 = ptrtoint ptr %base to i64
  %bi = insertelement <8 x i64> poison, i64 %bi0, i32 0
  %bs = shufflevector <8 x i64> %bi, <8 x i64> poison, <8 x i32> zeroinitializer
  %addr = add <8 x i64> %bs, %off
  %v = call <8 x float> @__pseudo_gather64_float(<8 x i64> %addr, <8 x i32> %mask)
  ret <8 x float> %v
}

; A constant added above the extension, like a struct field offset, is part
; of the i32 sum of the Wrapped form. Other arithmetic above the extension
; keeps the Extended form, since the i32 sum would truncate it: (x + 16) * 4 in i64 is
; 2 GiB for x = 0x1ffffff0.
; SSE4-LABEL: define <4 x i8> @split_below_add(
; SSE4: call <4 x i8> @__pseudo_gather_wrapped_factored_base_offsets32_i8(
; SSE4: ret
define <4 x i8> @split_below_add(ptr %base, <4 x i32> %x, <4 x i32> %mask) {
  %a = add nsw <4 x i32> %x, splat (i32 16)
  %e = sext <4 x i32> %a to <4 x i64>
  %off = add <4 x i64> %e, splat (i64 32)
  %bi0 = ptrtoint ptr %base to i64
  %bi = insertelement <4 x i64> poison, i64 %bi0, i32 0
  %bs = shufflevector <4 x i64> %bi, <4 x i64> poison, <4 x i32> zeroinitializer
  %addr = add <4 x i64> %bs, %off
  %v = call <4 x i8> @__pseudo_gather64_i8(<4 x i64> %addr, <4 x i32> %mask)
  ret <4 x i8> %v
}

; SSE4-LABEL: define <4 x i8> @split_below_mul(
; SSE4: call <4 x i8> @__pseudo_gather_factored_base_offsets32_i8(ptr %{{.*}}, <4 x i32> %x, i32 4,
; SSE4: ret
define <4 x i8> @split_below_mul(ptr %base, <4 x i32> %x, <4 x i32> %mask) {
  %a = add nsw <4 x i32> %x, splat (i32 16)
  %z = zext nneg <4 x i32> %a to <4 x i64>
  %off = mul nsw <4 x i64> %z, splat (i64 4)
  %bi0 = ptrtoint ptr %base to i64
  %bi = insertelement <4 x i64> poison, i64 %bi0, i32 0
  %bs = shufflevector <4 x i64> %bi, <4 x i64> poison, <4 x i32> zeroinitializer
  %addr = add <4 x i64> %bs, %off
  %v = call <4 x i8> @__pseudo_gather64_i8(<4 x i64> %addr, <4 x i32> %mask)
  ret <4 x i8> %v
}

; SSE4: declare <4 x i8> @__pseudo_gather_wrapped_factored_base_offsets32_i8(ptr{{.*}}, <4 x i32>, i32, <4 x i32>, <4 x i32>)
; AVX2: declare <8 x float> @__pseudo_gather_wrapped_base_offsets32_float(ptr{{.*}}, i32, <8 x i32>, <8 x i32>)
