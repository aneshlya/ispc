// Copyright (c) 2026, Intel Corporation
// SPDX-License-Identifier: BSD-3-Clause

// Resampling of an RGB image with a 2D Lanczos-3 filter: every output pixel is the normalized sum of 6x6 source
// pixels around its center, weighted by the product of the Lanczos-3 kernel in x and y. Taps outside the image are
// clamped to the edge. Lanes are output pixels of a row, so every tap is a gather with data-dependent addresses of
// 3 interleaved channels, and the result is a stride-3 scatter.

static inline float sinc(float x) {
    float px = PI * x;
    return select(x == 0.0f, 1.0f, sin(px) / px);
}

static inline float lanczos3(float x) { return select(abs(x) < 3.0f, sinc(x) * sinc(x / 3.0f), 0.0f); }

export void lanczos3_rgb8(uniform const uint8 src[], uniform int32 src_width, uniform int32 src_height,
                          uniform uint8 dst[], uniform int32 dst_width, uniform int32 dst_height) {
    uniform float scale_x = (float)src_width / dst_width;
    uniform float scale_y = (float)src_height / dst_height;
    for (uniform int32 y = 0; y < dst_height; y++) {
        uniform float center_y = (y + 0.5f) * scale_y - 0.5f;
        uniform int32 origin_y = (int32)floor(center_y);
        float wy[6];
        for (uniform int32 j = 0; j < 6; j++) {
            wy[j] = lanczos3(j - 2 - (center_y - origin_y));
        }
        foreach (x = 0 ... dst_width) {
            float center_x = (x + 0.5f) * scale_x - 0.5f;
            int32 origin_x = (int32)floor(center_x);
            float wx[6];
            for (uniform int32 i = 0; i < 6; i++) {
                wx[i] = lanczos3(i - 2 - (center_x - origin_x));
            }
            float r = 0.0f, g = 0.0f, b = 0.0f, sum = 0.0f;
            for (uniform int32 j = 0; j < 6; j++) {
                uniform int32 row = clamp(origin_y + j - 2, (uniform int32)0, src_height - 1) * src_width;
                for (uniform int32 i = 0; i < 6; i++) {
                    int32 tap = (row + clamp(origin_x + i - 2, (uniform int32)0, src_width - 1)) * 3;
                    float w = wy[j] * wx[i];
                    r += w * src[tap + 0];
                    g += w * src[tap + 1];
                    b += w * src[tap + 2];
                    sum += w;
                }
            }
            float norm = 1.0f / sum;
            int32 out = (y * dst_width + x) * 3;
            dst[out + 0] = (uint8)clamp((int32)(r * norm + 0.5f), (uniform int32)0, (uniform int32)255);
            dst[out + 1] = (uint8)clamp((int32)(g * norm + 0.5f), (uniform int32)0, (uniform int32)255);
            dst[out + 2] = (uint8)clamp((int32)(b * norm + 0.5f), (uniform int32)0, (uniform int32)255);
        }
    }
}

// The same for float channels, without rounding and clamping of the result.
export void lanczos3_rgb32f(uniform const float src[], uniform int32 src_width, uniform int32 src_height,
                            uniform float dst[], uniform int32 dst_width, uniform int32 dst_height) {
    uniform float scale_x = (float)src_width / dst_width;
    uniform float scale_y = (float)src_height / dst_height;
    for (uniform int32 y = 0; y < dst_height; y++) {
        uniform float center_y = (y + 0.5f) * scale_y - 0.5f;
        uniform int32 origin_y = (int32)floor(center_y);
        float wy[6];
        for (uniform int32 j = 0; j < 6; j++) {
            wy[j] = lanczos3(j - 2 - (center_y - origin_y));
        }
        foreach (x = 0 ... dst_width) {
            float center_x = (x + 0.5f) * scale_x - 0.5f;
            int32 origin_x = (int32)floor(center_x);
            float wx[6];
            for (uniform int32 i = 0; i < 6; i++) {
                wx[i] = lanczos3(i - 2 - (center_x - origin_x));
            }
            float r = 0.0f, g = 0.0f, b = 0.0f, sum = 0.0f;
            for (uniform int32 j = 0; j < 6; j++) {
                uniform int32 row = clamp(origin_y + j - 2, (uniform int32)0, src_height - 1) * src_width;
                for (uniform int32 i = 0; i < 6; i++) {
                    int32 tap = (row + clamp(origin_x + i - 2, (uniform int32)0, src_width - 1)) * 3;
                    float w = wy[j] * wx[i];
                    r += w * src[tap + 0];
                    g += w * src[tap + 1];
                    b += w * src[tap + 2];
                    sum += w;
                }
            }
            float norm = 1.0f / sum;
            int32 out = (y * dst_width + x) * 3;
            dst[out + 0] = r * norm;
            dst[out + 1] = g * norm;
            dst[out + 2] = b * norm;
        }
    }
}
