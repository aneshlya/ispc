#include "test_static.isph"
static float float4(uniform float a, uniform float b, uniform float c, 
                    uniform float d) {
    float ret = 0;
    for (uniform int i = 0; i < programCount; i += 4) {
        ret = insert(ret, i + 0, a);
        ret = insert(ret, i + 1, b);
        ret = insert(ret, i + 2, c);
        ret = insert(ret, i + 3, d);
    }
    return ret;
}



bool ok(float x, float ref) { return (abs(x - ref) < 1e-6) || abs((x-ref)/ref) < 1e-4; }

task void f_v(uniform float RET[]) {
    float v = float4((-9.424777984619141),(4.000000000000000),(10.000000000000000),(-10.000000000000000));
    float ref = float4((-0.000000023849761),(1.157821297645569),(0.648360848426819),(-0.648360848426819));
    RET[programIndex] = ok(tan(v), ref) ? 1. : 0.;
}
task void result(uniform float RET[]) { RET[programIndex] = 1.; }
