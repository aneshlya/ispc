; Known address gap of the scale extraction of ImproveMemoryOps: a partial
; scale (24 = 8 * 3) is pulled out of a multiply by dividing the splat
; multiplier by the scale. For an int8 or int16 index under a sext, as in
; a[i * (int16)24], the divisor is built as an i64 vector, and the sdiv of an
; i16 splat by it hits an LLVM assertion (without assertions, invalid IR).
; The checks are the right result: the division in the type of the
; multiplier.
;
; XFAIL until the scale is divided in the type of the multiplier.
;
; RUN: %{ispc-opt} --target=avx2-i32x8 --passes=improve-memory-ops %s -o - | FileCheck %s
; REQUIRES: X86_ENABLED
; XFAIL: *

declare <8 x i8> @__pseudo_gather64_i8(<8 x i64>, <8 x i32>)
declare <8 x i8> @__pseudo_gather_base_offsets64_i8(ptr, i32, <8 x i64>, <8 x i32>)
declare <8 x i8> @__pseudo_gather_base_offsets32_i8(ptr, i32, <8 x i32>, <8 x i32>)

; CHECK-LABEL: define <8 x i8> @partial_scale_i16(
; CHECK: [[DIV:%.*]] = sdiv <8 x i16> splat (i16 24), splat (i16 8)
; CHECK: [[MUL:%.*]] = mul <8 x i16> [[DIV]], %x
; CHECK: [[EXT:%.*]] = sext <8 x i16> [[MUL]] to <8 x i64>
; CHECK: call <8 x i8> @__pseudo_gather_base_offsets64_i8(ptr {{[^,]+}}, i32 8, <8 x i64> [[EXT]], <8 x i32> %mask)
; CHECK: ret
define <8 x i8> @partial_scale_i16(ptr %base, <8 x i16> %x, <8 x i32> %mask) {
  %b = ptrtoint ptr %base to i64
  %s = insertelement <8 x i64> poison, i64 %b, i32 0
  %bases = shufflevector <8 x i64> %s, <8 x i64> poison, <8 x i32> zeroinitializer
  %m = mul nsw <8 x i16> %x, splat (i16 24)
  %off = sext <8 x i16> %m to <8 x i64>
  %addresses = add <8 x i64> %bases, %off
  %v = call <8 x i8> @__pseudo_gather64_i8(<8 x i64> %addresses, <8 x i32> %mask)
  ret <8 x i8> %v
}

; CHECK-LABEL: define <8 x i8> @partial_scale_i8(
; CHECK: [[DIV:%.*]] = sdiv <8 x i8> splat (i8 12), splat (i8 4)
; CHECK: [[MUL:%.*]] = mul <8 x i8> [[DIV]], %x
; CHECK: [[EXT:%.*]] = sext <8 x i8> [[MUL]] to <8 x i64>
; CHECK: call <8 x i8> @__pseudo_gather_base_offsets64_i8(ptr {{[^,]+}}, i32 4, <8 x i64> [[EXT]], <8 x i32> %mask)
; CHECK: ret
define <8 x i8> @partial_scale_i8(ptr %base, <8 x i8> %x, <8 x i32> %mask) {
  %b = ptrtoint ptr %base to i64
  %s = insertelement <8 x i64> poison, i64 %b, i32 0
  %bases = shufflevector <8 x i64> %s, <8 x i64> poison, <8 x i32> zeroinitializer
  %m = mul nsw <8 x i8> %x, splat (i8 12)
  %off = sext <8 x i8> %m to <8 x i64>
  %addresses = add <8 x i64> %bases, %off
  %v = call <8 x i8> @__pseudo_gather64_i8(<8 x i64> %addresses, <8 x i32> %mask)
  ret <8 x i8> %v
}

; The division is signed: -24 / 8 = -3.
; CHECK-LABEL: define <8 x i8> @partial_scale_negative(
; CHECK: [[DIV:%.*]] = sdiv <8 x i16> splat (i16 -24), splat (i16 8)
; CHECK: [[MUL:%.*]] = mul <8 x i16> [[DIV]], %x
; CHECK: [[EXT:%.*]] = sext <8 x i16> [[MUL]] to <8 x i64>
; CHECK: call <8 x i8> @__pseudo_gather_base_offsets64_i8(ptr {{[^,]+}}, i32 8, <8 x i64> [[EXT]], <8 x i32> %mask)
; CHECK: ret
define <8 x i8> @partial_scale_negative(ptr %base, <8 x i16> %x, <8 x i32> %mask) {
  %b = ptrtoint ptr %base to i64
  %s = insertelement <8 x i64> poison, i64 %b, i32 0
  %bases = shufflevector <8 x i64> %s, <8 x i64> poison, <8 x i32> zeroinitializer
  %m = mul nsw <8 x i16> %x, splat (i16 -24)
  %off = sext <8 x i16> %m to <8 x i64>
  %addresses = add <8 x i64> %bases, %off
  %v = call <8 x i8> @__pseudo_gather64_i8(<8 x i64> %addresses, <8 x i32> %mask)
  ret <8 x i8> %v
}
