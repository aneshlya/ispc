;;  Copyright (c) 2016-2024, Intel Corporation
;;
;;  SPDX-License-Identifier: BSD-3-Clause

define(`WIDTH',`4')
define(`ISA',`AVX512SKX')

include(`target-avx512-common-4.ll')

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; broadcast/rotate/shuffle

shuffle_permute(i8)
shuffle_permute(i16)
shuffle_permute(half)
shuffle_permute(i32)
shuffle_permute(float)

declare <4 x i64> @llvm.x86.avx512.mask.permvar.di.256(<4 x i64>, <4 x i64>, <4 x i64>, i8)
define <4 x i64> @__shuffle_i64(<4 x i64>, <4 x i32>) nounwind readnone alwaysinline {
  %ind = zext <4 x i32> %1 to <4 x i64>
  %res = call <4 x i64> @llvm.x86.avx512.mask.permvar.di.256(<4 x i64> %0, <4 x i64> %ind, <4 x i64> zeroinitializer, i8 -1)
  ret <4 x i64> %res
}

declare <4 x double> @llvm.x86.avx512.mask.permvar.df.256(<4 x double>, <4 x i64>, <4 x double>, i8)
define <4 x double> @__shuffle_double(<4 x double>, <4 x i32>) nounwind readnone alwaysinline {
  %ind = zext <4 x i32> %1 to <4 x i64>
  %res = call <4 x double> @llvm.x86.avx512.mask.permvar.df.256(<4 x double> %0, <4 x i64> %ind, <4 x double> zeroinitializer, i8 -1)
  ret <4 x double> %res
}


shuffle2_permute(i8)
shuffle2_permute(i16)
shuffle2_permute(half)

declare <4 x i32> @llvm.x86.avx512.vpermi2var.d.128(<4 x i32>, <4 x i32>, <4 x i32>)
define internal <4 x i32> @__shuffle2_permute_i32(<4 x i32>, <4 x i32>, <WIDTH x i32>) nounwind readnone alwaysinline {
  %res = call <4 x i32> @llvm.x86.avx512.vpermi2var.d.128(<4 x i32> %0, <4 x i32> %2, <4 x i32> %1)
  ret <4 x i32> %res
}

declare <4 x float> @llvm.x86.avx512.vpermi2var.ps.128(<4 x float>, <4 x i32>, <4 x float>)
define internal <WIDTH x float> @__shuffle2_permute_float(<WIDTH x float>, <WIDTH x float>, <WIDTH x i32>) nounwind readnone alwaysinline {
  %res = call <4 x float> @llvm.x86.avx512.vpermi2var.ps.128(<4 x float> %0, <4 x i32> %2, <4 x float> %1)
  ret <WIDTH x float> %res
}

declare <4 x i64> @llvm.x86.avx512.vpermi2var.q.256(<4 x i64>, <4 x i64>, <4 x i64>)
define internal <WIDTH x i64> @__shuffle2_permute_i64(<WIDTH x i64>, <WIDTH x i64>, <WIDTH x i32>) nounwind readnone alwaysinline {
  %ind = zext <WIDTH x i32> %2 to <WIDTH x i64>
  %res = call <4 x i64> @llvm.x86.avx512.vpermi2var.q.256(<4 x i64> %0, <4 x i64> %ind, <4 x i64> %1)
  ret <WIDTH x i64> %res
}

declare <4 x double> @llvm.x86.avx512.vpermi2var.pd.256(<4 x double>, <4 x i64>, <4 x double>)
define internal <WIDTH x double> @__shuffle2_permute_double(<WIDTH x double>, <WIDTH x double>, <WIDTH x i32>) nounwind readnone alwaysinline {
  %ind = zext <WIDTH x i32> %2 to <WIDTH x i64>
  %res = call <4 x double> @llvm.x86.avx512.vpermi2var.pd.256(<4 x double> %0, <4 x i64> %ind, <4 x double> %1)
  ret <WIDTH x double> %res
}
define_shuffles_no_shuffle2_perm()

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; rcp/rsqrt declarations for half
rcph_rsqrth_decl

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; rcp, rsqrt

rcp14_uniform()
;; rcp float
declare <4 x float> @llvm.x86.avx512.rcp14.ps.128(<4 x float>, <4 x float>, i8) nounwind readnone
define <4 x float> @__rcp_fast_varying_float(<4 x float>) nounwind readonly alwaysinline {
  %ret = call <4 x float> @llvm.x86.avx512.rcp14.ps.128(<4 x float> %0, <4 x float> undef, i8 -1)
  ret <4 x float> %ret
}
define <4 x float> @__rcp_varying_float(<4 x float>) nounwind readonly alwaysinline {
  %call = call <4 x float> @__rcp_fast_varying_float(<4 x float> %0)
  ;; do one Newton-Raphson iteration to improve precision
  ;;  float iv = __rcp_v(v);
  ;;  return iv * (2. - v * iv);
  %v_iv = fmul <4 x float> %0, %call
  %two_minus = fsub <4 x float> <float 2., float 2., float 2., float 2.>, %v_iv
  %iv_mul = fmul <4 x float> %call,  %two_minus
  ret <4 x float> %iv_mul
}

;; rcp double
declare <4 x double> @llvm.x86.avx512.rcp14.pd.256(<4 x double>, <4 x double>, i8) nounwind readnone
define <4 x double> @__rcp_fast_varying_double(<4 x double> %val) nounwind readonly alwaysinline {
  %res = call <4 x double> @llvm.x86.avx512.rcp14.pd.256(<4 x double> %val, <4 x double> undef, i8 -1)
  ret <4 x double> %res
}
define <4 x double> @__rcp_varying_double(<4 x double>) nounwind readonly alwaysinline {
  %call = call <4 x double> @__rcp_fast_varying_double(<4 x double> %0)
  ;; do one Newton-Raphson iteration to improve precision
  ;;  double iv = __rcp_v(v);
  ;;  return iv * (2. - v * iv);
  %v_iv = fmul <4 x double> %0, %call
  %two_minus = fsub <4 x double> <double 2., double 2., double 2., double 2.>, %v_iv
  %iv_mul = fmul <4 x double> %call,  %two_minus
  ret <4 x double> %iv_mul
}

rsqrt14_uniform()
;; rsqrt float
declare <4 x float> @llvm.x86.avx512.rsqrt14.ps.128(<4 x float>,  <4 x float>,  i8) nounwind readnone
define <4 x float> @__rsqrt_fast_varying_float(<4 x float> %v) nounwind readonly alwaysinline {
  %ret = call <4 x float> @llvm.x86.avx512.rsqrt14.ps.128(<4 x float> %v,  <4 x float> undef,  i8 -1)
  ret <4 x float> %ret
}
define <4 x float> @__rsqrt_varying_float(<4 x float> %v) nounwind readonly alwaysinline {
  %is = call <4 x float> @__rsqrt_fast_varying_float(<4 x float> %v)
  ; Newton-Raphson iteration to improve precision
  ;  float is = __rsqrt_v(v);
  ;  return 0.5 * is * (3. - (v * is) * is);
  %v_is = fmul <4 x float> %v,  %is
  %v_is_is = fmul <4 x float> %v_is,  %is
  %three_sub = fsub <4 x float> <float 3., float 3., float 3., float 3.>, %v_is_is
  %is_mul = fmul <4 x float> %is,  %three_sub
  %half_scale = fmul <4 x float> <float 0.5, float 0.5, float 0.5, float 0.5>, %is_mul
  ret <4 x float> %half_scale
}

;; rsqrt double
declare <4 x double> @llvm.x86.avx512.rsqrt14.pd.256(<4 x double>,  <4 x double>,  i8) nounwind readnone
define <4 x double> @__rsqrt_fast_varying_double(<4 x double> %val) nounwind readonly alwaysinline {
  %res = call <4 x double> @llvm.x86.avx512.rsqrt14.pd.256(<4 x double> %val, <4 x double> undef, i8 -1)
  ret <4 x double> %res
}
declare <4 x i1> @llvm.x86.avx512.fpclass.pd.256(<4 x double>, i32)
define <4 x double> @__rsqrt_varying_double(<4 x double> %v) nounwind readonly alwaysinline {
  %corner_cases = call <4 x i1> @llvm.x86.avx512.fpclass.pd.256(<4 x double> %v, i32 14)
  %is = call <4 x double> @__rsqrt_fast_varying_double(<4 x double> %v)

  ; Precision refinement sequence based on minimax approximation.
  ; This sequence is a little slower than Newton-Raphson, but has much better precision
  ; Relative error is around 3 ULPs.
  ; t1 = 1.0 - (v * is) * is
  ; t2 = 0.37500000407453632 + t1 * 0.31250000550062401
  ; t3 = 0.5 + t1 * t2
  ; t4 = is + (t1*is) * t3
  %v_is = fmul <4 x double> %v,  %is
  %v_is_is = fmul <4 x double> %v_is,  %is
  %t1 = fsub <4 x double> <double 1., double 1., double 1., double 1.>, %v_is_is
  %t1_03125 = fmul <4 x double> <double 0.31250000550062401, double 0.31250000550062401, double 0.31250000550062401, double 0.31250000550062401>, %t1
  %t2 = fadd <4 x double> <double 0.37500000407453632, double 0.37500000407453632, double 0.37500000407453632, double 0.37500000407453632>, %t1_03125
  %t1_t2 = fmul <4 x double> %t1, %t2
  %t3 = fadd <4 x double> <double 0.5, double 0.5, double 0.5, double 0.5>, %t1_t2
  %t1_is = fmul <4 x double> %t1, %is
  %t1_is_t3 = fmul <4 x double> %t1_is, %t3
  %t4 = fadd <4 x double> %is, %t1_is_t3
  %ret = select <4 x i1> %corner_cases, <4 x double> %is, <4 x double> %t4
  ret <4 x double> %ret
}

;;saturation_arithmetic_novec()

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; dot product
dot_product_vnni_decl()
