#include "test_static.isph"
struct Foo {
    float x;
};

task void f_fu(uniform float RET[], uniform float aFOO[], uniform float b) {
    float a = aFOO[programIndex];
    uniform Foo struct_1;
    struct_1.x = b;
    varying Foo struct_2 = struct_1;
    RET[programIndex] = struct_2.x;
}


task void result(uniform float RET[]) {
    RET[programIndex] = 5;
}
