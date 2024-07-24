// RUN: %{ispc} --target=avx2-i32x8 --nowrap -O2 --emit-asm --x86-asm-syntax=intel %s -o - | FileCheck %s --check-prefix=CHECK_AVX2x8
// RUN: %{ispc} --target=avx2vnni-i32x8 --nowrap -O2 --emit-asm --x86-asm-syntax=intel %s -o - | FileCheck %s --check-prefix=CHECK_AVX2x8

// RUN: %{ispc} --target=avx2-i32x4 --nowrap -O2 --emit-asm --x86-asm-syntax=intel %s -o - | FileCheck %s --check-prefixes=CHECK_ALL,CHECK_AVX2x4
// RUN: %{ispc} --target=avx2vnni-i32x4 --nowrap -O2 --emit-asm --x86-asm-syntax=intel %s -o - | FileCheck %s --check-prefixes=CHECK_ALL,CHECK_AVX2x4
// RUN: %{ispc} --target=avx2-i64x4 --nowrap -O2 --emit-asm --x86-asm-syntax=intel %s -o - | FileCheck %s --check-prefixes=CHECK_ALL,CHECK_AVX2x4
// RUN: %{ispc} --target=avx1-i64x4 --nowrap -O2 --emit-asm --x86-asm-syntax=intel %s -o - | FileCheck %s --check-prefixes=CHECK_ALL,CHECK_AVX2x4
// RUN: %{ispc} --target=avx1-i32x4 --nowrap -O2 --emit-asm --x86-asm-syntax=intel %s -o - | FileCheck %s --check-prefixes=CHECK_ALL,CHECK_AVX2x4
// RUN: %{ispc} --target=sse4.2-i32x4 --nowrap -O2 --emit-asm --x86-asm-syntax=intel %s -o - | FileCheck %s --check-prefixes=CHECK_ALL,CHECK_SSE4x4
// RUN: %{ispc} --target=sse4.1-i32x4 --nowrap -O2 --emit-asm --x86-asm-syntax=intel %s -o - | FileCheck %s --check-prefixes=CHECK_ALL,CHECK_SSE4x4

// REQUIRES: X86_ENABLED

// CHECK_AVX2x8-LABEL: Shuffle_i16___
// CHECK_AVX2x8-COUNT-2: vpermq
unmasked varying int16 Shuffle_i16(varying int16 a, varying int16 b, varying int32 perm) {
    return shuffle(a, b, perm);
}

// CHECK_ALL-LABEL: Shuffle_i32___
// CHECK_AVX2x8-COUNT-2: vpermd
// CHECK_AVX2x4-COUNT-2: vpermilps
// CHECK_SSE4x4-COUNT-2: pshufb
unmasked varying int32 Shuffle_i32(varying int32 a, varying int32 b, varying int32 perm) {
    return shuffle(a, b, perm);
}

// CHECK_ALL-LABEL: Shuffle_float___
// CHECK_AVX2x8-COUNT-2: vpermd
// CHECK_AVX2x4-COUNT-2: vpermilps
// CHECK_SSE4x4-COUNT-2: pshufb
unmasked varying float Shuffle_float(varying float a, varying float b, varying int32 perm) {
    return shuffle(a, b, perm);
}
