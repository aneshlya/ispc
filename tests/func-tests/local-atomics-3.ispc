#include "test_static.isph"
uniform int32 s = 0xff;

task void f_f(uniform float RET[], uniform float aFOO[]) {
    float a = aFOO[programIndex]; 
    int32 bits = 0xfff0;
    float b = atomic_xor_local(&s, bits);
    RET[programIndex] = s;
}

task void result(uniform float RET[]) {
    RET[programIndex] = (programCount & 1) ? 0xff0f : 0xff;
}
