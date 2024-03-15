// This test checks max/min limits for ISPC basic types.
// RUN: %{ispc}  %s --emit-asm --nostdlib -o - | FileCheck %s

// REQUIRES: X86_ENABLED

// CHECK-LABEL: ui8_max
// CHECK: .byte	255 # 0xff
uniform uint8 ui8_max = ISPC_UINT8_MAX;
// CHECK-LABEL: ui16_max
// CHECK: short	65535 # 0xffff
uniform uint16 ui16_max = ISPC_UINT16_MAX;
// CHECK-LABEL: ui32_max
// CHECK: .long	4294967295 # 0xffffffff
uniform uint ui32_max = ISPC_UINT_MAX;
// CHECK-LABEL: ui64_max
// CHECK: .quad	-1 # 0xffffffffffffffff
uniform uint64 ui64_max = ISPC_UINT64_MAX;

// CHECK-LABEL: i8_max
// CHECK: .byte	127 # 0x7f
uniform int8 i8_max = ISPC_INT8_MAX;
// CHECK-LABEL: i16_max
// CHECK: .short 32767 # 0x7fff
uniform int16 i16_max = ISPC_INT16_MAX;
// CHECK-LABEL: i32_max
// CHECK: .long	2147483647 # 0x7fffffff
uniform int i32_max = ISPC_INT_MAX;
// CHECK-LABEL: i64_max
// CHECK: .quad	9223372036854775807 # 0x7fffffffffffffff
uniform int64 i64_max = ISPC_INT64_MAX;

// CHECK-LABEL: i8_min
// CHECK: .byte	128 # 0x80
uniform int8 i8_min = ISPC_INT8_MIN;
// CHECK-LABEL: i16_min
// CHECK: .short 32768 # 0x8000
uniform int16 i16_min = ISPC_INT16_MIN;
// CHECK-LABEL: i32_min
// CHECK: .long	2147483648 # 0x80000000
uniform int i32_min = ISPC_INT_MIN;
// CHECK-LABEL: i64_min
// CHECK: .quad	-9223372036854775808 # 0x8000000000000000
uniform int64 i64_min = ISPC_INT64_MIN;

// CHECK-LABEL: f16_max
// CHECK: .short 0x7bff # half 65504
uniform float16 f16_max = ISPC_FP16_MAX;
// CHECK-LABEL: f32_max
// CHECK: .long	0x7f7fffff # float 3.40282347E+38
uniform float f32_max = ISPC_FP_MAX;
// CHECK-LABEL: f64_max
// CHECK: .quad	0x7fefffffffffffff # double 1.7976931348623157E+308
uniform double f64_max = ISPC_FP64_MAX;

// CHECK-LABEL: f16_min
// CHECK: .short	0x0400 # half 6.1035E-5
uniform float16 f16_min = ISPC_FP16_MIN;
// CHECK-LABEL: f32_min
// CHECK: .long	0x00800000 # float 1.17549435E-38
uniform float f32_min = ISPC_FP_MIN;
// CHECK-LABEL: f64_min
// CHECK: .quad	0x0010000000000000 # double 2.2250738585072014E-308
uniform double f64_min = ISPC_FP64_MIN;
