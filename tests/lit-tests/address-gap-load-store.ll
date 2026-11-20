; Known address gaps of the rule of ImproveMemoryOps that turns a based
; gather into a load, a reverse load or a broadcast. Each case has its own
; prefix:
;
; REBUILT: the pointer of a broadcast is rebuilt at the offset width. A
;          factored float gather with offsets 0x20000000 and scale 4 reads
;          base + 2^31 (the factored gather of builtins/util.m4 sign extends
;          the offsets first), but 0x20000000 * 4 at 32 bits is -2^31: the
;          broadcast loads 2 GiB below the base.
; PHI:     LLVMVectorIsLinear follows a loop phi once and counts a phi it has
;          seen as linear for any stride: byte offsets that start at
;          <0, 1, 2, 3> and are negated on each iteration become a forward
;          load, also on the iterations where they are <0, -1, -2, -3>.
; OVERFLOW: the consecutive test of constant 64-bit offsets computes the next
;          lane in a C++ int64_t: <INT64_MAX, INT64_MIN, ...> pass as
;          consecutive.
; MASKBIT: the start of a load from the first lane that is on counts a lane
;          as on if its mask is nonzero, the mask status if its sign bit is
;          set: with the mask <1, -1, 0, 0>, zero extended offsets
;          u + <0, 1, 2, 3> and u = UINT32_MAX, the start comes from lane 0
;          and lane 1 reads base + 2^32 instead of base (the frontend's masks
;          are 0 or -1 per lane).
;
; XFAIL until the address of the load is the address of the lanes that are on.
;
; RUN: %{ispc-opt} --target=sse4-i32x4 --passes=improve-memory-ops %s -o - | FileCheck %s --check-prefix=REBUILT
; RUN: %{ispc-opt} --target=sse4-i32x4 --passes=improve-memory-ops %s -o - | FileCheck %s --check-prefix=PHI
; RUN: %{ispc-opt} --target=sse4-i32x4 --passes=improve-memory-ops %s -o - | FileCheck %s --check-prefix=OVERFLOW
; RUN: %{ispc-opt} --target=sse4-i32x4 --passes=improve-memory-ops %s -o - | FileCheck %s --check-prefix=MASKBIT
; REQUIRES: X86_ENABLED
; XFAIL: *

declare <4 x float> @__pseudo_gather_factored_base_offsets32_float(ptr, <4 x i32>, i32, <4 x i32>, <4 x i32>)
declare <4 x i8> @__pseudo_gather_factored_base_offsets32_i8(ptr, <4 x i32>, i32, <4 x i32>, <4 x i32>)
declare <4 x i8> @__pseudo_gather_factored_base_offsets64_i8(ptr, <4 x i64>, i32, <4 x i64>, <4 x i32>)
declare <4 x i8> @__masked_load_i8(ptr, <4 x i32>)
declare <4 x float> @__masked_load_float(ptr, <4 x i32>)

; REBUILT-LABEL: define <4 x float> @rebuilt_pointer(
; REBUILT-NOT: getelementptr i8, ptr %base, i32
; REBUILT: load float
; REBUILT: ret
define <4 x float> @rebuilt_pointer(ptr %base) {
  %v = call <4 x float> @__pseudo_gather_factored_base_offsets32_float(ptr %base, <4 x i32> splat (i32 536870912), i32 4, <4 x i32> zeroinitializer, <4 x i32> splat (i32 -1))
  ret <4 x float> %v
}

; PHI-LABEL: define void @phi_stride(
; PHI-NOT: load <4 x i8>
; PHI: ret void
define void @phi_stride(ptr %base, ptr %out, i32 %n) {
entry:
  br label %loop
loop:
  %i = phi i32 [ 0, %entry ], [ %i1, %loop ]
  %off = phi <4 x i32> [ <i32 0, i32 1, i32 2, i32 3>, %entry ], [ %neg, %loop ]
  %v = call <4 x i8> @__pseudo_gather_factored_base_offsets32_i8(ptr %base, <4 x i32> %off, i32 1, <4 x i32> zeroinitializer, <4 x i32> splat (i32 -1))
  %p = getelementptr <4 x i8>, ptr %out, i32 %i
  store <4 x i8> %v, ptr %p
  %neg = mul <4 x i32> %off, splat (i32 -1)
  %i1 = add i32 %i, 1
  %c = icmp slt i32 %i1, %n
  br i1 %c, label %loop, label %exit
exit:
  ret void
}

; OVERFLOW-LABEL: define <4 x i8> @overflow_offsets(
; OVERFLOW-NOT: load <4 x i8>
; OVERFLOW: ret
define <4 x i8> @overflow_offsets(ptr %base) {
  %v = call <4 x i8> @__pseudo_gather_factored_base_offsets64_i8(ptr %base, <4 x i64> <i64 9223372036854775807, i64 -9223372036854775808, i64 -9223372036854775807, i64 -9223372036854775806>, i32 1, <4 x i64> zeroinitializer, <4 x i32> splat (i32 -1))
  ret <4 x i8> %v
}

; MASKBIT-LABEL: define <4 x i8> @mask_bit(
; MASKBIT-NOT: icmp ne <4 x i32> <i32 1, i32 -1, i32 0, i32 0>, zeroinitializer
; MASKBIT: ret
define <4 x i8> @mask_bit(ptr %base, i32 %u) {
  %ui = insertelement <4 x i32> poison, i32 %u, i32 0
  %us = shufflevector <4 x i32> %ui, <4 x i32> poison, <4 x i32> zeroinitializer
  %a = add <4 x i32> %us, <i32 0, i32 1, i32 2, i32 3>
  %z = zext <4 x i32> %a to <4 x i64>
  %v = call <4 x i8> @__pseudo_gather_factored_base_offsets64_i8(ptr %base, <4 x i64> %z, i32 1, <4 x i64> zeroinitializer, <4 x i32> <i32 1, i32 -1, i32 0, i32 0>)
  ret <4 x i8> %v
}
