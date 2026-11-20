// Kernels and the runtime oracle of the address-gap-off-lanes-*.ispc and
// address-gap-coalesce-wide.ispc tests: wrong code of the memory passes found by
// scripts/memory_harness.py and reduced. Each case reads an array that lies
// against an unmapped page, so a read the program doesn't make faults. The
// driver takes the names of the cases to run and returns nonzero if one of
// them gives a wrong value.

#ifdef ISPC

// The factored gather of util.m4 reads the element at its base for the lanes
// that are off; the base q is one past the end of p.
export void gather_from_end(uniform int8 p[], uniform int n, uniform int idx[], uniform int m, uniform int out[]) {
    uniform int8 *uniform q = p + n;
    int r = -1;
    if (programIndex < m)
        r = q[idx[programIndex]];
    out[programIndex] = r;
}

// All lanes read one address, so the gather becomes a scalar load, made even
// when no lane reaches the case label.
export void broadcast_after_case(uniform int a[], uniform int u, uniform int k, uniform int out[]) {
    switch (k + programIndex) {
    case 0:;
        out[programIndex] = *(&a[u + programIndex] - programIndex);
        break;
    }
}

// The same in a function that the case calls with every lane off.
noinline void broadcast_callee(uniform int a[], uniform int u, uniform int out[]) {
    out[programIndex] = *(&a[u + programIndex] - programIndex);
}
export void broadcast_in_callee(uniform int a[], uniform int u, uniform int k, uniform int out[]) {
    switch (k + programIndex) {
    case 0:;
        broadcast_callee(a, u, out);
        break;
    }
}

// Two neighbouring ints per lane, lanes 2 ints apart: coalesced, the result
// of the second gather is assembled from the wrong loads at 32 and 64 lanes.
export uniform int gang_width() { return programCount; }
export void coalesce_stride2(uniform int32 p[], uniform int32 w, uniform int32 out[]) {
    foreach (fi = 0 ... programCount) {
        int32 i = 2 * fi + w;
        out[fi] = p[i] + p[i + 1];
    }
}

#else

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

// One readable page between two unmapped ones.
static char *page(void) {
    long pg = sysconf(_SC_PAGESIZE);
    char *m = (char *)mmap(NULL, 3 * pg, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (m == MAP_FAILED || mprotect(m, pg, PROT_NONE) || mprotect(m + 2 * pg, pg, PROT_NONE)) {
        perror("mmap");
        return NULL;
    }
    return m + pg;
}

int main(int argc, char **argv) {
    long pg = sysconf(_SC_PAGESIZE);
    char *m = page();
    int bad = 0;
    if (m == NULL)
        return 2;
    for (int c = 1; c < argc; c++) {
        const char *name = argv[c];
        int ok = 1;
        if (!strcmp(name, "gather_from_end")) {
            // p is the last 64 bytes of the page; lanes 0 and 1 read p[56], p[57].
            int n = 64, idx[64] = {-8, -7}, out[64];
            signed char *p = (signed char *)(m + pg - n);
            for (int i = 0; i < n; i++)
                p[i] = (signed char)i;
            gather_from_end(p, n, idx, 2, out);
            ok = out[0] == 56 && out[1] == 57 && out[2] == -1;
        } else if (!strcmp(name, "broadcast_after_case") || !strcmp(name, "broadcast_in_callee")) {
            // k = 1: no lane takes case 0; a[u] is the first int past the page.
            int out[64];
            for (int i = 0; i < 64; i++)
                out[i] = 7;
            if (!strcmp(name, "broadcast_after_case"))
                broadcast_after_case((int *)m, (int)(pg / 4), 1, out);
            else
                broadcast_in_callee((int *)m, (int)(pg / 4), 1, out);
            ok = out[0] == 7;
        } else if (!strcmp(name, "coalesce_stride2")) {
            // lane j reads p[2j + 10] and p[2j + 11]: the sum is 4j + 21
            int32_t *p = (int32_t *)m, out[64];
            for (int i = 0; i < 1024; i++)
                p[i] = i;
            coalesce_stride2(p, 10, out);
            for (int j = 0; j < gang_width(); j++)
                ok &= out[j] == 4 * j + 21;
        } else {
            printf("unknown case %s\n", name);
            return 2;
        }
        printf("%s: %s\n", name, ok ? "ok" : "WRONG");
        bad |= !ok;
    }
    return bad;
}

#endif // ISPC
