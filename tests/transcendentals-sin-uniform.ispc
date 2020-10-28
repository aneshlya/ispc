
export void f_du(uniform float RET[], uniform double aFOO[], uniform double b) {
  for (uniform int i = 0; i != programCount; ++i) {
    uniform double arg = aFOO[i] + b;
    uniform double ref = sin((float)arg);
    uniform double res = sin(arg);
    uniform double delta = abs((res - ref) / ref);
    // native single precision sin on genx is pretty imprecise
    RET[i] = delta < 1d-4 ? 0.0 : 1.0;
  }
}

export void result(uniform float RET[]) {
    RET[programIndex] = 0.0;
}
