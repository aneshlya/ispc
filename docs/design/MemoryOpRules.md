# The rewrites of ImproveMemoryOps and GatherCoalescePass

This document lists every rewrite that `ImproveMemoryOps`
(`src/opt/ImproveMemoryOps.cpp`) and `GatherCoalescePass`
(`src/opt/GatherCoalescePass.cpp`) make, and every case where one of them
computes a wrong address or touches memory the program doesn't access.

Each rewrite is described the same way:

* **Code**: the functions that implement it.
* **What it does**: what it matches and what it emits.
* **Wrong today**: the cases where its result is wrong, numbered, each
  with an example and the lit test that shows it (all of them in
  `tests/lit-tests`, marked `XFAIL` until the case is fixed), or "none
  yet".

An example marked **IR** is a counterexample for the IR the pass sees; it
may not be reachable from ISPC source today. The other examples come from
ISPC source.

## At a glance

| Rewrite | Wrong case | From | Test |
| --- | --- | --- | --- |
| gs-to-based, base recovery | the base is not the object the lanes access | source | `address-gap-provenance.ispc` |
| gs-to-based, scale extraction | the scale is taken from a multiply that wraps | IR | `address-gap-scale-extraction.ll` |
| gs-to-based, scale extraction | the splat is read through an `int` | source | `address-gap-scale-extraction.ll` |
| gs-to-based, scale extraction | the scale is taken out of a `trunc` | IR | `address-gap-scale-extraction.ll` |
| gs-to-based, scale extraction | `nneg` is moved to an operand that may be negative | IR | `address-gap-nneg-scale.ll` |
| gs-to-based, scale extraction | an int8 or int16 partial scale hits an assertion | IR | `address-gap-partial-scale.ll` |
| gs-to-based, narrowing | a sum is truncated to 32 bits as if it fitted | source | `address-gap-narrow-sum.ispc`, `address-gap-nested-sum.ispc` |
| gs-to-based, narrowing | an unfolded delta is assumed to fit in 32 bits | IR | `address-gap-narrow-constant.ll` |
| gs-to-based, narrowing | the address depends on the lowering | IR | `address-gap-narrow-lowering.ll` |
| gs-to-based, constant split | a split under a `sext` of an add without `nsw` | source | `address-gap-minus-const.ispc` |
| gs-to-based, constant split | a split under a `zext nneg` of an add that wraps | IR | `address-gap-nneg-split.ll` |
| gs-to-based, constant split | the regrouped constants wrap | source | `address-gap-split-source.ispc` |
| gs-to-based, constant split | a copied `nsw` makes a new shift poison | source | `address-gap-split-source.ispc` |
| more-const | `c * scale` wraps at 32 bits | IR | `address-gap-more-const.ll` |
| more-const | the split is inexact | source | `address-gap-split-source.ispc` |
| gs-to-load-store | the pointer is rebuilt at 32 bits | IR | `address-gap-load-store.ll` |
| gs-to-load-store | the start is taken from lane 0 when lane 0 is off | source | `address-gap-lane0.ispc` |
| gs-to-load-store | lanes that wrap are counted as consecutive | source | `address-gap-wrap.ispc` |
| gs-to-load-store | a loop PHI is counted as linear for any stride | IR | `address-gap-load-store.ll` |
| gs-to-load-store | the consecutive test overflows in the compiler | IR | `address-gap-load-store.ll`, `address-gap-linear-and.ll` |
| gs-to-load-store | the first lane that is on is found by another mask test | IR | `address-gap-load-store.ll` |
| gs-to-load-store | the broadcast reads an undefined lane 0 | IR | `address-gap-broadcast-undef-lane.ll` |
| gs-to-load-store | the broadcast loads when no lane is on | source | `address-gap-off-lanes-broadcast.ispc` |
| masked-store | the blend stores read the mask by another bit | IR | `address-gap-blend-mask.ll` |
| coalesce, grouping | a delta that isn't a constant vector hits an assertion | IR | `address-gap-coalesce-delta.ll` |
| coalesce, grouping | a call with more lanes than the target's overruns a buffer | IR | `address-gap-coalesce-lanes.ll` |
| coalesce, grouping | reads are moved across fences and calls that may not return | source, IR | `address-gap-coalesce-barrier.ispc`, `address-gap-coalesce.ll` |
| coalesce, slots | absolute byte offsets are divided instead of distances | IR | `address-gap-coalesce.ll` |
| coalesce, slots | a negative i32 delta is read zero extended | source | `3984.ispc` |
| coalesce, shared pointer | the product is taken at the offset width | IR | `address-gap-coalesce.ll` |
| coalesce, assembly | the results are assembled wrongly at 32 and 64 lanes | source | `address-gap-coalesce-wide.ispc` |
| lowering, emulated gather | the lanes that are off read the base | source | `address-gap-off-lanes-base.ispc` |
| lowering, blend store | the vector reaches past the stack object | source | `address-gap-blend-store.ispc` |
| lowering, blend store | the object is shared with a running task | source | `address-gap-blend-store-task.ispc` |
| frontend | a `varying bool` array is indexed by the register size | source | `varying-bool-array-index.ispc` |
| frontend | a struct member offset of 2 GiB is sign extended | source | `address-gap-struct-2gb.ispc` |
| frontend | `new` of 2^31 or more elements sign extends the count | source | `address-gap-new-2gb.ispc` |

The cases that compile correctly today are checked by
`address-gaps-ok.ispc` and `address-gap-off-lanes-ok.ispc`.

## Background

### Forms of a memory call

The frontend emits every varying memory access as a call to a pseudo
builtin. `ImproveMemoryOps` rewrites these calls into cheaper forms, and
`ReplacePseudoMemoryOps` later lowers what is left to target builtins.
The forms of a gather (a scatter is the same, with a stored value):

| Form | Operands | Address of lane j |
| --- | --- | --- |
| general | addresses, mask | `addr[j]` |
| base-offset | base, scale, offsets, mask | `base + offsets[j] * scale` |
| factored | base, offsets, scale, delta, mask | `base + offsets[j] * scale + delta[j]` |
| masked load or store | pointer, mask | `ptr + j * element size` |

The offsets and the delta are 32- or 64-bit vectors; the scale is an i32.
The base-offset form is meant for a hardware gather or scatter; the
factored form is the emulated one.

### What the lowerings compute

The address column above doesn't say at which width the products and sums
are taken, or whether the scale is signed. The lowerings don't agree:

| Lowering | Computes |
| --- | --- |
| hardware gather (base-offset form) | `base + sext(offsets) * scale`, at pointer width |
| factored gathers and scatters of `builtins/util.m4` (`__gather_factored_base_offsets32_*`, `__gather_factored_base_offsets64_*`, `gen_scatter_factored`) | `base + sext(offsets) * sext(scale) + sext(delta)`, at pointer width |
| base-offset gathers of `builtins/util.m4` for the element types without a gather instruction, such as i8, i16 and half on AVX2 (`__gather_base_offsets32_*`, `__gather_base_offsets64_*`) | 32-bit offsets: `offsets * scale` at 32 bits, then the factored gather. 64-bit offsets: `offsets * zext(scale)` at 64 bits |
| `builtins/generic.ispc`, base-offset form (`__gather_base_offsets32_*`) | `offsets * scale` at the offset width |
| `builtins/generic.ispc`, factored form (`__gather_factored_base_offsets32_*`) | `offsets * scale + delta` at the offset width, with `nsw` (the ISPC arithmetic of the builtin) |

They give the same address when every product and sum fits in a signed
integer of the offset width and the scale is positive. Otherwise one call
means two addresses: an emulated i8 base-offset gather with offsets
`0x40000000 + j` and scale 4 reads `base + 4 * j`, a hardware gather with
the same operands `base + 2^32 + 4 * j`. Several rewrites depend on the
products and sums fitting without checking it.

The factored gathers of `builtins/util.m4` (x86, and NEON through
`target-neon-common.ll`) are the lowering of every gather on SSE2, SSE4,
AVX1 and NEON, and of the i8, i16 and half gathers on AVX2 and AVX-512.

### Lanes that are on

A lane is on when the mask enables it. A rewrite may read only what the
program reads in the lanes that are on; reading anything else is correct
only if the memory is known to be readable and the value is discarded.
LLVM allows such a read to race with another thread's write: the racing
load returns an undefined value, which is discarded.

## How the passes run

* `ImproveMemoryOps` runs at pipeline stage 100 at `-O0`, always. At
  higher levels it runs at stages 211, 256 and 271, unless the gang width
  is 1 or `--opt=disable-gather-scatter-optimizations` is given.
* `GatherCoalescePass` runs at stage 261 under the same two conditions,
  and not with `--opt=disable-coalescing`.

`ImproveMemoryOps` asks its rules for a plan in a fixed order
(`lPlanAccess`): `gs-to-based`, `more-const`, `gs-to-load-store`,
`masked-store`, `masked-load`. It emits the first plan (`lEmitPlan`). If
the result is a memory call (a new call, or the same call with new
operands), the rules are asked again for it in the same run, so one
access goes through a chain:

```
__pseudo_gather64_float          (general)
  -> gs-to-based       __pseudo_gather_factored_base_offsets32_float
  -> more-const        same call, more of the offsets moved into the delta
  -> gs-to-load-store  __masked_load_float  (the lanes are consecutive)
  -> masked-load       load <8 x float>     (the mask is all on)
```

A reverse load ends the chain: its result is the shuffle that reverses
the lanes, so its masked load is looked at again only in the next run.

A rule doesn't record why it matched. It checks its own assumptions, in
its own way, while it matches.

The rule names are the ones `--dump-memory-plans` prints. A call no rule
rewrites is printed as `keep`, except the calls in the bodies of the
memory builtins. The coalescer prints `coalesce` for a group, and `keep`
with a reason for the pseudo gathers it doesn't coalesce: `mask`,
`varying offsets`, `element type` or `not a factored gather`.

## ImproveMemoryOps

### gs-to-based: general gather or scatter to a based form

**Code.** `lPlanBasedForm`, emitted by `lEmitBasedForm`.

**What it does.**

* Takes a general pseudo gather or scatter of the gang width. A prefetch
  is kept; it is lowered later, to scalar prefetches on CPU targets.
* The module must declare the replacement builtins (for a 64-bit general
  call with 32-bit addressing, those of both widths); hand-written IR that
  doesn't is left alone.
* The form depends on `useGather()` and `useScatter()`, which give one
  answer for all element types:
  * **base-offset form**, when the target uses its gather (scatter)
    instruction, also for an element type without one, which then gets
    the emulated base-offset lowering. Steps: base recovery, scale
    extraction, narrowing of the offsets.
  * **factored form** otherwise. Steps: base recovery, constant split,
    scale extraction from the variable part, narrowing of both parts.

The four steps are described next.

#### Base recovery

**Code.** `RecoverBaseOffsets` (`src/opt/MemoryAccess.cpp`).

**What it does.** Splits the address vector into a uniform scalar and
per-lane offsets. It finds the scalar:

* as a broadcast (an insert chain, a shuffle or a splat, with the
  residue of the broadcast vector's lane 0 kept in the offsets), possibly
  behind vector casts;
* as a term of adds and disjoint ors with the other terms;
* through a one-index `extractvalue` of an insert chain;
* as the global of a constant vector of a global's address plus
  displacements.

An integer loaded from memory is taken for a pointer. It gives up on
calls whose lanes have different bases, and after a fixed budget of
visited values. The scalar becomes the base through `inttoptr`.

**Wrong today.** It assumes the scalar is the pointer of the object every
lane accesses; nothing checks that the lanes stay inside that object.

1. **The base is not the object the lanes access.** When the varying part
   carries a pointer, as in an address computed in integers as
   `(int64)p + ((int64)q - (int64)p)` with a uniform `p` and a varying `q`,
   the base is `p` while the lanes access `q`'s object. LLVM then treats
   the access as one to `p`'s object and can reuse a load of `q`'s object
   across a store to it.
   *Test:* `address-gap-provenance.ispc` (#3983).

#### Scale extraction

**Code.** `lExtractOffsetVector248Scale`, with `lExtract248Scale`,
`lExtractNUWScale` and `lIsIntegerSplat`.

**What it does.**

* Looks for a multiply by a constant splat in the offsets, through casts
  and through adds (and disjoint ors) whose two sides have the same scale;
  an add of zero is skipped.
* A splat of 2, 4 or 8 becomes the scale. Another splat that is a
  multiple of 8, 4 or 2 is divided by the largest of these, which becomes
  the scale (`24 * i` gives scale 8 and offsets `3 * i`).
* Shifts are not looked at, except under a plain `zext`, where only a
  `nuw` multiply or shift by 2, 4 or 8 directly below it is taken.
* A `zext nneg` is treated like a `sext`, and the new cast of the
  multiplier's operand gets the `nneg` flag.

**Wrong today.** It assumes that moving the scale out of the cast keeps
the value, `ext(x * s) = ext(x) * s`, and that the multiplier's operand
is non-negative where the product was.

1. **The scale is taken from a multiply that wraps.** With
   `--wrap-signed-int` or an unsigned index, `sext((2^30 + j) * 4)` is
   `4 * j`, while the extracted `sext(2^30 + j) * 4` is `2^32 + 4 * j`.
   **IR.** *Test:* `address-gap-scale-extraction.ll` (WRAP).
2. **The splat is read through an `int`.** `a[i * 4294967298]` with an
   int64 `i` and `--addressing=64` on `avx2-i32x8` becomes a hardware
   gather of `a + 8 * i`, not `a + 4 * 4294967298 * i`.
   *Test:* `address-gap-scale-extraction.ll` (WIDE).
3. **The scale is taken out of a `trunc`.** `sext(trunc(i * (2^32 + 8)))`
   is 8 for `i = 1`; the extracted form gives `2^32 + 8`.
   **IR.** *Test:* `address-gap-scale-extraction.ll` (TRUNC).
4. **`nneg` is moved to an operand that may be negative.**
   `zext nneg (mul i32 x, 4)` is defined for `x = -1073741823` (the
   multiply wraps to 4), but the extracted `zext nneg x` is poison.
   **IR.** *Test:* `address-gap-nneg-scale.ll`.
5. **An int8 or int16 partial scale hits an assertion.** The partial
   scale of an int8 or int16 multiply is divided by an i64 constant (the
   frontend widens such indices before it multiplies).
   **IR.** *Test:* `address-gap-partial-scale.ll`.

#### Narrowing to 32-bit offsets

**Code.** `lOffsets32BitSafe`, in a form for one operand and one for
offsets and delta. Only with 32-bit addressing, the default on 64-bit
targets.

**What it does.**

* One operand (the base-offset form): accepts offsets that are i32, a
  `sext` of an i32 vector, a constant that fits in i32, or adds and
  disjoint ors of these, which it truncates to i32.
* Two operands (the factored form): accepts variable offsets that are a
  `sext` or a `zext nneg` of an i32 vector, or a constant that fits, and a
  delta that is a constant that fits. A delta that is a plain `zext` of an
  i32 constant that doesn't fit in a signed i32 is rejected; any other
  delta it can't evaluate yet (an expression that later folds) is assumed
  to fit (the `FIXME` there).

**Wrong today.** It assumes the narrowed offsets and delta, sign extended,
are the original values in every lane, and that the lowering's
arithmetic at 32 bits gives the same address as the 64-bit one.

1. **A sum is truncated as if it fitted.** Nested indexing sums two
   signed i32 byte offsets at 64 bits: for rows of 3 floats,
   `a[i][j]` with `i = 178956970` and `j = 2` is `2147483640 + 8 = 2^31`
   bytes from `a`, which the 32-bit offsets read as `-2^31`. The same for
   an int64 sum the program computes itself and converts to a pointer.
   *Test:* `address-gap-nested-sum.ispc`, `address-gap-narrow-sum.ispc`.
2. **An unfolded delta is assumed to fit.** A constant delta of `+2^31`
   that isn't folded yet when the rule runs is truncated to `-2^31`.
   **IR.** *Test:* `address-gap-narrow-constant.ll`.
3. **The address depends on the lowering.** For `offsets = 0x20000000`,
   `scale = 4`, `delta = -4`, the factored gather of `builtins/util.m4`
   computes `2^31 - 4`; the factored builtin of `builtins/generic.ispc`
   multiplies with `nsw` at 32 bits and gets poison.
   **IR.** *Test:* `address-gap-narrow-lowering.ll`.

#### Constant split

**Code.** `lExtractConstantOffset`, guarded by `lSplitIsNUW` under a
`zext`.

**What it does.**

* Splits the offsets into a constant and a variable part, through casts,
  adds, disjoint ors, shifts by constants and multiplies. For a multiply
  `(v0 + c0) * (v1 + c1)` the constant part is `c0 * c1` and the variable
  part the other three products.
* Sums the constants at the offset width, and builds new adds, shifts and
  multiplies for the variable part.
* Under a plain `zext` it splits only if every add, shift and multiply
  below it is `nuw` and no other cast is in between. A `zext nneg` is
  treated like a `sext`.
* A shift of the variable part copies the original shift's flags (`nsw`
  unless `--wrap-signed-int`; `nuw` only if the split is `nuw`).

**Wrong today.** It assumes `ext(v + c) = ext(v) + ext(c)`, where `v` and
`c` are the newly formed parts: that neither part, nor their sum, wraps
at its width.

1. **A split under a `sext` of an add without `nsw`.** With 32-bit
   addressing InstCombine turns the offset of `p[idx - 1]` (a signed
   `idx`) into `sext((idx << 2) - 4)`, and the split gives
   `sext(idx << 2) - 4`. For `idx = 2^29`, `(idx << 2) - 4` fits in 32 bits
   but `idx << 2` doesn't: the address is 4 GiB off.
   *Test:* `address-gap-minus-const.ispc`.
2. **A split under a `zext nneg` of an add that wraps.**
   `add i32 x, INT32_MIN` for `x = INT32_MIN` wraps to 0, so the offset
   is 0, but the split gives `sext(x) + sext(INT32_MIN) = -2^32`.
   **IR.** *Test:* `address-gap-nneg-split.ll`.
3. **The regrouped constants wrap.** Every original add can be `nsw` and
   the sum of the constants still wrap at 32 bits. In
   `p[(INT32_MAX + (programIndex + v)) + (programIndex << 1) + w]` for an
   int32 array with `--addressing=64`, `v = -INT32_MAX` and `w = 0`, the
   index is `3 * programIndex`. LLVM folds the lane constants to
   `<INT32_MAX, INT32_MAX + 3, ...>`, which wrap to
   `<2147483647, -2147483646, ...>`; the split sign extends them, and lane
   1 reads 16 GiB below the array (sse4-i32x4; the hardware gather of
   avx2-i32x8 is right).
   *Test:* `address-gap-split-source.ispc` (`wrapped_constants`).
4. **A copied `nsw` makes a new shift poison.** An int64 index with 32-bit
   addressing is truncated by the frontend, so its adds have no `nsw`,
   while the multiply by the element size has. For `uniform int16 p[]`,
   `p[(int64)INT32_MIN + programIndex + w]` with `w = 2^31 + 40` reads
   `p[40 + programIndex]`. LLVM folds the index to
   `splat(trunc w) + <INT32_MIN, INT32_MIN + 1, ...>`; the split gives
   `shl nsw (trunc w), 1`, which overflows, and the load starts at
   `p + 2 * sext(trunc w)`, 4 GiB below the array (sse4-i32x4; avx2-i32x8
   keeps the base-offset form and is right).
   *Test:* `address-gap-split-source.ispc` (`int64_index`).

### more-const: late constant split

**Code.** `lPlanMoreConst`, emitted by `lEmitBasedForm` (in place).

**What it does.**

* Takes a factored pseudo gather or scatter whose offsets aren't zero:
  after the passes since the call was made, more of them may be constant.
* Splits the offsets with `lExtractConstantOffset` and adds `c * scale` to
  the delta, on the same call, which is then planned again.
* The scale is asserted to be a constant.

**Wrong today.** It assumes the split is exact, and that `c * scale`,
computed at the offset width with the scale sign extended, is the
constant's share of the address.

1. **`c * scale` wraps at 32 bits.** Factored offsets `x + 0x40000000`
   with scale 4 and a zero delta are `2^32` bytes from the base at `x = 0`
   for the factored gather of `builtins/util.m4`; the split makes the delta
   `0x40000000 * 4 = 0` at 32 bits, and the gather then reads at the base.
   **IR.** *Test:* `address-gap-more-const.ll`.
2. **The split is inexact.** `lExtractConstantOffset` is wrong here in the
   same cases as when the call was made: under a `sext` of an add without
   `nsw`, under a `zext nneg` of an add that wraps, when the regrouped
   constants wrap at 32 bits, and when a copied `nsw` makes a new shift
   overflow.
   *Test:* `address-gap-split-source.ispc` (`late_split`: the gather of an inlined function, split after inlining).

### gs-to-load-store: based gather or scatter to a load, a store or a broadcast

**Code.** `lPlanLoadStore`, emitted by `lEmitLoadStore`.

**What it does.** Takes a based pseudo gather or scatter of either form
(its scale is asserted to be a constant) and computes the full offsets at
the offset width: `offsets * scale` (left out for a base-offset scale of
1; the hardware scale zero extended) plus the delta (the factored scale
sign extended). Then:

* **All lanes equal** (`LLVMVectorValuesAllEqual`, which ignores
  undefined lanes): a gather becomes one scalar load at lane 0's offset,
  broadcast to every lane. A scatter on a gang wider than 1 gets a
  warning that every lane writes one location, and is kept.
* **Consecutive, step = element size** (`LLVMVectorIsLinear`): a masked
  load (gather) or a pseudo masked store (scatter) at lane 0's offset.
  When the mask isn't all on and a `zext` in the offsets may wrap
  (`lHasWrappingZExt`), the start is the offset of the first lane that is
  on (lane 0 if none is), minus its distance from lane 0, computed at the
  offset width; here a lane is on if its mask is nonzero.
* **Consecutive, step = minus the element size, mask all on, not Xe**: a
  masked load at the last lane's offset, then the lanes reversed; or the
  value reversed, then a masked store.

On Xe with `--opt=enable-xe-unsafe-masked-load`, the masked load is the
blend load, which reads the whole vector, as the option asks. The module
must declare the masked load or store builtin; hand-written IR that
doesn't is left alone.

**Wrong today.** It assumes the full offsets at the offset width are the
addresses, the lanes that are on are consecutive, the lane the start
comes from has a defined offset, and the broadcast address is readable.

1. **The pointer is rebuilt at 32 bits.** A factored float gather with
   offsets `u = 0x20000000` in every lane and scale 4 has the full offset
   `0x20000000 * 4`, which wraps at 32 bits to `-2^31`: the broadcast
   loads 2 GiB below the base, where the factored gather of
   `builtins/util.m4` reads 2 GiB above it.
   **IR.** *Test:* `address-gap-load-store.ll` (REBUILT).
2. **The start is taken from lane 0 when lane 0 is off.** For a signed
   index whose byte offset in lane 0 doesn't fit in 32 bits while those of
   the lanes that are on do, the load's start is 4 GiB off or poison.
   *Test:* `address-gap-lane0.ispc`.
3. **Lanes that wrap are counted as consecutive.** `LLVMVectorIsLinear`
   accepts a splat plus a linear constant vector without checking that the
   add doesn't wrap. `p8[b + programIndex]` with an unsigned `b = 2^32 - 2`
   wraps from `2^32 - 1` to 0 between two lanes (or from `INT32_MAX` to
   `INT32_MIN` with `--wrap-signed-int`), and one vector load serves all
   lanes. The first-on-lane start doesn't help: it is used only for a
   `zext` it recognizes, and computes `INT32_MIN - 1` at 32 bits, which is
   `INT32_MAX` again.
   *Test:* `address-gap-wrap.ispc`.
4. **A loop PHI is counted as linear for any stride.** `LLVMVectorIsLinear`
   follows a PHI only once and counts a PHI it has seen as linear,
   whatever stride is asked for. Byte offsets in a PHI that starts at
   `<0, 1, 2, 3>` and is multiplied by -1 on each iteration pass as
   consecutive, and the gather becomes a forward load on the iterations
   where the offsets are `<0, -1, -2, -3>`.
   **IR.** *Test:* `address-gap-load-store.ll` (PHI).
5. **The consecutive test overflows in the compiler.** For constant
   64-bit offsets it computes `previous + step` in a C++ `int64_t`
   (offsets `<INT64_MAX, INT64_MIN, ...>`); for an `and` with a splat it
   computes the splat plus one, which is 0 for all ones (an assertion
   follows) and overflows for `INT64_MAX`.
   **IR.** *Test:* `address-gap-load-store.ll` (OVERFLOW), `address-gap-linear-and.ll`.
6. **The first lane that is on is found by another mask test.** The
   first-on-lane start counts a lane as on if its mask is nonzero, the
   mask status counts its sign bit. With byte offsets
   `zext(add(splat(u), <0, 1, 2, 3>))`, `u = UINT_MAX` and the mask
   `<1, -1, 0, 0>`, the start comes from lane 0, and the load of lane 1
   reads `base + 2^32` instead of `base` (the frontend's masks are 0 or -1
   per lane).
   **IR.** *Test:* `address-gap-load-store.ll` (MASKBIT).
7. **The broadcast reads an undefined lane 0.** When lane 0 is off and
   undefined (`LLVMVectorValuesAllEqual` ignores it), the broadcast takes
   its address from that lane. For offsets that are an insert chain without
   lane 0 the lane isn't there, and an assertion of `llvmutil.cpp` fails:
   the compiler crashes.
   **IR.** *Test:* `address-gap-broadcast-undef-lane.ll`.
8. **The broadcast loads when no lane is on.** The frontend guards the
   true and false parts of an `if` and the code after a varying `return`,
   but not the statements after a `case` label that no lane reaches:

   ```ispc
   switch (k + programIndex) {           // k = 1: no lane takes case 0
   case 0:;
       out[programIndex] = *(&a[u + programIndex] - programIndex);   // a[u]
   }
   ```

   This faults on sse4-i32x4 when `a[u]` is unmapped, and so does a
   function that isn't inlined and is called there with every lane off.
   The generated tests also fault this way on avx2-i32x8 and avx2-i8x32,
   with i16, i32, i64, float and double elements.
   *Test:* `address-gap-off-lanes-broadcast.ispc`.

### masked-store: masked store with a known mask

**Code.** `lPlanMaskedStore`, emitted by `lEmitUnmasked`.

**What it does.**

* Takes a masked store of any flavor whose mask is all off (removed) or
  all on (a store aligned to the element, or to the target's vector with
  `--opt=force-aligned-memory`).
* The mask status comes from `GetMaskStatusFromValue`, which counts a lane
  as on if its sign bit is set and reads undefined lanes of a constant
  mask from their known bits, so a mixed mask stays mixed.
* On Xe, a store to global memory whose mask is neither all on nor all
  off gets a warning that it needs a scatter, if the call has a source
  position.

**Wrong today.** It assumes the pointer is aligned for the element (the
manual doesn't say so, but every load and store that the passes build
from a memory call carries the element's alignment), and that every
flavor reads the mask by its sign bit.

1. **The blend stores read the mask by another bit.** They read its low
   bit: an i8 blend store with the mask `<1, 0, 0, 0>` writes lane 0, but
   the rule takes the mask for all off and removes the store; with
   `<-2, -2, -2, -2>` it writes nothing, and the rule turns it into a
   store of every lane (the frontend's masks are 0 or -1 per lane, for
   which the rule is correct).
   **IR.** *Test:* `address-gap-blend-mask.ll`.

### masked-load: masked load with a known mask

**Code.** `lPlanMaskedLoad`, emitted by `lEmitUnmasked`.

**What it does.** Takes a masked load whose mask is all off (the result
is poison) or all on (a load aligned to the element, or to the target's
vector with `--opt=force-aligned-memory`), with the mask status of
`GetMaskStatusFromValue`.

**Wrong today.** Nothing known: correct for the frontend's masks (0 or -1
per lane), assuming the pointer is aligned for the element.

## GatherCoalescePass

The pass plans a group of gathers (`lPlanCoalesce`) and then emits it
(`lEmitCoalesce`), in five steps.

### Grouping

**Code.** `coalesceGathersFactored`.

**What it does.**

* Starts at a factored pseudo gather of i32 or float, with 32- or 64-bit
  offsets, whose mask is all on and whose variable offsets are equal in
  every lane. Gathers of the base-offset form and of other element types
  aren't coalesced.
* Adds up to three later gathers of the same block that call the same
  builtin with the same base, variable offsets, scale and mask values.
* Stops at the first store, atomic read-modify-write or compare-exchange,
  indirect call, or direct call to a function that may write memory (this
  includes `llvm.lifetime.start`).
* Emits the loads at the first gather; a performance warning reports the
  coalesced gathers and loads.

**Wrong today.** It assumes equal operand values mean the same uniform
address part, every delta is a constant vector with the target's lane
count, and a read can move up to the first gather when nothing in
between may write.

1. **A delta that isn't a constant vector hits an assertion.** A delta
   that is neither a constant data vector nor zero (an instruction, or a
   `ConstantVector`) reaches an assertion when the slots are computed,
   even for one gather.
   **IR.** *Test:* `address-gap-coalesce-delta.ll`.
2. **A call with more lanes than the target's overruns a buffer.** Its
   lane offsets would be written past their part of the buffer when the
   slots are computed; today the group is planned with the target's vector
   types first and fails with "Unexpected gather type in lPlanCoalesce".
   **IR.** *Test:* `address-gap-coalesce-lanes.ll`.
3. **Reads are moved across fences and calls that may not return.** Only
   an instruction that may write memory stops the group. A `fence`
   instruction does, but `memory_barrier()` calls a builtin declared
   `readnone`, so from source the reads after a barrier are made before it,
   and another thread's stores that it orders before them may not be seen.
   A read-only or `readnone` call that may not return or may unwind doesn't
   stop the group either, although the read may not be safe before it
   (IR).
   *Test:* `address-gap-coalesce-barrier.ispc`, `address-gap-coalesce.ll` (CALL).

### Slots

**Code.** `lExtractConstOffsets`.

**What it does.** Reads each lane's delta as a 64-bit integer (an i32
delta zero extended by `LLVMExtractVectorInts`) and divides it by the
element size; the quotient is the lane's slot from the shared pointer.

**Wrong today.** It assumes the deltas are non-negative multiples of the
element size from an element-aligned pointer.

1. **Absolute byte offsets are divided instead of distances.** With a
   base that is 1 modulo 4 and deltas 3, 11, 19, 27, the slots are 0, 2,
   4, 6, and the loads start at the base, 3 bytes early, with an alignment
   of 4 that the pointer doesn't have.
   **IR.** *Test:* `address-gap-coalesce.ll` (SLOTS).
2. **A negative i32 delta is read zero extended**, about 4 GiB away. From
   source, with default options:

   ```ispc
   foreach (fi = 0 ... programCount) {
       int32 i = w - 8 * fi;
       out[fi] = p[i] + p[i + 1] + p[i + 2] + p[i + 3];
   }
   ```

   The four gathers are coalesced with the deltas `-32 * j` bytes, read as
   the slots `0, 1073741816, 1073741808, ...`, and the loads are at
   `p + 4 * w + 4294967200` and beyond: a fault on sse2-i32x4, sse4-i32x4
   and avx1-i32x8, right with `--opt=disable-coalescing` and on
   avx2-i32x8.
   *Test:* `3984.ispc` (the slots in IR).

### Load selection

**Code.** `lSelectLoads` and `lVectorLoadIsEfficient`.

**What it does.**

* Over the sorted distinct slots, from the lowest, tries loads of 8, 4
  and 2 elements, then a scalar load.
* A load is taken if every following slot it covers is at most 3
  elements after the previous one, and either the load ends exactly at a
  slot, or the next slot lies beyond the load and the last covered slot
  is past the middle of the load (not for 2 elements). The page test there
  is always true after the gap test.
* A load never reaches past the last slot it covers or the next one, so
  the extra elements it reads always lie between two requested elements.

**Wrong today.** Nothing known: correct where the slots and the shared
pointer are, assuming the requested elements around each gap are in one
object.

### Shared pointer and loads

**Code.** `lComputeBasePtr` and `lEmitLoads`.

**What it does.**

* The shared pointer is the base plus lane 0 of the variable offsets
  times the scale, multiplied at the offset width (the scale sign extended
  for 64-bit offsets).
* The loads are i32, i64 (split into two i32), 4 x i32 or 8 x i32,
  aligned to 4; with `--opt=force-aligned-memory` the 4- and 8-element
  loads are aligned to the target's vector.

**Wrong today.**

1. **The product is taken at the offset width.** Variable offsets
   `u = 0x20000000` with scale 4 are `2^31` bytes from the base for the
   factored gather of `builtins/util.m4`, but `-2^31` for the shared
   pointer, so the loads are 4 GiB off (with deltas that aren't
   consecutive, so that the gathers reach the pass as gathers).
   **IR.** *Test:* `address-gap-coalesce.ll` (PRODUCT).

### Assembly

**Code.** `lSplit8WideLoads` and `lAssembleResultVectors`.

**What it does.** Splits 8-element loads into two 4-element halves,
builds each gather's result 4 lanes at a time from the loads with inserts
and shuffles, and concatenates the 4-lane chunks. Widths other than 4, 8,
16, 32 and 64 are a fatal error.

**Wrong today.**

1. **The results are assembled wrongly at 32 and 64 lanes.** The chunks
   of every gather after the first come from the wrong place: the second
   gather of a group gets chunks 4 to 11 instead of 8 to 15 at 32 lanes,
   and 4 to 19 instead of 16 to 31 at 64 lanes. Such groups appear with
   `--opt=disable-gathers`, for example on avx2-i8x32:
   `p[2 * i + w] + p[2 * i + w + 1]` in a `foreach` reads the sums of the
   wrong elements there, and is right on avx2-i16x16. The generated tests
   hit it for every lane stride from 2 to 8 and 2 to 4 neighbouring reads,
   with int32 and int64 indices.
   *Test:* `address-gap-coalesce-wide.ispc`.

## Lowerings that touch memory beyond the lanes that are on

`ReplacePseudoMemoryOps` lowers what the two passes leave. Two of its
lowerings read or write memory that the lanes that are on don't access.

### Emulated gathers read the base for the lanes that are off

**Code.** The factored gathers of `builtins/util.m4` (x86, and NEON
through `target-neon-common.ll`).

**What it does.** Sets the offset and delta of the lanes that are off to
0, so it loads from the base for those lanes. These gathers are the
lowering of every gather on SSE2, SSE4, AVX1 and NEON, and of the i8, i16
and half gathers on AVX2 and AVX-512.

**Wrong today.** The base is not an address the program reads, so it may
not be readable; it is the program's first element only when it is a
parameter of the function the application calls, at offset 0.

1. **The lanes that are off read the base.** In

   ```ispc
   uniform int8 *uniform q = p + n;      // one past the end of p[0 .. n-1]
   if (programIndex < m)
       r = q[idx[programIndex]];         // idx = <-8, -7, ...>, m = 2
   ```

   lanes 0 and 1 read `p[n - 8]` and `p[n - 7]`, and the lanes that are off
   read `q[0] = p[n]`. With `p` ending at an unmapped page this faults on
   sse2-i32x4, sse4-i32x4, avx1-i32x8, avx2-i32x8, avx2-i8x32, avx2-i16x16
   and avx512skx-x16.
   *Test:* `address-gap-off-lanes-base.ispc`.

### Masked stores to stack objects become blends

**Code.** `lGetUnderlyingVectorType` in `src/opt/ReplacePseudoMemoryOps.cpp`.

**What it does.** Lowers a pseudo masked store whose pointer is rooted in
a stack object of vector type (a `varying` array, or a struct with a
`varying` member) to a load of the whole vector, a blend with the stored
value, and a store of the whole vector.

**Wrong today.** The check looks at the vector type of the object, not at
where the store is in it or who else can access it.

1. **The vector reaches past the stack object.** A constant offset into
   an array, or any variable offset, is accepted. In

   ```ispc
   varying int a[2];
   a[0] = 1;
   a[1] = 2;
   uniform int *uniform q = (uniform int *)&a[0];
   if (programIndex < programCount - 1)
       q[programCount + 1 + programIndex] = 100 + programIndex;
   out[programIndex] = a[1];
   ```

   the blend stores the vector at `&a[0] + 4 * (programCount + 1)`, 4 bytes
   past `a`. SROA treats that access as undefined and deletes it, so every
   lane reads 2 instead of `100 + i - 1` at `-O1` and `-O2` on sse2-i32x4,
   sse4-i32x4, avx1-i32x8, avx2-i32x8, avx2-i32x16, avx2-i64x4,
   avx2-i16x16 and avx2-i8x32; `-O0` is right.
   *Test:* `address-gap-blend-store.ispc`.
2. **The object is shared with a running task.** After
   `launch writer((uniform int *)&a, n)`, where the task writes the lanes
   `n` and up of `varying int a`, the caller's `if (programIndex < n) a = 1;`
   becomes a load of `a`, a select and a store of the whole vector before
   `sync`, which can undo the task's stores (avx2-i32x8).
   *Test:* `address-gap-blend-store-task.ispc`.

## Wrong code in the frontend

These address computations are wrong before the two passes run.

1. **A `varying bool` array is indexed by the register size.** It is
   stored with one byte per lane, but a varying index into it is scaled by
   the size of the mask register type (16 bytes on sse4-i32x4, 32 on
   avx2-i32x8), so every lane reads the wrong element; avx2-i8x32, whose
   mask is one byte per lane, is right.
   *Test:* `varying-bool-array-index.ispc`.
2. **A struct member offset of 2 GiB is sign extended.** With 32-bit
   addressing, the byte offset `2^31` of a member read through a varying
   pointer becomes `-2^31`, so the gather reads `p - 2 GiB` instead of
   `p + 2 GiB`. Through a uniform pointer, and with 64-bit addressing, it is
   right.
   *Test:* `address-gap-struct-2gb.ispc`.
3. **`new` of 2^31 or more elements sign extends the count.** With 32-bit
   addressing, `uniform new uniform int8[n]` sign extends an unsigned
   32-bit `n`, asks `posix_memalign` for about 2^64 bytes, and uses the
   pointer without checking the result. With 64-bit addressing `n` is zero
   extended.
   *Test:* `address-gap-new-2gb.ispc`.
