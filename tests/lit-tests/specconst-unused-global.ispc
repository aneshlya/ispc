// RUN: %{ispc} %s -o %t.ll --dump-file=%t --debug-phase=first --nostdlib --emit-llvm-text --target=xehpg-x8 --enable-specconst --specconst-map /dev/null
// RUN: FileCheck %s --input-file=%t/ir_0_VerifierPass.ll

// REQUIRES: XE_ENABLED

// CHECK-NOT: global i1
// CHECK-NOT: global i8
// CHECK-NOT: global i16
// CHECK-NOT: global i32
// CHECK-NOT: global i64
// CHECK-NOT: global i32
// CHECK-NOT: global half
// CHECK-NOT: global float
// CHECK-NOT: global double

specconst uniform bool A = false;

specconst uniform int8 B0 = 0;
specconst uniform uint8 B1 = 0;
specconst uniform int16 C0 = 0;
specconst uniform uint16 C1 = 0;
specconst uniform int32 D0 = 0;
specconst uniform uint32 D1 = 0;
specconst uniform int64 E0 = 0;
specconst uniform uint64 E1 = 0;

specconst uniform float16 F0 = 0;
specconst uniform float F1 = 0;
specconst uniform double F2 = 0;

uniform bool foo_A() { return A; }

uniform int8 foo_B0() { return B0; }
uniform uint8 foo_B1() { return B1; }
uniform int16 foo_C0() { return C0; }
uniform uint16 foo_C1() { return C1; }
uniform int32 foo_D0() { return D0; }
uniform uint32 foo_D1() { return D1; }
uniform int64 foo_E0() { return E0; }
uniform uint64 foo_E1() { return E1; }

uniform float16 foo_F0() { return F0; }
uniform float foo_F1() { return F1; }
uniform double foo_F2() { return F2; }
