# Inlinable ISPC kernels: `--emit-cpp` design proposal

Status: proposal (2026-10-16). No feature code has been changed.

Related: GitHub Discussions [#2722](https://github.com/ispc/ispc/discussions/2722)
and [#2644](https://github.com/ispc/ispc/discussions/2644) (bring back C++
output), roadmap section 12.7, the removed `--emit-c++` backend (commit
785028145, July 2020).

## 1. Problem

Every ISPC `export` function is an out-of-line call from C++. Customers ask
for ISPC kernels that inline into their C++ call sites, for three reasons:

- Small kernels called in hot loops pay call overhead and lose constant
  propagation from the caller.
- Engines want kernels compiled by their own toolchain on build farms that
  do not have ISPC installed, with the generated code checked in.
- Debugging and profiling are easier when the kernel is ordinary C++ in the
  user's translation unit.

Two approaches were evaluated. This document proposes the second.

### 1.1 Link-time optimization (works today, limited)

`ispc --emit-llvm` produces LLVM bitcode. Linking it with C++ objects under
`-flto` through lld inlines the kernel body into the caller. Verified on
2026-10-16 with ISPC 1.33.0dev (LLVM 21.1.8), clang 21 and lld: the
`export` function body appeared inside the C++ caller with no call.

Limits:

- The multi-target dispatcher selects the ISA at run time, so the per-target
  bodies cannot inline by construction. Only single-target builds benefit.
- ISPC does not set `target-features` on x86 functions (`src/ispc.cpp`,
  comment "not necessary to set target-cpu and target-features"). LLVM then
  inlines an AVX2 body into a caller compiled without `-mavx2` and legalizes
  it down to SSE silently. Adding the attributes is a small, independent fix
  and should be done regardless of this proposal.
- The linker's LTO plugin must come from an LLVM at least as new as the one
  ISPC was built with. MSVC `link.exe` cannot consume bitcode at all; Windows
  needs clang-cl plus lld-link.

LTO should be documented as the zero-effort option. It does not meet the
build-farm or MSVC-adjacent needs.

### 1.2 Why the old `--emit-c++` was removed

The 2012-2020 backend compiled "generic" targets through a fork of LLVM's C
backend (`cbackend.cpp`). The output called functions such as
`__add_float(__vec16_f32, __vec16_f32)`, which the user implemented by
including a hand-written per-ISA header (`examples/intrinsics/sse4.h`, 3,645
lines; `generic-16.h`, 1,787 lines; KNC and KNL variants). It was removed
because:

- The forked C backend broke with every LLVM upgrade.
- Each ISA needed its own intrinsics header, maintained by hand.
- Code quality depended on the host compiler seeing through thousands of
  tiny wrapper calls.

## 2. Proposal

Add `--emit-cpp`, available for the existing `generic-*` targets only. It
prints the optimized LLVM IR of the module as C++ that uses the clang/GCC
vector extensions, so the host compiler performs ISA selection,
legalization and inlining. There is no per-ISA header. One small
ISA-independent runtime header ships with ISPC for the few operations the
vector extensions do not cover.

### 2.1 Why this is tractable now

The current `generic-i32x4`, `generic-i32x8`, `generic-i32x16`,
`generic-i8x16`, `generic-i8x32`, `generic-i16x8` and `generic-i64x4`
targets implement their builtins in ISPC itself (`builtins/generic.ispc`)
and lower to plain LLVM vector IR with no x86 intrinsics. After ISPC's
optimization pipeline, the IR for these targets uses a small subset:

| LLVM IR construct | C++ vector-extension equivalent |
|---|---|
| vector `add fadd mul ...`, `icmp fcmp`, bitwise ops | native operators on `vector_size` types; comparisons yield integer vectors |
| `select` | `?:` on vectors (clang, GCC 12+) |
| `shufflevector` | `__builtin_shufflevector` |
| `sitofp fptosi trunc zext sext fpext fptrunc` | `__builtin_convertvector` |
| `llvm.fma llvm.sqrt llvm.fabs llvm.minnum llvm.maxnum` | `__builtin_elementwise_fma/sqrt/abs/min/max` |
| `llvm.vector.reduce.*` | `__builtin_reduce_add/and/or/xor/min/max` |
| `llvm.masked.load/store` | `__builtin_masked_load/store` (clang 21+) or runtime header |
| `llvm.masked.gather/scatter` | `__builtin_masked_gather/scatter` (clang 21+) or runtime header |
| `llvm.ctpop llvm.ctlz llvm.cttz` | `__builtin_elementwise_popcount/...` or runtime header |
| scalar IR, control flow, `alloca`, `phi` | ordinary C++, `goto` or structured loops |

A printer for this subset is a few thousand lines owned by ISPC. It depends
on the LLVM IR API, which is stable compared with the backend internals the
old `cbackend.cpp` reached into.

### 2.2 Outputs

```
ispc kernel.ispc --target=generic-i32x8 --emit-cpp -o kernel_ispc.inl -h kernel_ispc.h
```

- `kernel_ispc.h`: the existing generated header, unchanged.
- `kernel_ispc.inl`: the kernel bodies as `inline` C++ functions in
  `namespace ispc`, including `<ispc/cpp_runtime.h>`. Meant to be included,
  not compiled on its own.
- `ispc/cpp_runtime.h`: installed with ISPC. Defines `ispc_rt::vec<T, N>`
  as a `vector_size` typedef, lane-index constants, masked memory helpers,
  gathers and scatters, and a scalar fallback for compilers without vector
  extensions.

Example output shape for a `foreach` scale kernel:

```cpp
#include <ispc/cpp_runtime.h>
namespace ispc {
inline void scale(float *a, float s, int n) {
    using v8f = ispc_rt::vec<float, 8>;
    using v8i = ispc_rt::vec<int, 8>;
    v8f sv = ispc_rt::splat<v8f>(s);
    int i = 0;
    for (; i + 8 <= n; i += 8) {
        v8f x = ispc_rt::load<v8f>(a + i);
        ispc_rt::store(a + i, x * sv);
    }
    if (i < n) {
        v8i m = (ispc_rt::lane_index<v8i>() + i) < n;
        v8f x = ispc_rt::masked_load<v8f>(a + i, m);
        ispc_rt::masked_store(a + i, x * sv, m);
    }
}
} // namespace ispc
```

The printer does not try to reconstruct ISPC source structure. Readability
is a non-goal; correctness and host-compiler friendliness are the goals.

### 2.3 User workflow

1. Write the kernel as today. Only `export` functions are visible.
2. Run ISPC with `--emit-cpp` as a code-generation step. The `.inl` can be
   checked into the repository.
3. Include the `.inl` in translation units where inlining matters; include
   only the `.h` elsewhere, or wrap the `.inl` once in a `.cpp` for an
   out-of-line copy.
4. Compile with the host compiler and the desired ISA flags. The gang width
   is fixed by the ISPC target; the instruction set comes from `-m` flags:

```
clang++ -O2 -mavx2 -mfma app.cpp        # 8-wide vectors use one ymm each
clang++ -O2 app.cpp                     # same source legalized to two xmm each
clang++ -O2 --target=aarch64-linux-gnu  # NEON without an ISPC ARM build
```

### 2.4 Multi-target

Two modes:

- **User-managed.** The user compiles the including translation unit once
  per ISA and dispatches with their existing mechanism. This is the
  performance mode and matches how engines handle hand-written SIMD.
- **Compiler-managed.** With several `generic-*` targets, ISPC emits one
  body per width and wraps the exported entry in
  `__attribute__((target_clones(...)))` on clang and GCC. The compiler
  generates the resolver. Calls through the resolver do not inline; this is
  the convenience mode.

### 2.5 Compiler support matrix

| Host compiler | Vector arithmetic, shuffles, converts | Masked load/store, gather/scatter | Reductions | Result |
|---|---|---|---|---|
| clang 21+ | native | `__builtin_masked_*` | `__builtin_reduce_*` | full quality |
| clang 15-20 | native | runtime header (select + unmasked, or per-lane) | `__builtin_reduce_*` | good |
| GCC 12+ | native | runtime header | runtime header | good arithmetic, weaker masked memory |
| MSVC | runtime header, scalar | runtime header, scalar | runtime header | correct, scalar, unsupported for performance |

MSVC users are directed to clang-cl. The runtime header emits `#warning`
when it falls back to scalar code.

### 2.6 Semantics that need care

- **Masked memory operations.** ISPC guarantees that inactive lanes do not
  fault. The fallback for compilers without masked builtins must be per-lane
  conditional access, not an unmasked wide load with select.
- **Fast-math.** ISPC's `--opt=fast-math` and reciprocal/approximate flags
  become `#pragma clang fp` or `__attribute__((optnone))`-free annotations
  only where the host compiler supports them; otherwise the emitted code
  carries the ISPC-side transformations already applied in IR and the user
  controls the rest.
- **Integer division, shifts and undefined behavior.** The printer must
  emit the same guards ISPC's codegen already applies, since C++ vector
  extensions inherit C++ UB rules.
- **Alignment.** `vector_size` types are naturally aligned. All memory
  access in the emitted code must use unaligned load/store helpers or
  `__attribute__((aligned(1)))` typedefs, since ISPC does not promise
  alignment of user pointers.
- **Width versus host target.** If the ISPC width exceeds the host target's
  native width the code is still correct. The runtime header should
  `static_assert` or `#warning` when `__AVX2__`/`__AVX512F__` macros show a
  mismatch, so users notice a missing `-mavx2`.

### 2.7 Out of scope

- `launch`, `sync` and tasks: there is no ISPC task runtime in this mode.
  Using them with `--emit-cpp` is a compile error.
- Non-generic targets. `--emit-cpp --target=avx2-i32x8` is an error; the
  user is told to use `generic-i32x8`.
- Xe/GPU targets.
- Readable or stable output. The `.inl` is generated code.
- Emitting from the AST instead of IR. That would duplicate masking and
  control-flow lowering.

## 3. Implementation sketch

1. **Printer.** New `src/emit_cpp.cpp` implementing an `llvm::Module` to
   C++ printer for the IR subset in 2.1. Unknown instructions or intrinsics
   are a hard error naming the construct, so gaps surface in tests rather
   than as miscompiles.
2. **Runtime header.** `include/ispc/cpp_runtime.h`, a few hundred lines,
   installed alongside the existing headers.
3. **Driver.** `--emit-cpp` output type in `src/main.cpp` and `module.cpp`,
   restricted to generic targets, with `target_clones` emission for
   multi-target invocations.
4. **Testing.** Add a `--emit-cpp` leg to `scripts/run_tests.py`: compile
   each functional test through the printer, build with clang, run and
   compare against the native target. This differential test is the main
   correctness mechanism. Lit tests cover driver errors and the shape of
   emitted code for representative constructs.
5. **Documentation.** A section in `docs/ispc.rst` describing the workflow
   in 2.3, the support matrix in 2.5, and the LTO alternative in 1.1.

Order of work: printer and runtime header against `generic-i32x8` on
clang, then the differential test leg, then GCC, then `target_clones`.

## 4. Prior art

- **Halide** ships a C++ backend whose emitted kernels use a small header
  of vector types with a native vector-extension implementation and a
  scalar fallback. Closest precedent; maintained in-tree.
- **Slang** emits C++ for CPU execution through a prelude header, the same
  pattern as the old `--emit-c++`.
- **MLIR EmitC** is the general-purpose route for compilers wanting C++
  output. Lowering ISPC to the MLIR vector dialect and EmitC would work but
  adds a large dependency for one feature.
- **llvm-cbe** (JuliaHub) is the maintained descendant of the C backend the
  old `cbackend.cpp` forked. It prints vectors as structs with loops, so
  performance would rest entirely on the host autovectorizer.
- **Futhark** emits ISPC as one of its CPU backends.
- **Taichi** and **TVM** have C source backends that emit scalar code.

## 5. Beyond CPUs: what the emitter does and does not enable

The printer and runtime-header pattern are reusable for other hardware. The
value differs sharply between SIMT GPUs, AI accelerators and scalable
vector ISAs, and in every case the emitter is the delivery mechanism, not
the hard part.

### 5.1 AI accelerators

Programmable AI cores are driven by vendor LLVM forks that compile a C or
C++ dialect with wide vector types:

| Hardware | Programming surface | Vector width |
|---|---|---|
| Intel Gaudi TPC | TPC-C, C dialect with `float64`/`int64` vector types, `tpc-clang` | 64 x 32-bit |
| Qualcomm Hexagon HVX | C/C++ with clang vector extensions, upstream LLVM backend | 1024-bit |
| AMD XDNA AI Engine | C++ with vector intrinsics, Peano LLVM toolchain | 256 to 1024-bit |
| Tenstorrent, Intel NPU, Movidius SHAVE | C++ kernels with SIMD intrinsics on LLVM-based compilers | varies |

None accept bitcode from a foreign LLVM and none have an ISPC backend. The
C++ emitter is therefore the only realistic port path: ISPC is the SPMD
front end, the vendor compiler does legalization and scheduling, and ISPC
never links the vendor's LLVM. A port consists of a `generic-*` target with
the hardware lane count, a runtime-header dialect, and tests.

What a port needs beyond the emitter:

- **Runtime header per accelerator.** Maps masked loads and stores,
  gathers, scatters, reductions and shuffles to vendor intrinsics. Hexagon
  uses the standard vector-extension path. Gaudi needs TPC-C types emitted
  directly, so the printer requires a type-naming hook per dialect.
- **Explicit memory hierarchy.** The main gap. Accelerator kernels move
  tiles between HBM, scratchpad and registers by DMA, and performance lives
  in that choreography. ISPC has one flat pointer space with no way to say
  where data lives or to express asynchronous copies. Minimum language
  additions: address-space qualifiers on pointers, a `local` storage
  class, and a copy builtin with completion.
- **Matrix units.** The matrix engine is the point of the chip and is
  reached through intrinsics with fixed tile shapes. The ACE tile API in
  the 2027 roadmap (opaque accumulators, outer products, row transfers) is
  the abstraction to extend. Without it ISPC covers only the vector side.
- **Work partitioning.** `launch`, `taskIndex` and `taskCount` map to the
  vendor index space through generated entry wrappers; tasking inside the
  kernel is an error, as in 2.7.
- **Types.** BF16, FP8 and block-scaled formats are native here. The BF16
  and FP8 work in roadmap sections 6 and 12.3 serves both CPU and
  accelerator use.

Value assessment: matmul and attention come from vendor libraries. ISPC's
space is the long tail of elementwise, normalization, quantization and
data-reorganization kernels that users hand-write in the vendor dialect,
the same niche it fills on CPUs. Without the memory-hierarchy and tile
features the generated code is correct but slow, so an accelerator port is
a language project with the emitter as its delivery vehicle.

### 5.2 SIMT GPUs

ISPC had an NVPTX target from 2014 to 2019 (removed in commit 667fafb67).
It mapped a program instance to a CUDA thread and the gang to a warp using
a width-1 target: varying values were scalar, masks were `<1 x i1>`, and
cross-lane builtins used `shfl` and ballots. It rotted for lack of an owner.

The emitter helps in two ways: the same printer can produce CUDA, HIP or
SYCL C++ from scalar-per-thread IR with a runtime header mapping
collectives to `__shfl_sync`, `__ballot_sync` and `__reduce_*_sync`; and
emitting `__device__ inline` functions lets users call ISPC code from
inside their own kernels, which direct PTX output never allowed.

The vector-of-N IR from the CPU generic targets is the wrong shape for
SIMT; NVPTX would scalarize it into per-thread loops. SIMT needs a width-1
target first, plus: real branches on the per-thread predicate instead of
masked execution of both sides; convergence rules for `uniform` values and
collectives under independent thread scheduling; `launch` mapped to grids
through generated `__global__` wrappers; address spaces; and device-side
`print`. The CPU emitter provides roughly a third of a SIMT port. The
suggested sequence is CPU printer, then a `generic-simt` width-1 target
emitting scalar C++ testable on a CPU with a 32-thread emulation header,
then the CUDA and HIP dialects.

### 5.3 SVE, RVV and scalable vectors

**Fixed-length SVE.** Clang and GCC compile vector-extension code to SVE
under `-msve-vector-bits=N`, so emitted `generic-i32x8` code would use SVE
predicated loads, gathers and scatters. ISPC can reach the same result
natively by adding AArch64 targets with the `sve` features and a fixed
`vscale_range` attribute, which is how LLVM implements that flag. That is
smaller than the emitter and keeps the normal workflow; it is the right
answer to issue [#1947](https://github.com/ispc/ispc/issues/1947). Most
deployed Arm server cores (Neoverse N2, V2, Graviton 3 and 4) implement
128-bit SVE2, so the gain over `neon-i32x4` is predication and
gather/scatter, not width.

**Vector-length-agnostic code.** The emitter does not help. The obstacles
are in the language: `programCount` is a compile-time constant in user
code, the stdlib and the memory layout of `soa` and varying struct members,
so the C/C++ ABI for anything containing a varying value disappears with an
unknown width; ISPC codegen relies on arbitrary `shufflevector` masks and
lane-index constant vectors that scalable LLVM vectors forbid; clang's
sizeless SVE types support only a subset of operators, so there is no
vector-extension dialect to print into; and varying arrays and
`aos_to_soa` helpers have no length-agnostic form. RVV has the same shape
of problem. Recommendation: fixed-length SVE targets through the native
backend; no scalable-semantics work.

## 6. Open questions

- Should the `.inl` carry an `ISPC_CPP_RUNTIME_VERSION` check so a stale
  generated file fails loudly against a newer runtime header?
- Whether to emit `[[gnu::always_inline]]` on exported functions or leave
  the decision to the host compiler. Proposal: plain `inline`, with an
  `--emit-cpp-always-inline` opt-in.
- Whether GCC's weaker masked-memory codegen is acceptable for the first
  release or GCC should wait for a second phase.
- How to report the gang width and runtime header requirements in the
  generated header so mismatched includes are diagnosed.
