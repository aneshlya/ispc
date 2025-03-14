;;  Copyright (c) 2020-2025, Intel Corporation
;;
;;  SPDX-License-Identifier: BSD-3-Clause

define(`WIDTH',`32')
define(`ISA',`AVX512SKX')

include(`target-avx512-common-32.ll')

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; half precision rcp and rsqrt using native SPR instriuctions
;; they will be invoked only when hardware support is available.

declare <8 x half> @llvm.x86.avx512fp16.mask.rcp.sh(<8 x half>, <8 x half>, <8 x half>, i8)
define half @__rcp_uniform_half(half) nounwind readonly alwaysinline {
  %vec = insertelement <8 x half> undef, half %0, i32 0
  %rcp = tail call <8 x half> @llvm.x86.avx512fp16.mask.rcp.sh(<8 x half> %vec, <8 x half> %vec, <8 x half> undef, i8 -1)
  %ret = extractelement <8 x half> %rcp, i32 0
  ret half %ret
}

declare <32 x half> @llvm.x86.avx512fp16.mask.rcp.ph.512(<32 x half>, <32 x half>, i32)
define <32 x half> @__rcp_varying_half(<32 x half>) nounwind readonly alwaysinline {
  %ret = tail call <32 x half> @llvm.x86.avx512fp16.mask.rcp.ph.512(<32 x half> %0, <32 x half> undef, i32 -1)
  ret <32 x half> %ret
}

declare <8 x half> @llvm.x86.avx512fp16.mask.rsqrt.sh(<8 x half>, <8 x half>, <8 x half>, i8)
define half @__rsqrt_uniform_half(half) nounwind readnone alwaysinline {
  %vec = insertelement <8 x half> undef, half %0, i32 0
  %rsqrt = tail call <8 x half> @llvm.x86.avx512fp16.mask.rsqrt.sh(<8 x half> %vec, <8 x half> %vec, <8 x half> undef, i8 -1)
  %ret = extractelement <8 x half> %rsqrt, i32 0
  ret half %ret
}

declare <32 x half> @llvm.x86.avx512fp16.mask.rsqrt.ph.512(<32 x half>, <32 x half>, i32)
define <32 x half> @__rsqrt_varying_half(<32 x half>) nounwind readnone alwaysinline {
  %ret = tail call <32 x half> @llvm.x86.avx512fp16.mask.rsqrt.ph.512(<32 x half> %0, <32 x half> undef, i32 -1)
  ret <32 x half> %ret
}

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; dot product
declare <16 x i32> @llvm.x86.avx512.vpdpbusd.512(<16 x i32>, <16 x i32>, <16 x i32>) nounwind readnone
define <32 x i32> @__dot4add_u8i8packed(<32 x i32> %a, <32 x i32> %b, <32 x i32> %acc) nounwind readnone alwaysinline {
  v32tov16(i32, %a, %a0, %a1)
  v32tov16(i32, %b, %b0, %b1)
  v32tov16(i32, %acc, %acc0, %acc1)
  %ret0 = call <16 x i32> @llvm.x86.avx512.vpdpbusd.512(<16 x i32> %acc0, <16 x i32> %a0, <16 x i32> %b0)
  %ret1 = call <16 x i32> @llvm.x86.avx512.vpdpbusd.512(<16 x i32> %acc1, <16 x i32> %a1, <16 x i32> %b1)
  v16tov32(i32, %ret0, %ret1, %ret)
  ret <32 x i32> %ret
}

declare <16 x i32> @llvm.x86.avx512.vpdpbusds.512(<16 x i32>, <16 x i32>, <16 x i32>) nounwind readnone
define <32 x i32> @__dot4add_u8i8packed_sat(<32 x i32> %a, <32 x i32> %b, <32 x i32> %acc) nounwind readnone alwaysinline {
  v32tov16(i32, %a, %a0, %a1)
  v32tov16(i32, %b, %b0, %b1)
  v32tov16(i32, %acc, %acc0, %acc1)
  %ret0 = call <16 x i32> @llvm.x86.avx512.vpdpbusds.512(<16 x i32> %acc0, <16 x i32> %a0, <16 x i32> %b0)
  %ret1 = call <16 x i32> @llvm.x86.avx512.vpdpbusds.512(<16 x i32> %acc1, <16 x i32> %a1, <16 x i32> %b1)
  v16tov32(i32, %ret0, %ret1, %ret)
  ret <32 x i32> %ret
}

declare <16 x i32> @llvm.x86.avx512.vpdpwssd.512(<16 x i32>, <16 x i32>, <16 x i32>) nounwind readnone
define <32 x i32> @__dot2add_i16packed(<32 x i32> %a, <32 x i32> %b, <32 x i32> %acc) nounwind readnone alwaysinline {
  v32tov16(i32, %a, %a0, %a1)
  v32tov16(i32, %b, %b0, %b1)
  v32tov16(i32, %acc, %acc0, %acc1)
  %ret0 = call <16 x i32> @llvm.x86.avx512.vpdpwssd.512(<16 x i32> %acc0, <16 x i32> %a0, <16 x i32> %b0)
  %ret1 = call <16 x i32> @llvm.x86.avx512.vpdpwssd.512(<16 x i32> %acc1, <16 x i32> %a1, <16 x i32> %b1)
  v16tov32(i32, %ret0, %ret1, %ret)
  ret <32 x i32> %ret
}

declare <16 x i32> @llvm.x86.avx512.vpdpwssds.512(<16 x i32>, <16 x i32>, <16 x i32>) nounwind readnone
define <32 x i32> @__dot2add_i16packed_sat(<32 x i32> %a, <32 x i32> %b, <32 x i32> %acc) nounwind readnone alwaysinline {
  v32tov16(i32, %a, %a0, %a1)
  v32tov16(i32, %b, %b0, %b1)
  v32tov16(i32, %acc, %acc0, %acc1)
  %ret0 = call <16 x i32> @llvm.x86.avx512.vpdpwssds.512(<16 x i32> %acc0, <16 x i32> %a0, <16 x i32> %b0)
  %ret1 = call <16 x i32> @llvm.x86.avx512.vpdpwssds.512(<16 x i32> %acc1, <16 x i32> %a1, <16 x i32> %b1)
  v16tov32(i32, %ret0, %ret1, %ret)
  ret <32 x i32> %ret
}
