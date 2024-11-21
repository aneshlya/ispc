// RUN: %{ispc} --nowrap --target=neon-i16x8 --arch=arm --emit-asm -o %t.s %s 2>&1 | FileCheck %s --check-prefix=NEON_i16x8
// RUN: %{ispc} --nowrap --target=neon-i8x16 --arch=arm --emit-asm -o %t.s %s 2>&1 | FileCheck %s --check-prefix=NEON_i8x16

// REQUIRES: ARM_ENABLED && !MACOS_HOST

// NEON_i16x8: Warning: The target neon-i16x8 is deprecated and will be removed in the future.
// NEON_i8x16: Warning: The target neon-i8x16 is deprecated and will be removed in the future.

void foo() {}