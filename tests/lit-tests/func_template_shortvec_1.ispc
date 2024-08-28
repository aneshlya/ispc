// RUN: %{ispc}  %s --emit-llvm-text --target=host --nostdlib -o - | FileCheck %s

// CHECK-LABEL: define {{.*}} <4 x i32> @max___vyiCuni4{{.*}}(<4 x i32> %a, <4 x i32> %b, {{.*}}
// CHECK: ret <4 x i32> %r
template <typename T, uint N>
noinline uniform int<N> max(uniform const int<N> a, uniform const int<N> b, uniform int c) {
    uniform int<N> r = { c, 2*c, 3*c, 0};
    for (uniform int i = 0; i< N; i++) {
        r.x += i + c;
    }
    r.y = a.x + b.z;
    return r;
}

uniform int<4> foo(uniform const int<4> a, uniform const int<4> b, uniform int c) {
    return max<int, 4>(a, b, c);
}

// Nested template
template <typename T, int N> noinline uniform int<N> add(uniform T c) { 
    uniform int<N> r = { c, 2*c, 3*c, 0};
    return r;
}

template <typename T, int N> noinline uniform int<N> fma(uniform T c) {
    return add<T, N>(c);
}

uniform int<4> foo(uniform int c) { return fma<varying int, 4>(c); }