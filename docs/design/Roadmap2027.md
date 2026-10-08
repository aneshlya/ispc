# ISPC Roadmap: Q4 2026 - Q3 2027

Status: proposal, revision 2 (2026-10-08). Team: one engineer, one intern,
AI-assisted development. The document is about directions and features;
section 11 gives a loose ordering only.

Out of scope: Xe GPU (frozen, build stays opt-in). ARM and WASM stay in
maintenance mode, with one exception noted in section 5.4. The APX and AMX
dispatcher capability work shipped (PR #3922 and commit 5101e97e3) and is not
part of this plan.

Contents:

1. Positioning and evidence
2. Direction summary
3. Performance
4. New Intel hardware: Diamond Rapids and Nova Lake
5. New Intel hardware: ACE (with a primer on the ISA)
6. AI in the language: types, dot products, tiles
7. AI in the language: llama.cpp as a proving ground
8. AI inside the compiler
9. Language and interop enablers (including HLSL-style vectors)
10. What to stop or shrink
11. Sequencing
12. Sources

## 1. Positioning and evidence

### 1.1 What ISPC is for

ISPC is the only production SPMD-on-SIMD language for CPUs: automatic
masking, gang semantics, C ABI interop, and multi-ISA dispatch in one tool.
In 2026 Hacker News threads it is still the reference point for the idea
("the only two good choices are something like ISPC"; new projects are asked
"is it faster than ISPC?"). The tool itself gets little discussion: the last
three ISPC stories on HN drew zero comments. The dominant perception is
"right idea, old Intel-specific project", and the most concretely voiced
weakness is C++ interop and complex structs ("didn't reach as far as CUDA
with complex structs / C++ capabilities").

### 1.2 Who uses it

- **Rendering.** OSPRay, Open VKL, Embree bindings, Open Image Denoise (OIDN)
  CPU device, DreamWorks MoonRay, Blender through OIDN and Embree. Open VKL
  2.0.2 and OIDN now require ISPC 1.30+. OSPRay 3.2 and Open VKL 2.0.1 both
  removed ISPCRT and kept only the compiler; their GPU work moved to SYCL.
  Embree 4.3.2 lists a known failure building ISPC tutorials as a macOS
  universal binary.
- **Games.** Unreal Engine (Chaos physics, animation, texture compression;
  the engine source is private so this is not independently verifiable),
  Traverse Research, SuperTuxKart, GameTechDev texture compressor and
  occlusion culling samples.
- **AI-adjacent.** OIDN's whole CPU CNN is ISPC. Its `cpu_conv_amx.ispc`
  kernel uses fp16 inputs with fp32 accumulation via `amx_dpfp16ps`,
  hand-builds the tile config struct, fails to compile unless the target
  width equals the tile row count, and stores accumulators to memory to read
  them back. OIDN 2.4 and 2.5 release notes call out AMX-FP16 gains on
  Granite Rapids. This is the flagship AI customer today.
- **Long tail.** Open3D, xLights, Stanford CS149 coursework, Rust bindings
  (ispc-rs, intel-tex-rs), VapourSynth denoisers, research compilers that
  use ISPC as the baseline (LuisaCompute, kernel_slicer).

Release download counts spike when a downstream project raises its minimum
version (1.28.2 and 1.30.0), which says the audience is mostly transitive
users of the rendering libraries, not direct adopters.

### 1.3 What users ask for

Demand on the issue tracker is modest (no open issue has more than six
reactions). Ranked by a score of 3 x reactions + 2 x distinct external
commenters + comments, the top asks are:

| Rank | Issue | Ask |
|---|---|---|
| 1 | #791 | ISPC as a library (libispc, JIT, no subprocess) |
| 2 | #2903 | `avx2-i64x8` double-pumped target for 64-bit-heavy code |
| 3 | #2906 | Fast accurate transcendentals without proprietary SVML |
| 4 | #2236 | Documented ULP bounds for stdlib math |
| 5 | #2127 | WebAssembly SIMD target |
| 6 | #1947 | ARM SVE / SVE2 targets |
| 7 | #982 | Generic SPIR-V output |
| 8 | #3676 | Varying popcnt lowering on AVX2 |
| 9 | #3455 | Tighter loop codegen |
| 10 | #3771 | Native permute instructions for shuffle |
| 11 | #3697 | `constexpr` |
| 12 | #3741 | `pip install ispc` |
| 13 | #1305 | Official vcpkg port |

Themes: distribution and embedding, new targets, math library quality,
codegen performance. Language-feature demand is real but shows up as bug
reports and interop pain rather than votes, for example the short-vector
items behind the HLSL-style request in section 9.1.

Of about 480 commits in the last year, roughly 35% were CI, 21% LLVM upgrade
and trunk fixes, 8% new targets, 7% stdlib math, 2.5% optimizations and
under 1% language features. Cutting maintenance cost is itself a roadmap
item (section 8.5).

### 1.4 Competitive pressure

- **Google Highway** is now the default answer for portable SIMD in C++. It
  lists 27 targets including AVX10.2, SVE, SVE2, RVV and WASM, and adopters
  such as Chromium, Firefox, NumPy, TensorFlow and gemma.cpp. Its bf16 and
  fp16 types are load, store and convert only; compute goes through widening
  multiply-accumulate ops. It wins on build integration and target breadth,
  loses on readability.
- **C++26 std::simd** is in the standard but only GCC 16 libstdc++ has a
  partial implementation; libc++ and MSVC have nothing, and math, bit ops and
  permutes are unimplemented everywhere. Not a threat before 2028.
- **Mojo** has first-class `bfloat16` and six fp8 dtypes including the MX
  scale format. **Slang** and **MLIR** win on generics, modules and AI
  relevance.
- **LLVM auto-vectorization** keeps improving for simple loops, eroding the
  pitch for easy kernels.

## 2. Direction summary

| Priority | Direction | Headline deliverables |
|---|---|---|
| 1 | Performance | Gather/scatter and mask codegen overhaul, 64-bit and 8-bit lane codegen, math accuracy, public perf tracking with Highway comparison |
| 2 | New Intel HW | x16 faster than x8 on DMR and NVL and default on client, APX codegen quality, deprecated feature-name migration, ACE v1 target with a shared tile programming model |
| 3a | AI in the language | `bfloat16`, fp8 storage types, complete dot-product family, typed tile API designed for ACE, llama.cpp proving ground |
| 3b | AI inside the compiler | Offline-tuned pass pipelines, LLM differential fuzzing, fitted cost models for masking and gathers, intrinsics-to-ISPC porting agent, AI-assisted maintenance |
| Enabler | Language and interop | HLSL-style vector types, template completion, `constexpr`, header sharing with C++, packaging |

## 3. Performance

Performance is the reason users pick ISPC over intrinsics or Highway. The
items below are ordered by expected payoff.

### 3.1 Gather/scatter lowering

The oldest open performance issues are all here: #304 and #330 (coalescing),
#1256 (index scale immediate), #1531, #1581 (16-bit indices), #2778 (same
type source and destination), #2899 (performance warning), #3153
(`--opt=disable-gathers` failures on 32-wide targets).
`GatherCoalescePass` has TODOs at lines 851 and 970 about conservative store
handling and pseudo-call metadata, and `ImproveMemoryOps` predates many LLVM
improvements.

- Redesign gather coalescing around address-expression analysis (base plus
  per-lane offset decomposition, stride and permutation detection) rather
  than pattern matching, and cover the store side.
- Avoid gathers when the index is a known permutation of a contiguous block
  (#2778), emitting a load plus shuffle instead.
- Use 16-bit indices and scaled addressing where the ISA supports them.
- Emit `llvm.masked.load/store/gather/scatter` and
  `llvm.experimental.vector.compress` where LLVM now lowers them well, and
  retire the matching ISPC pseudo builtins. This also shrinks LLVM-upgrade
  cost.
- The gather-versus-shuffle-versus-scalarize choice is the first candidate
  for a fitted cost model (section 8.3).

### 3.2 Mask and control-flow codegen

- Bool and mask representation on AVX2 and AVX-512 (#2920). Use
  `llvm.vector.reduce.and/or` for coherent-control-flow tests (#1338).
- Masked execution versus branch-around for divergent `if`: today the choice
  is a fixed heuristic plus `cif`. Measure it across divergence rates and
  targets, then decide by cost model (section 8.3).
- Varying integer division and modulo (#3000, #3001): magic-number sequences
  for uniform divisors, better vectorized fallback otherwise.
- Inlining heuristics and code bloat (#3804), and stopping LLVM from
  replacing vector loops with libc calls (#3241).
- Loop overheads (#3455): mask recomputation, induction variable widening,
  unroll decisions.

### 3.3 Lane-width gaps

- `avx2-i64x8` double-pumped target (#2903, the top-voted codegen issue).
- `avx512-x8` targets use only ymm even for 64-bit values (#3161).
- Varying 8-bit and 16-bit codegen (#2901), `popcnt` (#3676), `shuffle`
  (#3771) lowered to native permutes, short-vector shuffle, rotate and shift
  (#3446).

### 3.4 Math library

- Finish the accuracy audit and publish maximum-ULP tables per function and
  target (#2236).
- Complete `float16` math coverage (#2290); prerequisite for section 6.
- Evaluate SVML-style and Sleef-derived kernels for transcendentals on
  AVX10.2 (#2906, #3406). Decision criterion: ULP tables plus the
  benchmarks below.

### 3.5 Performance infrastructure

- Make the daily internal performance run public: a dashboard from the
  existing tracking job over `benchmarks/`, with per-PR regression gating on
  a small subset.
- Fill `benchmarks/03_complex` (currently empty) with representative
  kernels: an OIDN-style convolution, a texture compressor block, a ray-box
  traversal loop, a Chaos-style particle update, and the llama.cpp kernels
  from section 7. Add x8-versus-x16, bf16 and int8 dot-product microbenchmarks.
- Publish comparisons against Highway and plain Clang on the same kernels.
  Highway already does this for itself; ISPC has no public numbers.

## 4. New Intel hardware: Diamond Rapids and Nova Lake

What LLVM's `X86.td` says about these CPUs (verified against main):

- **diamondrapids** = Granite Rapids-D plus AVX10.2, the full APX set (EGPR,
  NDD, NF, CCMP, ZU, Push2Pop2, PPX), AMX-FP8, AMX-MOVRS, AMX-AVX512,
  AVX-VNNI-INT8/INT16, AVX-NE-CONVERT, AVX-IFMA, MOVRS, SHA512, SM3/SM4,
  CMPCCXADD. All eight LLVM AMX features are present. There is no AMX-TF32 or
  AMX-TRANSPOSE feature in LLVM.
- **novalake** = Panther Lake plus AVX10.2, MOVRS, the full APX set, JMPABS
  and PREFETCHI. No AMX. Because the 256-bit-only AVX10 option was removed
  from the AVX10 specification, this means 512-bit AVX10.2 and APX on
  client, ending the AVX-512-on-client gap that has existed since Alder
  Lake. This is verified only from LLVM's CPU definition, not from an Intel
  product announcement.

ISPC already has `avx10.2dmr-*` and `avx10.2nvl-*` targets with APX on by
default and `--opt=disable-apx` to turn it off. The remaining work is quality,
and the first item is the most important one in this section.

- **Make x16 the default and fastest width on DMR and NVL.** Today
  `avx10.2dmr-x16` and `avx10.2nvl-x16` run slower than the x8 variants on the
  same CPU because of an LLVM codegen issue. The goal is that x16 (full
  512-bit gangs) is the default, recommended width on both server and client,
  Nova Lake included. Work items: pin down the LLVM issue with minimized
  reproducers from `benchmarks/` and the func-tests, file and fix it upstream
  (or work around it in ISPC's pipeline until the fix lands), add per-target
  x8 versus x16 performance tracking so the gap cannot come back, and then
  switch the recommended and default widths. This also matters for ACE,
  whose operands are full 512-bit vectors and are exposed only on x16 and
  x32 (section 5.2).

- **APX codegen audit.** Measure where EGPR relieves spills in wide gangs,
  where NDD and NF forms shorten mask arithmetic, and where CCMP helps mask
  tests. Add lit tests pinning expected forms. Fix regressions found.
- **Deprecated feature names.** Clang 21 release notes say `avx10.x-256`
  and `avx10.x-512`/`evex512` are deprecated and scheduled for removal in
  the next release. Migrate target definitions and builtins before LLVM 23
  becomes the only supported version.
- **AVX10.2 surface.** Expose instructions that have no ISPC surface yet:
  bf16 arithmetic (`VADDBF16`, `VFMADD*BF16`, `VCMPBF16`, `VSQRTBF16`,
  `VRNDSCALEBF16` and friends), fp16 to fp8 conversions
  (`VCVT[BIAS]PH2{BF8,HF8}[S]`, `VCVTHF82PH`), the fp16 dot product
  `VDPPHPS`, saturating converts, and minmax. These feed section 6.
- **AVX-VNNI-INT8/INT16.** Add the signed-signed and unsigned-unsigned
  dot-product variants to the existing `dot4add_*` family so DMR users are
  not limited to u8 x s8.
- **Default target for client.** Nova Lake makes 512-bit the client norm.
  Once x16 beats x8, update the recommended multi-target set in the docs for
  game-engine users (today typically sse4, avx2, avx512skx) to include
  `avx10.2nvl-x16`.

## 5. New Intel hardware: ACE

### 5.1 ACE primer for someone who has not read the spec

ACE (AI Compute Extensions) is a matrix-multiply ISA jointly specified by
AMD and Intel under the x86 Ecosystem Advisory Group. Spec v1.15 is dated
May 2026, the whitepaper April 2026. The whitepaper calls it "the standard
matrix acceleration architecture" for x86 "from laptop to data center". No
product names, code names or dates are public.

**Mental model.** ACE is "VNNI with a 16x16 accumulator". A VNNI
instruction like `VPDPBUSD` takes two 512-bit vectors, each lane holding 4
packed bytes, and produces 16 dot products into 16 int32 lanes. An ACE
outer-product instruction takes the same two 512-bit vectors but produces
all 16 x 16 = 256 pairwise dot products at once, accumulating into a tile
register. That is 1024 int8 multiplies per instruction instead of 64.

**Architectural state.**

- Eight tile registers, each 16 rows x 512 bits (1 KB), holding a 16x16
  grid of 32-bit accumulators (int32 or fp32). Only 32-bit accumulators
  exist in v1.
- The tiles reuse AMX's TILEDATA state and TILECFG register. ACE is
  "palette 2" under the AMX framework; AMX TMUL is palette 1. A process can
  use only one palette at a time, so AMX and ACE code cannot interleave.
- `LDTILECFG` is still required before first use, but the palette-2
  descriptor is a fixed 64-byte blob (byte 0 = 2, rest zero). Shapes are
  fixed; there is no per-tile rows/colsb configuration.
- There are **no tile loads or stores**. `TILELOADD` and `TILESTORED` raise
  #UD in palette 2. Data enters tiles only through ZMM registers via
  `TILEMOVROW` (write a row from a ZMM, or read a row to a ZMM) and
  `TILEMOVCOL` (write a column). Reading back a full tile is 16 row moves.
- One 1024-bit Block Scale Register (BSR) holds 4 groups x 16 E8M0 scales
  for the row input and 4 groups x 16 for the column input. New XSAVE
  component (XCR0 bit 20), so the OS must opt in.

**Instructions.**

- Integer: `TOP4BSSD`, `TOP4BSUD`, `TOP4BUSD`, `TOP4BUUD`. "4" means each
  32-bit lane packs 4 int8 values (the K dimension). The two letters give the
  signedness of the row and column inputs. Lane i of the first source feeds
  tile row i; lane j of the second feeds tile column j. Each instruction
  computes C[i][j] += sum over k of A[i].k * B[j].k, a 16x16x4 GEMM step.
- BF16: `TOP2BF16PS`, 2 packed bf16 per lane, fp32 accumulate, fixed
  round-to-nearest-even, no exceptions.
- MX FP8: `TOP4MX{B,H}{B,H}F8PS`, where B = E5M2 and H = E4M3, in all four
  format combinations. Products are summed exactly, scaled by the selected
  BSR groups (immediate picks the row and column group), then added as fp32.
- MX INT8: `TOP4MXBSSPS`, int8 with implicit 2^-6 per element and E8M0
  block scales, fp32 accumulate.
- Row conversion on readback: `TCVTROWD2PS` (int32 to fp32),
  `TCVTROWPS2BF16H/L` and `TCVTROWPS2PHH/L` (fp32 to bf16 or fp16 halves).
- `VUNPACKB` (part of AVX10_V2_AUX): expands packed 2-7-bit fields to bytes
  with optional sign extension. Useful for 4-bit and 6-bit quantized weights
  and for any bitfield unpacking, well beyond AI.
- AVX10_V2_AUX conversions (merged in LLVM): fp32 to fp8 with
  round-to-nearest, round-to-odd, saturating and bias (stochastic rounding
  support) forms; fp8 to fp32; fp8 to and from FP4 (E2M1) and FP6 (E2M3,
  E3M2); `VPMOVSSDB` int32 to int8 with symmetric saturation.

**Data layout.** For int8, the row input must be transposed at 4-byte
granularity (AT[k/4][m][k%4] = A[m][k]) and the column input interleaved the
same way (Bp[k/4][n][k%4] = B[k][n]). Each K-step is then two 64-byte loads.
With eight tiles you can hold a 4x2 arrangement (64x32 output window) and
amortize loads to 0.75 per outer product.

**ACE versus AMX for a compiler.**

| | AMX (palette 1) | ACE (palette 2) |
|---|---|---|
| Tile shape | Configurable per tile | Fixed 16x16 x 32-bit |
| Inputs | Loaded from memory into tiles | Fed from ZMM registers |
| Per-instruction work | 16x16x64 MACs | 16x16x4 MACs |
| Data types | int8, bf16, fp16, fp8, complex | int8 (4 signedness combos), bf16, OCP fp8, MX fp8, MX int8 |
| Scaling | None | Per row and column via BSR |
| Spill | `TILESTORED` | 16 row moves plus 16 stores |
| Fusion with vector code | Through memory | Direct, same registers |

The ACE model is a better fit for SPMD code because every operand is a
normal 512-bit vector, which is exactly what a `varying int32` is on an x16
gang. Pre- and post-processing (dequantize, activation, requantize) sit in
the same registers as the matrix inputs and outputs.

**Toolchain status (October 2026).** LLVM PR 206888 (`avx10v2aux`) is
merged. PR 208408 (`acev1`, header `acev1intrin.h`, type `__acetile`,
intrinsics `__tile_ace_*`) and PR 208706 (`x86_bsr` IR type) are open, with
changes requested on the latter. Intrinsic names differ between the spec and
the PR, and the spec itself has internal inconsistencies on BSR byte layout
and immediate bit positions. GCC, binutils and emulator support are
unverified. Expect ACE in LLVM 23 or 24.

### 5.2 ISPC plan for ACE

- **Target.** ACE as a capability on the AVX10.2 x16 and x32 targets rather
  than a new tier, following the APX precedent. Detection in the dispatcher:
  CPUID.(7,1):ECX[11], ACE_VSN >= 1 via leaf 1Dh sub-leaf 2, AVX10_V2_AUX via
  leaf 24h, and XCR0 bits 17, 18 and 20. Reuse the AMX OS-capability code.
- **Low-level surface** in a new `ace.isph`, mirroring `amx.isph` style but
  typed from day one (see 6.4): opaque `uniform tile_i32` and `tile_f32`
  handles, `tile_zero`, `tile_op4_ss/su/us/uu(tile&, varying int32 a,
  varying int32 b)`, `tile_op2bf16`, `tile_op4mx*` with uniform group
  selectors, `tile_getrow(uniform int r)` returning varying,
  `tile_setrow`, `tile_setcol`, `bsr_set(varying uint32 a_scales, varying
  uint32 b_scales)`. The compiler emits palette-2 `LDTILECFG` on entry and
  `TILERELEASE` on exit of functions that use tiles; tiles cannot be varying,
  cannot be address-taken, cannot be live across calls, and a spill is a
  diagnostic rather than silent code.
- **Gang mapping.** On x16 over 32-bit lanes, `varying int32` holding 4
  packed int8 is one operand; `varying uint32` holding 2 bf16 likewise. On
  x32 with 16-bit lanes the operand view is interleaved, so the stdlib
  provides reinterpret helpers as it already does for `dot2add_i16i16`. On x8
  gangs ACE is not exposed.
- **Matrix model.** ACE is the future x86 matrix ISA and AMX is expected to
  be deprecated, so the typed tile API in 6.4 is designed around ACE and
  lowers to ACE or to a VNNI/FMA fallback. An OIDN or llama.cpp kernel is
  written once.
- **Beyond matrix ops.** A stdlib `unpack_bits<N>()` lowering to `VUNPACKB`
  with a portable fallback; byte LUT helpers over `VPERMB/VPERMI2B`; fp8 and
  fp4/fp6 conversion builtins from 6.2.
- **Testing.** Lit tests for codegen as soon as the LLVM PRs land. Functional
  testing waits on SDE or hardware; track with Intel.

Risk: toolchain churn and no hardware date. The plan commits to the surface
design and a lit-test-only implementation; functional validation is
opportunistic.

### 5.3 Relationship to AMX work

AMX is expected to be deprecated in favor of ACE. The existing `amx.isph`
stays in maintenance mode: keep it working for OIDN and other current users,
but build no new API on top of it. New DMR AMX features (AMX-FP8, AMX-MOVRS,
AMX-AVX512) get raw intrinsics only if a customer asks. AMX work is worth
doing only where it is shared with ACE, for example the `TILEMOVROW` readback
path, which exists in both.

### 5.4 ARM exception

ARM stays in maintenance mode, but Embree and UE users hit macOS
universal-binary build failures. One small item: document and test a
supported CMake recipe for x86_64 plus arm64 universal outputs.

## 6. AI in the language: types, dot products, tiles

Evidence for demand: OIDN's CPU CNN (fp16 and AMX today), llama.cpp's
kernels (int4/int8 blocks with fp16 scales, bf16 GEMM paths), Highway and
Mojo both exposing bf16 and fp8, issue #2361 (bfloat16 type) open.

### 6.1 `bfloat16` type

- New scalar and varying type with the same lexer, type-system,
  constant-folding and debug-info treatment as `float16`.
- Semantics: storage and conversion on every target; native arithmetic on
  AVX10.2 via the `V*BF16` instructions; elsewhere promote to `float`, operate,
  demote. This matches Highway's fallback design and keeps results identical
  across targets up to the final rounding.
- Header generation as `uint16_t` with a typedef and conversion helpers.

### 6.2 fp8 and sub-byte storage types

- `float8_e4m3` and `float8_e5m2` as storage-only types with explicit
  conversion builtins. Lowering: `VCVT*PH2{BF8,HF8}` and `VCVTHF82PH` on
  AVX10.2, `VCVTPS2{BF8,HF8}` family and `VCVT{BF8,HF8}2PS` with
  `avx10v2aux`, portable bit manipulation elsewhere. Saturating,
  round-to-odd and bias forms exposed by name.
- `unpack_bits<2..7>()` and pack helpers for int4, FP4 and FP6 block
  formats, lowering to `VUNPACKB` where available. These are what llama.cpp
  nibble extraction needs (section 7).
- E8M0 scale helpers for MX block formats.

### 6.3 Dot-product and reduction builtins

Today: `dot4add_u8i8packed(_sat)`, mixed-sign variants, and
`dot2add_i16i16packed`, all mapping to VNNI. Missing:

- `dot4add_i8i8` and `dot4add_u8u8` (AVX-VNNI-INT8 on DMR, emulated
  elsewhere), `dot2add_u16u16` and mixed (AVX-VNNI-INT16).
- bf16 pair dot (`VDPBF16PS` on SPR/GNR, AVX10.2 forms on DMR/NVL) and fp16
  pair dot (`VDPPHPS`).
- Segmented reductions: sum per 4, 8 or 16 lanes within the gang, which
  block-quantized formats need for per-block sums and which today require
  `reduce_add` plus shuffles.
- Widening horizontal reductions for int8 and int16 into int32.

### 6.4 Typed tile API for ACE

ACE is the matrix ISA to design for; AMX is expected to be deprecated, so a
new API over `amx.isph` alone is not worth building. The API is designed
around ACE's model (fixed 16x16 tiles fed from vector registers), with the
llama.cpp GEMM path and an OIDN-style convolution as the driving kernels.

- `uniform tile<T>` or a small set of opaque handle types (`tile_i32`,
  `tile_f32`). The compiler owns palette-2 config and release; the user
  never writes a palette struct.
- Outer-product ops taking `varying` operands, and
  `tile_setrow`/`tile_getrow`/`tile_setcol` to and from varying values, so
  accumulators are consumed directly in registers.
- Native on x16 (one ZMM per operand); x32 through reinterpret helpers; no
  `TARGET_WIDTH == tile rows` constraint as in today's OIDN AMX kernel.
- Lowering targets: ACE v1, and a VNNI/FMA fallback so the same kernel
  compiles and is testable on AVX2 and AVX-512 today, before ACE hardware or
  emulation exists. AMX lowering is not a goal.
- Deliverable: an OIDN-style convolution and the llama.cpp Q4_K GEMM written
  against the API, validated through the fallback now and through ACE lit
  tests once LLVM support lands. OIDN's existing `cpu_conv_amx.ispc` keeps
  working on the maintained `amx.isph`.

### 6.5 Framework interop

Examples of an ISPC kernel as a PyTorch custom op and as an ONNX Runtime
custom op, with CMake glue. Low effort, mainly visibility.

## 7. AI in the language: llama.cpp as a proving ground

Purpose: find out whether ISPC can match or beat hand-written intrinsics in
the most scrutinized CPU inference code base, and surface holes in the
programming model with real kernels rather than synthetic tests. gemma.cpp
(Google, Highway-based) is the natural comparison: single-source portable
SIMD with runtime dispatch, bf16-oriented GEMM with fused weight
decompression, autotuned per matrix shape.

### 7.1 How the ggml CPU backend works (verified at commit 71ad059)

- Every weight type has a `vec_dot` function and a `vec_dot_type` in
  `type_traits_cpu[]`. Legacy quants (Q4_0, Q5_0, Q8_0, IQ4_NL, MXFP4) dot
  against Q8_0 activations (32 int8 plus an fp16 scale). K-quants and IQ
  formats dot against Q8_K (256 int8, fp32 scale, int16 block sums).
- `ggml_compute_forward_mul_mat` first tries the llamafile templated
  register-tile GEMM, otherwise quantizes activations and calls per-chunk
  vec_dot with chunks claimed by an atomic counter on ggml's own threadpool.
- Hot x86 kernels live in `arch/x86/quants.c` as ladders of
  `#if __AVX512F__ / __AVX2__ / __AVX__ / __SSSE3__`. The AVX2 Q4_0 kernel:
  multiply the two fp16 block scales, unpack 32 nibbles to bytes, subtract 8,
  `_mm256_dpbusd_epi32` (or maddubs plus madd without VNNI), fp32 FMA
  accumulate, one horizontal sum at the end.
- Repacked "8x8 interleaved" weights have dedicated `gemv_*_8x8` (decode)
  and `gemm_*_8x8` (prefill) kernels in `arch/x86/repack.cpp`; a newer
  `tiled/` directory adds tiled K-quant matmul and an MoE path.
- An AMX backend (`amx/mmq.cpp`) handles Q4_0, Q4_1, Q8_0, Q4_K, Q5_K, Q6_K,
  IQ4_XS, F16 and BF16 with 16x16x32 tiles, palette 1, per-thread
  tile-config init.
- Multi-ISA: `GGML_CPU_ALL_VARIANTS` builds the whole backend once per
  feature set (haswell, skylakex, cascadelake, cooperlake, alderlake,
  sapphirerapids and so on) into shared libraries and dlopens the highest
  scoring one. There is no AVX10.2 variant yet.

### 7.2 Where the ISPC model will be stressed

- **Block-structured formats versus the gang.** A 32-element block with one
  scalar fp16 scale maps to "one gang equals one block, lanes over elements"
  on i8x32, or to "lane equals output row" which needs repacked weights to
  avoid gathers. This is exactly why ggml repacks 8x8. ISPC needs a clean
  idiom for "block as uniform struct, elements as varying" without gathers.
- **Narrow and sub-byte lanes.** Nibble extraction into two lanes per byte is
  a byte-to-two-lane shuffle, awkward in SPMD form. Needs the
  `unpack_bits` helpers from 6.2 and good i8x32/i8x64 codegen (3.3).
- **Int8 dot semantics.** `dpbusd` sums four u8 x s8 products into one int32
  lane, changing element width across lanes. ISPC's `dot4add_u8i8packed`
  expresses this, but the signedness trick ggml uses (move the sign onto the
  other operand for maddubs) and the s8 x s8 variant need the additions in
  6.3, and codegen must hit `vpdpbusd` on AVX-VNNI-only CPUs (the
  `avx2vnni` target exists; verify quality).
- **Horizontal reductions.** ggml keeps a vector accumulator and does one
  horizontal sum per row. ISPC code must be written the same way; the
  segmented reductions in 6.3 help for per-block sums.
- **Table lookups.** IQ formats and the `iq4nl` codebook use `pshufb`-style
  in-register byte LUTs. In ISPC these become gathers unless a byte-shuffle
  builtin exists (ties to #3771).
- **Threading and interop.** ggml owns the threadpool, so ISPC kernels must
  be exported per-chunk functions with no `launch`. Block structs
  (`block_q4_0` with `ggml_half`) are mirrored as ISPC structs or passed as
  byte pointers plus strides.
- **Dispatch.** Inside each ggml variant build, ISPC should compile for one
  fixed target matching the variant's flags. Separately, a single ISPC
  multi-target binary versus `GGML_CPU_ALL_VARIANTS` is a useful comparison.

### 7.3 Exploration plan

Kernels to port, in order:

1. `ggml_vec_dot_q4_0_q8_0` and `q8_0_q8_0`: the simplest block format,
   about twenty lines of AVX2 intrinsics, a calibration point.
2. `ggml_vec_dot_q4_K_q8_K` and `q6_K_q8_K`: dominate decode on Q4_K_M
   models; stress nibbles, 6-bit sub-block scales and VNNI.
3. `ggml_gemm_q4_K_8x8_q8_K`: the prefill GEMM; stresses shuffles and
   register tiling, and is the entry point for the tile API in 6.4.
4. Elementwise and attention in fp32 and fp16: softmax with `expf`,
   rms_norm, swiglu, the flash-attention one-chunk path.

Harness: stage A is a standalone binary linking `ggml-base`, using the
generic C `*_generic` kernels as the oracle (bitwise for integer kernels,
tolerance for fp), with microbenchmarks against the `arch/x86` kernels.
Stage B adds a `GGML_ISPC` CMake option that compiles `.ispc` files per
variant and swaps the function pointers in `type_traits_cpu` and the repack
tables.

Measure with `llama-bench -p 512 -n 128` on a 7B or 8B Q4_K_M model across
AVX2 (Zen 3 or Alder Lake), AVX-512 VNNI (Sapphire Rapids with AMX off and
on) and AVX10.2 (Granite Rapids or DMR when available), threads swept from
one to all cores. Per kernel: nanoseconds per block, achieved bandwidth
against STREAM for decode, achieved ops against VNNI peak for prefill.

Expected outputs: a short report with the numbers, a list of language and
stdlib gaps with issues filed, and the kernels added to `benchmarks/`. The
candidate gap list is already long enough to justify the exercise: sub-byte
unpack, complete int8 dot family, byte LUT builtin, segmented reductions, the
block-as-uniform idiom, tile API, bf16 arithmetic, fp16 KV-cache loads, and
guidance for foreign threadpools.

## 8. AI inside the compiler

### 8.1 What exists and how mature it is

Verified state of the art, October 2026:

- **MLGO in LLVM** ships two learned heuristics: inlining for size and
  register-allocation eviction. Models are TensorFlow SavedModels compiled
  ahead of time into the compiler (`ReleaseModeModelRunner`), or TOSA lowered
  through MLIR to C++ (`EmitCModelRunner`), so compile-time inference is a
  small MLP call per decision. Enabled with `-mllvm -enable-ml-inliner=release`
  and `-mllvm -regalloc-enable-advisor=release`; models come from
  `LLVM_INLINER_MODEL_PATH=download`. Results: up to 7% smaller than -Oz for
  the size model; the speed-oriented MLGOPerf variant reached 1.8% on
  SPEC2006 and 2.2% on Cbench over -O3. Training takes about a day on 100+
  vCPUs and needs PGO. IR2Vec embeddings are now an upstream analysis.
- **Pass ordering by search or RL** is research only. CompilerGym was archived
  in May 2026. Recent results: Coreset-NVP picks from 50 candidate pass
  sequences and gets 4.7% size reduction over -Oz within 45 compilations;
  POSET-RL reports 6.2% size and 12% speed on SPEC2017 over -Oz; Protean
  (TACO 2026) 4.1% average speedup over -O3 with fine-grained phase ordering;
  a 2026 study finds 7-10% of pass transitions in -O3 make things worse and
  -O3 is Pareto-dominated on 29 of 30 PolyBench kernels. All require
  per-program search at compile time, which is why none shipped.
- **Learned cost models** are standard in TVM-style autotuners (TLP speeds
  tuning search 9x on CPU) and were shown for LLVM vectorization factors by
  NeuroVectorizer (1.3-4.7x over baseline, within 3% of brute force, 2019).
  No learned vectorizer cost model has landed in LLVM or GCC as of 2026, and
  nothing targets gather-versus-shuffle or masked-versus-branch decisions.
  That gap is ISPC's problem exactly.
- **LLM-proposed transformations with formal verification.** LLM-Vectorizer
  gets 1.1-9.4x over ICC, GCC and Clang on TSVC, but Alive2 could verify only
  38% of results. A 2026 fine-tuned LLM plus Alive2 loop matched or beat the
  target transformation in only 9.8% of cases. Trivet (LLM plus Lean
  translation validation) handles 147 of 148 transformations including ten
  where Alive2 times out. AlphaEvolve sped up a production Pallas matmul by
  23% and FlashAttention by up to 32.5%. Meta's LLM Compiler (7B/13B, trained
  on LLVM IR and assembly) reaches 77% of autotuning gains for flag tuning,
  under a restrictive license.
- **LLM-generated kernels.** KernelBench shows frontier models match PyTorch
  in under 20% of tasks; NVIDIA's DeepSeek-R1 verifier loop reached 100% and
  96% correctness on the easier levels with 1.1-2.1x attention speedups;
  Intel's Xe-Forge and KForge and AMD's AsmEvo report 1.2-5x on their GPUs.
  Sakana's "CUDA Engineer" claimed 100x and was found to be exploiting holes
  in the correctness check; real results were about 3x slower. Nothing
  published targets CPU SIMD kernel generation.
- **Testing.** WhiteFox (LLM reads optimizer source to generate targeted
  tests) found 101 bugs in DL compilers; Fuzz4All found 98 across GCC, Clang,
  Z3 and others; ReFuzzer raises validity of LLM-generated LLVM tests from
  47% to 97%; TargetFuzz fuzzes individual passes.
- **Policy.** LLVM's AI Tool Use Policy requires a human in the loop,
  disclosure of substantial generated content, bans unattended agents and
  unreviewed automated review, and forbids AI on good-first issues.

### 8.2 Principles for ISPC

- Every AI output passes an automatic gate: func-tests and lit tests,
  differential execution against a scalar reference, or Alive2. The Sakana
  episode and the 38% Alive2 rate are the reasons.
- Prefer offline learning that produces a table, a fixed pipeline or a tiny
  model embedded in the compiler. No network calls and no large models at
  compile time.
- Follow the LLVM disclosure norms for contributions.

### 8.3 Projects, ranked

1. **Offline-tuned pass pipeline per target.** ISPC owns its pipeline in
   `src/opt.cpp`. Search over pass order and parameters offline per target
   family (avx2 x8, avx512 x16, avx10.2 x16/x32, neon), using the
   `benchmarks/` corpus and func-tests as the gate, and ship one better fixed
   pipeline per family. The 2026 pass-transition study suggests a few percent
   is available with zero compile-time cost. Small to medium; good intern
   project.
2. **LLM differential fuzzer** in the WhiteFox and Fuzz4All style. Seed the
   generator with ISPC grammar and with the source of specific passes
   (mask ops, gather coalescing, uniform/varying analysis). Oracle: a scalar C
   reference compiled with Clang, compared across targets, widths and -O0/-O2.
   Build on the existing yarpgen CI job. Expected outcome: silent miscompiles
   of the #3882 kind found before users do. Medium; ideal intern project.
3. **Fitted cost models for masked-versus-branch and gather lowering.**
   Sweep microbenchmarks over divergence rate, gang width and target; fit a
   small model or table; embed it; fall back to the current heuristic when
   confidence is low. The gate is "never worse than today on the benchmark
   corpus". Medium; good intern project; directly serves 3.1 and 3.2.
4. **Width predictor.** For multi-target builds, compile each kernel at x8
   and x16 (or x16 and x32) and learn which features predict the winner, then
   offer it as advice (`--print-width-advice`) before considering automatic
   selection. Medium.
5. **Intrinsics-to-ISPC porting agent.** An agent loop that takes a C++
   intrinsics kernel, produces ISPC, compiles, checks numeric equivalence on
   seeded random inputs, and benchmarks across targets, with timing hardened
   against reward hacking. LLM-Vectorizer shows the loop works. This is the
   migration story for studios on SSE/AVX2 intrinsics who want AVX10.2 and
   ARM. The llama.cpp kernels in section 7 are the first corpus and
   benchmark. Medium; strategic visibility.
6. **LLM-proposed, Alive2-verified peepholes** for ISPC's mask and gather IR
   patterns. Collect hot IR from benchmarks, have an LLM propose rewrites,
   prove each with Alive2 (noting partial support for masked intrinsics), and
   port the proven ones into `PeepholePass` by hand. Medium; research or
   intern.
7. **MLGO inliner retrained on an ISPC corpus for speed.** ISPC's IR is
   stdlib-heavy and already vectorized, so Google's size model is out of
   distribution. Realistic ceiling is 1-3% based on MLGOPerf. Large because of
   training infrastructure; defer unless an intern wants it.

### 8.4 ISPC as a target for AI kernel generation

Nothing published targets CPU SIMD. An "ISPC-Bench" (TSVC, PolyBench, ispc
examples, llama.cpp kernels) with differential correctness and hardened
timing would make ISPC the natural language for LLM-written CPU kernels and
produce training data as a side effect. Medium; pairs with project 5.

### 8.5 AI-assisted maintenance

Over half of last year's commits were CI and LLVM churn, so this is the
largest lever on team capacity.

- Extend the existing `llvm-trunk-failure-analyzer` workflow from
  categorizing failures to proposing and testing fixes, posted as draft PRs
  for human review.
- Issue triage: reproducer minimization and regression bisection posted as
  comments.
- AI-drafted stdlib implementations (math functions, dot products, fallbacks
  for new types) gated by func-tests and ULP checks.
- AI-drafted lit tests from a one-line description, validated by running
  them.

## 9. Language and interop enablers

These gate adoption in all three directions and were almost absent from last
year's work.

### 9.1 HLSL-style vector types (customer request)

What exists today: templated short vectors `T<N>` for basic types, documented
in `docs/ispc.rst` around lines 3388-3505. Component-wise arithmetic, compare
to `bool<N>`, per-component `?:`, scalar broadcast, brace initialization,
`v[i]` with varying index, member access `.x .y .z .w`, `.r .g .b .a` and
`.u .v`, and multi-component swizzle **reads** (`v.zyx`, `v.xy`) via
`VectorMemberExpr::GetValue` in `src/expr.cpp`. Stdlib `short_vec.isph`
provides elementwise math and `select`.

What is missing, with the issues that show the pain:

- Swizzle **writes** (`v.xy = ...`, `v.xyz += ...`) are rejected in
  `VectorMemberExpr::GetLValue` (#17).
- Constructors `float4(a, b.xy, c)` and splat `float4(1)`. #1279 was closed
  because `T(args)` is ambiguous with C-style casts in the LALR grammar; this
  is avoidable when `T` is a known vector type name.
- Predefined `float2/3/4`, `int2/3/4`, `uint*`, `bool*`, `half*` typedefs.
  Must be opt-in (header or flag) because user code already defines them.
- Layout: `uniform float<3>` is padded to 16 bytes and `uniform float<100>`
  to 512 (#3106, Discussion #3447 on float3 alignment differing across
  ISAs). C++ interop needs a 12-byte option, and the header generator does
  not emit short-vector types at all (#2016, #1679).
- Geometric stdlib: no `dot`, `cross`, `length`, `normalize`, `lerp`,
  `saturate`, `step`, `smoothstep`, `reflect`, `refract`, `any`/`all` on
  `bool<N>`, or efficient pairwise reduce (#1670).
- Matrix types `float3x3`, `float4x4` with `mul`, `transpose`, `inverse`
  (#2252, 3 reactions).
- Short vectors inside `soa<>` (#243), crash with short-vector references in
  structs (#2766), shuffle/rotate/shift on uniform short vectors (#3446).

Plan, in independent steps that each deliver value:

1. Geometric and graphics functions in `short_vec.isph`, templated over `T`
   and `N`, uniform and varying.
2. Swizzle writes: masked insert on store, reject duplicates and mixed
   naming sets, support compound assignment.
3. Opt-in `hlsl_types.isph` (or a flag) with the standard typedefs.
4. Constructors as a grammar production when the callee is a vector type
   name, with flattening and count checking.
5. Unpadded layout option for uniform short vectors and header-generation
   support, with a migration note.
6. Matrix types as a second phase: row-vector arrays with `mul`,
   `transpose`, `determinant`, `inverse`, and header interop. Later these can
   lower to the dot-product and tile builtins.

### 9.2 Templates and compile-time evaluation

- Default template arguments and deduction in specializations (documented
  as planned); fix the open template bugs (#3016, #3025, #3040, #3232,
  #3240). Struct templates are a stretch goal and would unblock the typed
  tile API.
- `constexpr` per the design proposal in #3697. Needed for tile shapes,
  block sizes and compile-time dispatch in stdlib code.

### 9.3 C++ header sharing and ABI

OSPRay's `#ifdef ISPC` shared-header pattern and the UE struct-layout
complaints point the same way.

- Document guaranteed layout-compatibility rules.
- Fix return-by-value and pointer ABI issues (#1590, #1855, #2344).
- Fix multi-target header naming (#1666); emit unused structs on request
  (#2277); emit short-vector types (#2016).

### 9.4 Small quality items

`auto` (#2310), explicit bitcast syntax (#1809), warning on missing return
(#1708), consistent signed/unsigned conversions (#2714).

### 9.5 Packaging and embedding

- PyPI (#3741) and an official vcpkg port (#1305) remove the "separate
  compiler to install" friction that Highway never has. Low cost.
- `libispc` (#791, top-ranked ask). The JIT and library API exist in
  `src/ispc_impl.cpp` and the docs; stabilize, version and document them as
  the embedding story now that ISPCRT has lost its main consumers.
- Single object for multiple targets (#1850).

## 10. What to stop or shrink

- **ISPCRT.** OSPRay 3.2 and Open VKL 2.0.1 removed it. Finish the in-flight
  load-from-memory work (#3900), then freeze: security and build fixes only.
  Decide on deprecation at the end of the plan year.
- **Xe GPU targets.** Opt-in build, CI kept green, no new features.
- **ISPC-specific pseudo builtins that duplicate LLVM intrinsics.** Retire
  where generic intrinsics lower equally well; this shrinks LLVM-upgrade cost
  and is a prerequisite for 3.1.
- **Old targets.** Audit i686 (#1865) and the lowest SSE tiers for removal
  from release binaries to cut stdlib build time and matrix size.

## 11. Sequencing

Loose ordering by dependency, not a schedule.

1. **First.** x16-versus-x8 performance on DMR/NVL: root-cause the LLVM
   issue and start the upstream fix. Deprecated feature-name migration. Public perf dashboard and
   `03_complex` benchmarks. `bfloat16` design and implementation start. ACE
   surface design. llama.cpp stage A harness with the Q4_0 and Q4_K kernels.
   LLM fuzzing harness on the yarpgen job. Pass-pipeline offline tuning.
2. **Then.** `bfloat16` complete with AVX10.2 lowering. Dot-product family and
   segmented reductions. Gather/scatter redesign. Template defaults and bug
   fixes. HLSL-style vectors steps 1-3. ACE `ace.isph` with lit tests once the
   LLVM PRs land.
3. **Then.** Typed tile API with the VNNI/FMA fallback and the OIDN-style
   convolution as proof.
   fp8 storage types and `unpack_bits`. `constexpr`. llama.cpp stage B and
   report. Fitted cost models for masking and gathers. Porting agent published.
4. **Last.** Tile API lowered to ACE. x16 made the default and
   recommended width on DMR and NVL once it beats x8. `avx2-i64x8` and
   lane-width items. Header-sharing and ABI fixes, HLSL vectors steps 4-6. Highway comparison
   published. ISPCRT deprecation decision.

## 12. Sources

- ACE: spec v1.15 (x86ecosystem.org, May 2026), whitepaper v1.0 (April
  2026); LLVM PRs 206888 (merged), 208408 and 208706 (open).
- LLVM: `llvm/lib/Target/X86/X86.td` and `X86InstrAVX10.td` on main; Clang 21
  release notes; MLGO docs and google/ml-compiler-opt.
- Customers: RenderKit changelogs for oidn, ospray, openvkl, embree; OIDN
  `devices/cpu/cpu_conv_amx.ispc`; ispc/ispc release download counts; GitHub
  code search for `enable_language(ISPC)`.
- Issues: 271 open and 123 recently closed ispc/ispc issues, 64 discussions,
  pulled 2026-10-08.
- llama.cpp: ggml-org/llama.cpp at commit 71ad059, `ggml/src/ggml-cpu/`
  (`ggml-cpu.c`, `arch/x86/quants.c`, `arch/x86/repack.cpp`,
  `llamafile/sgemm.cpp`, `amx/mmq.cpp`, `simd-mappings.h`), `ggml/src/CMakeLists.txt`;
  google/gemma.cpp README.
- Landscape: Highway README and quick reference; cppreference C++26 compiler
  support; chipsandcheese on Zen 5; Hacker News via the Algolia API.
- AI in compilers: arXiv 2101.04808 (MLGO), 2207.08389 (MLGOPerf),
  1909.06228 (IR2Vec), 2301.05104 (Coreset-NVP), 2208.04238 (POSET-RL),
  2602.06142 (Protean), 2606.31238 (pass transitions), 2407.02524 (LLM
  Compiler), 1909.13639 (NeuroVectorizer), 2211.03578 (TLP), 2406.04693
  (LLM-Vectorizer), 2609.27214, 2609.19583 (Trivet), 2502.10517
  (KernelBench), 2310.15991 (WhiteFox), 2308.04748 (Fuzz4All), 2508.03603
  (ReFuzzer), 2512.04344 (TargetFuzz), 2605.26118 and 2606.02963 (Intel
  Xe-Forge, KForge), 2608.20711 (AMD AsmEvo), 2606.20128 (Correctness
  Illusion); AlphaEvolve blog; NVIDIA DeepSeek-R1 kernel blog; TechCrunch on
  Sakana; llvm.org AI Tool Use Policy.
- Unverified in this pass: Unreal Engine's vendored ISPC version and exact
  usage; game-studio talks; Zen 6 features; Nova Lake confirmation beyond
  LLVM's CPU definition; GCC and SDE support for ACE; whether anyone has
  already tried ISPC in ggml.
