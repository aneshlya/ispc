// Verifies that casting a SOA element address to integer doesn't crash the compiler.
// This was a regression where slice pointers (used for SOA) were not properly handled
// in pointer-to-integer casts. The fix extracts the base pointer and offset from the
// slice pointer struct, converts them, and adds them together.

// RUN: %{ispc} %s --target=host --nostdlib --nowrap --emit-llvm-text -o - | FileCheck %s

struct Point { uint64 x; float y; float z; };

// Test uniform slice pointer to uniform integer
// CHECK-LABEL: define i64 @"gp___
// CHECK: extractvalue { ptr, i32 }
// CHECK: extractvalue { ptr, i32 }
// CHECK: ptrtoint ptr
// CHECK: {{zext|sext}}
// CHECK: add {{.*}}i64
// CHECK: ret i64

uniform uint64 gp(soa<16> Point v[])
{
    return (uint64)&v[0].x;
}

// Test uniform slice pointer to varying integer
// CHECK-LABEL: define <{{[0-9]+}} x i64> @"vp___
// CHECK: extractvalue { ptr, i32 }
// CHECK: extractvalue { ptr, i32 }
// CHECK: ptrtoint ptr {{.*}} to i64
// CHECK: insertelement <{{[0-9]+}} x i64> poison, i64
// CHECK: shufflevector <{{[0-9]+}} x i64>
// CHECK: insertelement <{{[0-9]+}} x i32> poison, i32
// CHECK: shufflevector <{{[0-9]+}} x i32>
// CHECK: {{zext|sext}}{{.*}}<{{[0-9]+}} x i32>{{.*}}to <{{[0-9]+}} x i64>
// CHECK: add <{{[0-9]+}} x i64>
// CHECK: ret <{{[0-9]+}} x i64>

varying uint64 vp(soa<16> Point v[])
{
    return (varying uint64)&v[0].x;
}
