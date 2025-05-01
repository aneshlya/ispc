#include "test_static.isph"
struct S {
    unsigned int32 value;
};

// Assignment operator
inline S operator=(S &a, S b) {
    a.value = b.value + 1; // Adding 1 to the value for demonstration
    return a;
}

// Multiplication assignment operator
inline S operator*=(S &a, S b) {
    a.value *= b.value;
    return a;
}

// Division assignment operator
inline S operator/=(S &a, S b) {
    a.value /= b.value;
    return a;
}

// Modulo assignment operator
inline S operator%=(S &a, S b) {
    a.value %= b.value;
    return a;
}

// Addition assignment operator
inline S operator+=(S &a, S b) {
    a.value += b.value;
    return a;
}

// Subtraction assignment operator
inline S operator-=(S &a, S b) {
    a.value -= b.value;
    return a;
}

// Left shift assignment operator
inline S operator<<=(S &a, S b) {
    a.value <<= b.value;
    return a;
}

// Right shift assignment operator
inline S operator>>=(S &a, S b) {
    a.value >>= b.value;
    return a;
}

// Bitwise AND assignment operator
inline S operator&=(S &a, S b) {
    a.value &= b.value;
    return a;
}

// Bitwise XOR assignment operator
inline S operator^=(S &a, S b) {
    a.value ^= b.value;
    return a;
}

// Bitwise OR assignment operator
inline S operator|=(S &a, S b) {
    a.value |= b.value;
    return a;
}

// Assignment operators defined here as shown in previous response

// Example usage in a program
task void f_f(uniform float RET[], uniform float aFOO[]) {
    RET[programIndex] = 0;
    // Using standard assignment
    S x, y, z;
    x.value = 100;
    y.value = 5;
    print("Initial x.value = %\n", x.value);
    print("Initial y.value = %\n", y.value);
    
    // Using the assignment operators
    
    // Assignment
    S t = x;  // t.value = 100
    z = x;  // z.value = 101
    print("After assignment z = x: z.value = %\n", z.value);
    
    // Multiplication assignment
    z *= y;   // z.value = 500 (100 * 5)
    print("After z *= y: z.value = %\n", z.value);
    
    // Division assignment
    S div = z;
    div /= y;  // div.value = 100 (500 / 5)
    print("After div /= y: div.value = %\n", div.value);
    
    // Modulo assignment
    S mod;
    mod.value = 17;
    print("Initial mod.value = %\n", mod.value);
    mod %= y;  // mod.value = 2 (17 % 5)
    print("After mod mod= y: mod.value = %\n", mod.value);
    
    // Addition assignment
    S sum = x;
    sum += y;  // sum.value = 105 (100 + 5)
    print("After sum += y: sum.value = %\n", sum.value);
    
    // Subtraction assignment
    S diff = x;
    diff -= y;  // diff.value = 95 (100 - 5)
    print("After diff -= y: diff.value = %\n", diff.value);
    
    // Left shift assignment
    S lshift = x;
    S shift_amount;
    shift_amount.value = 3;
    print("Initial shift_amount.value = %\n", shift_amount.value);
    lshift <<= shift_amount;  // lshift.value = 800 (100 << 3)
    print("After lshift <<= shift_amount: lshift.value = %\n", lshift.value);
    
    // Right shift assignment
    S rshift;
    rshift.value = 64;
    print("Initial rshift.value = %\n", rshift.value);
    rshift >>= shift_amount;  // rshift.value = 8 (64 >> 3)
    print("After rshift >>= shift_amount: rshift.value = %\n", rshift.value);
    
    // Bitwise AND assignment
    S band;
    band.value = 0xFF;
    S mask;
    mask.value = 0x0F;
    print("Initial band.value = % (0x%x)\n", band.value, band.value);
    print("Initial mask.value = % (0x%x)\n", mask.value, mask.value);
    band &= mask;  // band.value = 15 (0xFF & 0x0F = 0x0F = 15)
    print("After band &= mask: band.value = % (0x%x)\n", band.value, band.value);
    
    // Bitwise XOR assignment
    S bxor;
    bxor.value = 0xAA;  // 10101010 in binary
    S xmask;
    xmask.value = 0x55;  // 01010101 in binary
    print("Initial bxor.value = % (0x%x)\n", bxor.value, bxor.value);
    print("Initial xmask.value = % (0x%x)\n", xmask.value, xmask.value);
    bxor ^= xmask;  // bxor.value = 255 (0xAA ^ 0x55 = 0xFF = 255)
    print("After bxor ^= xmask: bxor.value = % (0x%x)\n", bxor.value, bxor.value);
    
    // Bitwise OR assignment
    S bor;
    bor.value = 0xF0;  // 11110000 in binary
    S ormask;
    ormask.value = 0x0F;  // 00001111 in binary
    print("Initial bor.value = % (0x%x)\n", bor.value, bor.value);
    print("Initial ormask.value = % (0x%x)\n", ormask.value, ormask.value);
    bor |= ormask;  // bor.value = 255 (0xF0 | 0x0F = 0xFF = 255)
    print("After bor |= ormask: bor.value = % (0x%x)\n", bor.value, bor.value);
    
    // Chaining operations
    S chain;
    chain.value = 10;
    print("Initial chain.value = %\n", chain.value);
    chain += y;  // chain.value = 15
    print("After chain += y: chain.value = %\n", chain.value);
    chain *= y;  // chain.value = 75
    print("After chain *= y: chain.value = %\n", chain.value);
    chain /= y;  // chain.value = 15
    print("After chain /= y: chain.value = %\n", chain.value);
    
    // Storing results
    RET[0] = extract(z.value, 0);       // Multiplication result
    RET[1] = extract(sum.value, 0);   // Addition result
    RET[2] = extract(diff.value, 0);  // Subtraction result
    RET[3] = extract(bor.value, 0);   // Bitwise OR result
    RET[4] = extract(chain.value, 0);   // Bitwise OR result*/
}

task void result(uniform float RET[]) { 
    RET[programIndex] = 0;
    RET[0] = 505;
    RET[1] = 105;
    RET[2] = 95;
    RET[3] = 255;
    RET[4] = 15;
}