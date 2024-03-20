#include "../test_static.isph"
#define N 64

task void f_dot4add_u8i8(uniform int a[], uniform int b[], uniform int dst[]) {
    foreach (i = 0 ... N) {
        dst[i] = dot4add_u8i8packed(a[i], b[i], i);
    }    
}

task void result_u8i8(uniform uint8 a[], uniform int8 b[], uniform int dst[]) {
    for (uniform int i = 0; i < N*4; i += 4) {
        uniform int result = 0;
        for (uniform int j = 0; j < 4; ++j) {
            uniform int16 product = (uniform int16)(uniform uint16)(a[i + j]) * (uniform int16)(b[i + j]);
            result += (uniform int32)(product);
        }
        dst[i / 4] = result + i/4;
    }
}
