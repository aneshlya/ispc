#!/usr/bin/env python3
#
#  Copyright (c) 2026, Intel Corporation
#
#  SPDX-License-Identifier: BSD-3-Clause

"""Generated tests of varying memory accesses against a scalar reference.

Each kernel reads or writes p[EXPR] under a mask, where p = base + padj is
a uniform pointer into an array of the application and EXPR is a random
integer expression of programIndex, uniform values and constants (signed
and unsigned, 32 and 64 bits, constants near 2^31 and 2^32). The elements
are integers and floats of 1 to 8 bytes, or the second member of a struct.
The mask comes from a bit mask (an if, a switch, or a case label that the
frontend doesn't guard), or from the trip count of a foreach whose counter
replaces programIndex; the group and strided forms read 2 to 4 neighbouring
elements, the pattern GatherCoalescePass combines. The expected
result of every lane is computed here with exact integer semantics: a test
input is used only if the program is valid for it, that is, no signed
operation of a lane that is on overflows (unless --wrap-signed-int is
given) and every lane that is on accesses an element of the array.

The arrays are placed against a page that can't be accessed, at their end
or at their start, so a read or write of memory the program doesn't access
next to the array faults. With --guard=readonly the pages can be read, so
that an extra write is caught even where an extra read before it would
fault first. Every test runs under a signal handler: a fault
is reported as such and the next test runs.

A test fails if a lane that is on reads a wrong value, a lane that is off
changes its variable, a write changes an element the program doesn't
write, or the kernel faults.

Example:
    scripts/memory_harness.py --ispc build/bin/ispc --target avx2-i32x8 \\
        --count 400 --seed 1 --ispc-flags=--addressing=64
"""

import argparse
import os
import random
import subprocess
import sys
import tempfile

LENGTH = 512  # elements of every array: 8-byte elements fill the one readable page
SENTINEL = -7

INT_TYPES = {
    # name: (bits, signed)
    "int32": (32, True),
    "uint32": (32, False),
    "int64": (64, True),
    "uint64": (64, False),
}

ELEMENTS = {
    # ispc type: (C type, bytes, is float)
    "int8": ("int8_t", 1, False),
    "int16": ("int16_t", 2, False),
    "int32": ("int32_t", 4, False),
    "int64": ("int64_t", 8, False),
    "float": ("float", 4, True),
    "double": ("double", 8, True),
    # struct S2 { int32 x, y; }: the kernel accesses p[EXPR].y
    "S2": ("ispc::S2", 8, False),
}

INTERESTING = [0, 1, 2, 3, 7, 8, 16, 63, 64, -1, -2, -8, -64, 2**15, 2**16, 2**30, 2**31 - 1, 2**31, 2**31 + 1,
               -(2**31), -(2**31) + 1, -(2**31) - 1, 2**32 - 1, 2**32, 2**32 + 1, 3 * 2**30, 2**62, -(2**62)]


class UB(Exception):
    """The expression has undefined behavior for this input."""


def wrap(x, ty):
    bits, signed = INT_TYPES[ty]
    x &= (1 << bits) - 1
    if signed and x >= 1 << (bits - 1):
        x -= 1 << bits
    return x


def check(x, ty, wrap_signed):
    """The result of an operation of type ty whose exact value is x."""
    bits, signed = INT_TYPES[ty]
    if signed and not wrap_signed and not -(1 << (bits - 1)) <= x < (1 << (bits - 1)):
        raise UB()
    return wrap(x, ty)


class Node:
    def __init__(self, code, ty, fn):
        self.code, self.ty, self.fn = code, ty, fn


def const(c, ty):
    v = wrap(c, ty)
    suffix = "ull" if not INT_TYPES[ty][1] or v >= 2**63 else "ll"
    lit = "%d%s" % (v, suffix) if v >= 0 else "(%dll)" % v
    return Node("((%s)%s)" % (ty, lit), ty, lambda env, v=v: v)


def cast(n, ty):
    if n.ty == ty:
        return n
    return Node("((%s)%s)" % (ty, n.code), ty, lambda env, n=n, ty=ty: wrap(n.fn(env), ty))


def gen_expr(rng, depth, ty, opts):
    """A random expression of type ty."""
    r = rng.random()
    if depth <= 0 or r < 0.3:
        leaf = rng.random()
        if leaf < 0.35:
            # programIndex, maybe scaled: lanes are near each other
            n = Node("programIndex", "int32", lambda env: env["lane"])
            return cast(n, ty)
        if leaf < 0.6:
            name = rng.choice(["u_", "v_"])
            return cast(Node(name, "int64", lambda env, name=name: env[name]), ty)
        return const(rng.choice(INTERESTING), ty)
    op = rng.random()
    if op < 0.45:
        a, b = gen_expr(rng, depth - 1, ty, opts), gen_expr(rng, depth - 1, ty, opts)
        sym = rng.choice(["+", "-"])
        if sym == "+":
            fn = lambda env, a=a, b=b: check(a.fn(env) + b.fn(env), ty, opts.wrap_signed)
        else:
            fn = lambda env, a=a, b=b: check(a.fn(env) - b.fn(env), ty, opts.wrap_signed)
        return Node("(%s %s %s)" % (a.code, sym, b.code), ty, fn)
    if op < 0.65:
        a = gen_expr(rng, depth - 1, ty, opts)
        k = rng.choice([2, 3, 4, -1, -2, 8, 16, 65536])
        kc = const(k, ty)
        return Node("(%s * %s)" % (a.code, kc.code), ty,
                    lambda env, a=a, kv=kc.fn(None): check(a.fn(env) * kv, ty, opts.wrap_signed))
    if op < 0.75:
        a = gen_expr(rng, depth - 1, ty, opts)
        s = rng.choice([1, 2, 3])
        bits, signed = INT_TYPES[ty]
        # a shift of a negative signed value or one that overflows is undefined
        def fn(env, a=a, s=s):
            x = a.fn(env)
            if signed and not opts.wrap_signed and (x < 0 or (x << s) >= 1 << (bits - 1)):
                raise UB()
            return wrap(x << s, ty)
        return Node("(%s << %d)" % (a.code, s), ty, fn)
    # through another integer type
    other = rng.choice(list(INT_TYPES))
    return cast(gen_expr(rng, depth - 1, other, opts), ty)


def kernel_source(k):
    """The ispc source of kernel k."""
    elem = k["elem"]
    # the type of r and of out[]: a struct's member is an int32
    rtype = "int32" if elem == "S2" else elem
    member = ".y" if elem == "S2" else ""
    test = "((mbits >> programIndex) & 1)"
    code = k["expr"].code
    lane = "programIndex"
    if k["form"] in ("foreach", "group", "strided"):
        # the index uses the foreach counter, which is programIndex in the
        # one iteration of the loop
        code = code.replace("programIndex", "fi")
        lane = "fi"
    if k["write"]:
        body = "p[%s]%s = (%s)(100 + %s);" % (code, member, rtype, lane)
    elif k["form"] in ("group", "strided"):
        # neighbouring elements: the pattern the coalescer combines
        ty = k["expr"].ty
        body = "r = " + " + ".join("p[(%s) + ((%s)%d)]%s" % (code, ty, d, member)
                                   for d in range(k["span"])) + ";"
    else:
        body = "r = p[%s]%s;" % (code, member)
    if k["form"] == "if":
        stmt = "if (%s != 0) {\n        %s\n    }" % (test, body)
    elif k["form"] == "switch":
        stmt = "switch ((int)%s) {\n    case 1:\n        %s\n        break;\n    default:\n        break;\n    }" % (
            test, body)
    elif k["form"] in ("foreach", "group", "strided"):
        # mbits is a prefix of the lanes: their count is the trip count
        stmt = "foreach (fi = 0 ... (uniform int)popcnt(mbits)) {\n        %s\n    }" % body
    else:
        # a statement before the access in the case, as in an unguarded label
        stmt = ("switch ((int)%s) {\n    case 1:\n        ;\n        %s\n        break;\n    }" % (test, body))
    return """export void k%d(uniform %s base[], uniform int64 padj, uniform int64 u_, uniform int64 v_,
                  uniform int64 w_, uniform int64 mbits, uniform %s out[]) {
    uniform %s *uniform p = base + padj;
    %s r = (%s)(%d);
    %s
    out[programIndex] = r;
}
""" % (k["id"], elem, rtype, elem, rtype, rtype, SENTINEL, stmt)


def pattern(i, elem):
    """The value of element i of the array (for a struct, of its member y)."""
    if elem == "S2":
        return pattern(2 * i + 1, "int32")
    if ELEMENTS[elem][2]:
        return float(i) * 0.5 + 1.0
    bits = ELEMENTS[elem][1] * 8
    v = (i * 37 + 11) & ((1 << bits) - 1)
    return v - (1 << bits) if v >= 1 << (bits - 1) else v


def make_tests(rng, kernel, width, opts, n):
    """Up to n valid inputs of kernel; each is a dict of the C test record."""
    tests = []
    elem = kernel["elem"]
    for attempt in range(60 * n):
        if len(tests) >= n:
            break
        mask_kind = rng.choice(["all", "all", "sparse", "first-off", "last-off", "one", "none"])
        if kernel["form"] in ("foreach", "group", "strided"):
            # the trip count of the one iteration: a full one, a partial one or none
            mask_kind = rng.choice(["all", "all", "prefix", "none"])
            if mask_kind == "prefix":
                mbits = (1 << rng.randrange(1, width)) - 1
        if mask_kind == "all":
            mbits = (1 << width) - 1
        elif mask_kind == "sparse":
            mbits = rng.getrandbits(width)
        elif mask_kind == "first-off":
            mbits = ((1 << width) - 1) & ~1
        elif mask_kind == "last-off":
            mbits = ((1 << width) - 1) >> 1
        elif mask_kind == "one":
            mbits = 1 << rng.randrange(width)
        elif mask_kind == "none":
            mbits = 0
        on = [j for j in range(width) if mbits >> j & 1]
        padj = rng.choice([0, 0, LENGTH, LENGTH // 2, LENGTH - 1])
        u = rng.choice(INTERESTING + [rng.randrange(-2**33, 2**33)])
        v = rng.choice(INTERESTING + [rng.randrange(-100, 100)])
        expr = kernel["expr"]
        ty = expr.ty
        # w_ is added last, so that some lane lands in the array.
        try:
            if on:
                j0 = rng.choice(on)
                e0 = kernel["inner"].fn({"lane": j0, "u_": u, "v_": v})
                target = rng.randrange(LENGTH) - padj
                w = target - e0
            else:
                w = rng.choice(INTERESTING)
        except UB:
            continue
        if not -(2**63) <= w < 2**63:
            continue
        span = kernel.get("span", 1)
        positions = []
        try:
            for j in on:
                idx = expr.fn({"lane": j, "u_": u, "v_": v, "w_": w})
                for d in range(1, span):
                    check(idx + d, ty, opts.wrap_signed)
                pos = padj + idx
                if not 0 <= pos <= LENGTH - span:
                    raise UB()
                positions.append(pos)
        except UB:
            continue
        if kernel["write"] and len(set(positions)) != len(positions):
            continue
        place = rng.choice(["end", "start"])
        if kernel["write"]:
            if elem == "S2":
                expected = [pattern(i, "int32") for i in range(2 * LENGTH)]
                for j, pos in zip(on, positions):
                    expected[2 * pos + 1] = pattern_write(j, elem)
            else:
                expected = [pattern(i, elem) for i in range(LENGTH)]
                for j, pos in zip(on, positions):
                    expected[pos] = pattern_write(j, elem)
            out = [SENTINEL] * width
        else:
            expected = None
            out = [SENTINEL] * width
            for j, pos in zip(on, positions):
                if span == 1:
                    out[j] = pattern(pos, elem)
                else:
                    total = sum(pattern(pos + d, elem) for d in range(span))
                    out[j] = total if ELEMENTS[elem][2] else wrap_bits(total, 8 * ELEMENTS[elem][1])
        tests.append({"kernel": kernel["id"], "padj": padj, "u": u, "v": v, "w": w, "mbits": mbits,
                      "place": place, "out": out, "array": expected, "mask": mask_kind})
    return tests


def wrap_bits(x, bits):
    x &= (1 << bits) - 1
    return x - (1 << bits) if x >= 1 << (bits - 1) else x


def pattern_write(lane, elem):
    if elem == "S2":
        return pattern_write(lane, "int32")
    if ELEMENTS[elem][2]:
        return float(100 + lane)
    bits = ELEMENTS[elem][1] * 8
    v = (100 + lane) & ((1 << bits) - 1)
    return v - (1 << bits) if v >= 1 << (bits - 1) else v


def c_value(x, elem):
    if ELEMENTS[elem][2]:
        return repr(float(x))
    if elem == "int64":
        return "%dll" % x if x != -(2**63) else "(-9223372036854775807ll - 1)"
    return str(x)


def data_ctype(elem):
    """The C type of the values the driver compares: a struct is compared as int32 words."""
    return "int32_t" if elem == "S2" else ELEMENTS[elem][0]


def data_elem(elem):
    return "int32" if elem == "S2" else elem


def array_values(elem):
    """The initial contents of the array, as values of data_ctype(elem)."""
    if elem == "S2":
        return [pattern(i, "int32") for i in range(2 * LENGTH)]
    return [pattern(i, elem) for i in range(LENGTH)]


DRIVER_HEAD = r"""
#include <inttypes.h>
#include <setjmp.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include "kernels.h"
using namespace ispc;

static sigjmp_buf env;
static void on_fault(int sig) { siglongjmp(env, sig); }

struct Test {
    int kernel, elem, write, place;
    int64_t padj, u, v, w, mbits;
    const char *desc;
};
"""


def write_sources(directory, kernels, tests, width, guard):
    with open(os.path.join(directory, "kernels.ispc"), "w") as f:
        f.write("struct S2 {\n    int32 x, y;\n};\n\n")
        f.write("export uniform int width() { return programCount; }\n\n")
        for k in kernels:
            f.write(kernel_source(k))
            f.write("\n")
    elems = list(ELEMENTS)
    with open(os.path.join(directory, "driver.cpp"), "w") as f:
        f.write(DRIVER_HEAD)
        f.write("static const int W = %d, L = %d;\n" % (width, LENGTH))
        # expected values, per test
        f.write("#define GUARD %s\n" % ("PROT_READ" if guard == "readonly" else "PROT_NONE"))
        for i, t in enumerate(tests):
            k = kernels[t["kernel"]]
            ctype, de = data_ctype(k["elem"]), data_elem(k["elem"])
            f.write("static const %s exp_out_%d[] = {%s};\n" %
                    (ctype, i, ", ".join(c_value(x, de) for x in t["out"])))
            if t["array"] is not None:
                f.write("static const %s exp_arr_%d[] = {%s};\n" %
                        (ctype, i, ", ".join(c_value(x, de) for x in t["array"])))
        f.write("static const Test tests[] = {\n")
        for t in tests:
            k = kernels[t["kernel"]]
            desc = "k%d %s %s %s padj=%d u=%d v=%d w=%d mask=%s(0x%x) place=%s" % (
                k["id"], "write" if k["write"] else "read", k["elem"], k["form"], t["padj"], t["u"], t["v"],
                t["w"], t["mask"], t["mbits"], t["place"])
            f.write("    {%d, %d, %d, %d, %dll, %s, %s, %s, %s, \"%s\"},\n" % (
                k["id"], elems.index(k["elem"]), int(k["write"]), int(t["place"] == "end"), t["padj"],
                c_value(t["u"], "int64"), c_value(t["v"], "int64"), c_value(t["w"], "int64"),
                c_value(wrap(t["mbits"], "int64"), "int64"), desc))
        f.write("};\n\n")
        # the call of a kernel
        f.write("static void call(int k, void *base, int64_t padj, int64_t u, int64_t v, int64_t w, int64_t m,"
                " void *out) {\n    switch (k) {\n")
        for k in kernels:
            f.write("    case %d: k%d((%s *)base, padj, u, v, w, m, (%s *)out); break;\n" %
                    (k["id"], k["id"], ELEMENTS[k["elem"]][0], data_ctype(k["elem"])))
        f.write("    }\n}\n\n")
        # comparisons
        f.write("template <typename T> static bool same(const T *a, const T *b, int n) {\n"
                "    for (int i = 0; i < n; i++) if (!(a[i] == b[i])) return false;\n    return true;\n}\n")
        f.write("static bool check(int i, int elem, const void *out, const void *arr) {\n    switch (i) {\n")
        for i, t in enumerate(tests):
            k = kernels[t["kernel"]]
            ctype = data_ctype(k["elem"])
            cond = "same((const %s *)out, exp_out_%d, W)" % (ctype, i)
            if t["array"] is not None:
                cond += " && same((const %s *)arr, exp_arr_%d, %d)" % (ctype, i, len(t["array"]))
            f.write("    case %d: return %s;\n" % (i, cond))
        f.write("    }\n    return false;\n}\n")
        f.write("static void fill(int i, void *arr) {\n    switch (i) {\n")
        for i, t in enumerate(tests):
            k = kernels[t["kernel"]]
            ctype = data_ctype(k["elem"])
            vals = ", ".join(c_value(x, data_elem(k["elem"])) for x in array_values(k["elem"]))
            f.write("    case %d: { static const %s v[] = {%s}; memcpy(arr, v, sizeof(v)); break; }\n" %
                    (i, ctype, vals))
        f.write("    }\n}\n")
        f.write(r"""
static const int sizes[] = {1, 2, 4, 8, 4, 8, 8};

int main() {
    if (width() != W) { printf("width %d, expected %d\n", width(), W); return 2; }
    long pg = sysconf(_SC_PAGESIZE);
    char *m = (char *)mmap(0, 3 * pg, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    mprotect(m, pg, GUARD);
    mprotect(m + 2 * pg, pg, GUARD);
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_fault;
    sa.sa_flags = SA_NODEFER;
    sigaction(SIGSEGV, &sa, 0);
    sigaction(SIGBUS, &sa, 0);
    int n = sizeof(tests) / sizeof(tests[0]), wrong = 0, faults = 0;
    alignas(64) char out[64 * 8];
    for (int i = 0; i < n; i++) {
        const Test &t = tests[i];
        int bytes = L * sizes[t.elem];
        char *arr = t.place ? m + 2 * pg - bytes : m + pg;
        fill(i, arr);
        int sig = sigsetjmp(env, 1);
        if (sig == 0) {
            call(t.kernel, arr, t.padj, t.u, t.v, t.w, t.mbits, out);
            if (!check(i, t.elem, out, arr)) { printf("WRONG %s\n", t.desc); wrong++; }
        } else {
            printf("FAULT(%d) %s\n", sig, t.desc);
            faults++;
        }
    }
    printf("tests=%d wrong=%d faults=%d\n", n, wrong, faults);
    return wrong || faults;
}
""")


def target_width(target):
    return int(target.rsplit("x", 1)[1])


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--ispc", required=True)
    ap.add_argument("--target", required=True)
    ap.add_argument("--ispc-flags", default="", help="more ispc options, e.g. --addressing=64")
    ap.add_argument("--count", type=int, default=200, help="kernels to generate")
    ap.add_argument("--tests-per-kernel", type=int, default=4)
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--depth", type=int, default=3)
    ap.add_argument("--writes", type=float, default=0.3, help="share of kernels that write")
    ap.add_argument("--forms", default="if,switch,case,foreach,group,strided",
                    help="kernel forms: if, switch, case (masked by a bit mask), foreach, group (foreach, four "
                    "neighbouring reads), strided (foreach, 2 to 4 neighbouring reads at a constant lane stride)")
    ap.add_argument("--guard", choices=["none", "readonly"], default="none",
                    help="protection of the pages around the arrays: none faults on any access, readonly only "
                    "on writes (an extra read can't hide an extra write)")
    ap.add_argument("--wrap-exe", default="", help='e.g. "sde64 -skx --" for AVX-512 targets')
    ap.add_argument("--cxx", default=os.environ.get("CXX", "c++"))
    ap.add_argument("--keep", help="directory to write the sources and binary to")
    opts = ap.parse_args()
    opts.wrap_signed = "--wrap-signed-int" in opts.ispc_flags

    rng = random.Random(opts.seed)
    width = target_width(opts.target)
    kernels, tests = [], []
    for i in range(opts.count):
        ty = rng.choice(list(INT_TYPES))
        inner = gen_expr(rng, opts.depth, ty, opts)
        if "programIndex" not in inner.code:
            if rng.random() < 0.5:
                # the same index in every lane: a broadcast, or for a write
                # of more than one lane an invalid program
                inner = Node("((varying %s)%s)" % (ty, inner.code), ty, inner.fn)
            else:
                lane = cast(Node("programIndex", "int32", lambda env: env["lane"]), ty)
                inner = Node("(%s + %s)" % (inner.code, lane.code), ty,
                             lambda env, a=inner, b=lane, ty=ty: check(a.fn(env) + b.fn(env), ty, opts.wrap_signed))
        w = cast(Node("w_", "int64", lambda env: env["w_"]), ty)
        expr = Node("(%s + %s)" % (inner.code, w.code), ty,
                    lambda env, a=inner, b=w, ty=ty: check(a.fn(env) + b.fn(env), ty, opts.wrap_signed))
        form = rng.choice(opts.forms.split(","))
        if form == "strided":
            # a constant lane stride: c * programIndex, then w_
            ty = rng.choice(["int32", "int64"])
            c = rng.choice([2, 3, 4, 5, 8, -1, -2, -3, -4])
            lane = cast(Node("programIndex", "int32", lambda env: env["lane"]), ty)
            kc = const(c, ty)
            inner = Node("(%s * %s)" % (lane.code, kc.code), ty,
                         lambda env, a=lane, c=c, ty=ty: check(a.fn(env) * c, ty, opts.wrap_signed))
            w = cast(Node("w_", "int64", lambda env: env["w_"]), ty)
            expr = Node("(%s + %s)" % (inner.code, w.code), ty,
                        lambda env, a=inner, b=w, ty=ty: check(a.fn(env) + b.fn(env), ty, opts.wrap_signed))
        reads = form in ("group", "strided")
        elems = ["int32", "float"] if reads else list(ELEMENTS)
        k = {"id": i, "elem": rng.choice(elems), "write": not reads and rng.random() < opts.writes,
             "form": form, "inner": inner, "expr": expr,
             "span": 4 if form == "group" else rng.randrange(2, 5) if form == "strided" else 1}
        kt = make_tests(rng, k, width, opts, opts.tests_per_kernel)
        if not kt:
            continue
        k["id"] = len(kernels)
        for t in kt:
            t["kernel"] = k["id"]
        kernels.append(k)
        tests.extend(kt)

    directory = opts.keep or tempfile.mkdtemp(prefix="memory-harness-")
    os.makedirs(directory, exist_ok=True)
    write_sources(directory, kernels, tests, width, opts.guard)
    obj = os.path.join(directory, "kernels.o")
    cmd = [opts.ispc, "-O2", "--woff", "--target=" + opts.target, os.path.join(directory, "kernels.ispc"), "-o", obj,
           "-h", os.path.join(directory, "kernels.h")] + opts.ispc_flags.split()
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        print("ispc failed:", " ".join(cmd))
        print(r.stderr[-4000:])
        return 2
    exe = os.path.join(directory, "harness")
    r = subprocess.run([opts.cxx, "-O1", "-std=c++17", "-o", exe, os.path.join(directory, "driver.cpp"), obj],
                       capture_output=True, text=True)
    if r.returncode != 0:
        print("c++ failed")
        print(r.stderr[-4000:])
        return 2
    run = opts.wrap_exe.split() + [exe]
    r = subprocess.run(run, capture_output=True, text=True)
    sys.stdout.write(r.stdout)
    if r.returncode not in (0, 1):
        print("harness exited with %d" % r.returncode)
        sys.stdout.write(r.stderr[-2000:])
    print("kernels=%d directory=%s" % (len(kernels), directory))
    return 0 if r.returncode == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
