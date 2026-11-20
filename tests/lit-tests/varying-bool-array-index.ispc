// Known wrong code: an array of varying bool is stored with one byte per lane,
// but a varying index into it is scaled by the size of the mask register type
// (16 bytes on sse4-i32x4, 32 on avx2-i32x8), so every lane reads the wrong
// element. avx2-i8x32, whose mask is one byte per lane, is right.
//
// XFAIL until varying pointer arithmetic uses the size in memory.

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

// rows[1] is true in every lane, the others false; every lane reads rows[1].
export void read_rows(uniform int idx[], uniform int out[]) {
    varying bool rows[8];
    for (uniform int k = 0; k < 8; k++)
        rows[k] = false;
    rows[1] = true;
    int i = idx[programIndex];
    out[programIndex] = rows[i] ? 1 : 0;
}

#else

#include <stdio.h>

int main(void) {
    int idx[64], out[64], w = width(), bad = 0;
    for (int j = 0; j < 64; j++)
        idx[j] = 1;
    read_rows(idx, out);
    for (int j = 0; j < w; j++)
        bad += out[j] != 1;
    printf("%d of %d lanes wrong\n", bad, w);
    return bad != 0;
}

#endif // ISPC
