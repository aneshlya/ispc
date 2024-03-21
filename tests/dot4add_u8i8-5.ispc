#include "../test_static.isph"
#define N 256

varying int32 test(varying int32 a, varying int32 b, varying int32 acc) {
    int16 tmp1 = (int16)((uint16)(((uint8)((a >> 24) & 0xFF))) * (int16)((int8)((b >> 24) & 0xFF)));
    int16 tmp2 = (int16)((uint16)(((uint8)((a >> 16) & 0xFF))) * (int16)((int8)((b >> 16) & 0xFF)));
    int16 tmp3 = (int16)((uint16)(((uint8)((a >> 8) & 0xFF))) * (int16)((int8)((b >> 8) & 0xFF)));
    int16 tmp4 = (int16)((uint16)(((uint8)(a & 0xFF))) * (int16)((int8)(b & 0xFF)));
    return (int32)tmp1 + (int32)tmp2 + (int32)tmp3 + (int32)tmp4 + acc;
}

void init(uniform uint8 a[], uniform int8 b[]) {
    for (uniform int i = 0; i < N; i++) {
        a[i] = (uniform uint8)i;
        b[i] = (uniform int8)i;
    }
}
task void f_dot4add_u8i8(uniform int dst[]) {
    uniform uint8 a[N];
    uniform int8 b[N];
    init(a, b);
    uniform int a_packed[N/4];
    pack4toint<uniform uint8>(a, a_packed, N);
    uniform int b_packed[N/4];
    pack4toint<uniform int8>(b, b_packed, N);
    int acc = 0X80000000 +1;
    foreach (i = 0 ... N) {
        dst[i] = test(a_packed[i], b_packed[i], acc);
    }
}

task void result_u8i8(uniform int dst[]) {
    uniform int acc = 0X80000000 +1;
    uniform uint8 a[N];
    uniform int8 b[N];
    init(a, b);
    for (uniform int i = 0; i < N*4; i += 4) {
        uniform int result = 0;
        for (uniform int j = 0; j < 4; ++j) {
            uniform int16 product = (uniform int16)(uniform uint16)(a[i + j]) * (uniform int16)(b[i + j]);
            result += (uniform int32)(product);
        }
        dst[i / 4] = result + acc;
    }
}