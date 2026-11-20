; Known address gap of the masked-store rule of ImproveMemoryOps: it reads
; the mask of every masked store by the sign bit of each lane
; (GetMaskStatusFromValue), but the blend stores read the low bit. An i8
; blend store with the mask <1, 0, 0, 0> writes lane 0, and the rule takes
; the mask for all off and removes the store (LOWON); with <-2, -2, -2, -2> it
; writes nothing, and the rule turns it into a store of every lane (LOWOFF).
; The frontend's masks are 0 or -1 per lane, for which the rule is right.
;
; XFAIL until the rule reads the mask as the store it rewrites does.
;
; RUN: %{ispc-opt} --target=sse4-i32x4 --passes=improve-memory-ops %s -o - | FileCheck %s --check-prefix=LOWON
; RUN: %{ispc-opt} --target=sse4-i32x4 --passes=improve-memory-ops %s -o - | FileCheck %s --check-prefix=LOWOFF
; REQUIRES: X86_ENABLED
; XFAIL: *

declare void @__masked_store_blend_i8(ptr, <4 x i8>, <4 x i32>)

; LOWON-LABEL: define void @low_bit_on(
; LOWON: store
; LOWON: ret void
define void @low_bit_on(ptr %p, <4 x i8> %v) {
  call void @__masked_store_blend_i8(ptr %p, <4 x i8> %v, <4 x i32> <i32 1, i32 0, i32 0, i32 0>)
  ret void
}

; LOWON-LABEL: define void @low_bit_off(
; LOWOFF-LABEL: define void @low_bit_off(
; LOWOFF-NOT: store <4 x i8>
; LOWOFF: ret void
define void @low_bit_off(ptr %p, <4 x i8> %v) {
  call void @__masked_store_blend_i8(ptr %p, <4 x i8> %v, <4 x i32> <i32 -2, i32 -2, i32 -2, i32 -2>)
  ret void
}
