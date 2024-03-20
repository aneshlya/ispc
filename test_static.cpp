/*
  Copyright (c) 2010-2023, Intel Corporation

  SPDX-License-Identifier: BSD-3-Clause
*/

#if defined(__WASM__)
#define ISPC_IS_WASM
#elif defined(_WIN32) || defined(_WIN64)
#define ISPC_IS_WINDOWS
#elif defined(__linux__) || defined(__FreeBSD__)
#define ISPC_IS_LINUX
#elif defined(__APPLE__)
#define ISPC_IS_APPLE
#else
#error "Host OS was not detected"
#endif

#if defined(_WIN64)
#define ISPC_IS_WINDOWS64
#endif

#ifdef ISPC_IS_WINDOWS
#include <windows.h>
#endif // ISPC_IS_WINDOWS

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <vector>
#if defined ISPC_IS_LINUX || defined ISPC_IS_WASM
#include <malloc.h>
#endif

#if (TEST_SIG == 7)
#define varying_f_sz f_sz
#define v1_varying_f_sz f_sz
#define v2_varying_f_sz f_sz
#define v4_varying_f_sz f_sz
#define v8_varying_f_sz f_sz
#define v16_varying_f_sz f_sz
#define v32_varying_f_sz f_sz
#define v64_varying_f_sz f_sz
#include TEST_HEADER
#endif

#undef TEST_SIG_DOT4ADD
#if defined(TEST_SIG) && (TEST_SIG >= 16) && (TEST_SIG <= 32)
    #define TEST_SIG_DOT4ADD 1
#endif

// For current tests we need max width multiplied by 4, i.e. 64*4
#define ARRAY_SIZE 256

#ifdef ISPC_IS_WINDOWS64
// __vectorcall calling convention is off by default.
#define CALLINGCONV //__vectorcall
#else
#define CALLINGCONV
#endif

extern "C" {
extern void CALLINGCONV f_v_cpu_entry_point(float *result);
extern void CALLINGCONV f_f_cpu_entry_point(float *result, float *a);
extern void CALLINGCONV f_fu_cpu_entry_point(float *result, float *a, float b);
extern void CALLINGCONV f_fi_cpu_entry_point(float *result, float *a, int *b);
extern void CALLINGCONV f_du_cpu_entry_point(float *result, double *a, double b);
extern void CALLINGCONV f_duf_cpu_entry_point(float *result, double *a, float b);
extern void CALLINGCONV f_di_cpu_entry_point(float *result, double *a, int *b);
extern void CALLINGCONV f_dot4add_u8i8_cpu_entry_point(int *a, int *b, int *result);
extern void CALLINGCONV print_uf_cpu_entry_point(float a);
extern void CALLINGCONV print_f_cpu_entry_point(float *a);
extern void CALLINGCONV print_fuf_cpu_entry_point(float *a, float b);
extern void CALLINGCONV result_cpu_entry_point(float *val);
extern void CALLINGCONV result_u8i8_cpu_entry_point(uint8_t *a, int8_t *b, int *result);
extern void CALLINGCONV print_result_cpu_entry_point();

void ISPCLaunch(void **handlePtr, void *f, void *d, int, int, int);
void ISPCSync(void *handle);
void *ISPCAlloc(void **handlePtr, int64_t size, int32_t alignment);
}

int width() {
#if defined(TEST_WIDTH)
    return TEST_WIDTH;
#else
#error "Unknown or unset TEST_WIDTH value"
#endif
}

void ISPCLaunch(void **handle, void *f, void *d, int count0, int count1, int count2) {
    *handle = (void *)(uintptr_t)0xdeadbeef;
    typedef void (*TaskFuncType)(void *, int, int, int, int, int, int, int, int, int, int);
    TaskFuncType func = (TaskFuncType)f;
    int count = count0 * count1 * count2, idx = 0;
    for (int k = 0; k < count2; ++k)
        for (int j = 0; j < count1; ++j)
            for (int i = 0; i < count0; ++i)
                func(d, 0, 1, idx++, count, i, j, k, count0, count1, count2);
}

void ISPCSync(void *) {}

void *ISPCAlloc(void **handle, int64_t size, int32_t alignment) {
    *handle = (void *)(uintptr_t)0xdeadbeef;
    // and now, we leak...
#ifdef ISPC_IS_WINDOWS
    return _aligned_malloc(size, alignment);
#elif defined ISPC_IS_LINUX
    return memalign(alignment, size);
#elif defined ISPC_IS_APPLE || defined ISPC_IS_WASM
    void *mem = malloc(size + (alignment - 1) + sizeof(void *));
    char *amem = ((char *)mem) + sizeof(void *);
    amem = amem + uint32_t(alignment - (reinterpret_cast<uint64_t>(amem) & (alignment - 1)));
    ((void **)amem)[-1] = mem;
    return amem;
#else
#error "Host OS was not detected"
#endif
}

#if defined(_WIN32) || defined(_WIN64)
#define ALIGN __declspec(align(64))
#else
#define ALIGN __attribute__((aligned(64)))
#endif

#if TEST_SIG_DOT4ADD
template <typename T>
void pack4toint(const std::vector<T>& input, std::vector<int32_t>& packedOutput) {
    static_assert(std::is_same<T, uint8_t>::value || std::is_same<T, int8_t>::value,
                  "Template parameter T must be either uint8_t or int8_t.");

    packedOutput.resize(input.size() / 4);

    for (size_t i = 0; i < input.size(); i += 4) {
        int32_t packedValue = 0;
        packedValue = (static_cast<int32_t>(input[i+0]) << 24) |
           (static_cast<int32_t>(static_cast<uint8_t>(input[i+1])) << 16) |
           (static_cast<int32_t>(static_cast<uint8_t>(input[i+2])) << 8) |
           static_cast<int32_t>(static_cast<uint8_t>(input[i+3]));
        packedOutput[i / 4] = packedValue;
    }
}

#endif

int main(int argc, char *argv[]) {
    int w = width();
    assert(w <= 64);

    ALIGN float returned_result[ARRAY_SIZE];
    ALIGN float vfloat[ARRAY_SIZE];
    ALIGN double vdouble[ARRAY_SIZE];
    ALIGN int vint[ARRAY_SIZE];
    ALIGN int vint2[ARRAY_SIZE];
#if TEST_SIG_DOT4ADD
    std::vector<int8_t> dot4add_s_a(ARRAY_SIZE);
    std::vector<int8_t> dot4add_s_b(ARRAY_SIZE);
    std::vector<uint8_t> dot4add_u_a(ARRAY_SIZE);
    std::vector<uint8_t> dot4add_u_b(ARRAY_SIZE);
    std::vector<int32_t> dot4add_res(ARRAY_SIZE/4);
    std::vector<int32_t> dot4add_exp(ARRAY_SIZE/4);
#endif
    for (int i = 0; i < ARRAY_SIZE; ++i) {
        returned_result[i] = -1e20;
        vfloat[i] = i + 1;
        vdouble[i] = i + 1;
        vint[i] = 2 * (i + 1);
        vint2[i] = i + 5;
#if TEST_SIG_DOT4ADD
    dot4add_s_a[i] = static_cast<int8_t>(-i);
    dot4add_s_b[i] = static_cast<int8_t>(-i);
    dot4add_u_a[i] = static_cast<uint8_t>(i);
    dot4add_u_b[i] = static_cast<uint8_t>(i);
#endif
    }

#if TEST_SIG_DOT4ADD
    for (int i = 0; i < ARRAY_SIZE/4; ++i) {
        dot4add_res[i] = 0;
        dot4add_exp[i] = 0;
    }
    std::vector<int> dot4add_a_packed;
    pack4toint<uint8_t>(dot4add_u_a, dot4add_a_packed);
    std::vector<int> dot4add_b_packed;
    pack4toint<int8_t>(dot4add_s_b, dot4add_b_packed);
#endif
    float b = 5.;

#if (TEST_SIG == 0)
    f_v_cpu_entry_point(returned_result);
#elif (TEST_SIG == 1)
    f_f_cpu_entry_point(returned_result, vfloat);
#elif (TEST_SIG == 2)
    f_fu_cpu_entry_point(returned_result, vfloat, b);
#elif (TEST_SIG == 3)
    f_fi_cpu_entry_point(returned_result, vfloat, vint);
#elif (TEST_SIG == 4)
    f_du_cpu_entry_point(returned_result, vdouble, b);
#elif (TEST_SIG == 5)
    f_duf_cpu_entry_point(returned_result, vdouble, static_cast<float>(b));
#elif (TEST_SIG == 6)
    f_di_cpu_entry_point(returned_result, vdouble, vint2);
#elif (TEST_SIG == 7)
    *returned_result = sizeof(ispc::f_sz);
    w = 1;
#elif (TEST_SIG == 16)
    f_dot4add_u8i8_cpu_entry_point(dot4add_a_packed.data(), dot4add_b_packed.data(), dot4add_res.data());
#elif (TEST_SIG == 32)
    print_uf_cpu_entry_point(static_cast<float>(b));
#elif (TEST_SIG == 33)
    print_f_cpu_entry_point(vfloat);
#elif (TEST_SIG == 34)
    print_fuf_cpu_entry_point(vfloat, static_cast<float>(b));
#else
#error "Unknown or unset TEST_SIG value"
#endif

    float expected_result[ARRAY_SIZE];
    memset(expected_result, 0, ARRAY_SIZE * sizeof(float));
#if (TEST_SIG < 32) && (!TEST_SIG_DOT4ADD)
    result_cpu_entry_point(expected_result);
#elif (TEST_SIG == 16)
    result_u8i8_cpu_entry_point(dot4add_u_a.data(), dot4add_s_b.data(), dot4add_exp.data());
#else
    print_result_cpu_entry_point();
    return 0;
#endif

    int errors = 0;
#if (TEST_SIG_DOT4ADD)
    for (int i = 0; i < 64; ++i) {
        //if (dot4add_res[i] != dot4add_exp[i]) {
#ifdef EXPECT_FAILURE
            // bingo, failed
            return 1;
#else
            printf("%s: value %d disagrees: returned %d [%d], expected %d [%d]\n", argv[0], i, dot4add_res[i],
                   dot4add_res[i], dot4add_exp[i], dot4add_exp[i]);
            ++errors;
#endif // EXPECT_FAILURE
        //}
    }
#else
    for (int i = 0; i < w; ++i) {
        if (returned_result[i] != expected_result[i]) {
#ifdef EXPECT_FAILURE
            // bingo, failed
            return 1;
#else
            printf("%s: value %d disagrees: returned %f [%a], expected %f [%a]\n", argv[0], i, returned_result[i],
                   returned_result[i], expected_result[i], expected_result[i]);
            ++errors;
#endif // EXPECT_FAILURE
        }
    }
#endif
#ifdef EXPECT_FAILURE
    // Don't expect to get here
    return 0;
#else
    return errors > 0;
#endif
}
