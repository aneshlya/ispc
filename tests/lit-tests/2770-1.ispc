// RUN: %{ispc} --target=avx512skx-x4 --nowrap -O2 --emit-asm --x86-asm-syntax=intel %s -o - | FileCheck %s
// RUN: %{ispc} --target=avx512icl-x4 --nowrap -O2 --emit-asm --x86-asm-syntax=intel %s -o - | FileCheck %s
// RUN: %{ispc} --target=avx512spr-x4 --nowrap -O2 --emit-asm --x86-asm-syntax=intel %s -o - | FileCheck %s
// RUN: %{ispc} --target=avx512skx-x8 --nowrap -O2 --emit-asm --x86-asm-syntax=intel %s -o - | FileCheck %s
// RUN: %{ispc} --target=avx512icl-x8 --nowrap -O2 --emit-asm --x86-asm-syntax=intel %s -o - | FileCheck %s
// RUN: %{ispc} --target=avx512spr-x8 --nowrap -O2 --emit-asm --x86-asm-syntax=intel %s -o - | FileCheck %s
// RUN: %{ispc} --target=avx512skx-x16 --nowrap -O2 --emit-asm --x86-asm-syntax=intel %s -o - | FileCheck %s
// RUN: %{ispc} --target=avx512icl-x16 --nowrap -O2 --emit-asm --x86-asm-syntax=intel %s -o - | FileCheck %s
// RUN: %{ispc} --target=avx512spr-x16 --nowrap -O2 --emit-asm --x86-asm-syntax=intel %s -o - | FileCheck %s

// REQUIRES: X86_ENABLED && !MACOS_HOST

// CHECK-LABEL: Shuffle_i32___
// CHECK: vpermt2d
unmasked varying int32 Shuffle_i32(varying int32 a, varying int32 b, varying int32 perm) {
    return shuffle(a, b, perm);
}

// CHECK-LABEL: Shuffle_float___
// CHECK: vpermt2ps
unmasked varying float Shuffle_float(varying float a, varying float b, varying int32 perm) {
    return shuffle(a, b, perm);
}
