#include "test_static.isph"
task void f_fu(uniform float RET[], uniform float aFOO[], uniform float b) {
    float v = aFOO[programIndex];
    uniform uint16 m;
    if (v < 3) {
        #pragma ignore warning(perf)
        m = reduce_max((unsigned int16)v);
    }
    RET[programIndex] = m;
}

task void result(uniform float RET[]) { RET[programIndex] = (programCount == 1) ? 1 : 2; }
