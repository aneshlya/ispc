#include "test_static.isph"
task void f_fu(uniform float RET[], uniform float aFOO[], uniform float b) {
    #pragma ignore warning(perf)
    unsigned int8 a = aFOO[programIndex&1];
    RET[programIndex] = 1;
    if (programIndex & 1)
        RET[programIndex] = reduce_equal(a) ? 1 : 0; 
}

task void result(uniform float RET[]) {
    RET[programIndex] = 1;
}
