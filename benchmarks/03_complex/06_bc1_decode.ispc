// Copyright (c) 2026, Intel Corporation
// SPDX-License-Identifier: BSD-3-Clause

// Decode of a BC1 (DXT1) compressed image to RGBA8 pixels stored as uint32. A 64-bit block covers 4x4 pixels: two
// RGB565 endpoint colors in the low 32 bits and sixteen 2-bit palette indices in the high 32 bits. The image width and
// height are multiples of 4, blocks are stored row by row.

static inline uint32 pack_rgba(uint32 r, uint32 g, uint32 b, uint32 a) { return r | (g << 8) | (b << 16) | (a << 24); }

static inline uint32 expand5(uint32 v) { return (v << 3) | (v >> 2); }
static inline uint32 expand6(uint32 v) { return (v << 2) | (v >> 4); }

// The 4 palette colors of a block. If color0 > color1, the palette is the 2 endpoints and 2 colors at 1/3 and 2/3
// between them. Otherwise it is the 2 endpoints, their average and transparent black.
static inline void bc1_palette(uint64 block, uint32 &p0, uint32 &p1, uint32 &p2, uint32 &p3) {
    uint32 c0 = (uint32)(block & 0xffff);
    uint32 c1 = (uint32)((block >> 16) & 0xffff);
    uint32 r0 = expand5(c0 >> 11), g0 = expand6((c0 >> 5) & 63), b0 = expand5(c0 & 31);
    uint32 r1 = expand5(c1 >> 11), g1 = expand6((c1 >> 5) & 63), b1 = expand5(c1 & 31);
    p0 = pack_rgba(r0, g0, b0, 255);
    p1 = pack_rgba(r1, g1, b1, 255);
    if (c0 > c1) {
        p2 = pack_rgba((2 * r0 + r1) / 3, (2 * g0 + g1) / 3, (2 * b0 + b1) / 3, 255);
        p3 = pack_rgba((r0 + 2 * r1) / 3, (g0 + 2 * g1) / 3, (b0 + 2 * b1) / 3, 255);
    } else {
        p2 = pack_rgba((r0 + r1) / 2, (g0 + g1) / 2, (b0 + b1) / 2, 255);
        p3 = 0;
    }
}

static inline uint32 pick(uint32 code, uint32 p0, uint32 p1, uint32 p2, uint32 p3) {
    return select(code == 0, p0, select(code == 1, p1, select(code == 2, p2, p3)));
}

// Lanes are the blocks of a block row. Every pixel store has a stride of 4 pixels between lanes, and the 4 stores of a
// pixel row of the blocks cover a contiguous range.
export void bc1_decode(uniform const uint64 blocks[], uniform uint32 pixels[], uniform int32 width,
                       uniform int32 height) {
    uniform int32 blocks_wide = width / 4;
    for (uniform int32 by = 0; by < height / 4; by++) {
        foreach (bx = 0 ... blocks_wide) {
            uint64 block = blocks[by * blocks_wide + bx];
            uint32 p0, p1, p2, p3;
            bc1_palette(block, p0, p1, p2, p3);
            uint32 indices = (uint32)(block >> 32);
            for (uniform int32 y = 0; y < 4; y++) {
                for (uniform int32 x = 0; x < 4; x++) {
                    uint32 code = (indices >> (2 * (4 * y + x))) & 3;
                    pixels[(4 * by + y) * width + 4 * bx + x] = pick(code, p0, p1, p2, p3);
                }
            }
        }
    }
}

// Reference: the same, with the 4 pixels of a lane in a row written by a transpose (soa_to_aos4) for full gangs.
export void bc1_decode_transpose(uniform const uint64 blocks[], uniform uint32 pixels[], uniform int32 width,
                                 uniform int32 height) {
    uniform int32 blocks_wide = width / 4;
    for (uniform int32 by = 0; by < height / 4; by++) {
        uniform int32 bx0 = 0;
        for (; bx0 + programCount <= blocks_wide; bx0 += programCount) {
            uint64 block = blocks[by * blocks_wide + bx0 + programIndex];
            uint32 p0, p1, p2, p3;
            bc1_palette(block, p0, p1, p2, p3);
            uint32 indices = (uint32)(block >> 32);
            for (uniform int32 y = 0; y < 4; y++) {
                uint32 row = indices >> (8 * y);
                soa_to_aos4((int32)pick(row & 3, p0, p1, p2, p3), (int32)pick((row >> 2) & 3, p0, p1, p2, p3),
                            (int32)pick((row >> 4) & 3, p0, p1, p2, p3), (int32)pick((row >> 6) & 3, p0, p1, p2, p3),
                            (uniform int32 * uniform) & pixels[(4 * by + y) * width + 4 * bx0]);
            }
        }
        foreach (bx = bx0... blocks_wide) {
            uint64 block = blocks[by * blocks_wide + bx];
            uint32 p0, p1, p2, p3;
            bc1_palette(block, p0, p1, p2, p3);
            uint32 indices = (uint32)(block >> 32);
            for (uniform int32 y = 0; y < 4; y++) {
                for (uniform int32 x = 0; x < 4; x++) {
                    uint32 code = (indices >> (2 * (4 * y + x))) & 3;
                    pixels[(4 * by + y) * width + 4 * bx + x] = pick(code, p0, p1, p2, p3);
                }
            }
        }
    }
}

// Another layout: blocks are decoded one by one, lanes are the 16 pixels of a block and the palette is the same in all
// lanes. The pixel store covers 4 contiguous ranges of 4 pixels, one per pixel row of the block.
export void bc1_decode_pixel_lanes(uniform const uint64 blocks[], uniform uint32 pixels[], uniform int32 width,
                                   uniform int32 height) {
    uniform int32 blocks_wide = width / 4;
    for (uniform int32 by = 0; by < height / 4; by++) {
        for (uniform int32 bx = 0; bx < blocks_wide; bx++) {
            uniform uint64 block = blocks[by * blocks_wide + bx];
            uint32 p0, p1, p2, p3;
            bc1_palette(block, p0, p1, p2, p3);
            uniform uint32 indices = (uniform uint32)(block >> 32);
            foreach (i = 0 ... 16) {
                uint32 code = (indices >> (2 * i)) & 3;
                pixels[(4 * by + (i >> 2)) * width + 4 * bx + (i & 3)] = pick(code, p0, p1, p2, p3);
            }
        }
    }
}
