// Test case to understand the sext vs zext issue

export void test_sext_behavior(uniform uint8 data[], uniform float result[]) {
    // Simulate the 3D indexing pattern from OpenVKL
    uniform int32 dimensions_x = 100;
    uniform int32 dimensions_y = 100;
    uniform int32 dimensions_z = 100;
    
    foreach (i = 0 ... 10) {
        // 3D coordinates
        int32 x = i;
        int32 y = i;
        int32 z = i;
        
        // This is the pattern from OpenVKL - result is int32
        int32 index32 = x + dimensions_x * (y + dimensions_y * z);
        
        // Cast to uint64 (as OpenVKL does)
        uint64 index64 = (uint64)index32;
        
        // Access the data
        result[i] = (float)data[index64];
    }
}

export void test_explicit_uint(uniform uint8 data[], uniform float result[]) {
    uniform uint32 dimensions_x = 100;
    uniform uint32 dimensions_y = 100;
    uniform uint32 dimensions_z = 100;
    
    foreach (i = 0 ... 10) {
        uint32 x = i;
        uint32 y = i;
        uint32 z = i;
        
        // Calculate with unsigned types throughout
        uint32 index32 = x + dimensions_x * (y + dimensions_y * z);
        uint64 index64 = (uint64)index32;
        
        result[i] = (float)data[index64];
    }
}
