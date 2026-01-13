// Test case: unsigned loop counter with large constant offset
export void test_unsigned_large_offset(uniform float buffer[], float result[], uniform int count) {
    foreach (i = 0 ... count) {
        // Unsigned index
        unsigned int idx = i;
        // Large positive offset that looks negative in signed i32
        // 0x7FFFFFFF is max positive signed i32
        // Adding programIndex (0-3) keeps it in signed range
        result[i] = buffer[idx + 0x7FFFFFFE + programIndex];
    }
}

export void test_unsigned_overflow_offset(uniform float buffer[], float result[], uniform int count) {
    foreach (i = 0 ... count) {
        unsigned int idx = i;
        // This will overflow signed i32 range
        result[i] = buffer[idx + 0x80000000 + programIndex];
    }
}
