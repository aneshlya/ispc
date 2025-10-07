;;  Copyright (c) 2025, Intel Corporation
;;
;;  SPDX-License-Identifier: BSD-3-Clause

define(`WIDTH',`16')
define(`ISA',`avx2_2')

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; dot product
declare <8 x i32> @llvm.x86.avx2.vpdpbssd.256(<8 x i32>, <8 x i32>, <8 x i32>) nounwind readnone
define <16 x i32> @__dot4add_i8i8packed(<16 x i32> %a, <16 x i32> %b, <16 x i32> %acc) nounwind readnone alwaysinline {
  %a_lo = shufflevector <16 x i32> %a, <16 x i32> undef, <8 x i32> <i32 0, i32 1, i32 2, i32 3, i32 4, i32 5, i32 6, i32 7>
  %a_hi = shufflevector <16 x i32> %a, <16 x i32> undef, <8 x i32> <i32 8, i32 9, i32 10, i32 11, i32 12, i32 13, i32 14, i32 15>
  %b_lo = shufflevector <16 x i32> %b, <16 x i32> undef, <8 x i32> <i32 0, i32 1, i32 2, i32 3, i32 4, i32 5, i32 6, i32 7>
  %b_hi = shufflevector <16 x i32> %b, <16 x i32> undef, <8 x i32> <i32 8, i32 9, i32 10, i32 11, i32 12, i32 13, i32 14, i32 15>
  %acc_lo = shufflevector <16 x i32> %acc, <16 x i32> undef, <8 x i32> <i32 0, i32 1, i32 2, i32 3, i32 4, i32 5, i32 6, i32 7>
  %acc_hi = shufflevector <16 x i32> %acc, <16 x i32> undef, <8 x i32> <i32 8, i32 9, i32 10, i32 11, i32 12, i32 13, i32 14, i32 15>
  %ret_lo = call <8 x i32> @llvm.x86.avx2.vpdpbssd.256(<8 x i32> %acc_lo, <8 x i32> %a_lo, <8 x i32> %b_lo)
  %ret_hi = call <8 x i32> @llvm.x86.avx2.vpdpbssd.256(<8 x i32> %acc_hi, <8 x i32> %a_hi, <8 x i32> %b_hi)
  %ret = shufflevector <8 x i32> %ret_lo, <8 x i32> %ret_hi, <16 x i32> <i32 0, i32 1, i32 2, i32 3, i32 4, i32 5, i32 6, i32 7, i32 8, i32 9, i32 10, i32 11, i32 12, i32 13, i32 14, i32 15>
  ret <16 x i32> %ret
}
declare <8 x i32> @llvm.x86.avx2.vpdpbssds.256(<8 x i32>, <8 x i32>, <8 x i32>) nounwind readnone
define <16 x i32> @__dot4add_i8i8packed_sat(<16 x i32> %a, <16 x i32> %b, <16 x i32> %acc) nounwind readnone alwaysinline {
  %a_lo = shufflevector <16 x i32> %a, <16 x i32> undef, <8 x i32> <i32 0, i32 1, i32 2, i32 3, i32 4, i32 5, i32 6, i32 7>
  %a_hi = shufflevector <16 x i32> %a, <16 x i32> undef, <8 x i32> <i32 8, i32 9, i32 10, i32 11, i32 12, i32 13, i32 14, i32 15>
  %b_lo = shufflevector <16 x i32> %b, <16 x i32> undef, <8 x i32> <i32 0, i32 1, i32 2, i32 3, i32 4, i32 5, i32 6, i32 7>
  %b_hi = shufflevector <16 x i32> %b, <16 x i32> undef, <8 x i32> <i32 8, i32 9, i32 10, i32 11, i32 12, i32 13, i32 14, i32 15>
  %acc_lo = shufflevector <16 x i32> %acc, <16 x i32> undef, <8 x i32> <i32 0, i32 1, i32 2, i32 3, i32 4, i32 5, i32 6, i32 7>
  %acc_hi = shufflevector <16 x i32> %acc, <16 x i32> undef, <8 x i32> <i32 8, i32 9, i32 10, i32 11, i32 12, i32 13, i32 14, i32 15>
  %ret_lo = call <8 x i32> @llvm.x86.avx2.vpdpbssds.256(<8 x i32> %acc_lo, <8 x i32> %a_lo, <8 x i32> %b_lo)
  %ret_hi = call <8 x i32> @llvm.x86.avx2.vpdpbssds.256(<8 x i32> %acc_hi, <8 x i32> %a_hi, <8 x i32> %b_hi)
  %ret = shufflevector <8 x i32> %ret_lo, <8 x i32> %ret_hi, <16 x i32> <i32 0, i32 1, i32 2, i32 3, i32 4, i32 5, i32 6, i32 7, i32 8, i32 9, i32 10, i32 11, i32 12, i32 13, i32 14, i32 15>
  ret <16 x i32> %ret
}

declare <8 x i32> @llvm.x86.avx2.vpdpbuud.256(<8 x i32>, <8 x i32>, <8 x i32>) nounwind readnone
define <16 x i32> @__dot4add_u8u8packed(<16 x i32> %a, <16 x i32> %b, <16 x i32> %acc) nounwind readnone alwaysinline {
  %a_lo = shufflevector <16 x i32> %a, <16 x i32> undef, <8 x i32> <i32 0, i32 1, i32 2, i32 3, i32 4, i32 5, i32 6, i32 7>
  %a_hi = shufflevector <16 x i32> %a, <16 x i32> undef, <8 x i32> <i32 8, i32 9, i32 10, i32 11, i32 12, i32 13, i32 14, i32 15>
  %b_lo = shufflevector <16 x i32> %b, <16 x i32> undef, <8 x i32> <i32 0, i32 1, i32 2, i32 3, i32 4, i32 5, i32 6, i32 7>
  %b_hi = shufflevector <16 x i32> %b, <16 x i32> undef, <8 x i32> <i32 8, i32 9, i32 10, i32 11, i32 12, i32 13, i32 14, i32 15>
  %acc_lo = shufflevector <16 x i32> %acc, <16 x i32> undef, <8 x i32> <i32 0, i32 1, i32 2, i32 3, i32 4, i32 5, i32 6, i32 7>
  %acc_hi = shufflevector <16 x i32> %acc, <16 x i32> undef, <8 x i32> <i32 8, i32 9, i32 10, i32 11, i32 12, i32 13, i32 14, i32 15>
  %ret_lo = call <8 x i32> @llvm.x86.avx2.vpdpbuud.256(<8 x i32> %acc_lo, <8 x i32> %a_lo, <8 x i32> %b_lo)
  %ret_hi = call <8 x i32> @llvm.x86.avx2.vpdpbuud.256(<8 x i32> %acc_hi, <8 x i32> %a_hi, <8 x i32> %b_hi)
  %ret = shufflevector <8 x i32> %ret_lo, <8 x i32> %ret_hi, <16 x i32> <i32 0, i32 1, i32 2, i32 3, i32 4, i32 5, i32 6, i32 7, i32 8, i32 9, i32 10, i32 11, i32 12, i32 13, i32 14, i32 15>
  ret <16 x i32> %ret
}
declare <8 x i32> @llvm.x86.avx2.vpdpbuuds.256(<8 x i32>, <8 x i32>, <8 x i32>) nounwind readnone
define <16 x i32> @__dot4add_u8u8packed_sat(<16 x i32> %a, <16 x i32> %b, <16 x i32> %acc) nounwind readnone alwaysinline {
  %a_lo = shufflevector <16 x i32> %a, <16 x i32> undef, <8 x i32> <i32 0, i32 1, i32 2, i32 3, i32 4, i32 5, i32 6, i32 7>
  %a_hi = shufflevector <16 x i32> %a, <16 x i32> undef, <8 x i32> <i32 8, i32 9, i32 10, i32 11, i32 12, i32 13, i32 14, i32 15>
  %b_lo = shufflevector <16 x i32> %b, <16 x i32> undef, <8 x i32> <i32 0, i32 1, i32 2, i32 3, i32 4, i32 5, i32 6, i32 7>
  %b_hi = shufflevector <16 x i32> %b, <16 x i32> undef, <8 x i32> <i32 8, i32 9, i32 10, i32 11, i32 12, i32 13, i32 14, i32 15>
  %acc_lo = shufflevector <16 x i32> %acc, <16 x i32> undef, <8 x i32> <i32 0, i32 1, i32 2, i32 3, i32 4, i32 5, i32 6, i32 7>
  %acc_hi = shufflevector <16 x i32> %acc, <16 x i32> undef, <8 x i32> <i32 8, i32 9, i32 10, i32 11, i32 12, i32 13, i32 14, i32 15>
  %ret_lo = call <8 x i32> @llvm.x86.avx2.vpdpbuuds.256(<8 x i32> %acc_lo, <8 x i32> %a_lo, <8 x i32> %b_lo)
  %ret_hi = call <8 x i32> @llvm.x86.avx2.vpdpbuuds.256(<8 x i32> %acc_hi, <8 x i32> %a_hi, <8 x i32> %b_hi)
  %ret = shufflevector <8 x i32> %ret_lo, <8 x i32> %ret_hi, <16 x i32> <i32 0, i32 1, i32 2, i32 3, i32 4, i32 5, i32 6, i32 7, i32 8, i32 9, i32 10, i32 11, i32 12, i32 13, i32 14, i32 15>
  ret <16 x i32> %ret
}

declare <8 x i32> @llvm.x86.avx2.vpdpwuud.256(<8 x i32>, <8 x i32>, <8 x i32>) nounwind readnone
define <16 x i32> @__dot2add_u16u16packed(<16 x i32> %a, <16 x i32> %b, <16 x i32> %acc) nounwind readnone alwaysinline {
  %a_lo = shufflevector <16 x i32> %a, <16 x i32> undef, <8 x i32> <i32 0, i32 1, i32 2, i32 3, i32 4, i32 5, i32 6, i32 7>
  %a_hi = shufflevector <16 x i32> %a, <16 x i32> undef, <8 x i32> <i32 8, i32 9, i32 10, i32 11, i32 12, i32 13, i32 14, i32 15>
  %b_lo = shufflevector <16 x i32> %b, <16 x i32> undef, <8 x i32> <i32 0, i32 1, i32 2, i32 3, i32 4, i32 5, i32 6, i32 7>
  %b_hi = shufflevector <16 x i32> %b, <16 x i32> undef, <8 x i32> <i32 8, i32 9, i32 10, i32 11, i32 12, i32 13, i32 14, i32 15>
  %acc_lo = shufflevector <16 x i32> %acc, <16 x i32> undef, <8 x i32> <i32 0, i32 1, i32 2, i32 3, i32 4, i32 5, i32 6, i32 7>
  %acc_hi = shufflevector <16 x i32> %acc, <16 x i32> undef, <8 x i32> <i32 8, i32 9, i32 10, i32 11, i32 12, i32 13, i32 14, i32 15>
  %ret_lo = call <8 x i32> @llvm.x86.avx2.vpdpwuud.256(<8 x i32> %acc_lo, <8 x i32> %a_lo, <8 x i32> %b_lo)
  %ret_hi = call <8 x i32> @llvm.x86.avx2.vpdpwuud.256(<8 x i32> %acc_hi, <8 x i32> %a_hi, <8 x i32> %b_hi)
  %ret = shufflevector <8 x i32> %ret_lo, <8 x i32> %ret_hi, <16 x i32> <i32 0, i32 1, i32 2, i32 3, i32 4, i32 5, i32 6, i32 7, i32 8, i32 9, i32 10, i32 11, i32 12, i32 13, i32 14, i32 15>
  ret <16 x i32> %ret
}

declare <8 x i32> @llvm.x86.avx2.vpdpwuuds.256(<8 x i32>, <8 x i32>, <8 x i32>) nounwind readnone
define <16 x i32> @__dot2add_u16u16packed_sat(<16 x i32> %a, <16 x i32> %b, <16 x i32> %acc) nounwind readnone alwaysinline {
  %a_lo = shufflevector <16 x i32> %a, <16 x i32> undef, <8 x i32> <i32 0, i32 1, i32 2, i32 3, i32 4, i32 5, i32 6, i32 7>
  %a_hi = shufflevector <16 x i32> %a, <16 x i32> undef, <8 x i32> <i32 8, i32 9, i32 10, i32 11, i32 12, i32 13, i32 14, i32 15>
  %b_lo = shufflevector <16 x i32> %b, <16 x i32> undef, <8 x i32> <i32 0, i32 1, i32 2, i32 3, i32 4, i32 5, i32 6, i32 7>
  %b_hi = shufflevector <16 x i32> %b, <16 x i32> undef, <8 x i32> <i32 8, i32 9, i32 10, i32 11, i32 12, i32 13, i32 14, i32 15>
  %acc_lo = shufflevector <16 x i32> %acc, <16 x i32> undef, <8 x i32> <i32 0, i32 1, i32 2, i32 3, i32 4, i32 5, i32 6, i32 7>
  %acc_hi = shufflevector <16 x i32> %acc, <16 x i32> undef, <8 x i32> <i32 8, i32 9, i32 10, i32 11, i32 12, i32 13, i32 14, i32 15>
  %ret_lo = call <8 x i32> @llvm.x86.avx2.vpdpwuuds.256(<8 x i32> %acc_lo, <8 x i32> %a_lo, <8 x i32> %b_lo)
  %ret_hi = call <8 x i32> @llvm.x86.avx2.vpdpwuuds.256(<8 x i32> %acc_hi, <8 x i32> %a_hi, <8 x i32> %b_hi)
  %ret = shufflevector <8 x i32> %ret_lo, <8 x i32> %ret_hi, <16 x i32> <i32 0, i32 1, i32 2, i32 3, i32 4, i32 5, i32 6, i32 7, i32 8, i32 9, i32 10, i32 11, i32 12, i32 13, i32 14, i32 15>
  ret <16 x i32> %ret
}

declare <8 x i32> @llvm.x86.avx2.vpdpwusd.256(<8 x i32>, <8 x i32>, <8 x i32>) nounwind readnone
define <16 x i32> @__dot2add_u16i16packed(<16 x i32> %a, <16 x i32> %b, <16 x i32> %acc) nounwind readnone alwaysinline {
  %a_lo = shufflevector <16 x i32> %a, <16 x i32> undef, <8 x i32> <i32 0, i32 1, i32 2, i32 3, i32 4, i32 5, i32 6, i32 7>
  %a_hi = shufflevector <16 x i32> %a, <16 x i32> undef, <8 x i32> <i32 8, i32 9, i32 10, i32 11, i32 12, i32 13, i32 14, i32 15>
  %b_lo = shufflevector <16 x i32> %b, <16 x i32> undef, <8 x i32> <i32 0, i32 1, i32 2, i32 3, i32 4, i32 5, i32 6, i32 7>
  %b_hi = shufflevector <16 x i32> %b, <16 x i32> undef, <8 x i32> <i32 8, i32 9, i32 10, i32 11, i32 12, i32 13, i32 14, i32 15>
  %acc_lo = shufflevector <16 x i32> %acc, <16 x i32> undef, <8 x i32> <i32 0, i32 1, i32 2, i32 3, i32 4, i32 5, i32 6, i32 7>
  %acc_hi = shufflevector <16 x i32> %acc, <16 x i32> undef, <8 x i32> <i32 8, i32 9, i32 10, i32 11, i32 12, i32 13, i32 14, i32 15>
  %ret_lo = call <8 x i32> @llvm.x86.avx2.vpdpwusd.256(<8 x i32> %acc_lo, <8 x i32> %a_lo, <8 x i32> %b_lo)
  %ret_hi = call <8 x i32> @llvm.x86.avx2.vpdpwusd.256(<8 x i32> %acc_hi, <8 x i32> %a_hi, <8 x i32> %b_hi)
  %ret = shufflevector <8 x i32> %ret_lo, <8 x i32> %ret_hi, <16 x i32> <i32 0, i32 1, i32 2, i32 3, i32 4, i32 5, i32 6, i32 7, i32 8, i32 9, i32 10, i32 11, i32 12, i32 13, i32 14, i32 15>
  ret <16 x i32> %ret
}

declare <8 x i32> @llvm.x86.avx2.vpdpwusds.256(<8 x i32>, <8 x i32>, <8 x i32>) nounwind readnone
define <16 x i32> @__dot2add_u16i16packed_sat(<16 x i32> %a, <16 x i32> %b, <16 x i32> %acc) nounwind readnone alwaysinline {
  %a_lo = shufflevector <16 x i32> %a, <16 x i32> undef, <8 x i32> <i32 0, i32 1, i32 2, i32 3, i32 4, i32 5, i32 6, i32 7>
  %a_hi = shufflevector <16 x i32> %a, <16 x i32> undef, <8 x i32> <i32 8, i32 9, i32 10, i32 11, i32 12, i32 13, i32 14, i32 15>
  %b_lo = shufflevector <16 x i32> %b, <16 x i32> undef, <8 x i32> <i32 0, i32 1, i32 2, i32 3, i32 4, i32 5, i32 6, i32 7>
  %b_hi = shufflevector <16 x i32> %b, <16 x i32> undef, <8 x i32> <i32 8, i32 9, i32 10, i32 11, i32 12, i32 13, i32 14, i32 15>
  %acc_lo = shufflevector <16 x i32> %acc, <16 x i32> undef, <8 x i32> <i32 0, i32 1, i32 2, i32 3, i32 4, i32 5, i32 6, i32 7>
  %acc_hi = shufflevector <16 x i32> %acc, <16 x i32> undef, <8 x i32> <i32 8, i32 9, i32 10, i32 11, i32 12, i32 13, i32 14, i32 15>
  %ret_lo = call <8 x i32> @llvm.x86.avx2.vpdpwusds.256(<8 x i32> %acc_lo, <8 x i32> %a_lo, <8 x i32> %b_lo)
  %ret_hi = call <8 x i32> @llvm.x86.avx2.vpdpwusds.256(<8 x i32> %acc_hi, <8 x i32> %a_hi, <8 x i32> %b_hi)
  %ret = shufflevector <8 x i32> %ret_lo, <8 x i32> %ret_hi, <16 x i32> <i32 0, i32 1, i32 2, i32 3, i32 4, i32 5, i32 6, i32 7, i32 8, i32 9, i32 10, i32 11, i32 12, i32 13, i32 14, i32 15>
  ret <16 x i32> %ret
}
