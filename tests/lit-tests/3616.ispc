// Verifies that casting a SOA element address to integer doesn't crash the compiler.
// This was a regression where slice pointers (used for SOA) were not properly handled
// in pointer-to-integer casts. The fix extracts the base pointer and offset from the
// slice pointer struct, converts them, and adds them together.

// RUN: %{ispc} %s --target=host --nostdlib --nowrap --emit-llvm-text -o - | FileCheck %s

struct Point { uint64 x; float y; float z; };

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
