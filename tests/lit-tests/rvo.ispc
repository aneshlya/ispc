// This test checks LLVM IR in case when function returns structure by value.
// A structure that is returned by value forces the compiler to allocate
// additional space for the return structure in the caller and then the
// address of that structure is passed as a hidden first parameter to the function.

// RUN: %{ispc} %s --target=genx-x8 --nowrap --no-discard-value-names --emit-llvm-text -o - | FileCheck %s -check-prefix=CHECK_RVO
// RUN: %{ispc} %s --target=sse4 --nowrap --no-discard-value-names --emit-llvm-text -o - | FileCheck %s -check-prefix=CHECK_NO_RVO

// REQUIRES: GENX_ENABLED

struct S {
    int x;
    int y;
};

// CHECK_RVO: void @getStruct{{.*}}(%v{{[0-9]+}}_varying_S* noalias nocapture %s.ret
// CHECK_RVO-NOT: %return_value_memory
// CHECK_RVO-NOT: ret %v{{[0-9]+}}_varying_S*

// CHECK_NO_RVO: %v{{[0-9]+}}_varying_S @getStruct{{.*}}
// CHECK_NO_RVO: %return_value_memory
// CHECK_NO_RVO: ret %v{{[0-9]+}}_varying_S
noinline S getStruct(int num) {
    S s;
    s.x = s.y = 5 + num;
    if (programIndex % 2) {
        s.x = s.y * num;
        return s;
    }
    return s;
}

typedef S (*FuncType)(int);

// CHECK_RVO: void @getStructFunc{{.*}}(%v{{[0-9]+}}_varying_S* noalias nocapture %s.ret
// CHECK_RVO-NOT: %return_value_memory
// CHECK_RVO-NOT: ret %v{{[0-9]+}}_varying_S*

// CHECK_NO_RVO: %v{{[0-9]+}}_varying_S @getStructFunc{{.*}}
// CHECK_NO_RVO: %return_value_memory
// CHECK_NO_RVO: ret %v{{[0-9]+}}_varying_S
noinline S getStructFunc(int num) {
    S s;
    s.x = s.y = 10 + num;
    if (programIndex < 2) {
        s.x = s.y * num;
        return s;
    }
    return s;
}

export void f_fu(uniform float RET[], uniform float aFOO[], uniform float b) {
    float a = aFOO[programIndex];
    uniform FuncType func_uniform = getStructFunc;
    // CHECK_RVO: void @getStructFunc{{.*}}(%v{{[0-9]+}}_varying_S*
    // CHECK_RVO-NOT: call %v{{[0-9]+}}_varying_S @getStructFunc{{.*}}
    // CHECK_NO_RVO: call %v{{[0-9]+}}_varying_S @getStructFunc{{.*}}
    S s1 = func_uniform(b);
    RET[programIndex + programCount] = s1.x + s1.y;

    FuncType func_varying = getStructFunc;
    // CHECK_RVO: void @getStructFunc{{.*}}(%v{{[0-9]+}}_varying_S*
    // CHECK_RVO-NOT: call %v{{[0-9]+}}_varying_S @getStructFunc{{.*}}
    // CHECK_NO_RVO: call %v{{[0-9]+}}_varying_S @getStructFunc{{.*}}
    S s2 = func_varying(b);
    RET[programIndex + programCount] = s2.x + s2.y;

    S s3;
    // CHECK_RVO: void @getStruct{{.*}}(%v{{[0-9]+}}_varying_S*
    // CHECK_RVO-NOT: call %v{{[0-9]+}}_varying_S @getStruct{{.*}}
    // CHECK_NO_RVO: call %v{{[0-9]+}}_varying_S @getStruct{{.*}}
    if (programIndex > 5)
        s3 = getStruct(b);
    else {
        // CHECK_RVO: void @getStruct{{.*}}(%v{{[0-9]+}}_varying_S*
        // CHECK_RVO-NOT: call %v{{[0-9]+}}_varying_S @getStruct{{.*}}
        // CHECK_NO_RVO: call %v{{[0-9]+}}_varying_S @getStruct{{.*}}
        s3 = getStruct(a);
    }
    RET[programIndex] = s3.x + s3.y;
}