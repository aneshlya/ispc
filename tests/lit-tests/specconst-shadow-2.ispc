// The test checks that the assertion is replaced with an error

// RUN: not %{ispc} -DTEST1 %s -o %t.o --nostdlib --target=host --enable-specconst --nowrap 2>&1 | FileCheck %s
// RUN: not %{ispc} -DTEST2 %s -o %t.o --nostdlib --target=host --enable-specconst --nowrap 2>&1 | FileCheck %s

// CHECK: Specialization constants must be declared globally (for variable "A")

#ifdef TEST1
specconst uniform uint32 A = 64U;
task void baz(uniform float *uniform _out, uniform float *uniform _in) {
    specconst uniform uint32 A = 64U;
    _out[0] = _in[0] + A;
}
#endif

#ifdef TEST2
const uniform uint32 A = 64U;
task void baz(uniform float *uniform _out, uniform float *uniform _in) {
    specconst uniform uint32 A = 64U;
    _out[0] = _in[0] + A;
}
#endif
