#include "test_static.isph"
task void f_f(uniform float RET[], uniform float aFOO[]) {
    uniform unsigned int64 a[programCount];
    #pragma ignore warning(perf)
    a[programIndex] = aFOO[programIndex];
    unsigned int64 aa;
    packed_load_active(a, &aa);
    #pragma ignore warning(perf)
    RET[programIndex] = aa;
}

task void result(uniform float RET[]) {
    RET[programIndex] = 1+programIndex;
}
