// Copyright (c) 2026, Intel Corporation
// SPDX-License-Identifier: BSD-3-Clause

#include <benchmark/benchmark.h>
#include <cmath>
#include <cstdint>
#include <stdio.h>

#include "../common.h"
#include "08_lanczos_resample_ispc.h"

static Docs docs("Resampling of an RGB image to about a half in both directions with a 2D Lanczos-3 filter of a fixed\n"
                 "6x6 window (not widened by the scale):\n"
                 "lanczos3_rgb8 - 8-bit channels, lanczos3_rgb32f - float channels.\n"
                 "Lanes are output pixels of a row: every tap is a gather of 3 interleaved channels, the result is a\n"
                 "stride-3 scatter, and the weights need sin().\n"
                 "The argument is the number of output pixels (a square image).\n"
                 "Expectation:\n"
                 " - No regressions\n"
                 " - lanczos3_rgb8 can't be much faster than lanczos3_rgb32f: there are no byte gathers, so taps\n"
                 "   of 8-bit channels are read one by one\n");

WARM_UP_RUN();

// Output images of 18 * 18, 66 * 66 and 130 * 130 pixels, so that full gangs and a tail are mixed on all targets.
#define ARGS Arg(324)->Arg(4356)->Arg(16900)

static uint64_t next_random(uint64_t &s) {
    s ^= s << 13;
    s ^= s >> 7;
    s ^= s << 17;
    return s;
}

static double ref_lanczos3(double x) {
    const double pi = 3.14159265358979323846;
    auto sinc = [&](double v) { return v == 0.0 ? 1.0 : std::sin(pi * v) / (pi * v); };
    return std::abs(x) < 3.0 ? sinc(x) * sinc(x / 3.0) : 0.0;
}

// The filtered channel c of the output pixel (x, y), before rounding.
template <typename T> static double ref_pixel(const T *src, int src_side, int dst_side, int x, int y, int c) {
    double scale = static_cast<double>(src_side) / dst_side;
    double center_x = (x + 0.5) * scale - 0.5, center_y = (y + 0.5) * scale - 0.5;
    int origin_x = static_cast<int>(std::floor(center_x)), origin_y = static_cast<int>(std::floor(center_y));
    auto clampi = [&](int v) { return v < 0 ? 0 : v > src_side - 1 ? src_side - 1 : v; };
    double acc = 0.0, sum = 0.0;
    for (int j = 0; j < 6; j++) {
        double wy = ref_lanczos3(j - 2 - (center_y - origin_y));
        for (int i = 0; i < 6; i++) {
            double w = wy * ref_lanczos3(i - 2 - (center_x - origin_x));
            acc += w * src[(clampi(origin_y + j - 2) * src_side + clampi(origin_x + i - 2)) * 3 + c];
            sum += w;
        }
    }
    return acc / sum;
}

// The result is the filtered value rounded to nearest, with a margin for the float arithmetic of the kernel.
static bool check(const uint8_t *src, const uint8_t *dst, int src_side, int dst_side) {
    for (int y = 0; y < dst_side; y++) {
        for (int x = 0; x < dst_side; x++) {
            for (int c = 0; c < 3; c++) {
                double v = ref_pixel(src, src_side, dst_side, x, y, c);
                double want = v < 0.0 ? 0.0 : v > 255.0 ? 255.0 : v;
                int got = dst[(y * dst_side + x) * 3 + c];
                if (!(std::abs(got - want) <= 0.5 + 1e-2)) {
                    printf("Error y=%d x=%d c=%d: %d, expected %f\n", y, x, c, got, want);
                    return false;
                }
            }
        }
    }
    return true;
}

// float sin() near the zeros of the kernel gives errors up to about 2e-5, a wrong tap is off by much more.
static bool check(const float *src, const float *dst, int src_side, int dst_side) {
    for (int y = 0; y < dst_side; y++) {
        for (int x = 0; x < dst_side; x++) {
            for (int c = 0; c < 3; c++) {
                double want = ref_pixel(src, src_side, dst_side, x, y, c);
                float got = dst[(y * dst_side + x) * 3 + c];
                if (!(std::abs(got - want) <= 1e-4 * (1.0 + std::abs(want)))) {
                    printf("Error y=%d x=%d c=%d: %f, expected %f\n", y, x, c, got, want);
                    return false;
                }
            }
        }
    }
    return true;
}

static void init(uint8_t *src, int count, uint64_t &s) {
    for (int i = 0; i < count; i++) {
        src[i] = static_cast<uint8_t>(next_random(s));
    }
}

static void init(float *src, int count, uint64_t &s) {
    for (int i = 0; i < count; i++) {
        src[i] = static_cast<float>(next_random(s) % 1000) * 0.001f;
    }
}

#define BENCH(NAME, T)                                                                                                 \
    static void NAME(benchmark::State &state) {                                                                        \
        int dst_side = static_cast<int>(std::lround(std::sqrt(static_cast<double>(state.range(0)))));                  \
        int src_side = 2 * dst_side + 1;                                                                               \
        T *src = static_cast<T *>(aligned_alloc_helper(sizeof(T) * src_side * src_side * 3));                          \
        T *dst = static_cast<T *>(aligned_alloc_helper(sizeof(T) * dst_side * dst_side * 3));                          \
        uint64_t s = 0x9E3779B97F4A7C15ull;                                                                            \
        init(src, src_side * src_side * 3, s);                                                                         \
        for (int i = 0; i < dst_side * dst_side * 3; i++) {                                                            \
            dst[i] = 0;                                                                                                \
        }                                                                                                              \
        for (auto _ : state) {                                                                                         \
            ispc::NAME(src, src_side, src_side, dst, dst_side, dst_side);                                              \
            benchmark::ClobberMemory();                                                                                \
        }                                                                                                              \
        if (!check(src, dst, src_side, dst_side))                                                                      \
            fail_benchmark(state);                                                                                     \
        aligned_free_helper(src);                                                                                      \
        aligned_free_helper(dst);                                                                                      \
        state.SetComplexityN(state.range(0));                                                                          \
    }                                                                                                                  \
    BENCHMARK(NAME)->ARGS;

BENCH(lanczos3_rgb8, uint8_t)
BENCH(lanczos3_rgb32f, float)

CHECKED_BENCHMARK_MAIN();
