#include "test_static.isph"
// rule: skip on arch=xe64

struct Point {
    float x, y, z;
};

task void f_fu(uniform float RET[], uniform float aFOO[], uniform float b) {
    float a = aFOO[programIndex]; 
    varying Point * buf = new varying Point(0., b, a);
    #pragma ignore warning(perf)
    RET[programIndex] = buf->z;
    delete buf;
}

task void result(uniform float RET[]) {
    RET[programIndex] = 1+programIndex;
}
