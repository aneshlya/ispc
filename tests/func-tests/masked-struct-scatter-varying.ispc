#include "test_static.isph"
struct Foo { float x; float y; };

task void f_fu(uniform float ret[], uniform float aa[], uniform float b) {
    float a = aa[programIndex];
    Foo foo[programCount+1];
    for (uniform int i = 0; i < programCount+1; ++i)
        foo[i].x = i;
    #pragma ignore warning(perf)
    varying Foo fv = foo[a];
    fv.x += 1000;
//CO    print("fv.x = %\n", fv.x);
//CO    print("foo[a] = %\n", foo[a].x);
    #pragma ignore warning(perf)
    foo[a] = fv;
//CO    print("after assign foo[a] = %\n", foo[a].x);

    #pragma ignore warning(perf)
    ret[programIndex] = foo[a].x;
//CO    print("% - %\n", programIndex, ret[programIndex]);
}

task void result(uniform float ret[]) {
    ret[programIndex] = 1001+programIndex;
}
