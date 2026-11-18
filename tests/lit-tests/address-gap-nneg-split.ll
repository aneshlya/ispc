; nneg constrains the result of the add, not its operands. For x = INT32_MIN,
; add i32 x, INT32_MIN wraps to zero, so the original offset is defined and
; zero. Splitting it into sext(x) + sext(INT32_MIN) gives -2^32.
; Unlike address-gap-minus-const.ispc, this exercises zext nneg, not sext.
; Accept either the original 64-bit %off or the 32-bit %sum itself: nneg
; bounds the sum below 2^31. Tie the call operand to those values so dropping
; the constant and passing %x cannot satisfy the check.
;
; With 32-bit addressing, a zext nneg can narrow to the 32-bit factored form;
; then the constant must not be split off the exposed i32 sum later.
;
; RUN: %{ispc-opt} --target=sse4-i32x4 --addressing=64 --passes=improve-memory-ops %s -o - | FileCheck %s
; RUN: %{ispc-opt} --target=sse4-i32x4 --addressing=32 --passes=improve-memory-ops %s -o - | FileCheck %s --check-prefix=A32
; REQUIRES: X86_ENABLED

declare <4 x i8> @__pseudo_gather64_i8(<4 x i64>, <4 x i32>)
declare <4 x i64> @__pseudo_gather64_i64(<4 x i64>, <4 x i32>)
declare <4 x i64> @__pseudo_gather_base_offsets32_i64(ptr, i32, <4 x i32>, <4 x i32>)
declare <4 x i64> @__pseudo_gather_base_offsets64_i64(ptr, i32, <4 x i64>, <4 x i32>)
declare <4 x i64> @__pseudo_gather_factored_base_offsets32_i64(ptr, <4 x i32>, i32, <4 x i32>, <4 x i32>)
declare <4 x i64> @__pseudo_gather_factored_base_offsets64_i64(ptr, <4 x i64>, i32, <4 x i64>, <4 x i32>)
declare <4 x i8> @__pseudo_gather_base_offsets32_i8(ptr, i32, <4 x i32>, <4 x i32>)
declare <4 x i8> @__pseudo_gather_base_offsets64_i8(ptr, i32, <4 x i64>, <4 x i32>)
declare <4 x i8> @__pseudo_gather_factored_base_offsets64_i8(ptr, <4 x i64>, i32, <4 x i64>, <4 x i32>)
declare <4 x i8> @__pseudo_gather_factored_base_offsets32_i8(ptr, <4 x i32>, i32, <4 x i32>, <4 x i32>)
declare <4 x float> @__pseudo_gather_factored_base_offsets32_float(ptr, <4 x i32>, i32, <4 x i32>, <4 x i32>)
declare <4 x float> @__pseudo_gather_factored_base_offsets64_float(ptr, <4 x i64>, i32, <4 x i64>, <4 x i32>)
declare <4 x i8> @__masked_load_i8(ptr, <4 x i32>)

; CHECK-LABEL: define <4 x i8> @nneg_sum(
; CHECK: %sum = add <4 x i32> %x,
; CHECK: %off = zext nneg <4 x i32> %sum to <4 x i64>
; CHECK-NOT: sext <4 x i32> %x
; CHECK-NOT: sext <4 x i32> {{splat \(i32 -2147483648\)|<i32 -2147483648}}
; CHECK: call <4 x i8> @__pseudo_gather_factored_base_offsets{{32_i8\(ptr %base, <4 x i32> %sum, i32 1, <4 x i32>|64_i8\(ptr %base, <4 x i64> %off, i32 1, <4 x i64>}} zeroinitializer, <4 x i32> %mask)
; CHECK-NOT: sext <4 x i32> %x
; CHECK: ret
define <4 x i8> @nneg_sum(ptr %base, <4 x i32> %x, <4 x i32> %mask) {
  %sum = add <4 x i32> %x, <i32 -2147483648, i32 -2147483648, i32 -2147483648, i32 -2147483648>
  %off = zext nneg <4 x i32> %sum to <4 x i64>
  %v = call <4 x i8> @__pseudo_gather_factored_base_offsets64_i8(ptr %base, <4 x i64> %off, i32 1, <4 x i64> zeroinitializer, <4 x i32> %mask)
  ret <4 x i8> %v
}

; With an nsw add, the sum can't wrap, so the parts are sign extended and the
; constant becomes the delta.
; CHECK-LABEL: define <4 x i8> @nneg_nsw_sum(
; CHECK: %[[C:.*]] = sext <4 x i32> splat (i32 16) to <4 x i64>
; CHECK: %[[X:.*]] = sext <4 x i32> %x to <4 x i64>
; CHECK: %[[C1:.*]] = mul <4 x i64> %[[C]], splat (i64 1)
; CHECK: %[[D:.*]] = add <4 x i64> %[[C1]], zeroinitializer
; CHECK: call <4 x i8> @__pseudo_gather_factored_base_offsets64_i8(ptr %base, <4 x i64> %[[X]], i32 1, <4 x i64> %[[D]], <4 x i32> %mask)
; CHECK: ret
define <4 x i8> @nneg_nsw_sum(ptr %base, <4 x i32> %x, <4 x i32> %mask) {
  %sum = add nsw <4 x i32> %x, <i32 16, i32 16, i32 16, i32 16>
  %off = zext nneg <4 x i32> %sum to <4 x i64>
  %v = call <4 x i8> @__pseudo_gather_factored_base_offsets64_i8(ptr %base, <4 x i64> %off, i32 1, <4 x i64> zeroinitializer, <4 x i32> %mask)
  ret <4 x i8> %v
}

; A trunc is split modulo 2^32: for an i64 y = INT32_MIN the original offset
; is zero, but sign extending trunc(y) and trunc(INT32_MIN) gives -2^32.
; CHECK-LABEL: define <4 x i8> @nneg_trunc_sum(
; CHECK-NOT: sext
; CHECK: call <4 x i8> @__pseudo_gather_factored_base_offsets64_i8(ptr %base, <4 x i64> %off, i32 1, <4 x i64> zeroinitializer, <4 x i32> %mask)
; CHECK: ret
define <4 x i8> @nneg_trunc_sum(ptr %base, <4 x i64> %y, <4 x i32> %mask) {
  %s64 = add nsw <4 x i64> %y, splat (i64 -2147483648)
  %sum = trunc <4 x i64> %s64 to <4 x i32>
  %off = zext nneg <4 x i32> %sum to <4 x i64>
  %v = call <4 x i8> @__pseudo_gather_factored_base_offsets64_i8(ptr %base, <4 x i64> %off, i32 1, <4 x i64> zeroinitializer, <4 x i32> %mask)
  ret <4 x i8> %v
}

; Narrowing would expose the i32 sum to a later split, so the offset stays
; the whole 64-bit %off, with a zero delta.
; A32-LABEL: define <4 x i8> @narrowed_nneg_sum(
; A32-NOT: sext <4 x i32> %x
; A32: [[S:%.*]] = add <4 x i32> %x, splat (i32 -2147483648)
; A32: [[O:%.*]] = zext nneg <4 x i32> [[S]] to <4 x i64>
; A32: call <4 x i8> @__pseudo_gather_factored_base_offsets64_i8(ptr %{{.*}}, <4 x i64> [[O]], i32 1, <4 x i64> zeroinitializer, <4 x i32> %mask)
; A32: ret
define <4 x i8> @narrowed_nneg_sum(ptr %base, <4 x i32> %x, <4 x i32> %mask) {
  %sum = add <4 x i32> %x, splat (i32 -2147483648)
  %off = zext nneg <4 x i32> %sum to <4 x i64>
  %baseint = ptrtoint ptr %base to i64
  %bi = insertelement <4 x i64> poison, i64 %baseint, i32 0
  %bases = shufflevector <4 x i64> %bi, <4 x i64> poison, <4 x i32> zeroinitializer
  %addr = add <4 x i64> %bases, %off
  %v = call <4 x i8> @__pseudo_gather64_i8(<4 x i64> %addr, <4 x i32> %mask)
  ret <4 x i8> %v
}

; nsw holds for the original grouping, not for the regrouped constant: for
; x = INT32_MIN, (x + 0x40000000) + 0x40000000 is 0, but the constants sum to
; 0x80000000, INT32_MIN at i32 width, and sext(x) + sext(INT32_MIN) is -2^32.
; The constant part doesn't fit, so the offset is not split.
; CHECK-LABEL: define <4 x i8> @nneg_nsw_regroup(
; CHECK-NOT: sext <4 x i32> %x
; CHECK: call <4 x i8> @__pseudo_gather_factored_base_offsets64_i8(ptr %base, <4 x i64> %off, i32 1, <4 x i64> zeroinitializer, <4 x i32> %mask)
; CHECK: ret
define <4 x i8> @nneg_nsw_regroup(ptr %base, <4 x i32> %x, <4 x i32> %mask) {
  %a = add nsw <4 x i32> %x, splat (i32 1073741824)
  %b = add nsw <4 x i32> %a, splat (i32 1073741824)
  %off = zext nneg <4 x i32> %b to <4 x i64>
  %v = call <4 x i8> @__pseudo_gather_factored_base_offsets64_i8(ptr %base, <4 x i64> %off, i32 1, <4 x i64> zeroinitializer, <4 x i32> %mask)
  ret <4 x i8> %v
}

; The same for a sext of an nsw sum.
; CHECK-LABEL: define <4 x i8> @sext_nsw_regroup(
; CHECK-NOT: sext <4 x i32> %x
; CHECK: call <4 x i8> @__pseudo_gather_factored_base_offsets64_i8(ptr %base, <4 x i64> %off, i32 1, <4 x i64> zeroinitializer, <4 x i32> %mask)
; CHECK: ret
define <4 x i8> @sext_nsw_regroup(ptr %base, <4 x i32> %x, <4 x i32> %mask) {
  %a = add nsw <4 x i32> %x, splat (i32 1073741824)
  %b = add nsw <4 x i32> %a, splat (i32 1073741824)
  %off = sext <4 x i32> %b to <4 x i64>
  %v = call <4 x i8> @__pseudo_gather_factored_base_offsets64_i8(ptr %base, <4 x i64> %off, i32 1, <4 x i64> zeroinitializer, <4 x i32> %mask)
  ret <4 x i8> %v
}

; (x + 0x40000000) * 2 is 0 for x = -0x40000000, but its constant part
; 0x80000000 doesn't fit in i32. The offset stays unsplit in the 64-bit form,
; rather than being narrowed and split later into x * 2 and INT32_MIN.
; A32-LABEL: define <4 x i64> @nsw_product_constant(
; A32-NOT: base_offsets32
; A32: call <4 x i64> @__pseudo_gather_factored_base_offsets64_i64(
; A32: ret
define <4 x i64> @nsw_product_constant(ptr %base, <4 x i32> %x, <4 x i32> %mask) {
  %a = add nsw <4 x i32> %x, splat (i32 1073741824)
  %b = mul nsw <4 x i32> %a, splat (i32 2)
  %off = zext nneg <4 x i32> %b to <4 x i64>
  %bi0 = ptrtoint ptr %base to i64
  %bi = insertelement <4 x i64> poison, i64 %bi0, i32 0
  %bs = shufflevector <4 x i64> %bi, <4 x i64> poison, <4 x i32> zeroinitializer
  %addr = add <4 x i64> %bs, %off
  %v = call <4 x i64> @__pseudo_gather64_i64(<4 x i64> %addr, <4 x i32> %mask)
  ret <4 x i64> %v
}

; The nonnegative result minus the positive constant is always representable
; as a signed i32. Splitting this regrouped variable is safe.
; CHECK-LABEL: define <4 x i8> @nneg_positive_split(
; CHECK: [[VAR:%.*]] = add <4 x i32> %x, %y
; CHECK: [[WIDE:%.*]] = sext <4 x i32> [[VAR]] to <4 x i64>
; CHECK: call <4 x i8> @__pseudo_gather_factored_base_offsets64_i8(ptr %base, <4 x i64> [[WIDE]], i32 1,
; CHECK: ret
define <4 x i8> @nneg_positive_split(ptr %base, <4 x i32> %x, <4 x i32> %y, <4 x i32> %mask) {
  %a = add nsw <4 x i32> %x, splat (i32 4)
  %b = add nsw <4 x i32> %a, %y
  %off = zext nneg <4 x i32> %b to <4 x i64>
  %v = call <4 x i8> @__pseudo_gather_factored_base_offsets64_i8(ptr %base, <4 x i64> %off, i32 1, <4 x i64> zeroinitializer, <4 x i32> %mask)
  ret <4 x i8> %v
}

; A sext of an add without nsw, whose variable part x + y may wrap where the
; sum doesn't: x + y + INT32_MIN is 0 for x = INT32_MAX, y = 1, but
; sext(x + y) + sext(INT32_MIN) is -2^32. Without a proof, a constant this
; large is not split off.
; CHECK-LABEL: define <4 x i8> @sext_unproven_large(
; CHECK: call <4 x i8> @__pseudo_gather_factored_base_offsets64_i8(ptr %base, <4 x i64> %off, i32 1, <4 x i64> zeroinitializer, <4 x i32> %mask)
; CHECK: ret
define <4 x i8> @sext_unproven_large(ptr %base, <4 x i32> %x, <4 x i32> %y, <4 x i32> %mask) {
  %a = add <4 x i32> %x, %y
  %b = add <4 x i32> %a, splat (i32 -2147483648)
  %off = sext <4 x i32> %b to <4 x i64>
  %v = call <4 x i8> @__pseudo_gather_factored_base_offsets64_i8(ptr %base, <4 x i64> %off, i32 1, <4 x i64> zeroinitializer, <4 x i32> %mask)
  ret <4 x i8> %v
}

; A small constant is split off: x + y can only wrap if the sum is within
; that constant of the i32 limits.
; CHECK-LABEL: define <4 x i8> @sext_unproven_small(
; CHECK-NOT: call {{.*}}<4 x i64> %off,
; CHECK: call <4 x i8> @__pseudo_gather_factored_base_offsets64_i8(ptr %base, <4 x i64> %{{.*}}, i32 1, <4 x i64> %{{.*}}, <4 x i32> %mask)
; CHECK: ret
define <4 x i8> @sext_unproven_small(ptr %base, <4 x i32> %x, <4 x i32> %y, <4 x i32> %mask) {
  %a = add <4 x i32> %x, %y
  %b = add <4 x i32> %a, splat (i32 16)
  %off = sext <4 x i32> %b to <4 x i64>
  %v = call <4 x i8> @__pseudo_gather_factored_base_offsets64_i8(ptr %base, <4 x i64> %off, i32 1, <4 x i64> zeroinitializer, <4 x i32> %mask)
  ret <4 x i8> %v
}

; The constant is bounded by its exact value: (2^30 + j) << 2 wraps to 4 * j
; at i32 width, and (x + y) << 2 is 0 for x + y = -2^30, but its scale would
; be extracted as sext(x + y) * 4, -2^32. So the offset is not split.
; CHECK-LABEL: define <4 x float> @sext_wrapped_scaled(
; CHECK: call <4 x float> @__pseudo_gather_factored_base_offsets64_float(ptr %base, <4 x i64> %off, i32 1, <4 x i64> zeroinitializer, <4 x i32> %mask)
; CHECK: ret
define <4 x float> @sext_wrapped_scaled(ptr %base, <4 x i32> %x, <4 x i32> %y, <4 x i32> %mask) {
  %a = add <4 x i32> %x, %y
  %b = add <4 x i32> %a, <i32 1073741824, i32 1073741825, i32 1073741826, i32 1073741827>
  %s = shl nsw <4 x i32> %b, splat (i32 2)
  %off = sext <4 x i32> %s to <4 x i64>
  %v = call <4 x float> @__pseudo_gather_factored_base_offsets64_float(ptr %base, <4 x i64> %off, i32 1, <4 x i64> zeroinitializer, <4 x i32> %mask)
  ret <4 x float> %v
}

; Each part of the constant is bounded, not only their sum: the constants of
; (((x + 2^30) << 2) + INT32_MIN) + INT32_MIN sum to 0, but the split builds
; x << 2, whose scale would be extracted as sext(x) * 4, -2^32 for
; x = -2^30, where the offset is 0.
; CHECK-LABEL: define <4 x float> @sext_cancelling_parts(
; CHECK: call <4 x float> @__pseudo_gather_factored_base_offsets64_float(ptr %base, <4 x i64> %off, i32 1, <4 x i64> zeroinitializer, <4 x i32> %mask)
; CHECK: ret
define <4 x float> @sext_cancelling_parts(ptr %base, <4 x i32> %x, <4 x i32> %mask) {
  %a = add <4 x i32> %x, splat (i32 1073741824)
  %b = shl nsw <4 x i32> %a, splat (i32 2)
  %c = add <4 x i32> %b, splat (i32 -2147483648)
  %d = add <4 x i32> %c, splat (i32 -2147483648)
  %off = sext <4 x i32> %d to <4 x i64>
  %v = call <4 x float> @__pseudo_gather_factored_base_offsets64_float(ptr %base, <4 x i64> %off, i32 1, <4 x i64> zeroinitializer, <4 x i32> %mask)
  ret <4 x float> %v
}

; The lowering adds sext(delta) to sext(offsets) * scale in i64. Moving
; 0x40000000 * 4 into the i32 delta would wrap it to 0 instead of 4 GiB, so
; the constant stays in the offset.
; A32-LABEL: define <4 x float> @big_scaled_delta(
; A32: call <4 x float> @__pseudo_gather_factored_base_offsets32_float(ptr %base, <4 x i32> %off, i32 4, <4 x i32> zeroinitializer, <4 x i32> %mask)
; A32: ret
define <4 x float> @big_scaled_delta(ptr %base, <4 x i32> %x, <4 x i32> %mask) {
  %off = add nsw <4 x i32> %x, splat (i32 1073741824)
  %v = call <4 x float> @__pseudo_gather_factored_base_offsets32_float(ptr %base, <4 x i32> %off, i32 4, <4 x i32> zeroinitializer, <4 x i32> %mask)
  ret <4 x float> %v
}

; A small constant still moves to the delta.
; A32-LABEL: define <4 x float> @small_scaled_delta(
; A32: call <4 x float> @__pseudo_gather_factored_base_offsets32_float(ptr %base, <4 x i32> %x, i32 4, <4 x i32> <i32 8, i32 12, i32 16, i32 20>, <4 x i32> %mask)
; A32: ret
define <4 x float> @small_scaled_delta(ptr %base, <4 x i32> %x, <4 x i32> %mask) {
  %off = add nsw <4 x i32> %x, <i32 0, i32 1, i32 2, i32 3>
  %v = call <4 x float> @__pseudo_gather_factored_base_offsets32_float(ptr %base, <4 x i32> %off, i32 4, <4 x i32> splat (i32 8), <4 x i32> %mask)
  ret <4 x float> %v
}

; And for a 32-bit factored offset: 0x80000000 doesn't fit in the i32 delta.
; A32-LABEL: define <4 x float> @regroup32(
; A32: call <4 x float> @__pseudo_gather_factored_base_offsets32_float(ptr %base, <4 x i32> %off, i32 1, <4 x i32> zeroinitializer, <4 x i32> %mask)
; A32: ret
define <4 x float> @regroup32(ptr %base, <4 x i32> %x, <4 x i32> %mask) {
  %a = add nsw <4 x i32> %x, splat (i32 1073741824)
  %off = add nsw <4 x i32> %a, splat (i32 1073741824)
  %v = call <4 x float> @__pseudo_gather_factored_base_offsets32_float(ptr %base, <4 x i32> %off, i32 1, <4 x i32> zeroinitializer, <4 x i32> %mask)
  ret <4 x float> %v
}

; The exact evaluation applies only where the offset is all nsw. Here the
; inner i16 multiply wraps to 0, so the delta stays 0, not 65536.
; A32-LABEL: define <4 x float> @wrapped_inner_sext(
; A32: call <4 x float> @__pseudo_gather_factored_base_offsets32_float(ptr %base, <4 x i32> %x, i32 1, <4 x i32> zeroinitializer, <4 x i32> %mask)
; A32: ret
define <4 x float> @wrapped_inner_sext(ptr %base, <4 x i32> %x, <4 x i32> %mask) {
  %c = mul <4 x i16> splat (i16 256), splat (i16 256)
  %e = sext <4 x i16> %c to <4 x i32>
  %off = add nsw <4 x i32> %x, %e
  %v = call <4 x float> @__pseudo_gather_factored_base_offsets32_float(ptr %base, <4 x i32> %off, i32 1, <4 x i32> zeroinitializer, <4 x i32> %mask)
  ret <4 x float> %v
}

; The exact constant 0x40000000 << 30 times the scale 8 overflows i64: no split.
; A32-LABEL: define <4 x float> @huge_constant(
; A32: call <4 x float> @__pseudo_gather_factored_base_offsets32_float(ptr %base, <4 x i32> %off, i32 8, <4 x i32> zeroinitializer, <4 x i32> %mask)
; A32: ret
define <4 x float> @huge_constant(ptr %base, <4 x i32> %x, <4 x i32> %mask) {
  %c = shl nsw <4 x i32> splat (i32 1073741824), splat (i32 30)
  %off = add nsw <4 x i32> %x, %c
  %v = call <4 x float> @__pseudo_gather_factored_base_offsets32_float(ptr %base, <4 x i32> %off, i32 8, <4 x i32> zeroinitializer, <4 x i32> %mask)
  ret <4 x float> %v
}

; The exact constant 0x80000000 plus the delta INT32_MIN would fit, but the
; split's variable part x + y wraps at i32 width for x = INT32_MIN, y = -1, so
; the constant is taken as the IR computes it and the split is refused.
; A32-LABEL: define <4 x float> @regrouped_variable(
; A32: call <4 x float> @__pseudo_gather_factored_base_offsets32_float(ptr %base, <4 x i32> %off, i32 1, <4 x i32> splat (i32 -2147483648), <4 x i32> %mask)
; A32: ret
define <4 x float> @regrouped_variable(ptr %base, <4 x i32> %x, <4 x i32> %y, <4 x i32> %mask) {
  %a = add nsw <4 x i32> %x, splat (i32 1073741824)
  %b = add nsw <4 x i32> %a, splat (i32 1073741824)
  %off = add nsw <4 x i32> %b, %y
  %v = call <4 x float> @__pseudo_gather_factored_base_offsets32_float(ptr %base, <4 x i32> %off, i32 1, <4 x i32> splat (i32 -2147483648), <4 x i32> %mask)
  ret <4 x float> %v
}

; The same rule for the i32 offsets of the 32-bit factored form, which the
; lowering sign extends: INT32_MIN stays in the offset.
; A32-LABEL: define <4 x float> @late_unproven_large(
; A32: call <4 x float> @__pseudo_gather_factored_base_offsets32_float(ptr %base, <4 x i32> %off, i32 1, <4 x i32> zeroinitializer, <4 x i32> %mask)
; A32: ret
define <4 x float> @late_unproven_large(ptr %base, <4 x i32> %x, <4 x i32> %y, <4 x i32> %mask) {
  %a = add <4 x i32> %x, %y
  %off = add <4 x i32> %a, splat (i32 -2147483648)
  %v = call <4 x float> @__pseudo_gather_factored_base_offsets32_float(ptr %base, <4 x i32> %off, i32 1, <4 x i32> zeroinitializer, <4 x i32> %mask)
  ret <4 x float> %v
}
