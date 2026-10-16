# ISPC Roadmap Proposal: Q4 2026 - Q3 2027

Status: proposal, restructured revision (2026-10-16). Team: one engineer,
one intern, AI-assisted development.

Priorities: **1. performance; 2. enabling new Intel hardware, especially
ACE; 3. AI through the language and programming model, plus bounded
compiler research.**

How to read this document. Sections 1 to 7 describe the context and every
candidate topic once: the evidence (issues, discussions, customer code,
hardware and toolchain facts), what exists today, what is proposed and
what it depends on. Nothing in those sections is a commitment. Section 8
turns the topics into a recommended set of annual commitments, a
sequence and decision gates; it may mention a topic more than once when
one piece of work serves several goals.

Contents:

1. Context: users, demand, competition, capacity, evidence
2. Performance
3. New Intel hardware: Diamond Rapids, Nova Lake, ACE
4. AI in the language: BF16, low-precision types, inference kernels
5. AI inside the compiler
6. Language, interop and adoption
7. Maintenance
8. Recommended annual commitments

## 1. Context: users, demand, competition, capacity, evidence

### 1.1 What ISPC is for

ISPC is an established production SPMD-on-SIMD language for CPUs:
automatic masking, gang semantics, C ABI interop and multi-ISA dispatch in
one tool. Its stated goal is portable performance across supported
targets. In 2026 Hacker News threads it is still the reference point for
the idea ("the only two good choices are something like ISPC"; new
projects are asked "is it faster than ISPC?"). The tool itself gets little
discussion: the last three ISPC stories on HN drew zero comments. Sampled
comments suggest "right idea, old Intel-specific project", and the most
concretely voiced weakness is C++ interop and complex structs ("didn't
reach as far as CUDA with complex structs / C++ capabilities").

### 1.2 Who uses it

- **Rendering.** OSPRay, Open VKL, Embree bindings, Open Image Denoise
  (OIDN) CPU device, DreamWorks MoonRay, Blender through OIDN and Embree.
  Open VKL 2.0.2 and OIDN require ISPC 1.30+. OSPRay 3.2 and Open VKL
  2.0.1 both removed ISPCRT and kept only the compiler; their GPU work
  moved to SYCL. Embree 4.3.2 lists a known failure building ISPC
  tutorials as a macOS universal binary.
- **Games.** Unreal Engine (Chaos physics, animation, texture compression;
  the engine source is private so this is not independently verifiable),
  Traverse Research, SuperTuxKart, GameTechDev texture compressor and
  occlusion culling samples.
- **AI-adjacent.** OIDN's whole CPU CNN is ISPC. Its `cpu_conv_amx.ispc`
  kernel uses fp16 inputs with fp32 accumulation via `amx_dpfp16ps`,
  hand-builds the tile config struct, fails to compile unless the target
  width equals the tile row count, and stores accumulators to memory to
  read them back. OIDN 2.4 and 2.5 release notes call out AMX-FP16 gains
  on Granite Rapids. This is the flagship AI customer today.
- **Long tail.** Open3D, xLights, Stanford CS149 coursework, Rust bindings
  (ispc-rs, intel-tex-rs), VapourSynth denoisers, research compilers that
  use ISPC as the baseline (LuisaCompute, kernel_slicer).

Release download counts spike when a downstream project raises its
minimum version (1.28.2 and 1.30.0). Downstream releases drive downloads;
download counts alone do not separate transitive users from direct
adopters.

### 1.3 What users ask for

Demand on the issue tracker is modest: no open issue has more than six
reactions. Of 271 open and 123 recently closed issues pulled 2026-10-08,
ranked by 3 x reactions + 2 x distinct external commenters + comments, the
top sampled topics are:

| Rank | Issue | Ask |
|---|---|---|
| 1 | [#791](https://github.com/ispc/ispc/issues/791) | ISPC as a library (library/JIT exist; adoption gaps remain) |
| 2 | [#2903](https://github.com/ispc/ispc/issues/2903) | `avx2-i64x8` double-pumped target for 64-bit-heavy code |
| 3 | [#2906](https://github.com/ispc/ispc/issues/2906) | Fast accurate transcendentals without proprietary SVML |
| 4 | [#2236](https://github.com/ispc/ispc/issues/2236) | Documented ULP bounds for stdlib math |
| 5 | [#2127](https://github.com/ispc/ispc/issues/2127) | WebAssembly SIMD target |
| 6 | [#1947](https://github.com/ispc/ispc/issues/1947) | ARM SVE / SVE2 targets |
| 7 | [#982](https://github.com/ispc/ispc/issues/982) | Generic SPIR-V output |
| 8 | [#3676](https://github.com/ispc/ispc/issues/3676) | Varying popcnt lowering on AVX2 |
| 9 | [#3455](https://github.com/ispc/ispc/issues/3455) | Tighter loop codegen |
| 10 | [#3771](https://github.com/ispc/ispc/issues/3771) | Native permute instructions for shuffle |
| 11 | [#3697](https://github.com/ispc/ispc/issues/3697) | `constexpr` |
| 12 | [#3741](https://github.com/ispc/ispc/issues/3741) | `pip install ispc` |
| 13 | [#1305](https://github.com/ispc/ispc/issues/1305) | Official vcpkg port |

This ranking is a discovery aid, not a priority score: age, discussion
volume, implementation status and customer impact are assessed per topic
in sections 2 to 6. Themes: distribution and embedding, new targets, math
library quality, codegen performance. Language-feature demand is real but
shows up as bug reports and interop pain rather than votes.

GitHub Discussions (64 threads, read 2026-10-08) are mostly Q&A. The
recurring topics are gathers and memory layout, math accuracy and
coverage, short vectors and HLSL-style code, 64-bit lane width, missing
SIMD primitives, bfloat16, which target to pick for a given CPU, and build
and distribution. Two further requests recur: bringing back C++ output and
automatic differentiation through Enzyme. Nothing in discussions mentions
AMX, ACE or AVX10.2. Each discussion is cited in the topic it supports.

### 1.4 Competitive landscape

- **Google Highway** is an established option for portable SIMD in C++.
  It lists 27 targets including AVX10.2, SVE, SVE2, RVV and WASM, and
  adopters such as Chromium, Firefox, NumPy, TensorFlow and gemma.cpp. Its
  bf16 and fp16 types are load, store and convert only; compute goes
  through widening multiply-accumulate ops. Its strengths are build
  integration and target breadth; it offers an explicit-SIMD programming
  model rather than SPMD.
- **gemma.cpp** (Google, Highway-based) is the natural comparison for
  inference work: single-source portable SIMD with runtime dispatch,
  bf16-oriented GEMM with fused weight decompression, autotuned per matrix
  shape.
- **C++26 std::simd.** The October 2026 survey found a partial GCC 16
  libstdc++ implementation, no libc++/MSVC implementation, and gaps in
  math, bit operations and permutes. Standard-library SIMD removes the
  separate-compiler friction for suitable kernels. The snapshot needs
  rechecking against current releases; coverage gaps do not establish a
  date before which it cannot compete with ISPC.
- **Mojo** has first-class `bfloat16` and six fp8 dtypes including the MX
  scale format. **Slang** and **MLIR** win on generics, modules and AI
  relevance.
- **LLVM auto-vectorization** keeps improving for simple loops, eroding
  the pitch for easy kernels.
- **Vector math libraries.** Every mainstream vector stack ships a tested
  library with ULP guarantees: SLEEF, glibc libmvec, clang `-fveclib`.
  ISPC's stdlib does not publish bounds.

### 1.5 Where engineering time goes

Of about 480 commits in the last year, roughly 35% were CI, 21% LLVM
upgrade and trunk fixes, 8% new targets, 7% stdlib math, 2.5%
optimizations and under 1% language features. Commit percentages are not
engineering-time measurements; time spent on failures and releases is the
right measure for maintenance savings. Cutting maintenance cost is itself
a roadmap topic (section 7).

Capacity model for the year: one engineer and one intern. Reserve 25-30%
of engineer capacity for releases, LLVM compatibility, correctness fixes,
customer blockers and review. Intern timing must fit the work that needs
it rather than being assumed to cover the whole year. AI assistance is
contingency, not a line item.

### 1.6 Evidence basis and open assumptions

Facts in this document come from three kinds of sources.

Checked locally during review (roadmap worktree at commit
`3f5feb0e88a0455e6662da496d95c970f20e1bcc` plus later edits):

- `src/ispc.cpp`: DMR/NVL default widths, x16/x32 target definitions, the
  absence of x86 `target-features` attributes.
- `stdlib/stdlib.ispc`, `stdlib/include/stdlib.isph`,
  `builtins/target-avx10_2-x*-common.ll`, `tests/lit-tests/vnni-avx10.2dmr.ispc`
  and `vnni-avx10.2nvl_llvm22_plus.ispc`: the packed INT8/INT16 dot
  families, their lowering and tests.
- `docs/ispc.rst` ("Using ISPC as a Library"), `src/ispc_impl.cpp`,
  `src/include/ispc/ispc.h`: the library/JIT API.
- `src/opt.cpp`, `src/opt/ImproveMemoryOps.cpp`,
  `src/opt/GatherCoalescePass.cpp`: lowering and pipeline structure.
- `src/expr.h`: `ConstExpr` and per-expression `GetConstant` folding.
- `docs/ispc.rst` lines 3388-3505 and `stdlib/include/short_vec.isph`:
  short-vector support; `--nanobind-wrapper`, `examples/cpu/point_transform_nanobind`
  and `examples/cpu/attention`: Python binding support.
- Local OIDN `devices/cpu/cpu_conv_amx.ispc`; local llama.cpp
  `ggml/src/CMakeLists.txt`, `ggml/src/ggml-cpu/CMakeLists.txt` and CPU
  quantization sources at commit 71ad059.
- LLVM `X86.td` and `X86InstrAVX10.td` on main; LLVM PRs 206888, 208408
  and 208706; Clang 21 release notes.

Research and usage sources carried from the original proposal, not every
numerical result reverified: ACE spec v1.15 (x86ecosystem.org, May 2026)
and whitepaper v1.0 (April 2026); RenderKit changelogs for OIDN, OSPRay,
Open VKL and Embree; ISPC release download counts; GitHub code search for
`enable_language(ISPC)`; Highway README; cppreference C++26 compiler
support; chipsandcheese on Zen 5; Hacker News via the Algolia API; MLGO
docs and google/ml-compiler-opt; google/gemma.cpp README; the arXiv papers
cited in section 5.1; AlphaEvolve, NVIDIA DeepSeek-R1 kernel and Sakana
reporting; llvm.org AI Tool Use Policy. Pin revisions and direct links
when a claim becomes a project dependency.

Unverified: Unreal Engine's vendored ISPC version and exact usage;
game-studio talks; Zen 6 features; Nova Lake beyond LLVM's CPU definition;
GCC, binutils and SDE support for ACE; whether anyone has tried ISPC in
ggml. Planning assumptions still to confirm: intern availability;
representative benchmark machines and DMR/NVL access; ACE emulator or
hardware and OS permission interfaces; attributable AMX lifecycle
guidance; specific customer commitments behind language requests. None of
these is inferred from issue reactions, commit counts or compiler CPU
definitions.

## 2. Performance

Performance is the primary reason to choose ISPC. The topics below are
ranked in section 8; here each is described once with its evidence.

### 2.1 Measurement: baselines, corpus and method

Isolated codegen fixes must be tied to kernels from actual ISPC usage.
Today `benchmarks/03_complex` is empty and the daily internal performance
run over `benchmarks/` is not public.

Baseline corpus. Start small and reproducible: one existing customer
kernel, one memory/control-flow case, an image-layout/remap case for
gathers, `ggml_vec_dot_q4_0_q8_0` for packed-integer and shuffle codegen,
a BF16 dot kernel, and x8-versus-x16 reproducers. Record compiler and
LLVM revisions, target and gang width, machine and OS, input sizes,
numerical options and thread counts. Use stable machine configuration and
repeated measurements to characterize noise, and derive regression
thresholds from that noise before judging any change.

Comparisons. Compare with the previous ISPC baseline, hand-written
intrinsics, Highway and Clang implementations early enough to select
work. Match algorithm, data layout, precision and threading. Users
already make these comparisons without data: an ISPC-versus-alternatives
request ([#3226](https://github.com/ispc/ispc/issues/3226)) and discussions
comparing ISPC with JAX ([#3156](https://github.com/ispc/ispc/discussions/3156),
[#2933](https://github.com/ispc/ispc/discussions/2933)).

Fuller program, when capacity allows:

- Fill `benchmarks/03_complex` with representative kernels: an OIDN-style
  convolution, a texture compressor block, a ray-box traversal loop, a
  Chaos-style particle update, and the llama.cpp kernels of section 4.4.
  Add x8-versus-x16, bf16 and int8 dot-product microbenchmarks.
- Make the daily run public as a dashboard from the existing tracking job,
  with per-PR regression gating on a small subset once dedicated-runner
  stability and noise thresholds are established.
- Publish Highway and plain Clang comparisons on the same kernels, and an
  AI-flavored set (ISPC against numba, Triton-CPU and intrinsics on
  softmax, layernorm, RMSNorm, quantize/dequantize and attention).

Rules for every codegen change. Record its consuming workload, expected
benefit, cost estimate, supported targets, regression checks and stop
condition. Use functional and focused codegen tests plus repeatable
timing. Report distributions and individual losses as well as aggregate
gains. Retain the existing lowering if benefits do not justify
regressions or maintenance cost. If investigation disproves an assumed
defect or finds an already optimal lowering, record the evidence and
substitute the next item explicitly. For memory transformations,
establish legality before profitability: preserve inactive-lane fault
suppression, object bounds, aliasing and the specified behavior of
conflicting scatter addresses; a wider load must not introduce an invalid
access. Include math accuracy checks for functions the selected workloads
use (for example `exp` in softmax), distinguishing measured maximum error
over a stated domain from a proven ULP bound.

### 2.2 Gather, scatter and shuffle lowering

Evidence. Gathers and memory layout are the most common performance
question in discussions: interleaved RGBA
([#2943](https://github.com/ispc/ispc/discussions/2943)), H,W,C image
layouts and a bilinear remap where JAX ran 2x faster
([#2919](https://github.com/ispc/ispc/discussions/2919),
[#2933](https://github.com/ispc/ispc/discussions/2933)), a 4K bitmap rotate
([#3569](https://github.com/ispc/ispc/discussions/3569)), a 3D grid port
([#2744](https://github.com/ispc/ispc/discussions/2744)), scatter warnings
with `soa` ([#2308](https://github.com/ispc/ispc/discussions/2308)) and a
request to silence intentional gather warnings selectively
([#3184](https://github.com/ispc/ispc/discussions/3184)). The oldest open
performance issues are all here: [#304](https://github.com/ispc/ispc/issues/304)
and [#330](https://github.com/ispc/ispc/issues/330) (coalescing),
[#1256](https://github.com/ispc/ispc/issues/1256) (index scale immediate),
[#1531](https://github.com/ispc/ispc/issues/1531) and
[#1581](https://github.com/ispc/ispc/issues/1581) (16-bit indices),
[#2778](https://github.com/ispc/ispc/issues/2778) (gather whose source
and destination have the same type), [#2899](https://github.com/ispc/ispc/issues/2899)
(performance warning), [#3153](https://github.com/ispc/ispc/issues/3153)
(`--opt=disable-gathers` failures on 32-wide targets),
[#3771](https://github.com/ispc/ispc/issues/3771) (shuffle should use
native permutes), [#3721](https://github.com/ispc/ispc/issues/3721) and
[#2720](https://github.com/ispc/ispc/issues/2720). Inference adds byte
lookup tables: IQ quant formats and the `iq4nl` codebook in llama.cpp use
`pshufb`-style in-register byte LUTs, and current ISPC lowering may turn
them into gathers.

Current state. ISPC emits one generic `llvm.masked.gather` shape and lets
each backend cope. `GatherCoalescePass` has TODOs at lines 851 and 970
about conservative store handling and pseudo-call metadata, and
`ImproveMemoryOps` predates many LLVM improvements. Hardware diverged
meanwhile: AVX2 gathers are microcoded and slow after Downfall
mitigations, AVX-512 gathers are acceptable but scatters are not, NEON
has neither, SVE and RVV have both with predication. On x86 hardware
gather indices are 32 or 64 bits; 16-bit values should be retained before
extension where profitable rather than assuming a 16-bit-index gather.

Proposal, from bounded to full:

1. Turn a gather with a provable permutation of a contiguous block into a
   safe load plus shuffle, starting from the cases in #2778 and the
   coalescing reports, preserving bounds and inactive-lane fault
   behavior.
2. Lower the byte and permutation shuffle patterns needed by
   `ggml_vec_dot_q4_0_q8_0` to native shuffle/permute instructions
   (#3771, narrow-integer cases from
   [#2901](https://github.com/ispc/ispc/issues/2901)), adding any minimal
   helper needed to express them.
3. Redesign gather coalescing around address-expression analysis (base
   plus per-lane offset decomposition, stride and permutation detection)
   instead of pattern matching, and cover the store side.
4. An ISPC-owned per-target lowering with a cost table choosing among
   contiguous or strided loads plus shuffles, small-table permutes,
   scalarized loops and the native instruction, handling index width and
   scale. This is the single change that moves the most kernels on the
   most targets, and the natural consumer of the fitted cost model in
   section 5.3.
5. Emit `llvm.masked.load/store/gather/scatter` and
   `llvm.experimental.vector.compress` where LLVM lowers them well, and
   retire the matching ISPC pseudo builtins after semantic and
   performance checks. Verify intrinsic names in the chosen LLVM
   baseline. This may shrink LLVM-upgrade cost but is not a prerequisite
   for steps 1 and 2.

Steps 1 and 2 are bounded and serve both existing image/layout workloads
and quantized inference. Steps 3 to 5 are the full program.

### 2.3 Loop, mask and control-flow codegen

Evidence. Irregular computation expressed as SPMD pays for masks,
branches and loop bookkeeping even when the arithmetic vectorizes well.
[#3455](https://github.com/ispc/ispc/issues/3455) (loop overheads: mask
recomputation, induction variable widening, unroll decisions),
[#3018](https://github.com/ispc/ispc/issues/3018) (`foreach` remainders are
peeled rather than predicated),
[#3238](https://github.com/ispc/ispc/issues/3238) (unsigned loop counters
defeat trip-count analysis), [#3804](https://github.com/ispc/ispc/issues/3804)
(inlining heuristics and code bloat) and
[#2920](https://github.com/ispc/ispc/issues/2920) (bool and mask
representation on AVX2 and AVX-512) describe one root cause: peeled
remainders, unsigned counters, materialized masks and inlining losses.
[#3649](https://github.com/ispc/ispc/issues/3649) shows the same cause from
the other side: a 20 percent regression after LLVM 18.1 removed a pass the
hand-ordered ISPC pipeline relied on. Related: coherent-control-flow tests
should use `llvm.vector.reduce.and/or`
([#1338](https://github.com/ispc/ispc/issues/1338)); varying integer
division and modulo ([#3000](https://github.com/ispc/ispc/issues/3000),
[#3001](https://github.com/ispc/ispc/issues/3001)); LLVM replacing vector
loops with libc calls ([#3241](https://github.com/ispc/ispc/issues/3241)).

Current state. The masked-versus-branch choice for divergent `if` is a
fixed heuristic plus `cif`. x86 functions carry no `target-features`
attribute (`src/ispc.cpp`, comment "not necessary to set target-cpu and
target-features"); only ARM sets `target-cpu`. This hides the ISA from
LLVM's inliner and cost models, and lets an AVX2 body legalize to SSE
silently when linked with `-flto`.

Proposal:

- Set `target-cpu` and `target-features` on x86 functions. Small,
  independent, and a prerequisite for anything that inlines ISPC code into
  other code.
- Rebuild `foreach` lowering around a predicated (tail-folded) remainder
  and re-sync the hand-ordered pipeline with LLVM's default pass manager,
  so upgrades stop eroding codegen silently. The industry direction is
  tail folding with vector-predication intrinsics, which the RVV, SVE and
  AVX-512 backends consume well.
- Fix the measured loop cases (#3455, #3238), the bool/mask
  representation (#2920, #1338), magic-number sequences for uniform
  divisors and a better vectorized fallback for division/modulo (#3000,
  #3001), inlining and code-size tuning (#3804) and vector-loop
  preservation (#3241).
- Measure masked execution versus branch-around across divergence rates
  and targets; define which inputs are static and which would need
  profiling. A learned decision is an extension of the cost-model
  research in section 5.3.

### 2.4 Lane widths and narrow integers

Evidence. Double-precision code wants i64x4-like register pressure with
AVX-512's 32 registers ([#2170](https://github.com/ispc/ispc/discussions/2170))
and reports prologue spills of varying structs
([#2173](https://github.com/ispc/ispc/discussions/2173)).
[#2903](https://github.com/ispc/ispc/issues/2903) (`avx2-i64x8`
double-pumped target) is the top-voted codegen issue;
[#3161](https://github.com/ispc/ispc/issues/3161) reports a restriction to
ymm for 64-bit values on `avx512-x8` targets. Inference adds the opposite
pressure: byte and nibble unpacking and packed dot products need good
8/16-bit codegen ([#2901](https://github.com/ispc/ispc/issues/2901)),
`popcnt` ([#3676](https://github.com/ispc/ispc/issues/3676)), native
permutes for `shuffle` (#3771) and shuffle, rotate and shift on uniform
short vectors ([#3446](https://github.com/ispc/ispc/issues/3446)).
Discussions also ask for movemask ([#2632](https://github.com/ispc/ispc/discussions/2632)),
prefix sum ([#2436](https://github.com/ispc/ispc/discussions/2436)) and
sorting many small arrays ([#3533](https://github.com/ispc/ispc/discussions/3533)).

Proposal. These are separate cases rather than one preferred width: add
`avx2-i64x8`; investigate #3161 against current LLVM and ISPC codegen;
improve varying 8/16-bit, popcnt and permute lowering; add the missing
cross-lane primitives. New targets widen coverage rather than deepen it,
and inherit whatever quality sections 2.2 and 2.3 deliver. A blocker for a
selected inference or customer kernel can move ahead of the rest.

### 2.5 Math library

Evidence. Users expect C-library ULP behavior
([#3212](https://github.com/ispc/ispc/discussions/3212)), ask for
`log1p`/`expm1` ([#3476](https://github.com/ispc/ispc/discussions/3476)),
and report double `rcp_fast` slower than division on Xbox Series X
([#3257](https://github.com/ispc/ispc/discussions/3257)).
[#2236](https://github.com/ispc/ispc/issues/2236) asks for documented ULP
bounds; [#2906](https://github.com/ispc/ispc/issues/2906) is a production
user offering the Intel SVML algorithms as a basis for fast accurate
transcendentals without the proprietary library;
[#3406](https://github.com/ispc/ispc/issues/3406) covers Sleef-derived
kernels; [#2290](https://github.com/ispc/ispc/issues/2290) asks for complete
`float16` math; batched and fast variants were requested in
[#1310](https://github.com/ispc/ispc/issues/1310),
[#1570](https://github.com/ispc/ispc/issues/1570) and
[#25](https://github.com/ispc/ispc/issues/25).

Current state. The stdlib transcendentals derive from Cephes with
target-dependent accuracy and no published bounds. Transcendentals
dominate many kernels, so accuracy and throughput must be judged together
on the same per-function, per-target benchmarks.

Proposal:

- Finish the accuracy audit and publish ULP tables per function and target
  (#2236), distinguishing tested-domain maxima from proven bounds.
- Evaluate SVML-style and Sleef-derived kernels for AVX10.2 (#2906,
  #3406) using those tables plus benchmarks.
- Complete `float16` math (#2290). Independently useful; not a
  prerequisite for BF16 storage, conversion or dot products.
- Longer term, regenerate the transcendental stdlib from one ISPC source
  per precision level with a stated ULP target and a ULP test harness.
  Every target, including ARM and future ones, then gets identical
  accuracy, and `--opt=fast-math` picks cheaper polynomials consistently.
  This is the most portable performance item of all because it is target
  independent by construction.

## 3. New Intel hardware: Diamond Rapids, Nova Lake, ACE

### 3.1 Diamond Rapids and Nova Lake

What LLVM's `X86.td` says:

- **diamondrapids** = Granite Rapids-D plus AVX10.2, the full APX set
  (EGPR, NDD, NF, CCMP, ZU, Push2Pop2, PPX), AMX-FP8, AMX-MOVRS,
  AMX-AVX512, AVX-VNNI-INT8/INT16, AVX-NE-CONVERT, AVX-IFMA, MOVRS,
  SHA512, SM3/SM4, CMPCCXADD. All eight LLVM AMX features are present.
  There is no AMX-TF32 or AMX-TRANSPOSE feature in LLVM.
- **novalake** = Panther Lake plus AVX10.2, MOVRS, the full APX set,
  JMPABS and PREFETCHI. No AMX. Because the 256-bit-only option was
  removed from the AVX10 specification, this means 512-bit AVX10.2 and
  APX on client, ending the AVX-512-on-client gap that has existed since
  Alder Lake. Verified only from LLVM's CPU definition, not from a product
  announcement. Product dates must not be inferred from CPU names in LLVM.

What ISPC has. `avx10.2dmr-*` and `avx10.2nvl-*` targets, APX enabled by
default with `--opt=disable-apx`, and the APX/AMX dispatcher capability
work shipped (PR [#3922](https://github.com/ispc/ispc/pull/3922), commit
5101e97e3). **DMR defaults to x16; NVL defaults to x8** (`src/ispc.cpp`).
The signed/signed, unsigned/unsigned INT8 and unsigned/mixed INT16 packed
dot families already have stdlib implementations, fallbacks, DMR/NVL
lowering and tests.

Open problem: x16 versus x8. Users report avoidable codegen losses at x16
compared with x8 on these targets. Produce minimized reproducers, capture
the affected LLVM revision, and establish whether each case is an LLVM
regression, an ISPC lowering problem or a legitimate workload tradeoff
(divergence, tails, memory behavior and register pressure can favor x8).
Ship an upstream fix or a narrow ISPC workaround for confirmed
regressions; retain a reproducible result if no defect is found. Codegen
analysis can start before hardware access; width recommendations need
hardware. Begin early because an LLVM fix may have a long lead time.

Width guidance and defaults. Document the measured choice per workload
and CPU family. Change NVL's default to x16 and extend the commonly used
game-engine multi-target set (`sse4`, `avx2`, `avx512skx`) with
`avx10.2nvl-x16` only after representative hardware results. A question
about which target to use for Zen 5 AVX-512
([#3827](https://github.com/ispc/ispc/discussions/3827)) shows target
naming and recommended defaults need documenting generally. ACE's use of
ZMM operands does not require unrelated kernels to use x16.

APX. Three hypotheses from the original proposal: EGPR may relieve
scalar/address-register spills in wide gangs; NDD/NF may shorten mask
arithmetic; CCMP may improve mask tests. Inspect generated instructions
and benchmark the affected kernel before generalizing; preserve confirmed
improvements in lit tests. A broad APX codegen audit across wider
workloads is the extended form.

AVX10.2 instruction surface. Beyond the dot products already exposed,
AVX10.2 adds BF16 arithmetic (`VADDBF16`, `VFMADD*BF16`, `VCMPBF16`,
`VSQRTBF16`, `VRNDSCALEBF16`), FP16/FP8 conversions
(`VCVT[BIAS]PH2{BF8,HF8}[S]`, `VCVTHF82PH`), the FP16 pair dot `VDPPHPS`,
saturating converts and minmax. These connect the hardware to the AI
types in section 4 rather than adding another target name; their
exposure follows the type work.

LLVM compatibility. Clang 21 release notes deprecate the `avx10.x-256`,
`avx10.x-512`/`evex512` feature spellings. Identify remaining uses and the
affected LLVM version before scheduling a migration; treat the necessary
changes as maintenance.

Hardware for measurements. Use AVX2 (for example Zen 3 or Alder Lake) and
AVX-512 machines (Sapphire Rapids, Granite Rapids) for baselines. Granite
Rapids is not an AVX10.2 test machine. DMR/NVL performance conclusions
require that hardware; SDE and emulation establish functionality, not
timing.

### 3.2 ACE

#### ACE primer

ACE (AI Compute Extensions) is a matrix-multiply ISA jointly specified by
AMD and Intel under the x86 Ecosystem Advisory Group. Spec v1.15 is dated
May 2026, the whitepaper April 2026. The whitepaper calls it "the standard
matrix acceleration architecture" for x86 "from laptop to data center".
No product names, code names or dates are public.

Mental model: ACE is "VNNI with a 16x16 accumulator". A VNNI instruction
like `VPDPBUSD` takes two 512-bit vectors, each lane holding 4 packed
bytes, and produces 16 dot products into 16 int32 lanes. An ACE
outer-product instruction takes the same two vectors but produces all
16 x 16 = 256 pairwise dot products at once into a tile register: 1024
int8 multiplies per instruction instead of 64.

Architectural state:

- Eight tile registers, each 16 rows x 512 bits (1 KB), a 16x16 grid of
  32-bit accumulators (int32 or fp32). Only 32-bit accumulators exist in
  v1.
- Tiles reuse AMX's TILEDATA state and TILECFG register. ACE is "palette
  2"; AMX TMUL is palette 1. One active palette per thread; switching
  palettes is a defined state transition and live accumulators do not
  survive it.
- `LDTILECFG` is still required before first use, but the palette-2
  descriptor is a fixed 64-byte blob (byte 0 = 2, rest zero). Shapes are
  fixed; no per-tile rows/colsb configuration.
- **No tile loads or stores.** `TILELOADD` and `TILESTORED` raise #UD in
  palette 2. Data enters tiles only through ZMM registers via `TILEMOVROW`
  (write or read a row) and `TILEMOVCOL` (write a column). Reading a full
  tile is 16 row moves.
- One 1024-bit Block Scale Register (BSR) holds 4 groups x 16 E8M0 scales
  for the row input and 4 x 16 for the column input. New XSAVE component
  (XCR0 bit 20), so the OS must opt in.

Instructions:

- Integer: `TOP4BSSD`, `TOP4BSUD`, `TOP4BUSD`, `TOP4BUUD`. "4" means each
  32-bit lane packs 4 int8 values (the K dimension); the two letters give
  the signedness of row and column inputs. Lane i of the first source
  feeds tile row i, lane j of the second feeds column j:
  C[i][j] += sum over k of A[i].k * B[j].k, a 16x16x4 GEMM step.
- BF16: `TOP2BF16PS`, 2 packed bf16 per lane, fp32 accumulate, fixed
  round-to-nearest-even, no exceptions.
- MX FP8: `TOP4MX{B,H}{B,H}F8PS`, B = E5M2 and H = E4M3 in all four
  combinations. Products are summed exactly, scaled by the BSR groups
  selected by an immediate, then added as fp32.
- MX INT8: `TOP4MXBSSPS`, int8 with implicit 2^-6 per element and E8M0
  block scales, fp32 accumulate.
- Row conversion on readback: `TCVTROWD2PS` (int32 to fp32),
  `TCVTROWPS2BF16H/L` and `TCVTROWPS2PHH/L` (fp32 to bf16 or fp16 halves).
- `VUNPACKB` (AVX10_V2_AUX): expands packed 2-7-bit fields to bytes with
  optional sign extension. Useful for 4-bit and 6-bit quantized weights
  and any bitfield unpacking, well beyond AI.
- AVX10_V2_AUX conversions (merged in LLVM): fp32 to fp8 with
  round-to-nearest, round-to-odd, saturating and bias (stochastic
  rounding) forms; fp8 to fp32; fp8 to and from FP4 (E2M1) and FP6 (E2M3,
  E3M2); `VPMOVSSDB` int32 to int8 with symmetric saturation.

Data layout. For int8 the row input must be transposed at 4-byte
granularity (AT[k/4][m][k%4] = A[m][k]) and the column input interleaved
the same way (Bp[k/4][n][k%4] = B[k][n]). Each K-step is then two 64-byte
loads. Eight tiles allow a 4x2 arrangement (64x32 output window) and
amortize loads to 0.75 per outer product.

ACE versus AMX for a compiler:

| | AMX (palette 1) | ACE (palette 2) |
|---|---|---|
| Tile shape | Configurable per tile | Fixed 16x16 x 32-bit |
| Inputs | Loaded from memory into tiles | Fed from ZMM registers |
| INT8 work at full tile size | 16x16x64 MACs | 16x16x4 MACs |
| Data types | int8, bf16, fp16, fp8, complex | int8 (4 signedness combos), bf16, OCP fp8, MX fp8, MX int8 |
| Scaling | None | Per row and column via BSR |
| Spill | `TILESTORED` | 16 row moves plus 16 stores |
| Fusion with vector code | Through memory; AMX-AVX512 adds transfers | Direct ZMM/tile transfers |

ACE's vector inputs make fused pre- and post-processing attractive: a
`varying int32` on an x16 gang is exactly a 512-bit input operand. The
accumulator remains tile state, and cross-lane outer products need
collective semantics beyond per-lane operations. The data-movement
opportunity does not by itself establish a performance win.

Toolchain status (October 2026). LLVM PR
[206888](https://github.com/llvm/llvm-project/pull/206888) (`avx10v2aux`)
is merged. PR [208408](https://github.com/llvm/llvm-project/pull/208408)
(`acev1`, header `acev1intrin.h`, type `__acetile`, intrinsics
`__tile_ace_*`) and PR [208706](https://github.com/llvm/llvm-project/pull/208706)
(`x86_bsr` IR type) are open, with changes requested on the latter. Intrinsic names differ between the spec and the
PR, and the spec has internal inconsistencies on BSR byte layout and
immediate bit positions. The proposed LLVM backend supports tile spill
and reload. GCC, binutils and emulator support are unverified. LLVM 23/24
are candidate integration versions; inclusion is an external dependency,
not a date.

#### Proposed ISPC surface

Design first. Specify an x16 leaf-kernel interface in `ace.isph`: opaque
`uniform tile_i32` and `tile_f32`, initialization, INT8 outer products in
the four signedness combinations, BF16 outer products with FP32
accumulation, and the row/column transfers a proof kernel needs. Opaque
built-in types do not require struct templates or `constexpr`; existing
constant-expression checks suffice. Settle signatures and lifetime rules
before promising source compatibility.

| Operation | Proposed shape and meaning |
|---|---|
| Accumulator | Opaque `uniform tile_i32` or `uniform tile_f32`; `tile_zero` initializes it. |
| INT8 outer product | `tile_op4_ss/su/us/uu(tile&, varying int32 a, varying int32 b)`: each 32-bit lane holds four packed bytes; one operand supplies rows, the other columns. |
| BF16 outer product | `tile_op2bf16` takes two packed BF16 values per 32-bit lane and accumulates into FP32 tile elements. |
| Readback | `tile_getrow(uniform int r)` returns a varying row; on x16 its 16 elements map to the gang. The final signature also identifies the source tile. |
| Vector-to-tile transfer | `tile_setrow` and `tile_setcol` write varying values into a selected row or column. |
| MX extension (later) | `tile_op4mx*` with scale-group selectors; `bsr_set(varying uint32 a_scales, varying uint32 b_scales)` supplies two packed 512-bit scale vectors. Immediate selectors require compile-time constants, not merely uniform values. |

The source-level benefit is concrete: dequantization and unpacking
produce vector operands, outer products update tile accumulators, and
row readback feeds vector activation or requantization. This avoids the
store-and-reload idiom in OIDN's current AMX code. The compiler manages
palette-2 configuration and release; users never hand-build the 64-byte
descriptor.

Requirements to resolve in the design:

- **Execution and masks.** Define ACE operations as gang collectives with
  uniform control flow initially. Define or reject invocation under
  partial masks, varying branches and early exits. Specify independent
  row/column validity and K-tail handling for padded edge tiles; an
  execution mask alone is insufficient. Include negative tests and
  non-finite BF16 values where zero padding affects floating-point
  behavior.
- **Mapping.** Start with x16 packed 32-bit operands, one ZMM each, and a
  documented lane-to-row/column mapping. Restrict unsupported widths
  explicitly. On x32 a `varying int32` is 1,024 bits: subgroup mapping
  and row readback need a separate design, not reinterpret helpers.
  Native x8 was not proposed. The goal of removing OIDN's
  `TARGET_WIDTH == tile rows` constraint remains, but reinterpretation
  alone does not achieve it.
- **State ownership.** Define tile and BSR lifetime, initialization,
  release, caller/callee responsibilities and AMX palette transitions.
  Initially restrict live tiles across non-inlined calls. Coordinate
  `LDTILECFG` and `TILERELEASE` with LLVM's lowering on all exits. Tile
  references in builtins must not imply address-taking or a C ABI.
- **Constants.** Where an instruction encodes an immediate, require and
  validate a compile-time constant.
- **Register pressure.** Measure tile and ZMM pressure and report
  expensive spills. A strict no-spill diagnostic mode is a retained design
  option, not an initial promise; a fixed 16x16 accumulator can impose
  substantial vector-register pressure in any fallback.
- **Capability and OS contract.** Represent ACE as an explicit capability
  independent of the DMR/NVL names. Verify the detection recipe against
  the final ISA and OS interface: CPUID.(7,1):ECX[11], ACE_VSN >= 1
  through leaf 1Dh sub-leaf 2, AVX10_V2_AUX through leaf 24h, and the
  required XSTATE components (the proposal identifies bits 17, 18 and
  20). Define permission acquisition and failure/fallback behavior per
  thread. CPUID/XCR0 checks alone do not replace the OS permission
  contract.

Extended surface, after the x16 INT8/BF16 subset: MX operations with
`bsr_set` (BSR layout depends on LLVM PR 208706), x32 mapping, a generic
`uniform tile<T>` built on struct templates, and a VNNI/FMA fallback on
AVX2 and AVX-512 so the same source runs before ACE hardware exists.
Reference correctness and competitive fallback performance are separate
deliverables; competitive AVX2 tile emulation is its own performance
project. Proof kernels: a small GEMM with fused epilogue against a scalar
reference first; later the llama.cpp Q4_K GEMM and an OIDN-style
convolution.

#### Validation gates

| Gate | Evidence | What can be claimed |
|---|---|---|
| Semantic/reference | Reviewed execution/lifetime contract; independent reference checks for mapping, tails and numerics | The programming model has executable tests. |
| Native codegen | IR and assembly checks for supported operations, configuration, release and diagnostics | Experimental compiler support emits the intended code. |
| Native execution | Hardware or emulator runs against the reference, including OS capability failure paths | Functional support for the tested ISA/toolchain/OS. |
| Performance | Repeatable measurements on representative hardware, including packing and state-management costs | Performance results for that hardware/workload. |

A lit-only implementation is experimental. If native execution is
unavailable, publish the prototype's limits and dependency status; do not
report validated ACE support. The engineer owns upstream coordination and
hardware or emulator access; record owner, expected availability and the
LLVM baseline early, and track PRs 208408 and 208706. The INT8/BF16
subset should not require MX API work unless the chosen LLVM backend
imposes it.

#### Relationship to AMX and OIDN

The original proposal assumed AMX will be deprecated in favor of ACE but
provided no attributable product commitment or timeline. Record such
guidance if it exists; do not derive it from LLVM support. Keep
`amx.isph` working for OIDN and other consumers, allowing small measured
fixes and shared improvements such as `TILEMOVROW`-based row readback.
Customer-driven AMX additions (raw intrinsics for AMX-FP8, AMX-MOVRS,
AMX-AVX512) are admitted on measured benefit for a current consumer. An
AMX lowering for the new tile abstraction is not proposed.

OIDN's AMX convolution uses FP16 inputs with FP32 accumulation. ACE v1
multiplies BF16 but not FP16, and FP16-to-BF16 conversion loses
precision. An OIDN-style ACE port therefore requires a separate numerical
quality decision and is not the initial proof of transparent
portability.

### 3.3 Other targets and platforms

- **ARM and WASM** stay in maintenance mode, including correctness of
  supported portable fallbacks. One integration exception: for the
  reported Embree and Unreal macOS universal-binary build failures
  (discussions [#2854](https://github.com/ispc/ispc/discussions/2854),
  [#2046](https://github.com/ispc/ispc/discussions/2046)), reproduce the
  consuming build and document and test a supported CMake recipe for
  x86_64 plus arm64 outputs.
- **ARM SVE/SVE2** ([#1947](https://github.com/ispc/ispc/issues/1947)).
  Clang and GCC compile fixed-width vector code to SVE under
  `-msve-vector-bits=N`; ISPC can do the same by adding AArch64 targets
  with the `sve` features and a fixed `vscale_range` attribute. Most
  deployed Arm server cores (Neoverse N2, V2, Graviton 3 and 4) implement
  128-bit SVE2, so the gain over `neon-i32x4` is predication and
  gather/scatter, not width. Vector-length-agnostic code is out of reach:
  `programCount` is a compile-time constant in user code, the stdlib and
  the layout of `soa` and varying struct members, so the C ABI for
  anything containing a varying value disappears with unknown width;
  ISPC codegen relies on arbitrary `shufflevector` masks and lane-index
  constants that scalable vectors forbid. RVV has the same shape of
  problem. Recommendation if pursued: fixed-length SVE targets through
  the native backend, no scalable-semantics work.
- **WebAssembly SIMD** coverage gaps ([#2127](https://github.com/ispc/ispc/issues/2127))
  and **generic SPIR-V** ([#982](https://github.com/ispc/ispc/issues/982))
  remain survey requests; verify current support and a consumer before
  estimating.
- **Xe GPU** remains opt-in with no new feature commitments.
- **SIMT GPUs.** ISPC had an NVPTX target from 2014 to 2019 (removed in
  commit 667fafb67) that mapped a program instance to a CUDA thread with a
  width-1 target; it rotted for lack of an owner. A revival would need a
  width-1 target, real branches on the per-thread predicate, convergence
  rules for `uniform` values and collectives, `launch` mapped to grids,
  address spaces and device-side `print`. Not proposed.

## 4. AI in the language: BF16, low-precision types, inference kernels

### 4.1 `bfloat16`

Evidence. OIDN proves a production CPU neural network can be ISPC;
llama.cpp exposes block-quantized INT4/INT8 kernels with FP16 scales and
BF16 GEMM paths; Highway and Mojo already provide the low-precision types
AI developers expect. Direct requests: [#2361](https://github.com/ispc/ispc/issues/2361)
and [#1940](https://github.com/ispc/ispc/issues/1940) (`bfloat16` type),
discussion [#3627](https://github.com/ispc/ispc/discussions/3627), and
VNNI ([#2736](https://github.com/ispc/ispc/issues/2736)). These are the
only AI-motivated production requests on the tracker; Python bindings
without BF16 and FP8 land flat.

Value. A kernel can consume and produce the framework's stored format
while accumulating in FP32, and the type connects to ACE's BF16 outer
product and AVX10.2's BF16 instructions. OIDN's FP16 kernel provides
programming-model experience but converting it to BF16 is a separate
numerical decision.

Proposal, in order:

1. Uniform and varying BF16 storage, loads/stores, casts and conversion
   helpers, with lexer, type-system, constant-folding, diagnostics and
   debug-info support. Review aggregates, overload resolution, mixed-type
   expressions and memory layout. Test with an independent conversion
   oracle: exhaustively over BF16 input encodings where feasible, and at
   FP32-to-BF16 rounding boundaries.
2. A BF16-input dot-product kernel returning FP32, measured for
   conversion cost against matching intrinsics and an independent
   reference.
3. BF16 pair dot with FP32 accumulation: `VDPBF16PS` on suitable AVX-512
   CPUs (SPR, GNR) and AVX10.2 forms on DMR/NVL, with documented
   accumulation semantics and a correct fallback elsewhere.
4. Native BF16 arithmetic through the AVX10.2 `V*BF16` instructions,
   admitted operation by operation. The portable fallback promotes to
   `float`, computes and demotes; verify its rounding, contraction,
   subnormal and exceptional-value behavior against the contract rather
   than assuming equivalence to native instructions.

Contract. Specify rounding, intermediate precision, contraction/FMA,
subnormals, NaNs, signed zero, overflow and interaction with fast-math.
Ordinary expressions and explicit native intrinsics must be
distinguishable. Portable lowering must preserve the specified behavior
on all supported targets, including maintained ARM/WASM configurations.

ABI. A `uint16_t` representation with conversion helpers describes stored
bits; it does not establish a by-value BF16 calling convention. Validate
generated headers, alignment and ABI, or diagnose unsupported boundary
forms. Prefer a documented memory-based interface for the first example.
Full FP16 math, FP8 types, general templates and `constexpr` are not
dependencies.

### 4.2 FP8 and sub-byte formats

Proposal, admitted when a chosen kernel needs it and the numerical
contract is settled:

- `float8_e4m3` and `float8_e5m2` as storage-only types with explicit
  conversion builtins. Specify exact encodings, exceptional values and
  saturation, not only exponent and mantissa widths.
- Lower through `VCVT*PH2{BF8,HF8}` and `VCVTHF82PH` on AVX10.2,
  `VCVTPS2{BF8,HF8}` and `VCVT{BF8,HF8}2PS` with `avx10v2aux`, and
  portable bit manipulation elsewhere. Define direct versus staged
  FP32/FP16 conversions; intermediate rounding changes results. Expose
  saturating, round-to-odd and bias forms by name.
- `unpack_bits<2..7>()` and pack helpers for int4, FP4 (E2M1) and FP6
  (E2M3/E3M2) block formats, lowered to `VUNPACKB` with a portable
  fallback. A minimal nibble helper may precede the family; integer
  storage plus conversion helpers can precede types.
- E8M0 scale helpers for MX block formats and the symmetric
  INT32-to-INT8 saturating conversion (`VPMOVSSDB`).

### 4.3 Dot products, reductions and lookup helpers

Already implemented: `dot4add_u8i8packed`, `dot4add_i8i8packed`,
`dot4add_u8u8packed`, `dot2add_i16i16packed`, `dot2add_u16i16packed`,
`dot2add_u16u16packed` and their saturating variants, with native
lowering on VNNI targets. Audit their code quality and fallbacks on the
selected kernels, additional targets and emulated paths, including
`vpdpbusd` on AVX-VNNI-only CPUs (the `avx2vnni` target exists), rather
than reimplementing them.

| Primitive | Hardware and data-model connection | Status |
|---|---|---|
| Packed INT8/INT16 dots | Four byte or two 16-bit products per 32-bit lane; signedness and saturating variants map to VNNI forms. | Implemented; audit. |
| BF16 pair dot | `VDPBF16PS` on AVX-512 (SPR/GNR), AVX10.2 forms on DMR/NVL; FP32 accumulation. | Part of 4.1. |
| FP16 pair dot | `VDPPHPS`, FP16 inputs with FP32 accumulation. | Proposed. |
| Segmented reductions | Sum per 4, 8 or 16 lanes for block-quantized per-block results instead of hand-combined `reduce_add` and shuffles. Define subgroup, partial-mask and overflow rules. | Proposed; measure against existing idioms. |
| Widening reductions | Horizontal INT8/INT16 sums into INT32 with explicit overflow behavior. | Proposed. |
| Byte LUT and unpack helpers | `pshufb`/`VPERMB`/`VPERMI2B`-style codebooks and packed low-bit weights that are awkward as lane-wise code. | Minimal patterns for Q4_0 under section 2.2; general family proposed. |

Introduce only the helper an accepted workload needs, with defined
overflow, saturation, mask and cross-lane semantics.

### 4.4 llama.cpp exploration

Purpose: learn whether ISPC can match or beat hand-written intrinsics in
the most scrutinized CPU inference code base, and surface programming
model gaps with real kernels. gemma.cpp is the comparison point.

Backend snapshot (commit 71ad059):

- Every weight type has a `vec_dot` function and `vec_dot_type` in
  `type_traits_cpu[]`. Legacy quants (Q4_0, Q5_0, Q8_0, IQ4_NL, MXFP4)
  dot against Q8_0 activations (32 int8 plus an fp16 scale). K-quants and
  IQ formats dot against Q8_K (256 int8, fp32 scale, int16 block sums).
- `ggml_compute_forward_mul_mat` first tries the llamafile templated
  register-tile GEMM, otherwise quantizes activations and calls per-chunk
  vec_dot with chunks claimed by an atomic counter on ggml's threadpool.
- Hot x86 kernels live in `arch/x86/quants.c` as ladders of
  `#if __AVX512F__ / __AVX2__ / __AVX__ / __SSSE3__`. The AVX2 Q4_0
  kernel: multiply the two fp16 block scales, unpack 32 nibbles to bytes,
  subtract 8, `_mm256_dpbusd_epi32` (or maddubs plus madd without VNNI),
  fp32 FMA accumulate, one horizontal sum at the end.
- Repacked "8x8 interleaved" weights have dedicated `gemv_*_8x8` (decode)
  and `gemm_*_8x8` (prefill) kernels in `arch/x86/repack.cpp`; a newer
  `tiled/` directory adds tiled K-quant matmul and an MoE path.
- An AMX backend (`amx/mmq.cpp`) handles Q4_0, Q4_1, Q8_0, Q4_K, Q5_K,
  Q6_K, IQ4_XS, F16 and BF16 with 16x16x32 tiles, palette 1, per-thread
  tile-config init.
- `GGML_CPU_ALL_VARIANTS` builds the whole backend once per feature set
  (haswell, skylakex, cascadelake, cooperlake, alderlake, sapphirerapids
  and so on) into shared libraries and dlopens the highest scoring one.
  No AVX10.2 variant yet.
- `ggml-base` alone does not provide the CPU `*_generic` dot oracles; a
  harness needs the CPU-backend sources or linkage.

Where the ISPC model will be stressed:

- **Block-structured formats versus the gang.** A 32-element block with
  one fp16 scale maps to "one gang equals one block, lanes over elements"
  on i8x32, or to "lane equals output row", which needs repacked weights
  to avoid gathers (why ggml repacks 8x8). ISPC needs a clean idiom for
  "block as uniform struct, elements as varying" without gathers.
- **Narrow and sub-byte lanes.** Nibble extraction into two lanes per byte
  is a byte-to-two-lane shuffle, awkward in SPMD form. Test existing
  idioms and i8x32/i8x64 codegen first; `unpack_bits` (section 4.2) is
  the candidate helper.
- **Int8 dot semantics.** `dpbusd` sums four u8 x s8 products into one
  int32 lane. `dot4add_u8i8packed` expresses this and the s8 x s8 form
  exists; validate the signedness transformation ggml uses for maddubs
  and its edge cases.
- **Horizontal reductions.** ggml keeps a vector accumulator and does one
  horizontal sum per row; ISPC code must be written the same way.
  Segmented reductions (section 4.3) may help per-block sums.
- **Table lookups.** IQ formats and the `iq4nl` codebook use byte LUTs
  (section 2.2).
- **Threading and interop.** ggml owns the threadpool, so ISPC kernels
  are exported per-chunk functions with no `launch`. Block structs
  (`block_q4_0` with `ggml_half`) are mirrored as ISPC structs or passed
  as byte pointers plus strides.
- **Dispatch.** Inside each ggml variant build, ISPC compiles for one
  fixed target matching the variant's flags. A single ISPC multi-target
  binary versus `GGML_CPU_ALL_VARIANTS` is a separate useful comparison.

Staged plan:

- **Stage A, standalone harness.** Kernels in order of difficulty:
  `ggml_vec_dot_q4_0_q8_0` and `ggml_vec_dot_q8_0_q8_0` (calibration
  against a small AVX2 intrinsic path); `ggml_vec_dot_q4_K_q8_K` and
  `ggml_vec_dot_q6_K_q8_K` (nibbles, 6-bit sub-block scales, VNNI;
  confirm their contribution in the chosen Q4_K_M model by profiling, not
  by kernel name); `ggml_gemm_q4_K_8x8_q8_K` (prefill GEMM, register
  tiling, the tile API); softmax with `expf`, rms_norm, swiglu, the
  flash-attention one-chunk path and FP16 KV-cache loads. Add a
  BF16-input FP32-output dot kernel to exercise the BF16 type, since Q4
  dots alone do not. Use the `*_generic` routines as independent
  references; check integer suboperations exactly where defined; give
  scaled floating-point dots a stated tolerance and reduction-order
  policy; cover adversarial inputs, tails, overflow/saturation and
  non-finite values. Never generate both a kernel and its only oracle
  with the same unvalidated transformation.
- **Stage B, backend integration.** A `GGML_ISPC` CMake option compiling
  `.ispc` per CPU variant, replacing entries in `type_traits_cpu` and
  repack tables, validating shared or mirrored block layouts and
  fixed-target flags. Requires owning upstream churn, build and dispatch
  testing and end-to-end validation. Baseline: `llama-bench -p 512 -n 128`
  on a pinned 7B/8B Q4_K_M model, prefill and decode separately, threads
  swept from one to all cores, on AVX2, AVX-512 variants, AMX on/off and
  DMR/NVL AVX10.2 when hardware exists. Report nanoseconds/block,
  packing, conversion and dispatch costs, decode bandwidth against a
  STREAM-style baseline and prefill operations against VNNI/tile peak.

Measurement rules: compare against the matching x86 intrinsic kernels
and Clang or Highway implementations with the same layout and numerics;
include packing, conversion, dispatch and threading costs; distinguish
bandwidth-bound decode from compute-oriented prefill, since faster
isolated instructions need not improve tokens per second; emulation is
for correctness only; pin source revision, backend configuration and
workload. A finding that ISPC is not competitive is also a useful result.
Expected gap list: sub-byte unpacking, integer-dot lowering quality, byte
LUTs, segmented reductions, block-as-uniform idioms, tiles, BF16
operations, FP16 loads and foreign-threadpool guidance.

## 5. AI inside the compiler

### 5.1 Research context

Research notes from the October 2026 proposal; not all independently
re-audited. Verify primary sources, baselines and licenses before using a
claim to select a project. Reported speedups are workload-specific, not
ISPC estimates.

- **MLGO in LLVM** ships two learned heuristics: inlining for size and
  register-allocation eviction. Models are TensorFlow SavedModels compiled
  ahead of time (`ReleaseModeModelRunner`), or TOSA lowered through MLIR
  to C++ (`EmitCModelRunner`); compile-time inference is a small MLP call
  per decision. Enabled with `-mllvm -enable-ml-inliner=release` and
  `-mllvm -regalloc-enable-advisor=release`; models from
  `LLVM_INLINER_MODEL_PATH=download`. Results: up to 7% smaller than -Oz
  for the size model; MLGOPerf reached 1.8% on SPEC2006 and 2.2% on
  Cbench over -O3. Training takes about a day on 100+ vCPUs and needs
  PGO. IR2Vec embeddings are an upstream analysis. (arXiv 2101.04808,
  2207.08389, 1909.06228.)
- **Pass ordering by search or RL** is research only. CompilerGym was
  archived in May 2026. Coreset-NVP picks from 50 candidate sequences for
  4.7% size reduction over -Oz within 45 compilations (2301.05104);
  POSET-RL reports 6.2% size and 12% speed on SPEC2017 over -Oz
  (2208.04238); Protean (TACO 2026) 4.1% over -O3 with fine-grained phase
  ordering (2602.06142); a 2026 study finds 7-10% of pass transitions in
  -O3 make things worse and -O3 is Pareto-dominated on 29 of 30 PolyBench
  kernels (2606.31238). All require evaluating candidate programs.
- **Learned cost models** are standard in TVM-style autotuners (TLP, 9x
  faster tuning search on CPU, 2211.03578) and were shown for LLVM
  vectorization factors by NeuroVectorizer (1.3-4.7x over baseline,
  within 3% of brute force, 1909.13639). No deployed model exists for
  ISPC's gather-versus-shuffle or masked-versus-branch choices.
- **LLM-proposed transformations with formal verification.**
  LLM-Vectorizer gets 1.1-9.4x over ICC, GCC and Clang on TSVC but Alive2
  verified only 38% of results (2406.04693). A 2026 fine-tuned LLM plus
  Alive2 loop matched or beat the target transformation in only 9.8% of
  cases (2609.27214). Trivet (LLM plus Lean translation validation)
  handles 147 of 148 transformations including ten where Alive2 times out
  (2609.19583). AlphaEvolve sped up a production Pallas matmul by 23% and
  FlashAttention by up to 32.5%. Meta's LLM Compiler (7B/13B, trained on
  LLVM IR and assembly, 2407.02524) reaches 77% of autotuning gains for
  flag tuning under a restrictive license.
- **LLM-generated kernels.** KernelBench (2502.10517) shows frontier
  models match PyTorch in under 20% of tasks; NVIDIA's DeepSeek-R1
  verifier loop reached 100% and 96% correctness on the easier levels
  with 1.1-2.1x attention speedups; Intel's Xe-Forge and KForge
  (2605.26118, 2606.02963) and AMD's AsmEvo (2608.20711) report 1.2-5x on
  their GPUs. Sakana's "CUDA Engineer" claimed 100x and was found
  exploiting holes in the correctness check; real results were about 3x
  slower (see also "Correctness Illusion", 2606.20128). No directly
  reusable CPU SIMD generation system was found.
- **Testing.** WhiteFox (LLM reads optimizer source to generate targeted
  tests) found 101 bugs in DL compilers (2310.15991); Fuzz4All found 98
  across GCC, Clang, Z3 and others (2308.04748); ReFuzzer raises validity
  of LLM-generated LLVM tests from 47% to 97% (2508.03603); TargetFuzz
  fuzzes individual passes (2512.04344).
- **Policy.** LLVM's AI Tool Use Policy requires a human in the loop,
  disclosure of substantial generated content, bans unattended agents and
  unreviewed automated review, and forbids AI on good-first issues.

### 5.2 Evaluation principles

- Prefer offline learning that produces a small table, model or fixed
  pipeline. No network calls or large-model inference during compilation.
- Preserve transformation legality independently of the learned
  decision; use functional and codegen tests, differential execution and
  formal checking where applicable. Passing a test suite proves nothing
  about untested behavior.
- Split training and evaluation by workloads, not by random inputs to the
  same kernel. Keep evaluation data out of iterative tuning. "Never worse
  on the training corpus" is not a generalization criterion.
- Compare with the existing heuristic and a simple manually tuned rule,
  and with profiling/PGO approaches where relevant.
- Use features available at the compiler decision point. Divergence
  rates, cache behavior or address distributions need explicit profiling
  or runtime mechanisms.
- Measure compile time, code size, regressions and maintenance cost as
  well as speedup. Follow LLVM contribution and disclosure requirements.
- One experiment at a time, with an independent correctness mechanism,
  held-out evaluation and explicit engineering and review capacity.
  Labels such as "intern project" do not establish an effort estimate.

### 5.3 Candidate projects

The original proposal ranked these as follows. Sizes are proposal
context, not validated estimates.

| Rank | Project | Original sizing | Motivation |
|---|---|---|---|
| 1 | Offline-tuned pass pipelines | Small to medium; intern candidate | ISPC owns `src/opt.cpp`; offline search could produce a better fixed pipeline without runtime inference. |
| 2 | LLM differential fuzzer | Medium; intern candidate | Pass-aware generation on yarpgen CI could expose silent [#3882](https://github.com/ispc/ispc/issues/3882)-like miscompiles earlier. |
| 3 | Fitted masking/gather cost models | Medium; intern candidate | Directly addresses the decisions in sections 2.2 and 2.3. |
| 4 | Width predictor | Medium | Helps users choose x8/x16/x32, initially as advice. |
| 5 | Intrinsics-to-ISPC porting agent | Medium; adoption | Migration path for studios with SSE/AVX2 kernels seeking AVX10.2 and ARM. |
| 6 | LLM-proposed, verified peepholes | Medium; research | Exploit recurring mask/gather IR patterns with reviewed transformations. |
| 7 | Speed-oriented MLGO retraining | Large; training infrastructure | ISPC's stdlib-heavy, already-vectorized IR differs from Google's size-oriented corpus. |
| Companion | ISPC-Bench | Medium; pairs with porting agent | Make CPU SIMD kernel generation measurable and reuse kernels as data. |

Descriptions:

- **Fitted gather-lowering cost model.** Fit a small model or table to
  choose among legal gather, load-plus-shuffle and scalarized
  alternatives exposed by the lowering work in section 2.2, on one target
  family with an established correctness oracle. Compare with the current
  heuristic and a manually tuned rule on held-out workloads, using
  compile-time features. Deliver the corpus split, baselines, model or
  negative result and a reproducible report; ship only if held-out results
  justify the complexity, with a deterministic fallback. Extension: the
  masked-versus-branch decision of section 2.3 and more targets, sweeping
  divergence, gang width and memory patterns offline.
- **Offline-tuned pass pipelines.** Search `src/opt.cpp` pass order and
  parameters offline, starting with a small legal space on one target
  family and preserving mandatory lowerings. Gate on correctness,
  held-out performance, compile time and code size. Goal: better fixed
  pipelines per avx2 x8, avx512 x16, avx10.2 x16/x32 and neon family.
  Research speedups do not guarantee a few percent for ISPC. Also the
  natural pairing with the pipeline re-sync in section 2.3.
- **LLM differential fuzzer.** WhiteFox/Fuzz4All style: seed a generator
  with ISPC grammar and selected pass sources (mask ops, gather
  coalescing, uniform/varying analysis) on yarpgen CI. Compare targets,
  widths and `-O0`/`-O2`, plus an independent scalar C reference compiled
  with Clang for a defined subset. Scalar C must model ISPC collectives,
  masking and integer/FP rules; Clang is not an oracle for arbitrary ISPC.
  Include minimization, deduplication and triage; measure confirmed bugs
  and review time.
- **Width predictor.** Compile kernels at x8/x16 or x16/x32, collect
  features and timings, evaluate `--print-width-advice` before any
  automatic selection. Advice does not replace the hardware findings
  needed for default-width changes in section 3.1.
- **Intrinsics-to-ISPC porting agent.** Translate C++ intrinsics kernels
  to ISPC, compile, validate against the original on adversarial and
  seeded inputs, and benchmark across targets with timing protected
  against reward hacking. Start from the llama.cpp corpus. Publication
  and support are a separate commitment.
- **LLM-proposed, verified peepholes.** Collect hot mask/gather IR,
  propose rewrites, check with Alive2 where supported, implement reviewed
  transformations in `PeepholePass`. A timeout or unsupported proof is not
  verification.
- **MLGO inliner retraining.** Speed-oriented retraining on ISPC IR.
  Needs training/PGO infrastructure, expertise, compute and ongoing model
  ownership; the 1-3% range from other studies is not an ISPC estimate.
- **ISPC-Bench.** TSVC, PolyBench, ISPC examples and llama.cpp kernels
  with differential correctness and robust timing, as an extension of the
  section 2.1 corpus. Pairs with the porting agent, yields training data,
  and doubles as the KernelBench-style task set that shows AI coding
  assistants produce correct fast ISPC (section 6.6). Validate benchmark
  rights and provenance first.
- **AI-assisted maintenance.** Use AI for reproducer minimization,
  bisection, draft fixes and tests, with human review and independent
  validation; measure time saved and review cost rather than patches.
  Starting points: the `llvm-trunk-failure-analyzer` and yarpgen jobs.
  Extensions: the analyzer proposing and testing fixes as draft PRs;
  issue triage with minimization and bisection as reviewed comments;
  AI-drafted math functions, dots and new-type fallbacks against
  independent references; AI-drafted lit tests checked against intended
  behavior. Automation must justify its own operation and review cost.

## 6. Language, interop and adoption

Policy for this whole section: fix a language, header, ABI or build issue
early when it blocks a selected kernel or a named customer's supported
build; record the consumer and reproducer and count it in the work it
unblocks or in maintenance. Do not postpone a necessary ABI fix until
after a proof kernel that needs it. Larger features need a named
consumer and a bounded implementation and maintenance estimate.

### 6.1 C++ header sharing and ABI

Evidence. OSPRay uses `#ifdef ISPC` shared headers; the Unreal pain
involves struct layout; the HN critique is C++ interop. Issues:
return-by-value and pointer ABI ([#1590](https://github.com/ispc/ispc/issues/1590),
[#1855](https://github.com/ispc/ispc/issues/1855),
[#2344](https://github.com/ispc/ispc/issues/2344)), multi-target header
naming ([#1666](https://github.com/ispc/ispc/issues/1666)), optional
emission of unused structs ([#2277](https://github.com/ispc/ispc/issues/2277)),
short-vector types missing from generated headers
([#2016](https://github.com/ispc/ispc/issues/2016),
[#1679](https://github.com/ispc/ispc/issues/1679)).

Proposal. Document guaranteed layout-compatibility rules, including the
memory layout and boundary forms used by the BF16 example; fix the ABI
issues; fix header naming; emit unused structs on request; emit
short-vector types.

### 6.2 HLSL-style short vectors

What exists. Templated short vectors `T<N>` for basic types (documented in
`docs/ispc.rst` around lines 3388-3505): component-wise arithmetic,
comparison to `bool<N>`, per-component `?:`, scalar broadcast, brace
initialization, `v[i]` with varying index, member access `.x .y .z .w`,
`.r .g .b .a` and `.u .v`, and multi-component swizzle reads via
`VectorMemberExpr::GetValue` in `src/expr.cpp`. `short_vec.isph` provides
elementwise math and `select`.

What is missing, with evidence:

- Swizzle writes (`v.xy = ...`, `v.xyz += ...`), rejected in
  `VectorMemberExpr::GetLValue` ([#17](https://github.com/ispc/ispc/issues/17)).
- Constructors `float4(a, b.xy, c)` and splat `float4(1)`.
  [#1279](https://github.com/ispc/ispc/issues/1279) was closed because
  `T(args)` is ambiguous with C-style casts in the LALR grammar; discussion
  [#2914](https://github.com/ispc/ispc/discussions/2914) asks again.
- Predefined `float2/3/4`, `int2/3/4`, `uint*`, `bool*`, `half*`
  typedefs, which must be opt-in because user code defines them.
- Layout: `uniform float<3>` is padded to 16 bytes and `uniform float<100>`
  to 512 ([#3106](https://github.com/ispc/ispc/issues/3106)); `float3`
  alignment differs across ISAs ([#3447](https://github.com/ispc/ispc/discussions/3447)).
  C++ interop needs a 12-byte option.
- Geometric stdlib: no `dot`, `cross`, `length`, `normalize`, `lerp`,
  `saturate`, `step`, `smoothstep`, `reflect`, `refract`, `any`/`all` on
  `bool<N>`, or efficient pairwise reduce
  ([#1670](https://github.com/ispc/ispc/issues/1670)); users hand-write
  every uniform/varying combination of `Dot`
  ([#3356](https://github.com/ispc/ispc/discussions/3356)) and argmax over
  `float<4>` ([#2323](https://github.com/ispc/ispc/discussions/2323)).
- Matrix types `float3x3`, `float4x4` with `mul`, `transpose`, `inverse`
  ([#2252](https://github.com/ispc/ispc/issues/2252), 3 reactions).
- Short vectors inside `soa<>` ([#243](https://github.com/ispc/ispc/issues/243)),
  crash with short-vector references in structs
  ([#2766](https://github.com/ispc/ispc/issues/2766)), shuffle/rotate/shift
  on uniform short vectors (#3446).

Proposed sequence, each step independently admitted:

1. Geometric and graphics functions in `short_vec.isph`, templated over
   `T` and `N`, uniform and varying.
2. Swizzle writes: masked insert on store, reject duplicates and mixed
   naming sets, support compound assignment.
3. Opt-in `hlsl_types.isph` (or a flag) with the standard typedefs.
4. Constructors as a grammar production when the callee is a vector type
   name, with flattening and count checking; validate the grammar
   resolution rather than assuming it.
5. Unpadded layout option for uniform short vectors and header-generation
   support, with a migration note and ABI compatibility tests.
6. Matrix types as row-vector arrays with `mul`, `transpose`,
   `determinant`, `inverse` and header interop. Dot-product or tile
   builtins only where shapes and measured costs justify it; small
   graphics matrices do not automatically fit ACE.

### 6.3 Templates, constexpr and static_assert

Evidence. Open template bugs [#3016](https://github.com/ispc/ispc/issues/3016),
[#3025](https://github.com/ispc/ispc/issues/3025),
[#3040](https://github.com/ispc/ispc/issues/3040),
[#3232](https://github.com/ispc/ispc/issues/3232),
[#3240](https://github.com/ispc/ispc/issues/3240); default template
arguments and deduction in specializations are documented as planned.
`constexpr` has a design proposal ([#3697](https://github.com/ispc/ispc/issues/3697):
constexpr variables and functions, varying constexpr, no aggregates in
v1, not for `task`/`export`) and an earlier request
([#3341](https://github.com/ispc/ispc/issues/3341), closed).
`static_assert` has one open request from a production user
([#2146](https://github.com/ispc/ispc/issues/2146), 2021: compile-time
errors for invariants like `sizeof(data.array[0]) == 4`), where dbabokin
framed it as the missing third of `assume`, `assert`, `static_assert`.
Nothing in the stdlib uses `assert` today.

What exists. Function templates are used in `stdlib/stdlib.ispc` and
`short_vec.isph`. `src/expr.h` has `ConstExpr` and per-expression
`GetConstant` folding over `sizeof`, `programCount`, enum values, template
parameters and integer arithmetic, but no first-class constexpr.

Proposal:

- Fix the template bugs; add default arguments and specialization
  deduction; struct templates, which a generic `tile<T>` would use. The
  opaque fixed-shape ACE API does not depend on them.
- `constexpr` per #3697, for expressive tile shapes, block sizes and
  compile-time dispatch in stdlib code.
- `static_assert` over any expression the existing folder reduces to a
  constant, with an error (not a silent fallback to `assert`) when the
  condition is not constant. Conditions that call functions need
  constexpr functions, so `static_assert` is the natural first consumer
  and test bed of the #3697 evaluator. Honest value: a few days of work,
  one external requester, mainly a quality-of-life item for template
  kernels that assume a `programCount` or type size and for struct layout
  checks shared with C++ (section 6.1). Not a strategic item.

### 6.4 Small language quality items

`auto` ([#2310](https://github.com/ispc/ispc/issues/2310)), explicit
bitcast syntax ([#1809](https://github.com/ispc/ispc/issues/1809)), warning
on missing return ([#1708](https://github.com/ispc/ispc/issues/1708)),
consistent signed/unsigned conversions
([#2714](https://github.com/ispc/ispc/issues/2714)). Individually scoped;
admit for a specific customer.

### 6.5 Packaging and embedding

Evidence. PyPI ([#3741](https://github.com/ispc/ispc/issues/3741), with an
external volunteer who already produced a wheel under the PyPI size
limit), vcpkg ([#1305](https://github.com/ispc/ispc/issues/1305)), ISPC as
a library ([#791](https://github.com/ispc/ispc/issues/791), the top-ranked
issue), a single object for multiple targets
([#1850](https://github.com/ispc/ispc/issues/1850), discussion
[#3373](https://github.com/ispc/ispc/discussions/3373)), stdlib as linkable
bitcode ([#2194](https://github.com/ispc/ispc/discussions/2194)), MinGW
linking ([#3313](https://github.com/ispc/ispc/discussions/3313),
[#2356](https://github.com/ispc/ispc/discussions/2356)), Python bindings
([#2898](https://github.com/ispc/ispc/discussions/2898)).

What exists. `libispc` with a JIT in `src/ispc_impl.cpp`, documented under
"Using ISPC as a Library"; conda-forge packages on five platforms; a slim
binary build (`-DISPC_SLIM_BINARY=ON`, about 40 MB zipped).

Proposal. PyPI wheels and an official vcpkg port, with release
automation, platform coverage and ongoing distribution maintenance in the
estimate; stabilization, versioning and documentation of `libispc`, with
concrete missing behavior identified by an embedding consumer rather than
reimplementation; a single multi-target object.

### 6.6 Python and AI-ecosystem adoption

What exists. `--nanobind-wrapper=<file>` emits a nanobind wrapper, shown
working by `examples/cpu/point_transform_nanobind` and
`examples/cpu/attention`. The user still needs an ISPC install, CMake, a
C++ compiler and nanobind headers, and only numpy arrays map to pointers.
That is a high wall for the Python-first audience that dominates AI work.

Proposal, in order of cost:

1. **`pip install ispc`.** Per-platform wheels with the slim binary,
   headers and an `ispc.exe_path()` helper, plus a `scikit-build-core`
   example so `pip install .` builds a nanobind project. One CI job.
2. **Framework arrays through DLPack.** Emit
   `nb::ndarray<T, nb::device::cpu>` without the numpy tag so generated
   functions accept PyTorch, JAX, CuPy and Arrow arrays zero-copy. Small
   wrapper-emitter change. Map `float16` and `bfloat16` dtypes when the
   BF16 type lands.
3. **PyTorch custom-op registration.** Optionally emit a `torch.library`
   registration next to the nanobind module so a kernel has a schema,
   joins `torch.compile` graphs on CPU and can carry a Python fallback.
   This positions ISPC as the CPU custom-kernel language for PyTorch, a
   role Triton fills on GPU and nobody fills well on CPU. The attention
   example is the demo. An ONNX Runtime custom-op example is the
   alternative; select at most one as the BF16 consumer and budget API
   version compatibility, layout/conversion and threading tests.
4. **Build-free JIT module.** A Python package that compiles a source
   string through the `libispc` JIT and exposes exports via the buffer
   protocol, a numba-like experience with no CMake step. Largest effort
   and largest lever for notebooks and research code; needs a prototype
   gate and a named consumer.

Beyond Python:

- **Land in one framework.** An upstream ggml CPU path (section 4.4) or
  an ONNX Runtime custom op gives a reference people copy. ExecuTorch and
  vLLM CPU kernels are the other places where C++ SIMD kernels are
  hand-written.
- **Publish the comparison users make** (section 2.1): ISPC against
  numba, Triton-CPU and intrinsics on AI kernels.
- **Be friendly to AI coding assistants.** A growing share of kernel code
  is assistant-generated, and ISPC suits them: a small C-like language, no
  intrinsics to hallucinate, fast compile, scalar semantics. Cheap steps:
  an `llms.txt` and condensed language reference in the repo, an ISPC
  skill or MCP server that compiles and benchmarks a kernel, and the
  ISPC-Bench task set (section 5.3).
- **Ship the dtypes AI code uses** (sections 4.1 and 4.2).

### 6.7 Inlinable kernels: C++ output

Evidence. Discussions [#2722](https://github.com/ispc/ispc/discussions/2722)
and [#2644](https://github.com/ispc/ispc/discussions/2644) ask to bring
back C++ output. Users want kernels that inline into C++ call sites for
three reasons: small kernels in hot loops pay call overhead and lose
constant propagation from the caller; engines want kernels compiled by
their own toolchain on build farms without ISPC, with generated code
checked in; debugging and profiling are easier in the user's translation
unit.

History. The 2012-2020 `--emit-c++` backend compiled "generic" targets
through a fork of LLVM's C backend (`cbackend.cpp`) into calls like
`__add_float(__vec16_f32, __vec16_f32)`, implemented by hand-written
per-ISA headers (`examples/intrinsics/sse4.h`, 3,645 lines;
`generic-16.h`, 1,787 lines; KNC and KNL variants). It was removed in
commit 785028145 (July 2020) because the forked backend broke with every
LLVM upgrade, each ISA needed its own header, and code quality depended on
the host compiler seeing through thousands of wrapper calls.

What works today. `ispc --emit-llvm` bitcode linked with C++ under `-flto`
through lld inlines the kernel body into the caller; verified 2026-10-16
with ISPC 1.33.0dev, clang 21 and lld. Limits: the multi-target
dispatcher prevents inlining by construction, so only single-target
builds benefit; the missing x86 `target-features` attribute (section
2.3) lets an AVX2 body legalize to SSE silently; the linker's LTO plugin
must be from an LLVM at least as new as ISPC's; MSVC `link.exe` cannot
consume bitcode. LTO should be documented as the zero-effort option.

Proposal: `--emit-cpp` for the existing `generic-*` targets, printing the
optimized LLVM IR as C++ that uses clang/GCC vector extensions, so the
host compiler does ISA selection, legalization and inlining with no
per-ISA header. The IR for generic targets after ISPC's pipeline uses a
small subset (vector arithmetic and compares, `select`, `shufflevector`,
conversions, `llvm.fma/sqrt/fabs/minnum/maxnum`, `llvm.vector.reduce.*`,
masked load/store/gather/scatter, `ctpop/ctlz/cttz`, scalar control
flow), each with a vector-extension or small-runtime-header equivalent
(`__builtin_shufflevector`, `__builtin_convertvector`,
`__builtin_elementwise_*`, `__builtin_reduce_*`, `__builtin_masked_*` on
clang 21+). Output: the existing header plus an `.inl` of `inline`
functions in `namespace ispc` including an installed
`ispc/cpp_runtime.h`. Multi-target through `target_clones` or user-managed
per-ISA compilation. Full quality on clang 21+, good on clang 15-20 and
GCC 12+, scalar fallback on MSVC. `launch`/`sync`, non-generic targets, Xe
and readable output are out of scope. Implementation: a few thousand
lines of printer (`src/emit_cpp.cpp`) over the stable IR API, a runtime
header of a few hundred lines, driver support, and a differential test
leg in `scripts/run_tests.py` compiling every functional test through
the printer. Prior art: Halide's C++ backend (closest), Slang's C++
prelude, MLIR EmitC, llvm-cbe, Futhark emitting ISPC. The same printer
would be the only realistic port path to AI accelerators (Gaudi TPC-C,
Hexagon HVX, AMD XDNA, Intel NPU), but those ports are language projects
(address spaces, local storage, async copies, tiles) with the emitter as
delivery vehicle. Full design: [EmitCpp.md](EmitCpp.md).

Assessment. The most explicitly requested feature in discussions, but it
benefits small kernels on clang hosts and does nothing for the majority
of users who call out-of-line exported functions through the dispatcher.

### 6.8 Automatic differentiation

Discussion [#3371](https://github.com/ispc/ispc/discussions/3371) asks for
Enzyme-based automatic differentiation of ISPC code. Survey request only;
verify current Enzyme support for ISPC's IR and a consumer before
estimating.

## 7. Maintenance

- **Releases and LLVM.** Maintain supported versions, track trunk
  failures, reserve time for correctness regressions. Reduce recurring
  work through measured improvements to existing infrastructure and the
  AI-assisted maintenance in section 5.3. The pipeline re-sync and LLVM
  intrinsic adoption in sections 2.2 and 2.3 are the structural ways to
  make LLVM upgrades cheaper.
- **ISPCRT.** Keep security, build and current-consumer support. Review
  the in-flight load-from-memory work ([#3900](https://github.com/ispc/ispc/pull/3900))
  with its consumer. Proposal: freeze new functionality and assess
  deprecation at year end, after confirming remaining consumers and
  migration paths; OSPRay and Open VKL dropping it does not prove no users
  remain.
- **Xe GPU.** Keep the opt-in build and CI functional; no new features.
- **Duplicate pseudo builtins.** Retire them case by case when LLVM
  intrinsic equivalence and lowering quality are demonstrated (section
  2.2). Not a dependency of the first gather fixes.
- **Old targets.** Audit i686 ([#1865](https://github.com/ispc/ispc/issues/1865))
  and the lowest SSE tiers for removal from release binaries. Measure
  stdlib build-time and matrix savings, assess consumers and define a
  deprecation policy first.
- **ARM and WASM** in maintenance mode as described in section 3.3.

## 8. Recommended annual commitments

### 8.1 Selection criteria

A topic becomes a commitment when it has a consuming workload or
customer, correctness and performance evidence, known hardware and
toolchain dependencies, an owner, an acceptance criterion and capacity.
With one engineer at 70-75% after the maintenance reserve and one intern
for part of the year, the plan can hold one major compiler
implementation at a time. Everything not listed below stays a candidate;
promoting it requires naming the capacity added or the commitment it
replaces. Changes in scope must update this section and the sequence
together.

### 8.2 The five commitments

| Priority | Commitment | Serves |
|---|---|---|
| 1a. Performance | Gather-to-load/shuffle and native shuffle lowering (section 2.2, steps 1-2) | Image/layout workloads (2.2), quantized inference (4.4), narrow-integer gaps (2.4), the cost-model experiment (5.3) |
| 1b. Performance and new CPUs | Diagnose and fix confirmed DMR/NVL x16 regressions; publish width guidance (3.1) | New-CPU credibility, NVL default decision, game-engine target sets, width predictor data (5.3) |
| 2. Intel hardware | x16 ACE prototype for INT8/BF16 with a reference kernel (3.2) | ACE programming model, BF16 consumer (4.1), ggml GEMM path (4.4), future tile API |
| 3a. AI language | `bfloat16` storage, conversions, FP32-accumulating pair dots and a working dot kernel (4.1) | ACE BF16 (3.2), AVX10.2 BF16 exposure (3.1), DLPack dtype mapping (6.6), the only AI-motivated production requests |
| 3b. AI compiler research | One intern experiment on a learned gather-lowering decision, ending in an adopt/reject report (5.3) | Per-target cost table (2.2), later masking decisions (2.3), research credibility |

Why these five. Performance comes first because it is the product's
reason to exist and the gather cluster is the largest and oldest issue
group. The x16 regressions are the one item where new Intel hardware and
performance coincide and where an LLVM fix has a long lead time. ACE is
the hardware priority and its design can proceed before toolchain and
hardware exist. BF16 is the smallest language feature with a named
hardware connection and real requests. The research experiment attaches
to live performance work so even a negative result produces a useful
cost table.

Deliverables per commitment:

- **1a.** Two shipped transformations with codegen tests and before/after
  measurements on the remap and `ggml_vec_dot_q4_0_q8_0` kernels. The
  full redesign and per-target cost table (2.2 steps 3-5) stay deferred.
- **1b.** Minimized reproducers, upstream issues or narrow workarounds
  for confirmed regressions, x8/x16 tracking, and documented width
  recommendations from hardware results. Change NVL's default only on
  evidence.
- **2.** A reviewed `ace.isph` specification with execution, mask, tail
  and lifetime rules; reference tests; an experimental codegen
  implementation on a recorded LLVM baseline; a small GEMM with fused
  epilogue against an independent reference. Native execution is claimed
  only if hardware or an emulator runs it. MX, x32, `tile<T>` and the
  competitive fallback stay deferred.
- **3a.** Steps 1-3 of section 4.1 with the numerical contract, memory
  interop documentation and a benchmarked BF16 dot kernel. Native BF16
  arithmetic and FP8 stay deferred.
- **3b.** Four to six intern weeks after the harness exists; corpus
  split, baselines, model or table, reproducible report. If the decision
  or data is unavailable at the checkpoint, substitute a constrained
  pass-pipeline study, recorded explicitly, not a second project.

### 8.3 Shared infrastructure that pays for itself several times

- **The benchmark corpus** (2.1): remap, Q4_0, BF16 dot and x16
  reproducers. It validates 1a, 1b and 3a, feeds 3b, seeds the llama.cpp
  stage A harness (4.4), and is the start of the public comparison that
  adoption needs (6.6). The intern owns harness and data work when
  available.
- **x86 `target-features` attributes** (2.3). A small independent fix
  that improves LLVM's inlining and cost decisions for 1a and 1b, makes
  LTO-based inlining correct today, and is a prerequisite for any C++
  emitter (6.7).
- **Documented layout and boundary rules** (6.1). Required by the BF16
  example (3a), by the ACE kernel's struct interop (2), and asked for by
  every C++ interop complaint.

### 8.4 Cheap additions that fit alongside the commitments

Each is days to a few weeks and needs no new capacity decision:

- `pip install ispc` wheels and DLPack-capable nanobind output (6.6 items
  1-2). They remove the install wall for the Python audience and give
  the BF16 work a dtype to land in.
- PyTorch custom-op registration emitted next to the nanobind module
  (6.6 item 3), using the attention example as the demo and as the one
  framework consumer for BF16.
- Assistant-friendliness: `llms.txt`, condensed reference, a compile-and-
  benchmark skill (6.6).
- `static_assert` over constant-folded expressions (6.3), as the first
  consumer of the constexpr evaluator.
- The macOS universal-binary CMake recipe for Embree and Unreal (3.3).

### 8.5 Sequence and checkpoints

| Period | Work | Checkpoint |
|---|---|---|
| Q4 2026 | Establish the remap, Q4_0, BF16-dot and x16 baselines. Diagnose x16 losses; start the gather-to-load/shuffle change. Write the ACE execution and lifetime contract; contact upstream and validation owners. Set `target-features`. | Reproducible x16 findings; a tested gather prototype; an identified ACE LLVM/OS/validation path. |
| Q1 2027 | Ship gather/shuffle and confirmed width fixes; complete the native shuffle patterns for Q4_0. Then BF16 storage, conversions, pair dots and the proof kernel. | Release-ready performance changes and BF16 support with measurements, numerical tests and interop documentation. |
| Q2 2027 | Implement x16 ACE INT8/BF16 operations with reference and codegen tests. Run the learned gather-lowering experiment once harness and intern align. | Experimental ACE implementation on the recorded LLVM baseline; adopt/reject report. Missing native execution reported explicitly. |
| Q3 2027 | Complete available ACE execution validation; harden performance and BF16 changes; run consuming builds; publish results and width recommendations. | Achieved ACE validation level stated. Next-priority work considered only after accounting for unfinished commitments and maintenance. |

Capacity split of the non-maintenance time: about 50% performance, 30%
ACE and hardware, 20% BF16. These are planning envelopes.

### 8.6 Decision gates and fallbacks

- If ACE's LLVM dependency is unavailable, retain the design and
  reference tests, report the implementation as blocked, time-box
  prototype maintenance, and redirect time to the performance follow-ups
  below.
- If investigation disproves an assumed x16 defect or finds an optimal
  lowering, record the evidence and substitute the next priority.
- A research result that rejects the learned model still completes the
  experiment.
- For each implementation record reproducer or workload, owner, effort
  estimate, dependencies and correctness/performance checks before
  starting; derive regression thresholds from baseline noise before
  judging.

### 8.7 Next choices if capacity opens

In order, each inheriting the measurement rules of section 2.1:

1. Loop and mask codegen: predicated `foreach` tails and the pipeline
   re-sync (2.3). Fixes a family of issues and stops LLVM upgrades from
   eroding performance, which also lowers maintenance cost (7).
2. The generated, accuracy-specified math library (2.5). Target
   independent by construction, so the most portable item on the list,
   and it answers the two top-ranked math issues.
3. `avx2-i64x8` (2.4), the top-voted codegen issue.
4. The per-target gather/scatter cost table (2.2 steps 3-5), seeded by the
   3b experiment.

A named customer blocker can take precedence over any of these. The
broader programs (full HLSL vectors, templates and constexpr, FP8 and MX,
x32 and `tile<T>`, llama.cpp stage B, further AI research, the C++
emitter, SVE, ISPCRT deprecation, old-target removal) remain described in
sections 2 to 7 and are admitted only with a consumer, an owner and a
capacity decision.
