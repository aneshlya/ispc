// RUN: not %{ispc} -DTEST1 %s -o %t.o --nostdlib --target=host --nowrap --enable-specconst --specconst-map /dev/null 2>&1 | FileCheck %s
// RUN: not %{ispc} -DTEST2 %s -o %t.o --nostdlib --target=host --nowrap --enable-specconst --specconst-map /dev/null 2>&1 | FileCheck %s

// CHECK: Parameter types cannot be specconst

#ifdef TEST1
// Make sure specconst parameters are not allowed
void f(specconst uniform int);
#endif

#ifdef TEST2
// Make sure an error is issued and 'f' is not shadowed
void f(uniform int);
void f(specconst uniform int);
#endif

uniform int I = 0;
specconst uniform int K = 2;

uniform int foo() {
    f(I);
    f(K);
    return I + K;
}
