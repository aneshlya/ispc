#include "test_static.isph"
// rule: skip on cpu=tgllp
// rule: skip on cpu=dg2
task void f_f(uniform float RET[], uniform float aFOO[]) {
    double a;
    for (uniform int i = 0; i < programCount; ++i)
        a = insert(a, i, (double)i+10);
    RET[programIndex] = a; 
}

task void result(uniform float RET[]) {
    RET[programIndex] = programIndex+10;
}
