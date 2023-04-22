// The test checks that specialization constants are properly supported for CPU platforms.

// RUN: %{ispc} %s -o %t.o --nostdlib --target=host --nowrap 2>&1
// RUN: %{ispc} %s -o /dev/null --debug-phase=first --emit-llvm-text --nostdlib --target=host | FileCheck %s --check-prefix=CHECK_LLVMIR
// RUN: not FileCheck %s --input-file=%s.specconst

// CHECK_LLVMIR-LABEL: define void @baz
// CHECK_LLVMIR:           load i32, ptr @A
// CHECK_LLVMIR:           load float, ptr @B
// CHECK_LLVMIR:           load i8, ptr @C

specconst uniform uint32 A = 64U;
specconst uniform float B = 3.14F;
specconst uniform bool C = true;

export void baz(uniform float *uniform _out, uniform float *uniform _in) {
    if (C) {
#pragma unroll(16)
        foreach (i = 0 ... A) {
            _out[i] = B * _in[i];
        }
    } else {
        _out[0] = B * (uniform float)A;
    }
}
