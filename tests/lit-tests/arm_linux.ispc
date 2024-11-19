// The test checks different combinations of cpu/arch and target for linux. The target features and cpu are detected using HW flags.

// RUN: %{ispc} %s -o %t.o --nostdlib --target=neon-i32x4 --nowrap --print-target | FileCheck %s -check-prefix=CHECK_DEFAULT
// RUN: %{ispc} %s -o %t.o --nostdlib --target=neon-i32x4 --nowrap --arch=aarch64 --print-target | FileCheck %s -check-prefix=CHECK_AARCH64
// RUN: %{ispc} %s -o %t.o --nostdlib --target=neon-i32x4 --nowrap --arch=aarch64 --cpu=cortex-a15 --print-target 2>&1 | FileCheck %s -check-prefix=CHECK_AARCH64_A15
// RUN: %{ispc} %s -o %t.o --nostdlib --target=neon-i32x4 --nowrap --arch=aarch64 --cpu=cortex-a57 --print-target | FileCheck %s -check-prefix=CHECK_AARCH64_A57
// RUN: %{ispc} %s -o %t.o --nostdlib --target=neon-i32x4 --nowrap --arch=arm --print-target | FileCheck %s -check-prefix=CHECK_ARM
// RUN: %{ispc} %s -o %t.o --nostdlib --target=neon-i32x4 --nowrap --arch=arm --cpu=cortex-a15 --print-target | FileCheck %s -check-prefix=CHECK_ARM_A15
// RUN: %{ispc} %s -o %t.o --nostdlib --target=neon-i32x4 --nowrap --arch=arm --cpu=cortex-a57 --print-target | FileCheck %s -check-prefix=CHECK_ARM_A57

// REQUIRES: ARM_ENABLED && LINUX_HOST

// CHECK_DEFAULT: Triple: aarch64-unknown-linux-gnu
// CHECK_DEFAULT-NEXT: CPU: cortex-a35
// CHECK_DEFAULT-NEXT: Feature String: +aes,+crc,+fp-armv8,+neon,+sha2

// CHECK_AARCH64: Triple: aarch64-unknown-linux-gnu
// CHECK_AARCH64-NEXT: CPU: cortex-a35
// CHECK_AARCH64-NEXT: Feature String: +aes,+crc,+fp-armv8,+neon,+sha2

// CHECK_AARCH64_A15: 'cortex-a15' is not a recognized processor for this target (ignoring processor)

// CHECK_AARCH64_A57: Triple: aarch64-unknown-linux-gnu
// CHECK_AARCH64_A57-NEXT: CPU: cortex-a57
// CHECK_AARCH64_A57-NEXT: Feature String: +aes,+crc,+fp-armv8,+neon,+sha2

// CHECK_ARM: Triple: armv7-unknown-linux-gnueabihf
// CHECK_ARM-NEXT: CPU: cortex-a9
// CHECK_ARM-NEXT: Feature String: +fp16,+neon

// CHECK_ARM_A15: Triple: armv7-unknown-linux-gnueabihf
// CHECK_ARM_A15-NEXT: CPU: cortex-a15
// CHECK_ARM_A15-NEXT: Feature String: +fp16,+neon

// CHECK_ARM_A57: Triple: armv7-unknown-linux-gnueabihf
// CHECK_ARM_A57-NEXT: CPU: cortex-a57
// CHECK_ARM_A57-NEXT: Feature String: +aes,+crc,+fp-armv8,+neon,+sha2

uniform int i;

void foo() {}
