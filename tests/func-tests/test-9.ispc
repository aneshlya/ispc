#include "test_static.isph"
task void f_fu(uniform float RET[], uniform float aFOO[], uniform float b) {
    float a = aFOO[programIndex];
    float r = a;
    float ret;
    if (b == 5.) ret = b;
    else ret = r;
    RET[programIndex] = ret;
}


task void result(uniform float RET[]) {
    RET[programIndex] = 5;
}
