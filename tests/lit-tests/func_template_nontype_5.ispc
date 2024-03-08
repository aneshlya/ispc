// Basic test for templates with enum template parameters.
// RUN: %{ispc}  %s --emit-llvm-text --target=host --nostdlib -o - | FileCheck %s

// CHECK-LABEL: define {{.*}} @enum_foo___uniCunenum_5B_Foo_5D_ZERO___uni

// CHECK-LABEL: define {{.*}} @bar_uniform___uni
// CHECK: call i32 @enum_foo___uniCunenum_5B_Foo_5D_TWO___uni

// CHECK-LABEL: define linkonce_odr i32 @enum_foo___uniCunenum_5B_Foo_5D_TWO___uni

// CHECK-LABEL: define {{.*}} @bar_varying___uni
// CHECK: call <{{[0-9]*}} x i32> @enum_foo___vyiCunenum_5B_Foo_5D_TWO___uni

// CHECK-LABEL: define linkonce_odr <{{[0-9]*}} x i32> @enum_foo___vyiCunenum_5B_Foo_5D_TWO___uni

enum Foo {
    ZERO,
    ONE,
    TWO
};

template<typename T, Foo N>
noinline T enum_foo(uniform int k) {
    return k + N;
}

template<>
noinline uniform int enum_foo<uniform int, ZERO>(uniform int k) {
    return k;
}

uniform int bar_uniform(uniform int k) {
    return enum_foo<uniform int, TWO>(k);
}

int bar_varying(uniform int k) {
    return enum_foo<varying int, TWO>(k);
}

