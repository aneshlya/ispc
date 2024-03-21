#include "../test_static.isph"
#define N 256

varying int32 test(varying int32 a, varying int32 b, varying int32 acc) {
    int32 tmp1 = (int32)((int16)((a >> 16) & 0xFFFF) * (int16)((b >> 16) & 0xFFFF));
    int32 tmp2 = (int32)((int16)(a & 0xFFFF) * (int16)(b & 0xFFFF));
    return tmp1 + tmp2 + acc;
}

void init(uniform int16 a[], uniform int16 b[]) {
    for (uniform int i = 0; i < N; i++) {
        a[i] = (uniform int16)i;
        b[i] = (uniform int16)i;
    }
}
task void f_v(uniform float dst[]) {
    uniform int16 a[N];
    uniform int16 b[N];
    init(a, b);
    uniform int a_packed[N/2];
    pack2toint<uniform int16>(a, a_packed, N);
    uniform int b_packed[N/2];
    pack2toint<uniform int16>(b, b_packed, N);

    foreach (i = 0 ... N) {
        dst[i] = test(a_packed[i], b_packed[i], i);
    }
}

task void result(uniform float dst[]) {
    uniform int acc = 1;
    uniform int16 a[N];
    uniform int16 b[N];
    init(a, b);
    for (uniform int i = 0; i < N*2; i += 2) {
        uniform int result = 0;
        for (uniform int j = 0; j < 2; ++j) {
            uniform int32 product = (uniform int32)(a[i + j]) * (uniform int32)(b[i + j]);
            result += (uniform int32)(product);
        }
        dst[i / 2] = result + i /2;
    }
}
