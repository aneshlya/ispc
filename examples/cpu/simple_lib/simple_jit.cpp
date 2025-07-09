/*
  Copyright (c) 2025, Intel Corporation
  SPDX-License-Identifier: BSD-3-Clause
*/

#include <chrono>
#include <cmath>
#include <iostream>
#include <vector>

#include "ispc/ispc.h"

// Function signature for the ISPC 'simple' function
typedef void (*SimpleFunc)(float[], float[], int);

// Function signature for the ISPC 'multiply' function
typedef void (*MultiplyFunc)(float[], float[], int);

// Helper function to initialize test data
void initializeTestData(std::vector<float> &input, int count) {
    input.resize(count);
    for (int i = 0; i < count; ++i) {
        input[i] = static_cast<float>(i) * 0.5f;
    }
}

// Helper function to verify results
bool verifyResults(const std::vector<float> &input, const std::vector<float> &output, int count, const char *testName) {
    std::cout << "Verifying " << testName << " results...\n";

    bool success = true;
    for (int i = 0; i < count && i < 10; ++i) { // Check first 10 elements
        float expected;
        if (input[i] < 3.0f) {
            expected = input[i] * input[i];
        } else {
            expected = std::sqrt(input[i]);
        }

        if (std::abs(output[i] - expected) > 1e-6f) {
            std::cerr << "Mismatch at index " << i << ": expected " << expected << ", got " << output[i] << std::endl;
            success = false;
        }
    }

    if (success) {
        std::cout << "SUCCESS: " << testName << " verification passed!\n";
    } else {
        std::cout << "Error: " << testName << " verification failed!\n";
    }

    return success;
}

int main() {
    // Initialize ISPC
    std::cout << "=== ISPC JIT Compilation Demo ===\n";
    std::cout << "Initializing ISPC...\n";
    if (!ispc::Initialize()) {
        std::cerr << "Error: Failed to initialize ISPC\n";
        return 1;
    }

    // Test data
    const int count = 1000;
    std::vector<float> input, output1, output2;
    initializeTestData(input, count);
    output1.resize(count);
    output2.resize(count);

    std::cout << "\n=== Test 1: JIT Compilation from File ===\n";

    // Create engine for JIT compilation
    std::vector<const char *> jitArgs = {"ispc", "--target=host", "-O2"};
    auto engine =
        ispc::ISPCEngine::CreateFromArgs(static_cast<int>(jitArgs.size()), const_cast<char **>(jitArgs.data()));
    if (!engine) {
        std::cerr << "Error: Failed to create ISPC engine\n";
        ispc::Shutdown();
        return 1;
    }

    // JIT compile from file
    std::cout << "JIT compiling simple.ispc from file...\n";
    auto start = std::chrono::high_resolution_clock::now();

    int jitResult = engine->CompileFromFile("simple.ispc");

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    if (jitResult != 0) {
        std::cerr << "Error: JIT compilation from file failed\n";
        ispc::Shutdown();
        return 1;
    }

    std::cout << "JIT compilation from file completed in " << duration.count() << "ms\n";
    std::cout << "JIT mode active: " << (engine->IsJitMode() ? "Yes" : "No") << std::endl;

    // Get the JIT-compiled function
    SimpleFunc simpleJit = reinterpret_cast<SimpleFunc>(engine->GetFunction("simple"));
    if (!simpleJit) {
        std::cerr << "Error: Failed to get 'simple' function from JIT\n";
        ispc::Shutdown();
        return 1;
    }

    // Execute the JIT-compiled function
    std::cout << "Executing JIT-compiled function from file...\n";
    start = std::chrono::high_resolution_clock::now();

    simpleJit(input.data(), output1.data(), count);

    end = std::chrono::high_resolution_clock::now();
    auto execDuration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "Execution completed in " << execDuration.count() << "μs\n";

    // Verify results
    verifyResults(input, output1, count, "file-based JIT");

    std::cout << "\n=== Test 2: Multiple Function Access ===\n";

    // Test accessing the function multiple times
    std::cout << "Testing multiple function calls...\n";
    SimpleFunc simpleJit2 = reinterpret_cast<SimpleFunc>(engine->GetFunction("simple"));
    if (simpleJit2) {
        std::cout << "SUCCESS: 'simple' function accessible for multiple calls\n";

        // Quick test with different data
        std::vector<float> testInput = {1.0f, 2.0f, 4.0f, 5.0f};
        std::vector<float> testOutput(4);
        simpleJit2(testInput.data(), testOutput.data(), 4);

        std::cout << "Test results: ";
        for (int i = 0; i < 4; ++i) {
            std::cout << testInput[i] << "->" << testOutput[i] << " ";
        }
        std::cout << std::endl;

        // Execute the function again with original data
        std::cout << "Re-executing with original dataset...\n";
        simpleJit2(input.data(), output2.data(), count);

        // Verify results match the first execution
        bool resultsMatch = true;
        for (int i = 0; i < count; ++i) {
            if (std::abs(output1[i] - output2[i]) > 1e-9f) {
                resultsMatch = false;
                break;
            }
        }

        if (resultsMatch) {
            std::cout << "SUCCESS: Multiple executions produce consistent results\n";
        } else {
            std::cout << "Error: Multiple executions produce inconsistent results\n";
        }
    } else {
        std::cout << "Error: 'simple' function not accessible\n";
    }

    std::cout << "\n=== Performance Summary ===\n";
    std::cout << "Data size: " << count << " elements\n";
    std::cout << "All functions executed successfully using JIT compilation!\n";
    std::cout << "JIT mode: " << (engine->IsJitMode() ? "Active" : "Inactive") << std::endl;

    // Optional: Clear JIT code and test
    std::cout << "\n=== Test 3: JIT Code Cleanup ===\n";
    std::cout << "Clearing JIT code...\n";
    engine->ClearJitCode();
    std::cout << "JIT mode after clear: " << (engine->IsJitMode() ? "Active" : "Inactive") << std::endl;

    // Try to access function after clear (should fail)
    SimpleFunc simpleAfterClear = reinterpret_cast<SimpleFunc>(engine->GetFunction("simple"));
    if (!simpleAfterClear) {
        std::cout << "SUCCESS: Functions correctly cleared from JIT\n";
    } else {
        std::cout << "Error: Functions still accessible after clear\n";
    }

    std::cout << "\nCleaning up...\n";
    ispc::Shutdown();
    std::cout << "ISPC shutdown complete\n";
    std::cout << "\n=== JIT Demo Complete ===\n";

    return 0;
}