// rule: skip on arch=x86
// rule: skip on arch=x86-64
export void f_f(uniform float RET[], uniform float aFOO[]) {
    float16 a = aFOO[programIndex];
    RET[programIndex] = clamp((float16)3 * a, 5.f16, 10.f16);
}

export void result(uniform float RET[]) {
    RET[programIndex] = 10;
    RET[0] = 5;
    RET[1] = 6;
    RET[2] = 9;
}
