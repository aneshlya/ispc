export void test_simple(uniform float buffer[], float result[], uniform int count) {
    foreach (i = 0 ... count) {
        // Simple array access with varying index
        result[i] = buffer[i * 4];
    }
}

export void test_unsigned(uniform float buffer[], float result[], uniform int count) {
    foreach (i = 0 ... count) {
        unsigned int idx = i * 4;
        result[i] = buffer[idx];
    }
}

export void test_large_base(uniform float buffer[], float result[], uniform int count) {
    foreach (i = 0 ... count) {
        // Base offset of 1000 elements
        result[i] = buffer[1000 + i];
    }
}
