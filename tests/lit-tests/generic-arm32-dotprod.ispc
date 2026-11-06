// Generic targets do not implement the dot product builtins, so on 32-bit ARM CPUs with dotprod or i8mm the
// dot4add_*/dot2add_* functions must still use the fallbacks instead of calling the __not_supported stubs.

// RUN: %{ispc} %s --arch=arm --target=generic-i32x4 --target-os=linux --cpu=cortex-a55 --nowrap --emit-asm -o - \
// RUN:   | FileCheck %s --check-prefix=ASM --implicit-check-not=__not_supported
// RUN: %{ispc} %s --arch=arm --target=generic-i32x4 --target-os=linux --cpu=cortex-a55 --nowrap \
// RUN:   --emit-llvm-text -o - | FileCheck %s --check-prefix=IR --implicit-check-not=__not_supported

// REQUIRES: ARM32_ENABLED && LINUX_ENABLED

// ASM: {{^}}dots___{{.*}}:
// IR: define {{.*}}@dots___

void dots(uniform uint a[], uniform uint b[], uniform int si[], uniform uint ui[]) {
    uint x = a[programIndex], y = b[programIndex];
    int s = si[programIndex];
    uint u = ui[programIndex];
    si[programIndex] = dot4add_u8i8packed(x, y, s) + dot4add_u8i8packed_sat(x, y, s) + dot4add_i8i8packed(x, y, s) +
                       dot4add_i8i8packed_sat(x, y, s) + dot2add_i16i16packed(x, y, s) +
                       dot2add_i16i16packed_sat(x, y, s) + dot2add_u16i16packed(x, y, s) +
                       dot2add_u16i16packed_sat(x, y, s);
    ui[programIndex] = dot4add_u8u8packed(x, y, u) + dot4add_u8u8packed_sat(x, y, u) + dot2add_u16u16packed(x, y, u) +
                       dot2add_u16u16packed_sat(x, y, u);
}
