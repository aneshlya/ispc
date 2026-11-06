// Test checks emitted code for ARM (aarch64) dot product instructions.

// RUN: %{ispc} %s --arch=aarch64 --target=neon-i32x4 --cpu=cortex-a55 --emit-asm  -o - | FileCheck %s --check-prefixes=CHECK_ALL,CHECK_DOT_ONLY
// RUN: %{ispc} %s --arch=aarch64 --target=neon-i32x4 --cpu=cortex-a510 --emit-asm  -o - | FileCheck %s --check-prefixes=CHECK_ALL,CHECK_MIXED_SIGN

// RUN: %{ispc} %s --arch=aarch64 --target=neon-i32x8 --cpu=cortex-a55 --emit-asm  -o - | FileCheck %s --check-prefixes=CHECK_ALL,CHECK_DOT_ONLY
// RUN: %{ispc} %s --arch=aarch64 --target=neon-i32x8 --cpu=cortex-a510 --emit-asm  -o - | FileCheck %s --check-prefixes=CHECK_ALL,CHECK_MIXED_SIGN

// Test that on HW not supporting dot product natively *dot instructions are not produced.
// RUN: %{ispc} %s --arch=aarch64 --target=neon-i32x4 --cpu=cortex-a35 --emit-asm  -o - | FileCheck %s --check-prefix=CHECK_NO_HW_DOT
// RUN: %{ispc} %s --arch=aarch64 --target=neon-i32x4 --cpu=cortex-a53 --emit-asm  -o - | FileCheck %s --check-prefix=CHECK_NO_HW_DOT
// RUN: %{ispc} %s --arch=aarch64 --target=neon-i32x8 --cpu=cortex-a35 --emit-asm  -o - | FileCheck %s --check-prefix=CHECK_NO_HW_DOT
// RUN: %{ispc} %s --arch=aarch64 --target=neon-i32x8 --cpu=cortex-a53 --emit-asm  -o - | FileCheck %s --check-prefix=CHECK_NO_HW_DOT

// REQUIRES: ARM_ENABLED

// CHECK_ALL-LABEL: dot4add_u8u8___
// CHECK_ALL: udot

// CHECK_ALL-LABEL: dot4add_i8i8___
// CHECK_ALL: sdot

// CHECK_MIXED_SIGN-LABEL: dot4add_u8i8___
// CHECK_MIXED_SIGN: usdot

// Without i8mm, the mixed-sign forms are built from sdot.
// CHECK_DOT_ONLY-LABEL: dot4add_u8i8___
// CHECK_DOT_ONLY-COUNT-2: {{[[:space:]]}}sdot
// CHECK_DOT_ONLY-LABEL: dot4add_u8i8_sat___
// CHECK_DOT_ONLY-COUNT-2: {{[[:space:]]}}sdot

// CHECK_MIXED_SIGN-LABEL: dot4add_u8i8_sat___
// CHECK_MIXED_SIGN: usdot

// The saturating forms add the plain dot product to acc with one saturation.
// CHECK_ALL-LABEL: dot4add_u8u8_sat___
// CHECK_ALL: udot
// CHECK_ALL-LABEL: dot4add_i8i8_sat___
// CHECK_ALL: {{[[:space:]]}}sdot

// CHECK_NO_HW_DOT-NOT: udot
// CHECK_NO_HW_DOT-NOT: sdot
// CHECK_NO_HW_DOT-NOT: usdot
void dot4add_u8u8(uniform uint a[], uniform uint b[], uniform uint dst[]) {
    dst[programIndex] = dot4add_u8u8packed(a[programIndex], b[programIndex], programIndex);
}

void dot4add_i8i8(uniform uint a[], uniform uint b[], uniform int dst[]) {
    dst[programIndex] = dot4add_i8i8packed(a[programIndex], b[programIndex], programIndex);
}

void dot4add_u8i8(uniform uint a[], uniform uint b[], uniform int dst[]) {
    dst[programIndex] = dot4add_u8i8packed(a[programIndex], b[programIndex], programIndex);
}

void dot4add_u8i8_sat(uniform uint a[], uniform uint b[], uniform int dst[]) {
    dst[programIndex] = dot4add_u8i8packed_sat(a[programIndex], b[programIndex], programIndex);
}

void dot4add_u8u8_sat(uniform uint a[], uniform uint b[], uniform uint dst[]) {
    dst[programIndex] = dot4add_u8u8packed_sat(a[programIndex], b[programIndex], programIndex);
}

void dot4add_i8i8_sat(uniform uint a[], uniform uint b[], uniform int dst[]) {
    dst[programIndex] = dot4add_i8i8packed_sat(a[programIndex], b[programIndex], programIndex);
}
