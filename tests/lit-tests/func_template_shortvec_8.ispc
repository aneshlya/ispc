// RUN: not %{ispc} --target=host %s --nowrap 2>&1 | FileCheck %s
// CHECK: Error: Only atomic types (int, float, ...) are legal for vector types.
// CHECK: Error: Unable to resolve the value of the symbol "M" used to specify the size of a vector type

struct S {
    int x;
};
template<typename T, int N>
T CalculateDifferentWidths(T<N> x, T<M> y) {
    T<N> soaValue;
    foreach (i = 0 ... N) {
        soaValue[i] = x[i] + y[i];
    }
    return soaValue[2];
}

int test_CalculateDifferentWidths(int<4> x, int<4> y) {
    return CalculateDifferentWidths<S, 4>(x, y);
}