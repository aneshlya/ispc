
export void f_du(uniform float RET[], uniform double aFOO[], uniform double b) {
  varying double arg = aFOO[programIndex];
  varying float ref;
  varying float ref2;
  sincos((float)arg, &ref, &ref2);
  varying double res;
  varying double res2;
  sincos(arg, &res, &res2);
  varying double delta = abs((res - ref) / ref);
  varying double delta2 = abs((res2 - ref2) / ref2);
  // native single precision sin and cos on genx is pretty imprecise
  RET[programIndex] = (delta < 1d-4) && (delta2 < 1d-4) ? 0.0 : 1.0;
}

export void result(uniform float RET[]) {
    RET[programIndex] = 0.0;
}
