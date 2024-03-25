// Test checks emitted code for VNNI dot product instructions.

// RUN: %{ispc} %s --target=avx512skx-x32 --cpu=icl --emit-asm -o - | FileCheck %s -check-prefixes=CHECK_ALL,CHECK_ZMMX2
// RUN: %{ispc} %s --target=avx512skx-x32 --cpu=icx --emit-asm -o - | FileCheck %s -check-prefixes=CHECK_ALL,CHECK_ZMMX2
// RUN: %{ispc} %s --target=avx512skx-x32 --cpu=tgl --emit-asm -o - | FileCheck %s -check-prefixes=CHECK_ALL,CHECK_ZMMX2
// RUN: %{ispc} %s --target=avx512spr-x32 --emit-asm -o - | FileCheck %s -check-prefixes=CHECK_ALL,CHECK_ZMMX2

// RUN: %{ispc} %s --target=avx512skx-x64 --cpu=icl --emit-asm -o - | FileCheck %s -check-prefixes=CHECK_ALL,CHECK_ZMMX4
// RUN: %{ispc} %s --target=avx512skx-x64 --cpu=icx --emit-asm -o - | FileCheck %s -check-prefixes=CHECK_ALL,CHECK_ZMMX4
// RUN: %{ispc} %s --target=avx512skx-x64 --cpu=tgl --emit-asm -o - | FileCheck %s -check-prefixes=CHECK_ALL,CHECK_ZMMX4
// RUN: %{ispc} %s --target=avx512spr-x64 --emit-asm -o - | FileCheck %s -check-prefixes=CHECK_ALL,CHECK_ZMMX4

// REQUIRES: X86_ENABLED && !MACOS_HOST
// XFAIL: LLVM_17_0+

// CHECK_ALL-LABEL: dot4add_u8i8
// CHECK_ZMMX2-COUNT-2: vpdpbusd {{.*}} %zmm
// CHECK_ZMMX4-COUNT-4: vpdpbusd {{.*}} %zmm
void dot4add_u8i8(uniform int a[], uniform int b[], uniform int dst[]) {
    dst[programIndex] = dot4add_u8i8packed(a[programIndex], b[programIndex], programIndex);
}

// CHECK_ALL-LABEL: dot4add_u8i8_sat
// CHECK_ZMMX2-COUNT-2: vpdpbusds {{.*}} %zmm
// CHECK_ZMMX4-COUNT-4: vpdpbusds {{.*}} %zmm
void dot4add_u8i8_sat(uniform int a[], uniform int b[], uniform int dst[]) {
    dst[programIndex] = dot4add_u8i8packed_sat(a[programIndex], b[programIndex], programIndex);
}

// CHECK_ALL-LABEL: dot4add_i16
// CHECK_ZMMX2-COUNT-2: vpdpwssd {{.*}} %zmm
// CHECK_ZMMX4-COUNT-4: vpdpwssd {{.*}} %zmm
void dot4add_i16(uniform int a[], uniform int b[], uniform int dst[]) {
    dst[programIndex] = dot4add_i16packed(a[programIndex], b[programIndex], programIndex);
}

// CHECK_ALL-LABEL: dot4add_i16_sat
// CHECK_ZMMX2-COUNT-2: vpdpwssds {{.*}} %zmm
// CHECK_ZMMX4-COUNT-4: vpdpwssds {{.*}} %zmm
void dot4add_i16_sat(uniform int a[], uniform int b[], uniform int dst[]) {
    dst[programIndex] = dot4add_i16packed_sat(a[programIndex], b[programIndex], programIndex);
}
