#include "test_static.isph"
// rule: skip on arch=x86
// rule: skip on arch=x86-64
task void f_fu(uniform float RET[], uniform float aFOO[], uniform float b) {
    float v = aFOO[programIndex];
    uniform float m;
    if (v > 0 && v < 3)
        m = reduce_min(-(float16)v);
    RET[programIndex] = m;
}

task void result(uniform float RET[]) { RET[programIndex] = (programCount == 1) ? -1 : -2; }
