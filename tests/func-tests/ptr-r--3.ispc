#include "test_static.isph"
void foo(varying float * uniform a) {
    *a = 0;
}

task void f_fu(uniform float RET[], uniform float aFOO[], uniform float b) {
    float a = aFOO[programIndex];
    if (a >= 3)
        foo(&a);
    RET[programIndex] = a;
}

task void result(uniform float RET[4]) {
    RET[programIndex] = 0;
    RET[0] = 1;
    RET[1] = 2;
}
