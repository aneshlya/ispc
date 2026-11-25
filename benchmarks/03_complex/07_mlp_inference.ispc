// Copyright (c) 2026, Intel Corporation
// SPDX-License-Identifier: BSD-3-Clause

// Inference of a small multilayer perceptron for every pixel of an image, as in neural texture decoders: features of
// a pixel go through 2 hidden layers with the SiLU activation and an output layer without activation. The layer sizes
// are runtime values, at most MAX_WIDTH. The weights of a layer are a matrix with a row per input and a column per
// output (weights[input * outputs + output]). Features and results are stored pixel by pixel.

#define MAX_WIDTH 64

static inline float silu(float x) { return x / (1.0f + exp(-x)); }

// Lanes are the outputs of a layer, one pixel at a time: weight loads are contiguous, inputs are uniform.
static inline void layer_output_lanes(uniform const float src[], uniform int32 inputs, uniform const float weights[],
                                      uniform const float biases[], uniform int32 outputs, uniform float out[],
                                      uniform bool activation) {
    foreach (o = 0 ... outputs) {
        float acc = biases[o];
        for (uniform int32 i = 0; i < inputs; i++) {
            acc += src[i] * weights[i * outputs + o];
        }
        if (activation) {
            acc = silu(acc);
        }
        out[o] = acc;
    }
}

export void mlp_output_lanes(uniform const float features[], uniform float results[], uniform int32 count,
                             uniform int32 inputs, uniform int32 hidden, uniform int32 outputs,
                             uniform const float w0[], uniform const float b0[], uniform const float w1[],
                             uniform const float b1[], uniform const float w2[], uniform const float b2[]) {
    uniform float h0[MAX_WIDTH], h1[MAX_WIDTH];
    for (uniform int32 p = 0; p < count; p++) {
        layer_output_lanes(&features[p * inputs], inputs, w0, b0, hidden, h0, true);
        layer_output_lanes(h0, hidden, w1, b1, hidden, h1, true);
        layer_output_lanes(h1, hidden, w2, b2, outputs, &results[p * outputs], false);
    }
}

// Lanes are pixels: weights are uniform, features are gathered and results are scattered with a stride of the
// layer size.
static inline void layer_pixel_lanes(const float src[], uniform int32 inputs, uniform const float weights[],
                                     uniform const float biases[], uniform int32 outputs, float out[],
                                     uniform bool activation) {
    for (uniform int32 o = 0; o < outputs; o++) {
        float acc = biases[o];
        for (uniform int32 i = 0; i < inputs; i++) {
            acc += src[i] * weights[i * outputs + o];
        }
        if (activation) {
            acc = silu(acc);
        }
        out[o] = acc;
    }
}

export void mlp_pixel_lanes(uniform const float features[], uniform float results[], uniform int32 count,
                            uniform int32 inputs, uniform int32 hidden, uniform int32 outputs, uniform const float w0[],
                            uniform const float b0[], uniform const float w1[], uniform const float b1[],
                            uniform const float w2[], uniform const float b2[]) {
    foreach (p = 0 ... count) {
        float x[MAX_WIDTH], h0[MAX_WIDTH], h1[MAX_WIDTH], y[MAX_WIDTH];
        for (uniform int32 i = 0; i < inputs; i++) {
            x[i] = features[p * inputs + i];
        }
        layer_pixel_lanes(x, inputs, w0, b0, hidden, h0, true);
        layer_pixel_lanes(h0, hidden, w1, b1, hidden, h1, true);
        layer_pixel_lanes(h1, hidden, w2, b2, outputs, y, false);
        for (uniform int32 o = 0; o < outputs; o++) {
            results[p * outputs + o] = y[o];
        }
    }
}

#ifdef HAS_FP16
// The same as mlp_output_lanes, with float16 weights and float16 arithmetic.
static inline void layer_output_lanes_fp16(uniform const float16 src[], uniform int32 inputs,
                                           uniform const float16 weights[], uniform const float16 biases[],
                                           uniform int32 outputs, uniform float16 out[], uniform bool activation) {
    foreach (o = 0 ... outputs) {
        float16 acc = biases[o];
        for (uniform int32 i = 0; i < inputs; i++) {
            acc += src[i] * weights[i * outputs + o];
        }
        if (activation) {
            acc = acc / (1.0f16 + exp(-acc));
        }
        out[o] = acc;
    }
}

export void mlp_output_lanes_fp16(uniform const float features[], uniform float results[], uniform int32 count,
                                  uniform int32 inputs, uniform int32 hidden, uniform int32 outputs,
                                  uniform const float16 w0[], uniform const float16 b0[], uniform const float16 w1[],
                                  uniform const float16 b1[], uniform const float16 w2[], uniform const float16 b2[]) {
    uniform float16 x[MAX_WIDTH], h0[MAX_WIDTH], h1[MAX_WIDTH], y[MAX_WIDTH];
    for (uniform int32 p = 0; p < count; p++) {
        foreach (i = 0 ... inputs) {
            x[i] = (float16)features[p * inputs + i];
        }
        layer_output_lanes_fp16(x, inputs, w0, b0, hidden, h0, true);
        layer_output_lanes_fp16(h0, hidden, w1, b1, hidden, h1, true);
        layer_output_lanes_fp16(h1, hidden, w2, b2, outputs, y, false);
        foreach (o = 0 ... outputs) {
            results[p * outputs + o] = (float)y[o];
        }
    }
}
#endif
