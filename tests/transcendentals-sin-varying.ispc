
export void f_du(uniform float RET[], uniform double aFOO[], uniform double b) {
  varying double arg = aFOO[programIndex] + b;
  varying double ref = sin((float)arg);
  varying double res = sin(arg);
  varying double delta = abs((res - ref) / ref);
  // native single precision sin on genx is pretty imprecise
  RET[programIndex] = delta < 1d-4 ? 0.0 : 1.0;
}

export void result(uniform float RET[]) {
    RET[programIndex] = 0.0;
}
