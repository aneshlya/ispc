#include "test_static.isph"
task void f_fu(uniform float RET[], uniform float aFOO[], uniform float b) {
    float a = aFOO[programIndex];
    RET[programIndex] = extract(a, min((uniform int)b-3, programCount-1));
}


task void result(uniform float RET[]) {
    RET[programIndex] = (programCount == 1) ? 1 : 3;
}
