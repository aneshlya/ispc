; Known compiler crash of the consecutive test of ImproveMemoryOps
; (LLVMVectorIsLinear): for offsets "and" a splat it computes the splat plus
; one in a C++ integer, which is 0 for all ones and overflows for INT64_MAX,
; and an assertion of llvmutil.cpp fails for both.
;
; XFAIL until the test handles every splat.
;
; RUN: %{ispc-opt} --target=sse4-i32x4 --passes=improve-memory-ops %s -o - | FileCheck %s
; REQUIRES: X86_ENABLED
; XFAIL: *

declare <4 x i8> @__pseudo_gather_factored_base_offsets64_i8(ptr, <4 x i64>, i32, <4 x i64>, <4 x i32>)

; CHECK-LABEL: define <4 x i8> @and_all_ones(
; CHECK: ret
define <4 x i8> @and_all_ones(ptr %base, i64 %u) {
  %ui = insertelement <4 x i64> poison, i64 %u, i32 0
  %us = shufflevector <4 x i64> %ui, <4 x i64> poison, <4 x i32> zeroinitializer
  %a = add <4 x i64> %us, <i64 0, i64 1, i64 2, i64 3>
  %m = and <4 x i64> %a, splat (i64 -1)
  %v = call <4 x i8> @__pseudo_gather_factored_base_offsets64_i8(ptr %base, <4 x i64> %m, i32 1, <4 x i64> zeroinitializer, <4 x i32> splat (i32 -1))
  ret <4 x i8> %v
}

; CHECK-LABEL: define <4 x i8> @and_int64_max(
; CHECK: ret
define <4 x i8> @and_int64_max(ptr %base, i64 %u) {
  %ui = insertelement <4 x i64> poison, i64 %u, i32 0
  %us = shufflevector <4 x i64> %ui, <4 x i64> poison, <4 x i32> zeroinitializer
  %a = add <4 x i64> %us, <i64 0, i64 1, i64 2, i64 3>
  %m = and <4 x i64> %a, splat (i64 9223372036854775807)
  %v = call <4 x i8> @__pseudo_gather_factored_base_offsets64_i8(ptr %base, <4 x i64> %m, i32 1, <4 x i64> zeroinitializer, <4 x i32> splat (i32 -1))
  ret <4 x i8> %v
}
