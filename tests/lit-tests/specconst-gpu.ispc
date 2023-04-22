// The test checks that specialization constants are properly supported for GPU platforms.

// REQUIRES: XE_ENABLED

// RUN: not %{ispc} %s -o %t.o --nostdlib --arch=xe64 --target=xehpg-x8 --nowrap --specconst-map %t.specconst 2>&1 | FileCheck %s
// RUN: %{ispc} %s -o %t.ll --emit-llvm-text --nostdlib --arch=xe64 --target=xehpg-x8 --enable-specconst --specconst-map %t.specconst
// RUN: FileCheck %s --input-file=%t.ll --check-prefix=CHECK_LLVMIR
// RUN: FileCheck %s --input-file=%t.specconst --check-prefix=CHECK_MAPPING

// CHECK: "specconst" qualifier is disabled.

// CHECK_LLVMIR-DAG:   declare spir_func i32 @_Z20__spirv_SpecConstantii(i32, i32)
// CHECK_LLVMIR-DAG:   declare spir_func float @_Z20__spirv_SpecConstantif(i32, float)
// CHECK_LLVMIR-DAG:   declare spir_func i8 @_Z20__spirv_SpecConstantic(i32, i8)
// CHECK_LLVMIR-LABEL: define dllexport spir_kernel void @baz
// CHECK_LLVMIR-DAG:   %C.specconst{{.*}}i1 = {{.*}}call i8 @_Z20__spirv_SpecConstantic(i32 102, i8 [[VALUE0:.*]])
// CHECK_LLVMIR-DAG:   %A.specconst{{.*}}i32 = {{.*}}call i32 @_Z20__spirv_SpecConstantii(i32 100, i32 [[VALUE1:.*]])
// CHECK_LLVMIR-DAG:   %B.specconst{{.*}}f32 = {{.*}}call float @_Z20__spirv_SpecConstantif(i32 101, float [[VALUE2:.*]])

// CHECK_MAPPING: 100:i32:A
// CHECK_MAPPING: 101:f32:B
// CHECK_MAPPING: 102:i1:C

specconst uniform uint32 A = 64U;
specconst uniform float B = 3.14F;
specconst uniform bool C = true;

task void baz(uniform float *uniform _out, uniform float *uniform _in) {
    if (C) {
#pragma unroll(16)
        foreach (i = 0 ... A) {
            _out[i] = B * _in[i];
        }
    } else {
        _out[0] = B * (uniform float)A;
    }
}
