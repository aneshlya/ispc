#include "test_static.isph"
task void f_fu(uniform float RET[], uniform float aFOO[], uniform float b) {
    uint64 a = aFOO[programIndex]; 
    int reverse = programCount - 1 - programIndex + (int)b - 5;
    uint64 shuf = shuffle(a, reverse);
    RET[programIndex] = shuf;
}

task void result(uniform float RET[]) {
    RET[programIndex] = programCount - programIndex;
}
