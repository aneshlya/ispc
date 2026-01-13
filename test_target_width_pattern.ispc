#define VKL_TARGET_WIDTH 4

// This mimics the OpenVKL pattern: samples[i * VKL_TARGET_WIDTH + programIndex]
export void test_varying_store(uniform float samples[], uniform int M) {
    for (uniform uint32 i = 0; i < M; i++) {
        // Calculate some varying value
        float value = (float)(i * 10 + programIndex);
        
        // This is the problematic pattern from OpenVKL line 1962
        samples[i * VKL_TARGET_WIDTH + programIndex] = value;
    }
}

// Simpler version for comparison
export void test_direct_store(uniform float samples[], uniform int count) {
    foreach (idx = 0 ... count) {
        samples[idx] = (float)idx;
    }
}
