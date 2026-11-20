// Known wrong code: a masked store into a stack object of vector type is
// lowered to a load, blend and store of the whole vector without looking at
// where the store is in the object. A store at the end of varying int a[2]
// whose vector reaches 4 bytes past a is then partly out of bounds, SROA
// deletes it as undefined, and the program's store is lost. -O0 is right.
//
// XFAIL until the blend is used only for a vector that lies inside the object.

// RUN: %{ispc} --pic --target=sse4-i32x4 -O2 --woff -h %t-sse4.h %s -o %t-sse4.o
// RUN: %{cc} -O2 -x c -c %s -o %t-sse4.c.o --include %t-sse4.h
// RUN: %{cc} %t-sse4.o %t-sse4.c.o -o %t-sse4.bin
// RUN: %t-sse4.bin

// RUN: %{ispc} --pic --target=avx2-i32x8 -O2 --woff -h %t-avx2.h %s -o %t-avx2.o
// RUN: %{cc} -O2 -x c -c %s -o %t-avx2.c.o --include %t-avx2.h
// RUN: %{cc} %t-avx2.o %t-avx2.c.o -o %t-avx2.bin
// RUN: %t-avx2.bin

// REQUIRES: LINUX_HOST && X86_ENABLED && X86_64_HOST
// XFAIL: *

#ifdef ISPC

export uniform int width() { return programCount; }

// Lane j writes int W + 1 + j of a, the last lane is off (its int would be
// past a). Lane i > 0 of a[1] is int W + i, written by lane i - 1.
export void blend_end(uniform int out[]) {
    varying int a[2];
    a[0] = 1;
    a[1] = 2;
    uniform int *uniform q = (uniform int *)&a[0];
    if (programIndex < programCount - 1)
        q[programCount + 1 + programIndex] = 100 + programIndex;
    out[programIndex] = a[1];
}

#else

#include <stdio.h>

int main(void) {
    int out[64], w = width(), bad = 0;
    blend_end(out);
    for (int i = 0; i < w; i++)
        bad += out[i] != (i == 0 ? 2 : 99 + i);
    printf("%d of %d lanes wrong (lane 1 = %d, expected 100)\n", bad, w, out[1]);
    return bad != 0;
}

#endif // ISPC
