// Copyright (c) 2026, Intel Corporation
// SPDX-License-Identifier: BSD-3-Clause

#include <benchmark/benchmark.h>
#include <cmath>
#include <cstdint>
#include <stdio.h>

#include "../common.h"
#include "06_bc1_decode_ispc.h"

static Docs
    docs("Decode of a BC1 (DXT1) compressed image to RGBA8 pixels (uint32), 4x4 pixels per 64-bit block:\n"
         "bc1_decode - lanes are the blocks of a block row, every lane writes 4 adjacent pixels per row\n"
         "bc1_decode_transpose - reference: the same, the 4 pixels written by soa_to_aos4 for full gangs\n"
         "bc1_decode_pixel_lanes - another layout: lanes are the 16 pixels of one block, the palette is computed\n"
         "  for every block\n"
         "The argument is the number of pixels (a square image).\n"
         "Expectation:\n"
         " - No regressions\n"
         " - bc1_decode as fast as bc1_decode_transpose: the 4 stores of a pixel row cover a contiguous\n"
         "   range, so the stride-4 scatters can be a transpose and vector stores\n");

WARM_UP_RUN();

// Square images of 16 * 16, 68 * 68 and 260 * 260 pixels: rows of 4, 17 and 65 blocks, so that full gangs and a
// tail are mixed on all targets.
#define ARGS Arg(256)->Arg(4624)->Arg(67600)

static uint64_t next_random(uint64_t &s) {
    s ^= s << 13;
    s ^= s >> 7;
    s ^= s << 17;
    return s;
}

// Random blocks: about a half of them use each of the 2 palette modes, and every 8th block has equal endpoints
// (the 3-color mode).
static void init(uint64_t *blocks, uint32_t *pixels, int side) {
    uint64_t s = 0x9E3779B97F4A7C15ull;
    for (int i = 0; i < (side / 4) * (side / 4); i++) {
        blocks[i] = next_random(s);
        if (i % 8 == 7) {
            blocks[i] = (blocks[i] & ~0xffff0000ull) | ((blocks[i] & 0xffff) << 16);
        }
    }
    for (int i = 0; i < side * side; i++) {
        pixels[i] = 0xdeadbeef;
    }
}

static uint32_t pack_rgba(uint32_t r, uint32_t g, uint32_t b, uint32_t a) {
    return r | (g << 8) | (b << 16) | (a << 24);
}

static uint32_t ref_pixel(uint64_t block, int x, int y) {
    uint32_t c0 = block & 0xffff, c1 = (block >> 16) & 0xffff;
    uint32_t e0[3] = {(c0 >> 11) & 31, (c0 >> 5) & 63, c0 & 31};
    uint32_t e1[3] = {(c1 >> 11) & 31, (c1 >> 5) & 63, c1 & 31};
    for (int c = 0; c < 3; c++) {
        // 5- and 6-bit channels are scaled to 8 bits by repeating their high bits.
        int bits = c == 1 ? 6 : 5;
        e0[c] = (e0[c] << (8 - bits)) | (e0[c] >> (2 * bits - 8));
        e1[c] = (e1[c] << (8 - bits)) | (e1[c] >> (2 * bits - 8));
    }
    uint32_t code = (block >> (32 + 2 * (4 * y + x))) & 3;
    switch (code) {
    case 0:
        return pack_rgba(e0[0], e0[1], e0[2], 255);
    case 1:
        return pack_rgba(e1[0], e1[1], e1[2], 255);
    case 2:
        if (c0 > c1) {
            return pack_rgba((2 * e0[0] + e1[0]) / 3, (2 * e0[1] + e1[1]) / 3, (2 * e0[2] + e1[2]) / 3, 255);
        }
        return pack_rgba((e0[0] + e1[0]) / 2, (e0[1] + e1[1]) / 2, (e0[2] + e1[2]) / 2, 255);
    default:
        if (c0 > c1) {
            return pack_rgba((e0[0] + 2 * e1[0]) / 3, (e0[1] + 2 * e1[1]) / 3, (e0[2] + 2 * e1[2]) / 3, 255);
        }
        return 0;
    }
}

static bool check(const uint64_t *blocks, const uint32_t *pixels, int side) {
    for (int y = 0; y < side; y++) {
        for (int x = 0; x < side; x++) {
            uint32_t want = ref_pixel(blocks[(y / 4) * (side / 4) + x / 4], x % 4, y % 4);
            uint32_t got = pixels[y * side + x];
            if (got != want) {
                printf("Error y=%d x=%d: %08x, expected %08x\n", y, x, got, want);
                return false;
            }
        }
    }
    return true;
}

#define BENCH(NAME)                                                                                                    \
    static void NAME(benchmark::State &state) {                                                                        \
        int side = static_cast<int>(std::lround(std::sqrt(static_cast<double>(state.range(0)))));                      \
        uint64_t *blocks = static_cast<uint64_t *>(aligned_alloc_helper(sizeof(uint64_t) * (side / 4) * (side / 4)));  \
        uint32_t *pixels = static_cast<uint32_t *>(aligned_alloc_helper(sizeof(uint32_t) * side * side));              \
        init(blocks, pixels, side);                                                                                    \
        for (auto _ : state) {                                                                                         \
            ispc::NAME(blocks, pixels, side, side);                                                                    \
            benchmark::ClobberMemory();                                                                                \
        }                                                                                                              \
        if (!check(blocks, pixels, side))                                                                              \
            fail_benchmark(state);                                                                                     \
        aligned_free_helper(blocks);                                                                                   \
        aligned_free_helper(pixels);                                                                                   \
        state.SetComplexityN(state.range(0));                                                                          \
    }                                                                                                                  \
    BENCHMARK(NAME)->ARGS;

BENCH(bc1_decode)
BENCH(bc1_decode_transpose)
BENCH(bc1_decode_pixel_lanes)

CHECKED_BENCHMARK_MAIN();
