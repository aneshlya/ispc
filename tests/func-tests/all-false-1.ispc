#include "test_static.isph"
task void f_f(uniform float RET[], uniform float aFOO[]) {

    int errs = 0;
    int a = aFOO[programIndex];
    if (programIndex < 2) {
        // if all() doesn't return 'false', we have an error.
        if (all(a <= 1)) {
             ++errs;
        }
    }


    RET[programIndex] = errs;
}

task void result(uniform float RET[]) {
    RET[programIndex] = 0;
}
