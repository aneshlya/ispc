#include "test_static.isph"
task void f_fu(uniform float RET[], uniform float aFOO[], uniform float b) {
    float a = aFOO[programIndex];
    float r = a;
    if (a <= 2.) { RET[programIndex] = b; return; }
    RET[programIndex] = r;
}


task void result(uniform float RET[]) {
    RET[programIndex] = 1+programIndex;
    RET[0] = RET[1] = 5;
}
