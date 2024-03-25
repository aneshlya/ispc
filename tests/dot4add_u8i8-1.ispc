#include "../test_static.isph"
#define N 256

void init(uniform uint8 a[], uniform int8 b[]) {
    for (uniform int i = 0; i < N; i++) {
        a[i] = i % 128;
        b[i] = i % 128;
    }
}
task void f_v(uniform float dst[]) {
    uniform uint8 a[N];
    uniform int8 b[N];
    init(a, b);
    uniform int a_packed[N / 4];
    pack4toint<uniform uint8>(a, a_packed, N);
    uniform int b_packed[N / 4];
    pack4toint<uniform int8>(b, b_packed, N);

    foreach (i = 0 ... N) {
        dst[i] = dot4add_u8i8packed(a_packed[i], b_packed[i], i);
    }
}

task void result(uniform float dst[]) {
    uniform int acc = 1;
    uniform uint8 a[N];
    uniform int8 b[N];
    init(a, b);
    for (uniform int i = 0; i < N * 4; i += 4) {
        uniform int result = 0;
        for (uniform int j = 0; j < 4; ++j) {
            uniform int16 product = (uniform int16)(uniform uint16)(a[i + j]) * (uniform int16)(b[i + j]);
            result += (uniform int32)(product);
        }
        dst[i / 4] = result + i / 4;
    }
}