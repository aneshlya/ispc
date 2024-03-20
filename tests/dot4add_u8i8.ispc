#include "../test_static.isph"
#define N 64

noinline varying int32 test(varying int32 a, varying int32 b, varying int32 src) {
    uint8 u_a[4];
    u_a[0] = (uint8)((a >> 24) & 0xFF); // Extract the first i8 value
    u_a[1] = (uint8)((a >> 16) & 0xFF); // Extract the second i8 value
    u_a[2] = (uint8)((a >> 8) & 0xFF);  // Extract the third i8 value
    u_a[3] = (uint8)(a & 0xFF);         // Extract the fourth i8 value
    //print("ISPC unpacked a: % % % % \n", u_a[0], u_a[1], u_a[2], u_a[3]);
    int8 u_b[4];
    u_b[0] = (int8)((b >> 24) & 0xFF); // Extract the first i8 value
    u_b[1] = (int8)((b >> 16) & 0xFF); // Extract the second i8 value
    u_b[2] = (int8)((b >> 8) & 0xFF);  // Extract the third i8 value
    u_b[3] = (int8)(b & 0xFF);         // Extract the fourth i8 value
    //print("ISPC unpacked b: % % % % \n", u_b[0], u_b[1], u_b[2], u_b[3]);
    int16 u_ab[4];
    u_ab[0] = (int16)((int16)(uint16)u_a[0] * (int16)u_b[0]);
    u_ab[1] = (int16)((int16)(uint16)u_a[1] * (int16)u_b[1]);
    u_ab[2] = (int16)((int16)(uint16)u_a[2] * (int16)u_b[2]);
    u_ab[3] = (int16)((int16)(uint16)u_a[3] * (int16)u_b[3]);
    //int32 tmp_32 = (int32)(u_ab[0] + u_ab[1] + u_ab[2] + u_ab[3]);
    //print("tmp_32 % \n", tmp_32);
    return (int32)u_ab[0] + (int32)u_ab[1] + (int32)u_ab[2] + (int32)u_ab[3] + src;
}

varying int32 test_sat(varying int32 a, varying int32 b, varying int32 acc) {
    uint8 u_a[4];
    u_a[0] = (uint8)((a >> 24) & 0xFF); // Extract the first i8 value
    u_a[1] = (uint8)((a >> 16) & 0xFF); // Extract the second i8 value
    u_a[2] = (uint8)((a >> 8) & 0xFF);  // Extract the third i8 value
    u_a[3] = (uint8)(a & 0xFF);         // Extract the fourth i8 value
    int8 u_b[4];
    u_b[0] = (int8)((b >> 24) & 0xFF); // Extract the first i8 value
    u_b[1] = (int8)((b >> 16) & 0xFF); // Extract the second i8 value
    u_b[2] = (int8)((b >> 8) & 0xFF);  // Extract the third i8 value
    u_b[3] = (int8)(b & 0xFF);         // Extract the fourth i8 value
    int32 u_ab[4];
    u_ab[0] = (int32)((int16)(uint16)u_a[0] * (int16)u_b[0]);
    u_ab[1] = (int32)((int16)(uint16)u_a[1] * (int16)u_b[1]);
    u_ab[2] = (int32)((int16)(uint16)u_a[2] * (int16)u_b[2]);
    u_ab[3] = (int32)((int16)(uint16)u_a[3] * (int16)u_b[3]);
    int32 tmp01 = saturating_add(u_ab[0], u_ab[1]);
    int32 tmp012 = saturating_add(tmp01, u_ab[2]);
    int32 tmp0123 = saturating_add(tmp012, u_ab[3]);
    return saturating_add(tmp0123, acc);
}

varying int32 dot4add_u8i8packed2(varying int32 a, varying int32 b, varying int32 acc) {
    int16 tmp1 = (int16)((uint16)(((uint8)((a >> 24) & 0xFF))) * (int16)((int8)((b >> 24) & 0xFF)));
    int16 tmp2 = (int16)((uint16)(((uint8)((a >> 16) & 0xFF))) * (int16)((int8)((b >> 16) & 0xFF)));
    int16 tmp3 = (int16)((uint16)(((uint8)((a >> 8) & 0xFF))) * (int16)((int8)((b >> 8) & 0xFF)));
    int16 tmp4 = (int16)((uint16)(((uint8)(a & 0xFF))) * (int16)((int8)(b & 0xFF)));
    return (int32)tmp1 + (int32)tmp2 + (int32)tmp3 + (int32)tmp4 + acc;
}

task void f_dot4add_u8i8(uniform int a[], uniform int b[], uniform int dst[]) {
    int acc = 2147483645;
    foreach (i = 0 ... N) {
        dst[i] = test_sat(a[i], b[i], acc);
    }    
}

task void result_u8i8(uniform uint8 a[], uniform int8 b[], uniform int dst[]) {
    uniform int acc = 2147483645;
    for (uniform int i = 0; i < N*4; i += 4) {
        uniform int result = 0;
        for (uniform int j = 0; j < 4; ++j) {
            uniform int16 product = (uniform int16)(uniform uint16)(a[i + j]) * (uniform int16)(b[i + j]);
            result += (uniform int32)(product);
        }
        //dst[i / 4] = result + acc; // result + i /4
        dst[i / 4] = saturating_add(result, acc);
    }
}
