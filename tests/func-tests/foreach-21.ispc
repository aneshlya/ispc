#include "test_static.isph"
uniform int foo(int i);

task void f_f(uniform float RET[], uniform float aFOO[]) {
    for (uniform int i = 0; i < programCount; ++i)
        RET[i] = 0;

    foreach (i = 0 ... programCount) {
        #pragma ignore warning(perf)
        if (i % 2)
            continue;
        RET[i] = 1;
    }
}

task void result(uniform float RET[]) {
    RET[programIndex] = (programIndex % 2) ? 0 : 1;
}
