#include "test_static.isph"
task void f_fu(uniform float RET[], uniform float aFOO[], uniform float b) {
    varying unsigned int32 a_max = 0xFFFFFFFF, a_min = 0; // max and min unsigned int32
    if (programIndex % 2 == 0) {
        #pragma ignore warning(perf)
        RET[programIndex] = saturating_add(a_max, b);
    }
    else {
        #pragma ignore warning(perf)
        RET[programIndex] = saturating_add(a_min, b);
    } 
}

task void result(uniform float RET[]) {
    if (programIndex % 2 == 0) {
        RET[programIndex] = (varying unsigned int32) 0xFFFFFFFF; // max unsigned int32
    }
    else {
        RET[programIndex] = (varying unsigned int32) 5;
    } 
}
