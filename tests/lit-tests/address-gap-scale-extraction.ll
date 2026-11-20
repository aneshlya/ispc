; Known address gaps of the scale extraction of ImproveMemoryOps: it pulls a
; scale of 2, 4 or 8 out of the offsets of a gather as if the multiply it
; came from didn't change under the extension around it. Each case has its
; own prefix:
;
; WRAP:  sext(x * 4) with a multiply that wraps at 32 bits (--wrap-signed-int
;        or an unsigned index) is 4 * j for x = 2^30 + j, but the extracted
;        form sext(x) * 4 is 2^32 + 4 * j.
; WIDE:  an i64 multiplier that doesn't fit in 32 bits is read through an
;        int: 2^34 + 8 is taken for a scale of 8 and the multiply is dropped
;        (from source: a[i * 4294967298] with an int64 i and --addressing=64).
; TRUNC: sext(trunc(i * (2^32 + 8))) is 8 for i = 1, but a scale of 8 pulled
;        out of the trunc gives 2^32 + 8.
;
; XFAIL until the scale is pulled out only where the address stays the same.
;
; RUN: %{ispc-opt} --target=avx2-i32x8 --passes=improve-memory-ops %s -o - | FileCheck %s --check-prefix=WRAP
; RUN: %{ispc-opt} --target=avx2-i32x8 --passes=improve-memory-ops %s -o - | FileCheck %s --check-prefix=WIDE
; RUN: %{ispc-opt} --target=avx2-i32x8 --passes=improve-memory-ops %s -o - | FileCheck %s --check-prefix=TRUNC
; REQUIRES: X86_ENABLED
; XFAIL: *

declare <8 x i8> @__pseudo_gather64_i8(<8 x i64>, <8 x i32>)
declare <8 x i8> @__pseudo_gather_base_offsets64_i8(ptr, i32, <8 x i64>, <8 x i32>)
declare <8 x i8> @__pseudo_gather_base_offsets32_i8(ptr, i32, <8 x i32>, <8 x i32>)

; WRAP-LABEL: define <8 x i8> @wrapping_multiply(
; WRAP-NOT: i32 4, <8 x i32> %x
; WRAP: call <8 x i8> @__pseudo_gather_base_offsets{{32|64}}_i8(ptr {{[^,]+}}, i32 1,
; WRAP: ret
define <8 x i8> @wrapping_multiply(ptr %base, <8 x i32> %x, <8 x i32> %mask) {
  %b = ptrtoint ptr %base to i64
  %s = insertelement <8 x i64> poison, i64 %b, i32 0
  %bases = shufflevector <8 x i64> %s, <8 x i64> poison, <8 x i32> zeroinitializer
  %m = mul <8 x i32> %x, splat (i32 4)
  %off = sext <8 x i32> %m to <8 x i64>
  %addresses = add <8 x i64> %bases, %off
  %v = call <8 x i8> @__pseudo_gather64_i8(<8 x i64> %addresses, <8 x i32> %mask)
  ret <8 x i8> %v
}

; WIDE-LABEL: define <8 x i8> @wide_multiplier(
; WIDE-NOT: i32 8, <8 x i64> %idx
; WIDE: call <8 x i8> @__pseudo_gather_base_offsets64_i8(
; WIDE: ret
define <8 x i8> @wide_multiplier(ptr %base, <8 x i64> %idx, <8 x i32> %mask) {
  %b = ptrtoint ptr %base to i64
  %s = insertelement <8 x i64> poison, i64 %b, i32 0
  %bases = shufflevector <8 x i64> %s, <8 x i64> poison, <8 x i32> zeroinitializer
  %off = mul <8 x i64> %idx, splat (i64 17179869192)
  %addresses = add <8 x i64> %bases, %off
  %v = call <8 x i8> @__pseudo_gather64_i8(<8 x i64> %addresses, <8 x i32> %mask)
  ret <8 x i8> %v
}

; TRUNC-LABEL: define <8 x i8> @trunc_scale(
; TRUNC-NOT: i32 8,
; TRUNC: call <8 x i8> @__pseudo_gather_base_offsets32_i8(ptr {{[^,]+}}, i32 1, <8 x i32> %t, <8 x i32> %mask)
; TRUNC: ret
define <8 x i8> @trunc_scale(ptr %base, <8 x i64> %idx, <8 x i32> %mask) {
  %b = ptrtoint ptr %base to i64
  %s = insertelement <8 x i64> poison, i64 %b, i32 0
  %bases = shufflevector <8 x i64> %s, <8 x i64> poison, <8 x i32> zeroinitializer
  %m = mul <8 x i64> %idx, splat (i64 4294967304)
  %t = trunc <8 x i64> %m to <8 x i32>
  %off = sext <8 x i32> %t to <8 x i64>
  %addresses = add <8 x i64> %bases, %off
  %v = call <8 x i8> @__pseudo_gather64_i8(<8 x i64> %addresses, <8 x i32> %mask)
  ret <8 x i8> %v
}
