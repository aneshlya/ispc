# Root Cause Found: Sign-Extension vs Zero-Extension Mismatch

## Critical Discovery

Comparing IR between:
- **`ispc-fixed`** (with 65270a8f3 reverted - test PASSES)
- **`ispc`** (with our 2 fixes - test FAILS)

Found **879 sign-extensions changed to zero-extensions** in SharedStructuredVolume.ispc!

## The Key Difference

### Working Version (ispc-fixed):
```llvm
%add_mul_offsets_load_scale_load_broadcast7_delta_load_to_uint64.i =
    sext <4 x i32> %voxelIndex_calculation to <4 x i64>
```

### Broken Version (ispc - our fixes):
```llvm
%offset_cast.i.i =
    zext <4 x i32> %voxelIndex_calculation to <4 x i64>
```

## What's Happening

### The 3D Voxel Index Calculation

The calculation is for 3D array indexing:
```ispc
const uint64 index64 =
    (uint64)localCoordinates.x +
    self->dimensions.x * ((uint64)localCoordinates.y +
         self->dimensions.y * ((uint64)localCoordinates.z));
```

This produces an i32 result that needs to be extended to i64 for addressing.

### The Problem

1. **Before commit 65270a8f3:**
   - Code accepted ZExt (unsigned)
   - Stripped the ZExt
   - Used the i32 value with 32-bit gather
   - 32-bit gather **sign-extends** to i64
   - **Bug hidden**: Small positive values accidentally worked

2. **After commit 65270a8f3 (without our fixes):**
   - Code rejects ZExt (correctly!)
   - Falls back to some other path that uses sext
   - Result: Sign-extended offsets
   - **Test PASSES** (but for wrong reasons - relies on sext behavior)

3. **After commit 65270a8f3 WITH our fixes:**
   - Code rejects ZExt (correctly!)
   - Our Fix #1 adds Mul/Shl support
   - Our Fix #2 prevents unconditional truncation
   - Optimization decides to use zext (zero-extension)
   - **Result: Offsets are now correctly zero-extended**
   - **But**: Test FAILS because the code was designed to work with sign-extended offsets!

## The Real Bug

The real bug is **semantic confusion about signedness in the codebase**:

1. **ISPC source uses `uint64`** - clearly unsigned:
   ```ispc
   const uint64 index64 = ...
   ```

2. **But the working code relies on sign-extension** (`sext`)

3. **Array indices should be unsigned** - they can't be negative!

4. **But 32-bit gather intrinsics sign-extend** (builtins/util.m4:7205):
   ```llvm
   %offset64 = sext i32 %offset32 to i64
   ```

## Why This Causes Wrong Sample Values

When voxel indices are calculated:

### With sext (working by accident):
```
i32 voxelIndex = 0x00000100  (256)
sext to i64    = 0x0000000000000100  (256) ✓ Correct!
```

But for larger indices:
```
i32 voxelIndex = 0x80000000  (2147483648 as unsigned, -2147483648 as signed)
sext to i64    = 0xFFFFFFFF80000000  (-2147483648) ✗ WRONG!
```

### With zext (correct extension, but incompatible):
```
i32 voxelIndex = 0x80000000  (2147483648)
zext to i64    = 0x0000000080000000  (2147483648) ✓ Correct!
```

## The OpenVKL Test Failure Explained

The hit_iterator test:
- Samples a structured regular volume
- Uses 3D voxel indexing with the formula above
- The bisection algorithm samples at different positions
- With **sext**: Happens to read correct voxels (by accident for small volumes)
- With **zext**: Reads different voxels (correct addressing, but code expects sext behavior)
- **Result**: Wrong sample values → wrong `t` values → test fails

Expected `t`: 1.1000000238
Actual `t`: 1.11023
**Root cause**: Sampling from wrong voxels due to addressing mode mismatch

## Why The IR Changed So Much

- **879 `sext` operations removed**
- **2528 `zext` operations added**

This massive change indicates:
1. Commit 65270a8f3 correctly rejected unsigned (ZExt) offsets
2. This forced a different code path
3. The old path somehow ended up using sext
4. Our fixes make the optimizer choose the "correct" zext path
5. But this exposes that the code was designed around sext behavior!

## The Deeper Issue

There are **three incompatible requirements**:

1. **ISPC semantics**: Array indices are unsigned (uint64)
2. **32-bit gather intrinsics**: Sign-extend offsets (sext i32 to i64)
3. **Our fixes**: Correctly preserve unsigned semantics (zext)

These three things can't all be true simultaneously!

## Why Reverting 65270a8f3 "Fixes" It

Reverting allows:
- ZExt offsets to be accepted
- Some path converts them to use sext
- Code works by accident
- But it's semantically wrong - unsigned values shouldn't be sign-extended

## The Solution Options

### Option 1: Accept That Unsigned Indices Need Sign-Extension (Pragmatic)
- Recognize that when using 32-bit addressing with sign-extending gathers
- Unsigned indices must be sign-extended too for compatibility
- This means treating array indices as signed i32, not unsigned
- **Problem**: Semantically weird, limits to 2GB arrays

### Option 2: Always Use 64-Bit for Unsigned (Conservative)
- When indices are unsigned (uint64 in source), always use 64-bit addressing
- Never try to optimize to 32-bit
- **Problem**: Performance hit, but semantically correct

### Option 3: Create Unsigned 32-Bit Gather Intrinsics (Correct but Complex)
- Add new builtins that zero-extend instead of sign-extend
- Modify ImproveMemoryOps to choose based on signedness
- **Problem**: Requires LLVM builtin changes, complex

### Option 4: Fix The Frontend (Possible but Risky)
- Make the frontend track signedness properly
- Generate appropriate extensions based on actual type
- **Problem**: Large change, risk of regressions

## Immediate Action Needed

**We need to decide**: Should we make the code work with the "correct" zext behavior, or should we preserve the old sext behavior for compatibility?

The test failure tells us: **The existing OpenVKL code expects sext behavior**, even though it uses `uint64` types.

## Recommended Fix

**Short term**: Modify our fixes to preserve sext behavior for 32-bit addressing to maintain compatibility with existing code.

**Long term**: OpenVKL should either:
1. Use signed types if it relies on sign-extension, OR
2. Use 64-bit addressing for truly unsigned large indices

## Files Affected

- `/Users/aneshlya/ispc/src/opt/ImproveMemoryOps.cpp` - Our fixes
- `/Users/aneshlya/libraries.graphics.renderkit.openvkl/openvkl/devices/cpu/volume/SharedStructuredVolume.ispc` - Uses uint64 but expects sext behavior
- `builtins/util.m4` - 32-bit gather intrinsics use sext

## Statistics

- SharedStructuredVolume.ll: 199,252 lines
- Addressing mode changes: 879 sext → ~879 changed to zext
- New zext operations: 2,528
- **Conclusion**: This is a systemic change affecting nearly all array accesses in the file
