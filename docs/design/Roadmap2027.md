# ISPC Roadmap: Q4 2026 - Q3 2027

Status: proposal, revision 3 (2026-10-08). Team: one engineer, one intern,
AI-assisted development.

Priorities remain: **1. performance; 2. enabling new Intel hardware,
especially ACE; 3. AI through the language/programming model and bounded
compiler research.** The annual plan commits to measured performance
improvements, a restricted ACE implementation with explicit validation
levels, and useful `bfloat16` support. It includes one time-boxed AI
optimization experiment, whose outcome may be a report rather than a
shipped optimization.

Section 12 retains the other proposed features as deferred or stretch work.
They remain candidates; moving them into the annual plan requires a named
consumer, an owner, an acceptance criterion, and either available capacity
or an explicit tradeoff with active work. Completed features are identified
as such rather than scheduled for reimplementation.

Xe GPU remains opt-in with no new feature commitments. ARM and WASM stay in
maintenance mode, including correctness of supported portable fallbacks,
with the macOS integration exception in section 5.4. The APX and AMX
dispatcher capability work shipped (PR #3922 and commit 5101e97e3); this plan
builds on it.

Contents:

1. Positioning and evidence
2. Annual commitments and capacity
3. Performance: measure, select, fix
4. New Intel hardware: Diamond Rapids and Nova Lake
5. New Intel hardware: ACE
6. AI in the language: BF16 and a usable kernel
7. AI workload exploration: llama.cpp
8. AI inside the compiler: one bounded experiment
9. Customer and interop blockers
10. Maintenance scope
11. Sequencing and decision gates
12. Deferred and stretch proposals
13. Sources and verification status

## 1. Positioning and evidence

### 1.1 What ISPC is for

ISPC is an established production SPMD-on-SIMD language for CPUs: automatic
masking, gang semantics, C ABI interop, and multi-ISA dispatch in one tool.
In 2026 Hacker News threads it is still the reference point for the idea
("the only two good choices are something like ISPC"; new projects are asked
"is it faster than ISPC?"). The tool itself gets little discussion: the last
three ISPC stories on HN drew zero comments. Some of the sampled comments suggest
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
version (1.28.2 and 1.30.0). This suggests downstream releases influence
downloads; download counts alone do not establish the balance between
transitive users and direct adopters.

### 1.3 What users ask for

Demand on the issue tracker is modest (no open issue has more than six
reactions). Ranked by a score of 3 x reactions + 2 x distinct external
commenters + comments, the sampled topics rank as follows. This is a
discovery aid, not a priority score: age, discussion volume, current
implementation status and customer impact must be assessed separately.

| Rank | Issue | Ask |
|---|---|---|
| 1 | #791 | ISPC as a library: library/JIT already implemented; identify remaining adoption gaps |
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
items behind the HLSL-style request retained in section 12.6.1.

GitHub Discussions (64 threads, read 2026-10-08) are mostly Q&A, but the
recurring topics line up with this plan:

- **Gathers and memory layout** are the most common performance question:
  interleaved RGBA (#2943), H,W,C image layouts and a bilinear remap where
  JAX ran 2x faster (#2919, #2933), a 4K bitmap rotate (#3569), a 3D grid
  port (#2744), scatter warnings with `soa` (#2308), and a request to
  silence intentional gather warnings selectively (#3184). Supports 3.2 and 12.1.1.
- **Math accuracy and coverage**: a user expecting C-library ULP bounds
  (#3212), `log1p`/`expm1` (#3476), `rcp_fast` for double slower than
  division on Xbox Series X (#3257). Supports 3.3 and 12.1.4.
- **Short vectors and HLSL-style code**: `vec2(0, 0)` constructors
  (#2914), hand-writing every uniform/varying combination of a `Dot`
  function (#3356), `float3` alignment differing across ISAs (#3447),
  argmax over `float<4>` (#2323). Supports 9 and 12.6.
- **64-bit lane width**: a double-precision workload that wants i64x4
  register pressure with AVX-512's 32 registers (#2170), prologue spills of
  varying structs (#2173). Supports 3.2 and 12.1.3.
- **Missing SIMD primitives**: movemask (#2632), prefix sum (#2436), sorting
  many small arrays (#3533). Supports the shuffle, reduction and segmented
  reduction items in 12.1.3 and 12.3.3.
- **AI types**: "Will ispc support bfloat16?" (#3627). Supports 6.1.
- **Targets**: which target to use for Zen 5 AVX-512 (#3827), a sign that
  target naming and recommended defaults need documenting (section 4).
- **Build and distribution**: one object file for multiple targets (#3373),
  macOS universal binaries (#2854, #2046), Python bindings (#2898), stdlib
  as linkable bitcode (#2194), MinGW linking (#3313, #2356). Supports 5.4
  and 12.6.5.

Two further discussion topics are retained as future candidates in 12.7:
bringing back C++-with-intrinsics output (#2722, #2644), and automatic
differentiation through Enzyme (#3371). Nothing in discussions mentions AMX,
ACE or AVX10.2.

Of about 480 commits in the last year, roughly 35% were CI, 21% LLVM upgrade
and trunk fixes, 8% new targets, 7% stdlib math, 2.5% optimizations and
under 1% language features. Cutting maintenance cost is itself a roadmap
item (sections 8.4 and 10). Commit percentages are not engineering-time
measurements; use time spent on failures and releases to estimate savings.

### 1.4 Competitive pressure

- **Google Highway** is an established option for portable SIMD in C++. It
  lists 27 targets including AVX10.2, SVE, SVE2, RVV and WASM, and adopters
  such as Chromium, Firefox, NumPy, TensorFlow and gemma.cpp. Its bf16 and
  fp16 types are load, store and convert only; compute goes through widening
  multiply-accumulate ops. Its strengths include build integration and target breadth;
  it offers a different explicit-SIMD programming model.
- **C++26 std::simd** reduces the installation and integration advantage
  of a separate compiler for suitable kernels. Implementation coverage
  varies by toolchain; reassess it during the year rather than assuming
  competition starts in a particular future year.
- **Mojo** has first-class `bfloat16` and six fp8 dtypes including the MX
  scale format. **Slang** and **MLIR** win on generics, modules and AI
  relevance.
- **LLVM auto-vectorization** keeps improving for simple loops, eroding the
  pitch for easy kernels.

The usage and landscape survey above is context. Commitments below are
selected by a consuming workload, correctness and performance evidence,
hardware requirements, and maintenance cost. See section 13 for the
distinction between source checks and research notes carried forward.

## 2. Annual commitments and capacity

| Priority | Annual outcome | Owner and dependencies | Acceptance criterion |
|---|---|---|---|
| 1 | Representative performance corpus and two or three measured codegen improvements | Engineer owns compiler changes; intern owns harness/data. Requires reproducible workloads and machines. | Correctness checks pass; improvements reproduce against a pinned baseline; report per-workload regressions, compile time and code size. |
| 2 | DMR/NVL regression fixes and restricted ACE enablement | Engineer; upstream LLVM, CPU/OS capability contract, hardware or emulator access. | Width recommendations follow measurements. ACE has a reviewed semantic contract, reference tests and native codegen checks; execution validation is a separate gate. |
| 3a | BF16 storage/conversions, FP32 compute and accumulation, and one useful kernel | Engineer owns semantics, compiler and ABI; intern assists workload integration. | Documented numerical contract, conversion/edge-case coverage, supported memory interop, and a kernel compared with an independent reference. |
| 3b | One bounded AI optimization experiment | Intern, with engineer review; requires the performance corpus and legal transformation alternatives. | Reproducible comparison with existing and simple tuned heuristics on held-out workloads; ship only if the evidence justifies it. A negative result is acceptable. |
| Supporting work | Fix specific customer/interop blockers and maintain supported releases | Engineer; named consumer or demonstrated failure. | Consuming build or kernel works, with a regression check and bounded scope. |

These are shared workstreams: the BF16 kernel contributes to the performance
corpus; the ACE surface is one project, not a second generic tile project;
the AI experiment evaluates a decision arising from the performance work.

Reserve roughly 25-30% of engineer capacity for releases, LLVM compatibility,
correctness regressions, customer support and review, including intern
supervision. Of the remaining capacity, allocate approximately 50% to
performance, 30% to ACE/hardware enablement, and 20% to BF16 and its proof
kernel. These are budget envelopes, not task-duration estimates. Estimate
specific changes after initial investigation; reduce scope when they do not
fit rather than consuming the maintenance reserve.

Limit implementation work in progress to one major engineer-owned compiler
change and one intern-owned measurement/research task. Early ACE dependency
tracking and semantic notes can proceed alongside performance work, but do
not imply simultaneous full implementations. The intern's dates and duration
are not specified: schedule the experiment relative to actual availability
and do not make a core release dependent on year-round intern capacity.
AI assistance creates contingency; it is not counted as another reviewer
or an additional engineering FTE.

## 3. Performance: measure, select, fix

### 3.1 Establish baselines first

Start with a small reproducible corpus: one existing customer kernel, one
memory/control-flow case, and the selected inference kernels from section 7.
Use `benchmarks/03_complex` and the existing performance tracking job.
Record compiler/LLVM revisions, target and gang width, machine/OS details,
input sizes, numerical options and thread counts. Use stable machine
configuration and repeated measurements to characterize noise.

Compare with the previous ISPC baseline, suitable hand-written intrinsics,
Highway and Clang implementations early enough to select work. Match
algorithms, data layout, precision and threading in comparisons. Add x8/x16,
BF16 and packed INT8 microbenchmarks as relevant. Publish reproducible
results using existing reporting; a full public dashboard and broad per-PR
gating remain in section 12.1.5.

### 3.2 Select a small set of fixes

Choose two or three codegen problems by measured workload impact and cost.
Candidates include gather-to-load/shuffle transformations, mask or loop
overhead, narrow-integer/shuffle lowering, and avoidable spills. The full
issue inventory remains in section 12.1. Move a lane-width fix early if it
blocks the chosen customer or inference kernel; issue age and votes alone
do not set the order.

For the reported DMR/NVL x16 regression, first capture the affected LLVM
revision, minimized reproducer, issue link and measurements (section 4).
An upstream fix or a narrow ISPC workaround can be one of the selected
performance changes.

For memory transformations, establish legality before choosing a profitable
lowering: preserve inactive-lane fault suppression, object bounds, aliasing,
and the specified behavior of conflicting scatter addresses. A wider load
must not introduce an invalid access. Prototype bounded transformations
before committing to the broader gather/scatter redesign in section 12.1.1.
Replacing pseudo builtins with LLVM intrinsics is an independently measured
implementation choice, not a prerequisite.

### 3.3 Acceptance and numerical behavior

For each selected change, record its consuming workload, expected benefit,
cost estimate, supported targets, regression checks and stop condition.
Use functional and focused codegen tests plus repeatable timing. Agree on
regression thresholds from baseline noise before evaluating the change;
report distributions and individual losses as well as aggregate gains.
Retain the existing lowering if benefits do not justify regressions or
maintenance cost.

Include math accuracy checks for functions used by the selected workloads,
such as `exp` in softmax if that kernel is selected. Distinguish measured
maximum error over a stated domain from a proven ULP bound. A full math
audit, additional transcendental implementations and full FP16 coverage
remain in section 12.1.4; full FP16 math is not a prerequisite for BF16.

## 4. New Intel hardware: Diamond Rapids and Nova Lake

LLVM's `X86.td` definitions inspected during the proposal review:

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

ISPC already has `avx10.2dmr-*` and `avx10.2nvl-*`, APX enabled by
default and `--opt=disable-apx`. **DMR already defaults to x16; NVL defaults
to x8** (`src/ispc.cpp`, target selection and default target strings).

- **Fix demonstrated width regressions.** Reproduce the reported x16/x8
  gap and establish whether it is an LLVM regression, an ISPC lowering
  problem or a legitimate workload tradeoff. Track minimized reproducers,
  upstream issues, affected revisions and x8/x16 performance. Fix confirmed
  problems upstream or use a bounded workaround. Do not promise x16 wins
  every workload: divergence, tails, memory behavior and register pressure
  can favor x8.
- **Recommend widths from evidence.** Document the measured choice per
  workload/CPU family. Consider changing NVL's default and the recommended
  game-engine multi-target set only after representative hardware results.
  ACE's use of ZMM operands does not require unrelated kernels to use x16.
- **Audit relevant existing codegen.** The signed-signed/unsigned-unsigned
  INT8 and unsigned/mixed INT16 packed dot families already have stdlib
  implementations, fallbacks, DMR/NVL lowering and tests. Check quality for
  the selected kernels rather than reimplementing them. Address APX spills
  or mask-arithmetic regressions when measurements identify them.
- **LLVM compatibility.** Confirm any deprecated AVX10 feature spelling is
  actually present in supported code/build paths before scheduling a
  migration. Treat necessary changes as maintenance, with the affected
  LLVM revision and path recorded.

The broader APX audit and AVX10.2 instruction exposure proposals are retained
in section 12.2. BF16 lowering belongs to section 6.

Use AVX2 and AVX-512 hardware for current performance baselines, including
Granite Rapids for its supported features. Granite Rapids is not an
AVX10.2 test machine. DMR/NVL performance conclusions require appropriate
hardware access; SDE/emulation can establish functionality, not timing.
Product dates and ACE support must not be inferred from CPU names in LLVM.

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
  "palette 2" under the AMX framework; AMX TMUL is palette 1. Tile state
  has one active palette per executing thread/context. Switching palettes
  requires a defined state transition; live accumulators cannot simply
  survive an AMX/ACE switch.
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
| INT8 work at full tile size | 16x16x64 MACs | 16x16x4 MACs |
| Data types | int8, bf16, fp16, fp8, complex | int8 (4 signedness combos), bf16, OCP fp8, MX fp8, MX int8 |
| Scaling | None | Per row and column via BSR |
| Spill | `TILESTORED` | 16 row moves plus 16 stores |
| Fusion with vector code | Baseline path through memory; AMX-AVX512 adds transfers | Direct ZMM/tile transfers |

ACE's vector inputs make fused pre- and post-processing attractive:
a packed `varying int32` on an x16 gang matches a 512-bit input operand.
The accumulator remains tile state, and cross-lane outer products need
collective semantics beyond ordinary per-lane operations. The data-movement
opportunity does not by itself establish a performance win.

**Toolchain status (October 2026).** LLVM PR 206888 (`avx10v2aux`) is
merged. PR 208408 (`acev1`, header `acev1intrin.h`, type `__acetile`,
intrinsics `__tile_ace_*`) and PR 208706 (`x86_bsr` IR type) are open, with
changes requested on the latter. Intrinsic names differ between the spec and
the PR, and the spec itself has internal inconsistencies on BSR byte layout
and immediate bit positions. GCC, binutils and emulator support are
unverified. LLVM 23/24 were candidate integration versions in the original
proposal; inclusion is an external dependency, not a committed date.

### 5.2 Annual scope: a restricted, typed ACE surface

**Design milestone first.** Specify an initial x16 leaf-kernel interface in
`ace.isph`: opaque `uniform tile_i32` and `tile_f32`, tile initialization,
INT8 outer products with the four signedness combinations, BF16 outer
products with FP32 accumulation, and row/column transfer operations needed
by the proof kernel. Opaque built-in types do not require struct templates
or general `constexpr` support. Settle signatures and lifetime rules before
promising source compatibility.

The first proof workload is naturally BF16 or INT8, for example a small
GEMM with a fused epilogue and a scalar reference. It shares infrastructure
with sections 3 and 7. MX operations, x32 mapping, a generic `tile<T>` and
broader portable lowering remain in section 12.3.4.

Resolve these requirements in the design:

- **Execution and masks.** Define ACE operations as gang collectives with
  uniform control flow for the initial surface. Define or reject invocation
  under partial masks, varying branches and early exits. Specify independent
  row/column validity and K-tail handling for padded edge tiles; an ordinary
  execution mask alone is insufficient. Include negative tests and edge
  cases, including non-finite BF16 values where zero padding may affect
  floating-point behavior.
- **Mapping.** Start with x16 packed 32-bit operands, one ZMM each, and a
  documented mapping from lanes to rows/columns. Restrict unsupported widths
  explicitly. On x32, `varying int32` is 1,024 bits: subgroup mapping and row
  readback require a separate design, not only reinterpret helpers.
- **State ownership.** Define tile/BSR lifetime, initialization and release,
  caller/callee responsibilities and AMX palette transitions. Initially
  restrict live tiles across non-inlined calls. Coordinate palette-2
  `LDTILECFG` and `TILERELEASE` with LLVM's lowering, including all exits.
  Tile references used by builtins must not imply general address-taking or
  an ordinary C ABI.
- **Constants.** Where an instruction encodes an immediate, require and
  validate a compile-time constant. `uniform` by itself is not sufficient.
- **Register pressure.** Measure tile/ZMM pressure and report expensive
  spills. LLVM's proposed backend supports tile spill/reload; a strict
  no-spill mode is a retained design option, not an initial semantic promise.
  The reference fallback is for correctness; competitive AVX2 tile
  emulation is a separate performance project.
- **Capability and OS contract.** Represent ACE as an explicit capability,
  independently of the DMR/NVL names. Verify the proposed detection recipe
  against the final ISA and OS interface: CPUID.(7,1):ECX[11], ACE_VSN >= 1
  through leaf 1Dh sub-leaf 2, applicable AVX10_V2_AUX support through leaf
  24h, and required XSTATE components (the proposal identifies bits 17, 18
  and 20). Determine requirements per supported operation, reuse relevant
  AMX checks, and define permission acquisition and failure/fallback
  behavior for calling threads. CPUID/XCR0 checks alone are not a substitute
  for the OS permission contract.

### 5.3 Validation gates and relationship to AMX

Engineer owns upstream coordination and acquisition of hardware or emulator
access; record the owner, expected availability and LLVM baseline during Q4.
Track LLVM PRs 208408 and 208706, their dependencies and unresolved spec/API
questions. The initial INT8/BF16 subset should not require MX API work unless
the chosen LLVM backend imposes that dependency.

| Gate | Evidence | What can be claimed |
|---|---|---|
| Semantic/reference | Reviewed execution/lifetime contract; independent reference checks for mapping, tails and numerical behavior | The proposed programming model has executable tests. |
| Native codegen | LLVM IR and assembly checks for supported operations, configuration, release and diagnostics | Experimental compiler support emits the intended code. |
| Native execution | Hardware or emulator runs against the reference, including OS capability failure paths | Functional support for the tested ISA/toolchain/OS configuration. |
| Performance | Repeatable measurements on representative hardware, including packing and state-management costs | Performance results for that hardware/workload. |

A lit-only implementation remains experimental. If native execution is
unavailable, publish the prototype's limits and dependency status; do not
report validated ACE support. If LLVM integration slips, complete the design
and reference work, time-box prototype maintenance, and return remaining
capacity to performance work.

The original proposal assumes AMX will be deprecated in favor of ACE, but
does not provide an attributable product commitment or timeline. Record
such guidance explicitly if available; do not derive it from LLVM support.
Keep `amx.isph` working for current consumers, including OIDN, and allow
small measured customer fixes or shared improvements such as row readback.
Broader AMX feature exposure is retained in section 12.2.

OIDN's existing AMX convolution uses FP16 inputs and FP32 accumulation.
The ACE v1 multiply surface described here supports BF16 but no FP16
multiply. FP16-to-BF16 conversion loses precision; an OIDN-style ACE port
requires a separate quality decision and evaluation. It is not the initial
proof of transparent portability.

### 5.4 ARM integration exception

ARM stays in maintenance mode. For the reported Embree/UE macOS build
failures, reproduce the consuming build and document/test a supported CMake
recipe for x86_64 plus arm64 universal outputs. Bound this as a customer
integration fix; additional target development remains deferred.

## 6. AI in the language: BF16 and a usable kernel

Evidence includes the bfloat16 request (#2361 and Discussion #3627),
BF16-oriented inference paths, and the ACE BF16 operation. OIDN is an
existing AI customer, but its current FP16 kernel is not evidence that
conversion to BF16 is numerically acceptable.

### 6.1 `bfloat16` semantics and first milestone

The engineer owns the language/numerical contract. Add uniform and varying
BF16 storage, loads/stores, casts/conversion helpers, and the necessary
lexer, type-system, constant-folding, diagnostics and debug-info support.
Review aggregates, overload resolution, mixed-type expressions and memory
layout. Portable lowering must preserve the specified behavior on supported
targets, including maintained ARM/WASM configurations where applicable.

Specify rounding, intermediate precision, contraction/FMA, subnormals,
NaNs, signed zero, overflow and interaction with fast-math options. Ordinary
expression and explicit native-intrinsic behavior must be distinguishable.
Widening to FP32, operating and narrowing does not automatically guarantee
equivalence to every native BF16 instruction; validate each lowering
against the contract.

Sequence implementation:

1. Storage, conversion and memory interop, with an independent conversion
   oracle and numerical edge cases. Exhaustively test BF16 input encodings
   where feasible and test FP32-to-BF16 rounding boundaries.
2. A useful kernel using BF16 data with FP32 compute/accumulation; measure
   conversion cost and compare against a suitable existing implementation.
3. BF16 pair-dot support (`VDPBF16PS` where available), with documented
   accumulation semantics and a correct fallback, to serve the chosen
   kernel and hardware direction.

Define supported C/C++ boundary forms explicitly. A `uint16_t` representation
with conversion helpers can describe stored bits; it does not establish a
compatible by-value BF16 calling convention. Validate generated headers,
alignment and ABI behavior, or diagnose unsupported boundary forms.
Prefer a documented memory-based interface for the initial example.

Native BF16 arithmetic is retained in section 12.3.1 and can be admitted
operation by operation when required. Full FP16 math coverage, FP8 language
types, general templates and `constexpr` are not dependencies of this
milestone.

### 6.2 Existing dots and kernel-driven gaps

Already implemented: `dot4add_u8i8packed`, `dot4add_i8i8packed`,
`dot4add_u8u8packed`, `dot2add_i16i16packed`, `dot2add_u16i16packed`,
`dot2add_u16u16packed`, and their saturating variants. Check their native
lowering and portable fallbacks in the selected kernels.

BF16 pair dots belong to section 6.1. FP16 pair dots, segmented reductions,
widening reductions, byte LUT helpers and sub-byte pack/unpack are retained
in section 12.3. Introduce only the minimal helper needed by an accepted
workload, with defined overflow, saturation, mask and cross-lane semantics.

### 6.3 Reuse the ACE work and select one consumer

The restricted typed tile surface is the same project as section 5.2.
It is not a second annual commitment to a general tile library.
Use the bounded workload exercise in section 7 to validate BF16 usability;
choose a BF16-relevant kernel explicitly because quantized Q4 dots alone
do not exercise the new type.

A PyTorch or ONNX Runtime custom-op example may be selected if it is the
best way to validate a named consumer. Both proposals and their CMake glue
remain in section 12.6.6. Count framework compatibility and testing work in
the budget rather than treating it as free visibility.

## 7. AI workload exploration: llama.cpp

Purpose: find out whether ISPC can match or beat hand-written intrinsics in
the most scrutinized CPU inference code base, and surface holes in the
programming model with real kernels rather than synthetic tests. gemma.cpp
(Google, Highway-based) is the natural comparison: single-source portable
SIMD with runtime dispatch, bf16-oriented GEMM with fused weight
decompression, autotuned per matrix shape.

### 7.1 Backend research snapshot (commit 71ad059)

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
  a byte-to-two-lane shuffle, awkward in SPMD form. Test existing idioms
  and i8x32/i8x64 codegen first; `unpack_bits` is a candidate in 12.3.2.
- **Int8 dot semantics.** `dpbusd` sums four u8 x s8 products into one int32
  lane, changing element width across lanes. ISPC's `dot4add_u8i8packed`
  expresses this; the s8 x s8 API also already exists. Validate the
  signedness transformation ggml uses for maddubs and its edge cases.
  Check `vpdpbusd` lowering on AVX-VNNI-only CPUs (the
  `avx2vnni` target exists; verify quality).
- **Horizontal reductions.** ggml keeps a vector accumulator and does one
  horizontal sum per row. ISPC code must be written the same way; the
  segmented reductions retained in 12.3.3 may help for per-block sums.
- **Table lookups.** IQ formats and the `iq4nl` codebook use `pshufb`-style
  in-register byte LUTs. Check whether current ISPC lowering introduces
  gathers and whether a byte-shuffle helper is needed (ties to #3771).
- **Threading and interop.** ggml owns the threadpool, so ISPC kernels must
  be exported per-chunk functions with no `launch`. Block structs
  (`block_q4_0` with `ggml_half`) are mirrored as ISPC structs or passed as
  byte pointers plus strides.
- **Dispatch.** Inside each ggml variant build, ISPC should compile for one
  fixed target matching the variant's flags. Separately, a single ISPC
  multi-target binary versus `GGML_CPU_ALL_VARIANTS` is a useful comparison.

### 7.3 Bounded annual exploration

Start with `ggml_vec_dot_q4_0_q8_0` as a packed-integer calibration point,
plus one BF16-relevant kernel (conversion plus FP32 normalization, a fused
epilogue, or a small BF16 dot/GEMM chosen from a consuming workload).
Profile the pinned backend/model first; do not infer hot paths from kernel
names alone. Limit the initial exercise to these two kernels and the
compiler gaps they expose. The expanded kernel list and backend integration
remain in section 12.4.

Stage A is a standalone harness with the relevant CPU-backend sources or
linkage: `ggml-base` alone does not provide the CPU `*_generic` dot oracles.
Use those generic routines as independent references where applicable.
Check integer suboperations exactly when their arithmetic is defined;
quantized dots with scales return floating-point results and need a stated
error tolerance/reduction-order policy. Cover adversarial inputs, tails,
overflow/saturation and non-finite values where supported. Do not generate
both a kernel and its only oracle with the same unvalidated transformation.

Measure against the matching x86 intrinsic kernels and suitable Clang or
Highway implementations with the same layout and numerical behavior.
Include packing, conversion, dispatch and threading costs. Distinguish
bandwidth-bound decode from compute-oriented prefill; faster isolated
instructions need not improve tokens/second.

Use AVX2 (for example Zen 3 or Alder Lake), available AVX-512 hardware
(Sapphire Rapids/Granite Rapids as appropriate), and DMR/NVL for AVX10.2
when hardware is available. Emulation is for correctness. Record thread
counts and pin the source revision, backend configuration and workload.
Add `llama-bench` end-to-end measurements only if stage B is admitted.

Deliver a reproducible report, the two kernels/harness in the benchmark
corpus, and issues for demonstrated gaps. Expand only if the result shows
useful performance, maintainability or adoption value within the annual
budget; a finding that current ISPC is not competitive is also useful.

## 8. AI inside the compiler: one bounded experiment

### 8.1 Research context

Research notes from the October 2026 proposal. These results were not
all independently re-audited in revision 3; verify primary sources,
baselines and licenses before using a claim to select a project. Reported
speedups are workload-specific and are not ISPC benefit estimates:

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
  evaluating candidate programs or pipelines; assess search cost and
  deployment constraints for any ISPC adaptation.
- **Learned cost models** are standard in TVM-style autotuners (TLP speeds
  tuning search 9x on CPU) and were shown for LLVM vectorization factors by
  NeuroVectorizer (1.3-4.7x over baseline, within 3% of brute force, 2019).
  The survey did not establish a deployed model for ISPC's gather-versus-
  shuffle or masked-versus-branch choices. Those are candidate decisions
  to investigate, subject to data and feature availability.
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
  in this survey established a directly reusable CPU SIMD generation
  system; this is not a claim that no such work exists.
- **Testing.** WhiteFox (LLM reads optimizer source to generate targeted
  tests) found 101 bugs in DL compilers; Fuzz4All found 98 across GCC, Clang,
  Z3 and others; ReFuzzer raises validity of LLM-generated LLVM tests from
  47% to 97%; TargetFuzz fuzzes individual passes.
- **Policy.** LLVM's AI Tool Use Policy requires a human in the loop,
  disclosure of substantial generated content, bans unattended agents and
  unreviewed automated review, and forbids AI on good-first issues.

### 8.2 Evaluation principles

- Prefer offline learning that produces a small table/model or a fixed
  pipeline. No network calls or large-model inference during compilation.
- Preserve transformation legality independently of the learned decision.
  Use functional/codegen tests, differential execution and formal checking
  where applicable; passing a test suite is not a proof of all behavior.
- Split training and evaluation by workloads, not only by random inputs to
  the same kernel. Keep evaluation data out of iterative tuning.
- Compare with the existing heuristic and a simple manually tuned rule.
  Include current profiling/PGO approaches where relevant.
- Use features available at the compiler decision point. Actual divergence
  rates, cache behavior or address distributions need explicit profiling or
  runtime mechanisms; offline measurements do not make them statically known.
- Measure compile time, code size, regressions and maintenance cost as well
  as speedup. Follow current LLVM contribution/disclosure requirements.

### 8.3 Selected experiment and stop condition

Preferred project: a fitted decision table/model for choosing among legal
gather/load-shuffle/scalarization alternatives exposed by the performance
work. Start with one target family and an established correctness oracle.
The intern owns data collection and evaluation; the engineer owns legality,
review and any compiler integration.

Time-box the experiment to four to six intern weeks after the harness and
candidate decisions exist. Agree on noise-aware performance and regression
thresholds before tuning. Deliver the corpus split, baseline comparisons,
model/table or negative result, and a reproducible report. Ship only if
held-out results justify complexity; retain a deterministic fallback.
The experiment is optional to ship, not optional to evaluate honestly.

If the required decision/data is unavailable at the selection checkpoint,
substitute a constrained pass-pipeline experiment (section 12.5.1), not a
second simultaneous project. Preserve mandatory pass ordering and start
with a small set of legal parameters/orderings on one target family.
Expansion to masking decisions, more targets or more research projects
requires a separate admission decision.

### 8.4 AI-assisted maintenance within the maintenance budget

Use AI for reproducer minimization, bisection assistance, draft fixes and
tests for active work, with human review and independent validation.
Measure time saved and review cost rather than counting generated patches.
The existing `llvm-trunk-failure-analyzer` and yarpgen jobs provide starting
points. Workflow automation, a dedicated LLM fuzzer and a porting-agent
product are retained in section 12.5; they are not additional annual
deliverables.

## 9. Customer and interop blockers

Fix a language, header, ABI or build issue early when it blocks a selected
kernel or a named customer's supported build. Record the consumer and
reproducer; count it in the performance/BF16 work or maintenance budget.
Do not postpone a necessary ABI fix until after a proof kernel that needs it.

Document the memory layout and C/C++ boundary forms used by the BF16 example,
and keep the initial ACE tile surface internal to a kernel. Small
short-vector stdlib additions or template bug fixes can be admitted for a
specific customer within the same budget.

The complete HLSL-vector proposal, template extensions, `constexpr`,
header-sharing improvements, quality items, packaging/embedding and
framework examples are retained in section 12.6. They are independently
useful proposals, not prerequisites for the fixed-shape ACE prototype.
`libispc` and its JIT already exist; any new embedding work must identify
the missing behavior or support contract.

## 10. Maintenance scope

- **Releases and LLVM.** Maintain supported versions, track trunk failures,
  and reserve time for correctness regressions. Reduce recurring work using
  measured improvements to existing infrastructure.
- **ISPCRT.** Keep security, build and current-consumer support. Review the
  in-flight load-from-memory work (#3900) with its consumer. A subsequent
  freeze/deprecation decision is retained in section 12.7 and depends on
  remaining users and migration cost.
- **Xe GPU.** Keep the opt-in build and existing CI functional; no new
  feature commitment.
- **LLVM intrinsic adoption.** Retire duplicate pseudo builtins case by
  case when correctness and lowering quality are demonstrated. This is not
  a dependency of the first gather fixes.
- **Old targets.** Retain supported behavior. The i686/SSE removal proposal
  remains in section 12.7; require measured build/maintenance savings and a
  consumer/migration assessment before changing release contents.

## 11. Sequencing and decision gates

Checkpoints organize dependencies; they do not guarantee upstream LLVM or
hardware delivery dates. Within each period, sequence engineer-owned
implementations under the work-in-progress limit in section 2.

| Period | Main work | Exit or rescoping decision |
|---|---|---|
| Q4 2026 | Establish a small corpus and comparison baselines; reproduce x16 issues; select two or three performance fixes. Write BF16 semantics and the restricted ACE contract; confirm upstream/OS/validation dependencies. | Record consumers, owners, estimates, baseline noise and acceptance criteria. If a reported regression is not reproduced, investigate rather than schedule an assumed fix. |
| Q1 2027 | Ship the first measured performance fixes, then BF16 storage/conversions and a useful kernel. Continue ACE coordination; implement a slice only when the engineer's active change and dependencies allow it. | Require numerical/interop validation for BF16. Review workload results before admitting more helpers or llama.cpp integration. |
| Q2 2027 | Concentrate engineer implementation on the restricted ACE surface and available validation gates. Run the one intern experiment if the corpus and internship timing permit. | If LLVM or execution access slips, retain an explicitly experimental prototype and redirect remaining capacity to performance. Stop or ship the AI experiment according to held-out results. |
| Q3 2027 | Harden completed work, validate consuming builds, publish comparisons and document evidence-based width recommendations. | Claim only achieved ACE validation levels. Admit stretch work only from real remaining capacity; record remaining proposals in section 12 for the next cycle. |

Before implementation, every selected work item records: consuming
workload/customer; owner; estimated effort and budget; LLVM/hardware/OS
dependencies; correctness and performance acceptance; and a stop/rescope
condition. For external dependencies, record who follows up and the next
decision date. Report progress as validated outcomes, not counts of
features or generated changes.

## 12. Deferred and stretch proposals

This catalog retains the proposed features outside the smaller annual
commitment. It is not an additional implementation schedule. Admission
requires the item-level information in section 11 and an explicit capacity
decision. A selected subset may move into the core plan; remaining features
stay here for subsequent cycles. Existing functionality is marked so its
intended outcome is preserved without duplicating implementation.

### 12.1 Broader performance program

The original performance inventory follows. Select individual items through
section 3; the full redesign, coverage and infrastructure program is not
committed for this year.

#### 12.1.1 Gather/scatter lowering

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
- Improve narrow-index codegen and scaled addressing. On x86, hardware
  gather indices are 32 or 64 bits; retain 16-bit values before extension
  where profitable rather than assuming a 16-bit-index gather instruction.
- Emit `llvm.masked.load/store/gather/scatter` and
  `llvm.experimental.vector.compress` where LLVM now lowers them well, and
  retire the matching ISPC pseudo builtins after semantic and performance
  checks. Verify intrinsic names in the chosen LLVM baseline. This may
  shrink LLVM-upgrade cost; it is not a prerequisite for initial fixes.
- The gather-versus-shuffle-versus-scalarize choice is the first candidate
  for a fitted cost model (section 8.3).

#### 12.1.2 Mask and control-flow codegen

- Bool and mask representation on AVX2 and AVX-512 (#2920). Use
  `llvm.vector.reduce.and/or` for coherent-control-flow tests (#1338).
- Masked execution versus branch-around for divergent `if`: today the choice
  is a fixed heuristic plus `cif`. Measure it across divergence rates and
  targets, then evaluate a cost model as an extension of section 8.3.
  Define which inputs are static and which would require profiling.
- Varying integer division and modulo (#3000, #3001): magic-number sequences
  for uniform divisors, better vectorized fallback otherwise.
- Inlining heuristics and code bloat (#3804), and stopping LLVM from
  replacing vector loops with libc calls (#3241).
- Loop overheads (#3455): mask recomputation, induction variable widening,
  unroll decisions.

#### 12.1.3 Lane-width gaps

- `avx2-i64x8` double-pumped target (#2903, the top-voted codegen issue).
- Investigate the reported restriction to ymm for 64-bit values on
  `avx512-x8` targets (#3161) against current LLVM/ISPC codegen.
- Varying 8-bit and 16-bit codegen (#2901), `popcnt` (#3676), `shuffle`
  (#3771) lowered to native permutes, short-vector shuffle, rotate and shift
  (#3446).

#### 12.1.4 Math library

- Finish the accuracy audit and publish ULP tables per function and target
  (#2236), distinguishing tested-domain maxima from proven error bounds.
- Complete `float16` math coverage (#2290); independently useful, but
  not a prerequisite for BF16 storage, conversion or dot products.
- Evaluate SVML-style and Sleef-derived kernels for transcendentals on
  AVX10.2 (#2906, #3406). Decision criterion: ULP tables plus the
  benchmarks below.

#### 12.1.5 Performance infrastructure

- Make the daily internal performance run public: a dashboard from the
  existing tracking job over `benchmarks/`, with per-PR regression gating on
  a small subset after dedicated-runner stability and noise thresholds
  are established.
- Fill `benchmarks/03_complex` (currently empty) with representative
  kernels: an OIDN-style convolution, a texture compressor block, a ray-box
  traversal loop, a Chaos-style particle update, and the llama.cpp kernels
  from section 7. Add x8-versus-x16, bf16 and int8 dot-product microbenchmarks.
- Publish comparisons against Highway and plain Clang on the same kernels.
  Initial comparisons belong to the core plan; broad public coverage is
  the stretch proposal.

### 12.2 Broader hardware exposure

Admission: a consuming kernel/customer and available validation justify the
surface or audit beyond the core hardware work.

- **Broad APX codegen audit.** Measure EGPR spill reduction, NDD/NF mask
  arithmetic and CCMP mask tests across wider workloads; add relevant lit
  tests and fix confirmed regressions.
- **AVX10.2 surface expansion.** BF16 arithmetic (`VADDBF16`,
  `VFMADD*BF16`, `VCMPBF16`, `VSQRTBF16`, `VRNDSCALEBF16` and related
  operations); FP16-to-FP8 conversions (`VCVT[BIAS]PH2{BF8,HF8}[S]`,
  `VCVTHF82PH`); FP16 dot product `VDPPHPS`; saturating converts and minmax.
- **Feature-name migration.** Preserve the proposal to remove deprecated
  `avx10.x-256`, `avx10.x-512`/`evex512` spellings if actual remaining uses
  are found. Record the code path and supported LLVM versions; required
  compatibility fixes are maintenance.
- **Client default and target recommendations.** Retain the proposal to add
  `avx10.2nvl-x16` to recommended game-engine multi-target sets and consider
  it as NVL's default, conditional on representative hardware results.
  DMR already defaults to x16; no universal x16 performance guarantee.
- **Existing INT8/INT16 dots.** The proposed signed-signed/unsigned-unsigned
  INT8 and unsigned/mixed INT16 API additions are already implemented.
  Preserve their intended outcome as broader codegen/fallback coverage,
  not duplicate API work.
- **Customer-driven AMX additions.** Raw intrinsics for AMX-FP8, AMX-MOVRS
  and AMX-AVX512, and shared row-readback improvements such as `TILEMOVROW`.
  Prioritize measured benefits for current consumers.

### 12.3 Additional AI types, primitives and tile programming

Admission: the chosen kernel needs the feature, the numerical/execution
contract is settled, and implementation plus target maintenance fits.

#### 12.3.1 Native BF16 arithmetic

Extend the uniform/varying BF16 type to native AVX10.2 arithmetic with
software lowering elsewhere. Preserve the proposed widen-operate-narrow
fallback as a candidate implementation, but verify rounding, contraction,
subnormal and exceptional-value behavior against the language contract.
Expand constant folding, debug info and header/ABI support with the surface.

#### 12.3.2 FP8 and sub-byte storage/conversions

- `float8_e4m3` and `float8_e5m2` as storage-only types with explicit
  conversion builtins. Specify exact encodings, exceptional values and
  saturation, not only the exponent/mantissa widths.
- Lower through `VCVT*PH2{BF8,HF8}` and `VCVTHF82PH` on AVX10.2,
  `VCVTPS2{BF8,HF8}` and `VCVT{BF8,HF8}2PS` with `avx10v2aux`,
  and portable bit manipulation elsewhere. Define direct versus staged
  FP32/FP16 conversions; intermediate rounding can change the result.
- Saturating, round-to-odd and bias conversion forms exposed by name.
- `unpack_bits<2..7>()` and pack helpers for int4, FP4 (E2M1) and FP6
  (E2M3/E3M2) block formats; `VUNPACKB` lowering with a portable fallback.
  A minimal nibble helper may precede the full family if a core kernel
  requires it. Integer storage plus conversion helpers can precede types.
- E8M0 scale helpers for MX block formats and the auxiliary symmetric
  INT32-to-INT8 saturating conversion (`VPMOVSSDB`).

#### 12.3.3 Dot products, reductions and lookup helpers

- Extend BF16 pair-dot coverage (`VDPBF16PS` on suitable AVX-512 CPUs and
  AVX10.2 forms); add FP16 pair dot (`VDPPHPS`).
- Audit the already implemented INT8/INT16 signedness and saturating
  families on additional targets and emulated paths.
- Segmented sums per 4, 8 or 16 lanes and widening INT8/INT16 horizontal
  reductions into INT32. Define subgroup, partial-mask and overflow rules.
- Byte LUT helpers using suitable `VPERMB`/`VPERMI2B` or byte-shuffle
  instructions, with portable fallbacks and explicit lane semantics.

#### 12.3.4 Expanded ACE and portable typed tile API

Retain the original larger proposal: `uniform tile<T>` or opaque
`tile_i32`/`tile_f32`, compiler-managed configuration/release,
`tile_zero`, `tile_op4_ss/su/us/uu`, `tile_op2bf16`, `tile_op4mx*`,
`tile_getrow`, `tile_setrow`, `tile_setcol`, and `bsr_set` scale loading.
MX group selectors must be compile-time constants where encoded as
immediates. Resolve BSR layout/API dependencies with LLVM PR 208706.

Extend beyond x16 to x32 through an explicit subgroup/operand and row
readback design. The original goal of removing the
`TARGET_WIDTH == tile rows` constraint remains, but reinterpretation alone
does not achieve it. Initial native x8 support was not proposed; any
broader width-independent interface must state its supported mappings.

Provide a VNNI/FMA fallback on AVX2 and AVX-512 so the same source can run
before ACE hardware is available. Reference correctness and competitive
fallback performance are separate deliverables; a fixed 16x16 accumulator
can impose substantial vector-register pressure. Retain a strict no-spill
diagnostic mode as an optional design investigation, with LLVM backend
support and optimization-dependent behavior assessed first.

Proof kernels retained: the llama.cpp Q4_K GEMM and an OIDN-style convolution.
The latter requires an explicit FP16/BF16 quality assessment. Keep existing
OIDN AMX code supported. An AMX lowering for the new abstraction is not
currently proposed; revisit only for a named consumer, rather than relying
on an unverified AMX deprecation assumption.

### 12.4 Expanded llama.cpp exploration and integration

Admission: stage A identifies a useful result and profiling justifies the
next kernel. A backend integration requires ownership of upstream churn,
build/dispatch testing and end-to-end validation.

The complete original kernel list is retained:

1. `ggml_vec_dot_q4_0_q8_0` and `ggml_vec_dot_q8_0_q8_0`, the simple block
   formats for calibration (the first is in the core experiment).
2. `ggml_vec_dot_q4_K_q8_K` and `ggml_vec_dot_q6_K_q8_K`, stressing
   nibbles, 6-bit sub-block scales and VNNI. Confirm their contribution in
   the selected Q4_K_M model/backend rather than assuming they dominate.
3. `ggml_gemm_q4_K_8x8_q8_K`, testing prefill GEMM, shuffles, register
   tiling and the expanded tile API.
4. FP32/FP16 elementwise and attention kernels: softmax with `expf`,
   rms_norm, swiglu, the flash-attention one-chunk path; FP16 KV-cache loads.

Stage B: a `GGML_ISPC` CMake option compiling `.ispc` per CPU variant,
replacing entries in `type_traits_cpu` and repack tables. Respect ggml's
threadpool with per-chunk exports and no ISPC `launch`; validate shared or
mirrored block layouts, byte-pointer/stride interfaces and fixed-target
flags. Separately compare a single ISPC multi-target binary with
`GGML_CPU_ALL_VARIANTS`.

End-to-end baseline: `llama-bench -p 512 -n 128` on a pinned 7B/8B Q4_K_M
model, reporting prefill/decode separately and sweeping threads from one
to all cores. Test AVX2, appropriate AVX-512 variants, AMX on/off where
supported, and DMR/NVL AVX10.2 when hardware exists. Granite Rapids belongs
in the AVX-512/AMX group. Report nanoseconds/block, packing/conversion and
dispatch costs, decode bandwidth versus a suitable bandwidth baseline
(including STREAM where useful), and prefill operations versus the relevant
VNNI/tile peak. Hardware measurements, not emulator timing, set conclusions.

Retain the report, benchmark contributions and demonstrated gap list:
sub-byte unpacking, integer-dot lowering quality, byte LUTs, segmented
reductions, block-as-uniform idioms, tiles, BF16 operations, FP16 loads and
foreign-threadpool guidance. These are candidate gaps, not a commitment
to implement every helper before measuring.

### 12.5 Additional AI compiler research and automation

Admission: one experiment at a time, an independent correctness mechanism,
held-out evaluation and explicit engineering/review capacity. Labels such
as "intern project" do not establish an effort estimate.

#### 12.5.1 Offline-tuned pass pipelines

Search ISPC's `src/opt.cpp` pass order and parameters offline. Start with a
small legal search space on one target family, preserving mandatory
lowerings and analysis requirements. Compare with the current pipeline and
manual tuning; gate on correctness, held-out performance, compile time and
code size. Retain the goal of better fixed pipelines per avx2 x8,
avx512 x16, avx10.2 x16/x32 and neon family, but expand only on evidence.
Research speedups do not guarantee a few percent for ISPC. This may replace
the core fitted-model experiment under section 8.3.

#### 12.5.2 LLM differential fuzzer

Use the WhiteFox/Fuzz4All approach: seed a generator with ISPC grammar and
selected pass sources (mask ops, gather coalescing, uniform/varying
analysis); build on yarpgen CI. Compare supported targets, widths and
optimization levels, using independent references for a defined semantic
subset. Scalar C must model ISPC collectives, masking and integer/FP rules;
Clang is not automatically an oracle for arbitrary ISPC programs.
Include minimization, deduplication and human triage. Measure useful
confirmed bugs and review time; finding #3882-like miscompiles is the goal.

#### 12.5.3 Wider fitted cost models

Extend the initial memory-lowering experiment to masked-versus-branch
decisions and further targets. Sweep divergence, gang width and memory
patterns offline; distinguish compile-time features from inputs requiring
profiling/runtime support. Fit a small model/table with a deterministic
fallback. Validate on held-out workloads and report losses; "never worse
on the training corpus" is not a generalization criterion.

#### 12.5.4 Width predictor

Compile kernels at x8/x16 or x16/x32, collect features and timings, and
evaluate width advice (`--print-width-advice`) before considering automatic
selection. Account for unavailable hardware, extra compilation and
workload-specific behavior. Advice does not replace the hardware findings
needed for default-width changes.

#### 12.5.5 Intrinsics-to-ISPC porting agent

Retain an agent loop that translates C++ intrinsics kernels to ISPC,
compiles, validates against an independent original/reference on adversarial
as well as seeded inputs, and benchmarks across targets with timing
protected against reward hacking. Start from the llama.cpp corpus; assess
the SSE/AVX2-to-AVX10.2 and ARM migration use case. Publication and ongoing
support are a separate commitment after the experiment demonstrates value.

#### 12.5.6 LLM-proposed, verified peepholes

Collect hot mask/gather IR, propose rewrites, check them with Alive2 where
supported, and implement reviewed transformations in `PeepholePass`.
Record verifier limitations for masked intrinsics and unsupported
operations; a timeout or unsupported proof is not successful verification.

#### 12.5.7 MLGO inliner retraining

Retain speed-oriented retraining on ISPC's stdlib-heavy, already-vectorized
IR. Assess training/PGO infrastructure and compare against existing
heuristics and manual tuning. The 1-3% range suggested by other studies is
not an ISPC ceiling or promised gain. Admit only with sufficient expertise,
compute and ongoing model ownership.

#### 12.5.8 ISPC as a target for AI kernel generation

Retain "ISPC-Bench" using TSVC, PolyBench, ISPC examples and llama.cpp
kernels, with differential correctness and robust timing, as an extension
of the performance corpus. It can pair with the porting agent and produce
training data. Validate benchmark rights/provenance and measure utility
before committing to a separately maintained public benchmark product.

#### 12.5.9 AI-assisted maintenance automation

- Extend `llvm-trunk-failure-analyzer` from categorization to proposing
  and testing fixes, with draft PRs for human review.
- Issue triage with reproducer minimization and regression bisection,
  publishing reviewed results as comments.
- AI-drafted math functions, dot products and new-type fallbacks, validated
  against independent numerical references and appropriate tests.
- AI-drafted lit tests from descriptions, checked against intended behavior
  and known failures as well as by running them.

Keep these within a measured maintenance-saving budget. Automatically
produced patches or tests do not establish correctness, and automation
must justify its own operation and review cost.

### 12.6 Language, interop, distribution and framework features

Admission: a named consumer and bounded implementation/maintenance estimate.
Small fixes blocking core work may be selected under section 9. Preserve
the full feature set below for later prioritization.

#### 12.6.1 HLSL-style vector types (customer request)

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
  because `T(args)` is ambiguous with C-style casts in the LALR grammar;
  investigate a production for known vector type names and validate
  compatibility rather than assuming the grammar issue is solved.
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

Retained feature sequence, with each step independently scoped and admitted:

1. Geometric and graphics functions in `short_vec.isph`, templated over `T`
   and `N`, uniform and varying.
2. Swizzle writes: masked insert on store, reject duplicates and mixed
   naming sets, support compound assignment.
3. Opt-in `hlsl_types.isph` (or a flag) with the standard typedefs.
4. Constructors as a grammar production when the callee is a vector type
   name, with flattening and count checking.
5. Unpadded layout option for uniform short vectors and header-generation
   support, with a migration note and explicit ABI compatibility testing.
6. Matrix types as a second phase: row-vector arrays with `mul`,
   `transpose`, `determinant`, `inverse`, and header interop. Later these can
   use dot-product or tile builtins where shapes and measured costs
   justify it; small graphics matrices do not automatically fit ACE.

#### 12.6.2 Templates and compile-time evaluation

- Default template arguments and deduction in specializations (documented
  as planned); fix the open template bugs (#3016, #3025, #3040, #3232,
  #3240). Struct templates can support a general `tile<T>` abstraction;
  the initial opaque fixed-shape API does not depend on them.
- `constexpr` per the design proposal in #3697, for more expressive tile
  shapes, block sizes and compile-time dispatch in stdlib code. Existing
  constant-expression checks suffice for the restricted ACE milestone.

#### 12.6.3 C++ header sharing and ABI

OSPRay's `#ifdef ISPC` shared-header pattern and the UE struct-layout
complaints point the same way.

- Document guaranteed layout-compatibility rules.
- Fix return-by-value and pointer ABI issues (#1590, #1855, #2344).
- Fix multi-target header naming (#1666); emit unused structs on request
  (#2277); emit short-vector types (#2016).

#### 12.6.4 Small quality items

`auto` (#2310), explicit bitcast syntax (#1809), warning on missing return
(#1708), consistent signed/unsigned conversions (#2714).

#### 12.6.5 Packaging and embedding

- PyPI (#3741) and an official vcpkg port (#1305) remove the "separate
  compiler to install" friction. Include release automation, platform
  coverage and ongoing distribution maintenance in the estimate.
- `libispc` (#791). The JIT and library API already exist in
  `src/ispc_impl.cpp` and the docs. Retain stabilization, versioning and
  documentation improvements, but select concrete missing behavior or
  support guarantees with an embedding consumer.
- Single object for multiple targets (#1850).

#### 12.6.6 Framework interop

Retain both PyTorch custom-op and ONNX Runtime custom-op examples with CMake
glue. Initially select at most one if it validates the BF16/AI workload;
budget API/version compatibility, layout/conversion and threading tests.

### 12.7 Maintenance reductions and target-policy proposals

- **ISPCRT freeze/deprecation.** Retain the proposal to complete the
  load-from-memory work (#3900), freeze new functionality and assess
  deprecation at year end. First confirm remaining consumers and migration
  paths; OSPRay/Open VKL removal alone does not establish that no users remain.
- **Duplicate pseudo builtins.** Preserve the broader retirement program
  after generic LLVM intrinsic equivalence and performance are established.
- **Old targets.** Audit i686 (#1865) and the lowest SSE tiers for possible
  removal from release binaries. Measure stdlib build-time/matrix savings,
  assess consumers and define a deprecation/migration policy first.
- **Other target/research requests from the survey.** Keep ARM SVE/SVE2
  (#1947), WebAssembly SIMD coverage gaps (#2127), generic SPIR-V (#982),
  C++-with-intrinsics output (#2722, #2644), and Enzyme automatic
  differentiation (#3371) visible as future candidates. Verify current
  support and a consumer before estimating them. They are not part of the
  year's Intel-hardware commitment or a restart of Xe GPU feature work.

## 13. Sources and verification status

Revision 3 checked the roadmap worktree at commit `3f5feb0e88a0455e6662da496d95c970f20e1bcc`,
including its uncommitted discussion-survey additions. The following local
source checks support corrections to the plan:

- `src/ispc.cpp`: DMR/NVL default widths and x16/x32 target definitions.
- `stdlib/stdlib.ispc`, `stdlib/include/stdlib.isph`,
  `builtins/target-avx10_2-x*-common.ll`, and
  `tests/lit-tests/vnni-avx10.2dmr.ispc` /
  `vnni-avx10.2nvl_llvm22_plus.ispc`: existing packed INT8/INT16 dot
  families, native lowering and tests.
- `docs/ispc.rst` ("Using ISPC as a Library"), `src/ispc_impl.cpp` and
  `src/include/ispc/ispc.h`: implemented library/JIT API.
- `src/opt.cpp`, `src/opt/ImproveMemoryOps.cpp` and
  `src/opt/GatherCoalescePass.cpp`: current lowering/pipeline structure.
- Local OIDN `devices/cpu/cpu_conv_amx.ispc`: FP16 AMX inputs/operations
  and the target-width constraint.
- Local llama.cpp `ggml/src/CMakeLists.txt`, `ggml/src/ggml-cpu/CMakeLists.txt`
  and CPU quantization sources: reference routines belong to the CPU
  backend; scaled dot results are floating point.
- LLVM `X86.td` definitions inspected during review: Granite Rapids versus
  Diamond Rapids features. These definitions are not product-date evidence.
- Public LLVM PRs 208408 and 208706: open implementation dependencies,
  proposed tile spill/reload and BSR infrastructure.

Research and usage sources retained from the original proposal follow.
Their presence does not mean every numerical result, absence claim or
release-status assertion was independently reverified. Pin revisions and
direct source links when a claim becomes a project dependency.

- ACE: spec v1.15 (x86ecosystem.org, May 2026), whitepaper v1.0 (April
  2026), as cited in the original proposal;
  [LLVM PR 206888](https://github.com/llvm/llvm-project/pull/206888)
  (reported merged), [208408](https://github.com/llvm/llvm-project/pull/208408)
  and [208706](https://github.com/llvm/llvm-project/pull/208706)
  (both checked open during review).
- LLVM: `llvm/lib/Target/X86/X86.td` and `X86InstrAVX10.td` on main; Clang 21
  release notes; MLGO docs and google/ml-compiler-opt.
- Customers: RenderKit changelogs for oidn, ospray, openvkl, embree; OIDN
  `devices/cpu/cpu_conv_amx.ispc`; ispc/ispc release download counts; GitHub
  code search for `enable_language(ISPC)`.
- Issues and discussions: 271 open and 123 recently closed ispc/ispc issues,
  64 discussions, pulled 2026-10-08.
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

Planning assumptions still requiring confirmation: intern availability;
representative benchmark machines and DMR/NVL access; ACE emulator/hardware
and OS permission interfaces; attributable AMX lifecycle guidance; specific
customer commitments behind language requests. None is inferred solely
from issue reactions, commit counts or compiler CPU definitions.
