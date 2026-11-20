# The rewrites of ImproveMemoryOps and GatherCoalescePass

This document lists every rewrite that `ImproveMemoryOps`
(`src/opt/ImproveMemoryOps.cpp`) and `GatherCoalescePass`
(`src/opt/GatherCoalescePass.cpp`) make: what each one matches, what it
emits, and where it gets the address or the memory it touches wrong
today.

An example marked "IR" is a counterexample for the IR the pass sees; it
may not be reachable from ISPC source today. Unmarked examples come from
ISPC source.

## Memory calls and their lowerings

The frontend emits every varying memory access as a call to a pseudo
builtin. `ImproveMemoryOps` rewrites these calls into cheaper forms, and
`ReplacePseudoMemoryOps` later lowers what is left to target builtins.
The forms of a gather (scatters are the same, with a stored value):

| Form | Operands | Address of lane j |
| --- | --- | --- |
| general | addresses, mask | `addr[j]` |
| base-offset | base, scale, offsets, mask | `base + offsets[j] * scale` |
| factored | base, offsets, scale, delta, mask | `base + offsets[j] * scale + delta[j]` |
| masked load or store | pointer, mask | `ptr + j * element size` |

The offsets and the delta are 32- or 64-bit vectors; the scale is an i32.
The base-offset form is meant for a hardware gather or scatter; the
factored form is the emulated one. The address column is what the forms
are meant to compute. It doesn't say at which width the products and sums
are taken, or whether the scale is signed, and the lowerings don't agree:

| Lowering | Computes |
| --- | --- |
| hardware gather (base-offset form) | `base + sext(offsets) * scale`, at pointer width |
| x86 factored gathers and scatters (`__gather_factored_base_offsets32_*`, `__gather_factored_base_offsets64_*` and the scatters of `gen_scatter_factored` in `builtins/util.m4`) | `base + sext(offsets) * sext(scale) + sext(delta)`, at pointer width |
| x86 base-offset gathers for the element types without a gather instruction, such as i8, i16 and half on AVX2 (`__gather_base_offsets32_*` and `__gather_base_offsets64_*` in `builtins/util.m4`) | 32-bit offsets: `offsets * scale` at 32 bits, then the factored gather. 64-bit offsets: `offsets * zext(scale)` at 64 bits |
| `builtins/generic.ispc`, base-offset form (`__gather_base_offsets32_*`) | `offsets * scale` at the offset width |
| `builtins/generic.ispc`, factored form (`__gather_factored_base_offsets32_*`) | `offsets * scale + delta` at the offset width, with `nsw` (the ISPC arithmetic of the builtin) |

They give the same address when every product and sum fits in a signed
integer of the offset width and the scale is positive. So the same call
can mean two addresses: an emulated i8 base-offset gather with offsets
`0x40000000 + j` and scale 4 reads `base + 4 * j`, a hardware gather with
the same operands `base + 2^32 + 4 * j`. Several rewrites depend on the
products and sums fitting without checking it.

The x86 factored gathers set the offset and delta of the lanes that are
off to 0, so they load from the base for those lanes.

A lane is on when the mask enables it. A rewrite may read only what the
program reads in the lanes that are on; reading anything else is correct
only if the memory is known to be readable and the value is discarded.
LLVM allows such a read to race with another thread's write: the racing
load returns an undefined value, which is discarded.

## How the passes run

`ImproveMemoryOps` runs at pipeline stage 100 at `-O0`, always. At higher
levels it runs at stages 211, 256 and 271, unless the gang width is 1 or
`--opt=disable-gather-scatter-optimizations` is given.
`GatherCoalescePass` runs at stage 261 under the same two conditions, and
not with `--opt=disable-coalescing`.

`ImproveMemoryOps` asks its rules for a plan in a fixed order
(`lPlanAccess`): `gs-to-based`, `more-const`, `gs-to-load-store`,
`masked-store`, `masked-load`. It emits the first plan (`lEmitPlan`). If
the result is a memory call (a new call, or the same call with new
operands), the rules are asked again for it in the same run. So one
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

The rule names are the ones `--dump-memory-plans` prints, in the lines of
`ImproveMemoryOps`, and `coalesce` in those of `GatherCoalesce`. A call no
rule rewrites is printed as `keep`, except the calls in the bodies of the
memory builtins; the coalescer prints `keep` for the pseudo gathers it
looks at and doesn't coalesce, with the reason: `mask`,
`varying offsets`, `element type` or `not a factored gather`.

A rule doesn't record why it matched. It checks its own assumptions, in
its own way, while it matches.

## ImproveMemoryOps

### gs-to-based: general gather or scatter to a based form

`lPlanBasedForm`, emitted by `lEmitBasedForm`.

**Does.** Takes a general pseudo gather or scatter of the gang width. A
prefetch is kept; it is lowered later, to scalar prefetches on CPU
targets. The module must declare the replacement builtins (for a 64-bit
general call with 32-bit addressing, those of both widths); hand-written
IR that doesn't is left alone.

The call becomes:

* the base-offset form when the target uses its gather (scatter)
  instruction (`useGather()`, `useScatter()`, one answer for all element
  types, so an element type without a gather instruction gets the
  base-offset form and its emulated lowering). The rule recovers the
  base, extracts a scale from the offsets, then narrows them to 32 bits;
* the factored form otherwise. The rule recovers the base, splits off a
  constant delta, extracts a scale from the variable part, then narrows
  both parts.

These steps follow.

#### Base recovery

`RecoverBaseOffsets` (`src/opt/MemoryAccess.cpp`).

**Does.** Splits the address vector into a uniform scalar and per-lane
offsets. It finds the scalar as a broadcast (an insert chain, a shuffle
or a splat, with the residue of the broadcast vector's lane 0 kept in the
offsets), possibly behind vector casts, as a term of adds and disjoint
ors with the other terms, through a one-index `extractvalue` of an insert
chain, or as the global of a constant vector of a global's address plus
displacements. An integer loaded from memory is taken for a pointer. It
gives up on calls whose lanes have different bases, and after a fixed
budget of visited values. The scalar becomes the base through
`inttoptr`.

**Wrong today.** It assumes the scalar is the pointer of the object every
lane accesses, and nothing checks that the lanes stay inside that
object. If the varying part carries a pointer, as in an address computed
in integers as `(int64)p + ((int64)q - (int64)p)` with a uniform pointer
`p` and a varying pointer `q`, and the terms survive to this pass, the
base is `p` while the lanes access `q`'s object (IR: LLVM usually folds
the expression to `q` first).

#### Scale extraction

`lExtractOffsetVector248Scale`, with `lExtract248Scale`,
`lExtractNUWScale` and `lIsIntegerSplat`.

**Does.** Looks for a multiply by a constant splat in the offsets,
through casts and through adds (and disjoint ors) whose two sides have
the same scale; an add of zero is skipped. A splat of 2, 4 or 8 becomes
the scale. Another splat that is a multiple of 8, 4 or 2 is divided by
the largest of these, which becomes the scale (`24 * i` gives scale 8
and offsets `3 * i`). Shifts are not looked at, except under a plain
`zext`, where only a `nuw` multiply or shift by 2, 4 or 8 directly below
it is taken. A `zext nneg` is treated like a `sext`, and the new cast of
the multiplier's operand gets the `nneg` flag.

**Wrong today.** It assumes that moving the scale out of the cast keeps
the value, `ext(x * s) = ext(x) * s`, and that the multiplier's operand
is non-negative where the product was.

* A scale taken from a multiply that wraps at 32 bits: with
  `--wrap-signed-int` or an unsigned index, `sext((2^30 + j) * 4)` is
  `4 * j`, while the extracted form `sext(2^30 + j) * 4` is `2^32 + 4 * j`
  (IR).
* The splat is read through an `int`: `a[i * 4294967298]` with an int64
  `i` and `--addressing=64` on `avx2-i32x8` becomes a hardware gather of
  `a + 8 * i`, not `a + 4 * 4294967298 * i`.
* The scale is taken out of a `trunc`: `sext(trunc(i * (2^32 + 8)))` is 8
  for `i = 1`, but the extracted form gives `2^32 + 8` (IR).
* `nneg` is moved to an operand that may be negative:
  `zext nneg (mul i32 x, 2)` is 0 for `x = INT_MIN` (the multiply
  wraps), while the extracted `zext nneg x` is poison (IR).
* A partial scale of an int8 or int16 multiply is divided by an i64
  constant and hits an LLVM assertion (IR; the frontend widens such
  indices before it multiplies).

#### Narrowing to 32-bit offsets

`lOffsets32BitSafe`, in a form for one operand and one for offsets and
delta. Only with 32-bit addressing, the default on 64-bit targets.

**Does.** For one operand (the base-offset form), accepts offsets that
are i32, a `sext` of an i32 vector, a constant that fits in i32, or adds
and disjoint ors of these, which it truncates to i32. For two operands
(the factored form), accepts variable offsets that are a `sext` or a
`zext nneg` of an i32 vector, or a constant that fits, and a delta that
is a constant that fits. A delta that is a plain `zext` of an i32
constant that doesn't fit in a signed i32 is rejected; any other delta
it can't evaluate yet (an expression that later folds) is assumed to fit
(the `FIXME` there).

**Wrong today.** It assumes the narrowed offsets and delta, sign
extended, are the original values in every lane, and that the lowering's
arithmetic at 32 bits gives the same address as the 64-bit one.

* A sum of two sign-extended i32 values is truncated as if it fitted:
  `sext(x) + sext(y)` with `x = y = 2^30` becomes `2^31` truncated, which
  is `-2^31` (IR).
* With the offsets narrowed, the address depends on the lowering: for
  `offsets = 0x20000000`, `scale = 4`, `delta = -4`, the x86 factored
  gather computes `2^31 - 4`, the generic factored builtin multiplies
  with `nsw` at 32 bits and gets poison (IR).

#### Constant split

`lExtractConstantOffset`, guarded by `lSplitIsNUW` under a `zext`.

**Does.** Splits the offsets into a constant and a variable part,
through casts, adds, disjoint ors, shifts by constants and multiplies.
For a multiply `(v0 + c0) * (v1 + c1)` the constant part is `c0 * c1`
and the variable part the other three products. It sums the constants at
the offset width and builds new adds, shifts and multiplies for the
variable part. Under a plain `zext` it splits only if every add, shift
and multiply below it is `nuw` and no other cast is in between. A
`zext nneg` is treated like a `sext`. A shift of the variable part copies
the original shift's flags (`nsw` unless `--wrap-signed-int`; `nuw` only
if the split is `nuw`).

**Wrong today.** It assumes `ext(v + c) = ext(v) + ext(c)`, where `v`
and `c` are the newly formed parts: that neither part, nor their sum,
wraps at its width.

* A split under a `sext` of an add that isn't `nsw`: a regrouped index
  `(x + y) + 0x80000000` becomes `sext(x + y) + sext(0x80000000)`, 4 GiB
  off.
* Regrouped constants wrap even where every original add is `nsw`:
  `sext((x + INT_MAX) + 1)` with `x = -INT_MAX` is 1, but the split sums
  the constants to `INT_MAX + 1 = INT_MIN` at 32 bits and gives
  `sext(x) + sext(INT_MIN) = 1 - 2^32` (IR).
* The copied `nsw` makes the new shift poison: in
  `shl nsw (add nsw x, INT_MAX), 1` with `x = -INT_MAX`, the original is
  0, while the new `shl nsw x, 1` overflows (IR).

### more-const: late constant split

`lPlanMoreConst`, emitted by `lEmitBasedForm` (in place).

**Does.** Takes a factored pseudo gather or scatter whose offsets aren't
zero: after the passes since the call was made, more of them may be
constant. Splits the offsets with `lExtractConstantOffset` and adds
`c * scale` to the delta, on the same call, which is then planned again.
The scale is asserted to be a constant.

**Wrong today.** It assumes the split is exact, and that `c * scale`,
computed at the offset width with the scale sign extended, is the
constant's share of the address.

* `c * scale` wraps at 32 bits: factored offsets `x + 0x40000000` with
  scale 4 and a zero delta are `2^32` bytes from the base at `x = 0` for
  the x86 factored gather; the split makes the delta
  `0x40000000 * 4 = 0` at 32 bits, and the gather then reads at the base
  (IR).
* The split itself: under a `sext` of an add that isn't `nsw`, with
  constants that wrap when they are summed at 32 bits, and with `nsw`
  copied to a new shift that overflows.

### gs-to-load-store: based gather or scatter to a load, a store or a broadcast

`lPlanLoadStore`, emitted by `lEmitLoadStore`.

**Does.** Takes a based pseudo gather or scatter of either form (its
scale is asserted to be a constant) and computes the full offsets at the
offset width: `offsets * scale` (left out for a base-offset scale of 1;
the hardware scale zero extended) plus the delta (the factored scale sign
extended). Then:

* **All lanes equal** (`LLVMVectorValuesAllEqual`, which ignores
  undefined lanes). A gather becomes one scalar load at lane 0's offset,
  broadcast to every lane. A scatter on a gang wider than 1 gets a
  warning that every lane writes one location, and is kept.
* **Consecutive, step = element size** (`LLVMVectorIsLinear`). A masked
  load (gather) or a pseudo masked store (scatter) at lane 0's offset.
  When the mask isn't all on and a `zext` in the offsets may wrap
  (`lHasWrappingZExt`), the start is the offset of the first lane that is
  on (lane 0 if none is), minus its distance from lane 0, computed at the
  offset width; here a lane is on if its mask is nonzero. On Xe with
  `--opt=enable-xe-unsafe-masked-load`, the masked load is the blend
  load, which reads the whole vector, as the option asks.
* **Consecutive, step = minus the element size, mask all on, not Xe.** A
  masked load at the last lane's offset, then the lanes reversed; or the
  value reversed, then a masked store.

The module must declare the masked load or store builtin; hand-written
IR that doesn't is left alone.

**Wrong today.** It assumes the full offsets at the offset width are the
addresses, the lanes that are on are consecutive, the lane the start
comes from has a defined offset, and the broadcast address is readable.

* The pointer is rebuilt at 32 bits: a factored float gather with offsets
  `u = 0x20000000` in every lane and scale 4 has the full offset
  `0x20000000 * 4`, which wraps at 32 bits to `-2^31`, so the broadcast
  loads 2 GiB below the base, where the x86 factored gather reads 2 GiB
  above it (IR).
* Lanes that wrap are counted as consecutive. `LLVMVectorIsLinear`
  accepts a splat plus a linear constant vector without checking that the
  add doesn't wrap (for a constant vector it compares the sign-extended
  lanes, which catches the wrap). For a byte gather with offsets
  `add(splat(u), <0, 1, 2, 3>)` at i32, without `nsw` (`--wrap-signed-int`),
  `u = INT_MAX` and lanes 1 to 3 on, the offsets are
  `<INT_MAX, INT_MIN, INT_MIN + 1, INT_MIN + 2>`; the lanes that are on
  read below the base, the load starts at lane 0's `base + INT_MAX`, 4 GiB
  too high (IR). The first-on-lane start doesn't help: it is only used
  for a `zext` it recognizes, and it computes `INT_MIN - 1` at 32 bits,
  which is `INT_MAX` again.
* `LLVMVectorIsLinear` follows a loop PHI only once: a PHI it has seen
  counts as linear, whatever stride is asked for. Byte offsets in a PHI
  that starts at `<0, 1, 2, 3>` and is multiplied by -1 on each
  iteration pass as consecutive, and the gather becomes a forward load on
  the iterations where the offsets are `<0, -1, -2, -3>` (IR).
* The arithmetic of the consecutive test overflows in the compiler: for
  constant 64-bit offsets it computes `previous + step` in a C++ `int64_t`
  (offsets `<INT64_MAX, INT64_MIN, ...>`), and for an `and` with a splat
  it computes the splat plus one, which is 0 for all ones (an assertion
  follows) and overflows for `INT64_MAX` (IR).
* The first-on-lane start counts a lane as on if its mask is nonzero,
  while the mask status counts its sign bit: with byte offsets
  `zext(add(splat(u), <0, 1, 2, 3>))`, `u = UINT_MAX` and the mask
  `<1, -1, 0, 0>`, the start comes from lane 0, and the load of lane 1
  reads `base + 2^32` instead of `base` (IR; the frontend's masks are 0
  or -1 per lane).
* The broadcast loads lane 0's offset even when lane 0 is off: if that
  lane is undefined (`LLVMVectorValuesAllEqual` ignores it), the load's
  address is poison (IR).
* The broadcast loads even when no lane is on, for example in code the
  frontend doesn't guard with a mask test, and then reads an address no
  lane requested.

### masked-store: masked store with a known mask

`lPlanMaskedStore`, emitted by `lEmitUnmasked`.

**Does.** Takes a masked store of any flavor whose mask is all off
(removed) or all on (a store aligned to the element, or to the target's
vector with `--opt=force-aligned-memory`). The mask status comes from
`GetMaskStatusFromValue`, which counts a lane as on if its sign bit is
set and reads undefined lanes of a constant mask from their known bits,
so a mixed mask stays mixed. On Xe, a store to global memory whose mask
is neither all on nor all off gets a warning that it needs a scatter, if
the call has a source position.

**Wrong today.** It assumes the pointer is aligned for the element (the
manual doesn't say so, but every load and store that the passes build
from a memory call carries the element's alignment), and that every
flavor reads the mask by its sign bit. The blend stores read it by its
low bit: an i8 blend store with the mask `<1, 0, 0, 0>` writes lane 0,
but the rule takes the mask for all off and removes the store; with
`<-2, -2, -2, -2>` it writes nothing, and the rule turns it into a store
of every lane (IR; the frontend's masks are 0 or -1 per lane, for which
the rule is correct).

### masked-load: masked load with a known mask

`lPlanMaskedLoad`, emitted by `lEmitUnmasked`.

**Does.** Takes a masked load whose mask is all off (the result is
poison) or all on (a load aligned to the element, or to the target's
vector with `--opt=force-aligned-memory`), with the mask status of
`GetMaskStatusFromValue`.

**Wrong today.** Correct for the frontend's masks (0 or -1 per lane),
assuming the pointer is aligned for the element.

## GatherCoalescePass

The pass plans a group of gathers (`lPlanCoalesce`) and then emits it
(`lEmitCoalesce`).

### Grouping

`coalesceGathersFactored`.

**Does.** Starts at a factored pseudo gather of i32 or float, with 32- or
64-bit offsets, whose mask is all on and whose variable offsets are equal
in every lane. Adds up to three later gathers of the same block that call
the same builtin with the same base, variable offsets, scale and mask
values. Stops at the first store, atomic read-modify-write or
compare-exchange, indirect call, or direct call to a function that may
write memory (this includes `llvm.lifetime.start`). Gathers of the
base-offset form and of other element types aren't coalesced. The loads
are emitted at the first gather; a performance warning reports the
coalesced gathers and loads.

**Wrong today.** It assumes equal operand values mean the same uniform
address part, every delta is a constant vector with the target's lane
count, and a read can move up to the first gather when nothing in
between may write.

* A delta that is neither a constant data vector nor zero (an
  instruction, or a `ConstantVector`) reaches an assertion when the slots
  are computed, even for one gather (IR).
* A call with more lanes than the target's writes past its part of the
  lane offsets when the slots are computed (IR).
* Reads are moved across instructions that don't write but still order
  reads or decide whether the read happens: a fence or an atomic load
  with acquire ordering, after which another thread's stores must be
  seen (a fence comes from `memory_barrier()`, which calls a builtin
  declared `readnone`); a read-only or `readnone` call that may not
  return or may unwind, before which the read may not be safe (IR).

### Slots

`lExtractConstOffsets`.

**Does.** Reads each lane's delta as a 64-bit integer (an i32 delta zero
extended by `LLVMExtractVectorInts`) and divides it by the element size;
the quotient is the lane's slot from the shared pointer.

**Wrong today.** It assumes the deltas are non-negative multiples of the
element size from an element-aligned pointer.

* The absolute byte offsets are divided, not their distances: with a base
  that is 1 modulo 4 and deltas 3, 11, 19, 27, the slots are 0, 2, 4, 6,
  and the loads start at the base, 3 bytes early, with an alignment of 4
  that the pointer doesn't have (IR).
* A negative i32 delta is read zero extended, about 4 GiB away.

### Load selection

`lSelectLoads` and `lVectorLoadIsEfficient`.

**Does.** Over the sorted distinct slots, from the lowest, tries loads of
8, 4 and 2 elements. A load is taken if every following slot it covers is
at most 3 elements after the previous one, and either the load ends
exactly at a slot, or the next slot lies beyond the load and the last
covered slot is past the middle of the load (not for 2 elements). The
page test there is always true after the gap test. Otherwise a scalar
load. A load never reaches past the last slot it covers or the next one,
so the extra elements it reads always lie between two requested
elements.

**Wrong today.** Correct where the slots and the shared pointer are,
assuming the requested elements around each gap are in one object.

### Shared pointer and loads

`lComputeBasePtr` and `lEmitLoads`.

**Does.** The shared pointer is the base plus lane 0 of the variable
offsets times the scale, multiplied at the offset width (the scale sign
extended for 64-bit offsets). The loads are i32, i64 (split into two
i32), 4 x i32 or 8 x i32, aligned to 4; with `--opt=force-aligned-memory`
the 4- and 8-element loads are aligned to the target's vector.

**Wrong today.** The product at the offset width: variable offsets
`u = 0x20000000` with scale 4 are `2^31` bytes from the base for the x86
factored gather, but `-2^31` for the shared pointer, so the loads are 4
GiB off (IR, with deltas that aren't consecutive, so that the gathers
reach this pass as gathers).

### Assembly

`lSplit8WideLoads` and `lAssembleResultVectors`.

**Does.** Splits 8-element loads into two 4-element halves. Builds each
gather's result 4 lanes at a time from the loads, with inserts and
shuffles, and concatenates the 4-lane chunks. Widths other than 4, 8, 16,
32 and 64 are a fatal error.

**Wrong today.** At 32 and 64 lanes the chunks of every gather after the
first come from the wrong place: the second gather of a group gets chunks
4 to 11 instead of 8 to 15 at 32 lanes, and 4 to 19 instead of 16 to 31
at 64 lanes. Width 32 and 64 groups appear with `--opt=disable-gathers`,
for example on `avx2-i8x32`.
