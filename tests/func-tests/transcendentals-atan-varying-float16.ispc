#include "test_static.isph"
// rule: skip on arch=x86
// rule: skip on arch=x86-64
bool ok(float16 x, float16 ref) { return (abs(x - ref) < 1e-7) || abs((x-ref)/ref) < 1e-6; }
task void f_fu(uniform float RET[], uniform float aFOO[], uniform float b) {
  varying float16 arg = aFOO[programIndex];
  varying float16 ref = atan((float)arg);
  varying float16 res = atan(arg);
  RET[programIndex] = ok(res, ref) ? 0. : 1.;
}

task void result(uniform float RET[]) {
    RET[programIndex] = 0.0;
}
