// Copyright (c) 2026, Intel Corporation
// SPDX-License-Identifier: BSD-3-Clause

#include <benchmark/benchmark.h>
#include <cmath>
#include <cstdint>
#include <stdio.h>

#include "../common.h"
#include "07_mlp_inference_ispc.h"

static Docs
    docs("Inference of a small MLP for every pixel, as in neural texture decoders: 12 features, 2 hidden layers\n"
         "of 32 with SiLU, 12 results. Layer sizes are runtime values.\n"
         "mlp_output_lanes - one pixel at a time, lanes are the outputs of a layer (contiguous weight loads)\n"
         "mlp_pixel_lanes - lanes are pixels (uniform weights, features and results with a stride of 12)\n"
         "mlp_output_lanes_fp16 - mlp_output_lanes with float16 weights and arithmetic\n"
         "The argument is the number of pixels.\n"
         "Expectation:\n"
         " - No regressions\n"
         " - mlp_pixel_lanes faster than mlp_output_lanes: a partial gang only at the end, not in every layer\n"
         "   of every pixel\n"
         " - mlp_output_lanes_fp16 has no scalar calls of exp() for float16. On targets without float16\n"
         "   arithmetic every operation is done in float and rounded to float16, so it is slower than\n"
         "   mlp_output_lanes there\n");

WARM_UP_RUN();

// Pixel counts that aren't multiples of the gang size, to run the tail of mlp_pixel_lanes.
#define ARGS Arg(255)->Arg(4095)->Arg(16383)

static const int INPUTS = 12;
static const int HIDDEN = 32;
static const int OUTPUTS = 12;

static uint64_t next_random(uint64_t &s) {
    s ^= s << 13;
    s ^= s >> 7;
    s ^= s << 17;
    return s;
}

// A value in [-scale, scale).
static float random_float(uint64_t &s, float scale) {
    return (static_cast<float>(next_random(s) % 2000) * 0.001f - 1.0f) * scale;
}

struct Network {
    float *w[3];
    float *b[3];
    int inputs[3];
    int outputs[3];
};

// Weights are scaled by 1 / sqrt(inputs) to keep values of every layer about the same.
static void init_network(Network &net, uint64_t &s) {
    const int sizes[4] = {INPUTS, HIDDEN, HIDDEN, OUTPUTS};
    for (int l = 0; l < 3; l++) {
        net.inputs[l] = sizes[l];
        net.outputs[l] = sizes[l + 1];
        net.w[l] = static_cast<float *>(aligned_alloc_helper(sizeof(float) * sizes[l] * sizes[l + 1]));
        net.b[l] = static_cast<float *>(aligned_alloc_helper(sizeof(float) * sizes[l + 1]));
        float scale = 1.0f / std::sqrt(static_cast<float>(sizes[l]));
        for (int i = 0; i < sizes[l] * sizes[l + 1]; i++) {
            net.w[l][i] = random_float(s, scale);
        }
        for (int i = 0; i < sizes[l + 1]; i++) {
            net.b[l][i] = random_float(s, 0.1f);
        }
    }
}

static void free_network(Network &net) {
    for (int l = 0; l < 3; l++) {
        aligned_free_helper(net.w[l]);
        aligned_free_helper(net.b[l]);
    }
}

static void init(float *features, float *results, int count, uint64_t &s) {
    for (int i = 0; i < count * INPUTS; i++) {
        features[i] = random_float(s, 1.0f);
    }
    for (int i = 0; i < count * OUTPUTS; i++) {
        results[i] = NAN;
    }
}

// Rounding to float16 after every operation, as in mlp_output_lanes_fp16, if half is set.
static double round_to(double v, bool half) {
#ifdef HAS_FP16
    if (half) {
        return static_cast<float>(static_cast<__fp16>(static_cast<float>(v)));
    }
#endif
    return v;
}

// The reference in double, or with the float16 network and float16 arithmetic of mlp_output_lanes_fp16.
static void ref_pixel(const Network &net, const float *features, double *out, bool half) {
    double a[HIDDEN], b[HIDDEN];
    for (int i = 0; i < INPUTS; i++) {
        a[i] = round_to(features[i], half);
    }
    for (int l = 0; l < 3; l++) {
        for (int o = 0; o < net.outputs[l]; o++) {
            double acc = round_to(net.b[l][o], half);
            for (int i = 0; i < net.inputs[l]; i++) {
                acc = round_to(acc + round_to(a[i] * round_to(net.w[l][i * net.outputs[l] + o], half), half), half);
            }
            if (l < 2) {
                double e = half ? round_to(std::exp(static_cast<float>(-acc)), true) : std::exp(-acc);
                acc = round_to(acc / round_to(1.0 + e, half), half);
            }
            b[o] = acc;
        }
        for (int o = 0; o < net.outputs[l]; o++) {
            a[o] = b[o];
        }
    }
    for (int o = 0; o < OUTPUTS; o++) {
        out[o] = a[o];
    }
}

static bool check(const Network &net, const float *features, const float *results, int count, double tolerance,
                  bool half) {
    for (int p = 0; p < count; p++) {
        double want[OUTPUTS];
        ref_pixel(net, &features[p * INPUTS], want, half);
        for (int o = 0; o < OUTPUTS; o++) {
            float got = results[p * OUTPUTS + o];
            double error = std::abs(got - want[o]) / (1.0 + std::abs(want[o]));
            if (!(error <= tolerance)) {
                printf("Error pixel=%d output=%d: %f, expected %f\n", p, o, got, want[o]);
                return false;
            }
        }
    }
    return true;
}

#define BENCH(NAME)                                                                                                    \
    static void NAME(benchmark::State &state) {                                                                        \
        int count = static_cast<int>(state.range(0));                                                                  \
        uint64_t s = 0x9E3779B97F4A7C15ull;                                                                            \
        Network net;                                                                                                   \
        init_network(net, s);                                                                                          \
        float *features = static_cast<float *>(aligned_alloc_helper(sizeof(float) * count * INPUTS));                  \
        float *results = static_cast<float *>(aligned_alloc_helper(sizeof(float) * count * OUTPUTS));                  \
        init(features, results, count, s);                                                                             \
        for (auto _ : state) {                                                                                         \
            ispc::NAME(features, results, count, INPUTS, HIDDEN, OUTPUTS, net.w[0], net.b[0], net.w[1], net.b[1],      \
                       net.w[2], net.b[2]);                                                                            \
            benchmark::ClobberMemory();                                                                                \
        }                                                                                                              \
        if (!check(net, features, results, count, 1e-5, false))                                                        \
            fail_benchmark(state);                                                                                     \
        aligned_free_helper(features);                                                                                 \
        aligned_free_helper(results);                                                                                  \
        free_network(net);                                                                                             \
        state.SetComplexityN(state.range(0));                                                                          \
    }                                                                                                                  \
    BENCHMARK(NAME)->ARGS;

BENCH(mlp_output_lanes)
BENCH(mlp_pixel_lanes)

#ifdef HAS_FP16
// The network is rounded to float16 for the kernel and for the reference.
static void mlp_output_lanes_fp16(benchmark::State &state) {
    int count = static_cast<int>(state.range(0));
    uint64_t s = 0x9E3779B97F4A7C15ull;
    Network net;
    init_network(net, s);
    __fp16 *w[3], *b[3];
    for (int l = 0; l < 3; l++) {
        int n = net.inputs[l] * net.outputs[l];
        w[l] = static_cast<__fp16 *>(aligned_alloc_helper(sizeof(__fp16) * n));
        b[l] = static_cast<__fp16 *>(aligned_alloc_helper(sizeof(__fp16) * net.outputs[l]));
        for (int i = 0; i < n; i++) {
            w[l][i] = static_cast<__fp16>(net.w[l][i]);
        }
        for (int i = 0; i < net.outputs[l]; i++) {
            b[l][i] = static_cast<__fp16>(net.b[l][i]);
        }
    }
    float *features = static_cast<float *>(aligned_alloc_helper(sizeof(float) * count * INPUTS));
    float *results = static_cast<float *>(aligned_alloc_helper(sizeof(float) * count * OUTPUTS));
    init(features, results, count, s);
    for (auto _ : state) {
        ispc::mlp_output_lanes_fp16(features, results, count, INPUTS, HIDDEN, OUTPUTS, w[0], b[0], w[1], b[1], w[2],
                                    b[2]);
        benchmark::ClobberMemory();
    }
    // The reference repeats the roundings of the kernel. The tolerance allows a few differences of a float16 ulp, e.g.
    // for another exp() implementation.
    if (!check(net, features, results, count, 1e-3, true))
        fail_benchmark(state);
    for (int l = 0; l < 3; l++) {
        aligned_free_helper(w[l]);
        aligned_free_helper(b[l]);
    }
    aligned_free_helper(features);
    aligned_free_helper(results);
    free_network(net);
    state.SetComplexityN(state.range(0));
}
BENCHMARK(mlp_output_lanes_fp16)->ARGS;
#endif

CHECKED_BENCHMARK_MAIN();
