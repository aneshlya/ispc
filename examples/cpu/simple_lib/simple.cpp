/*
  Copyright (c) 2025, Intel Corporation
  SPDX-License-Identifier: BSD-3-Clause
*/

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>

#include "ispc/ispc.h"

int main() {
    // Initialize ISPC
    std::cout << "Initializing ISPC...\n";
    if (!ispc::Initialize()) {
        std::cerr << "Error: Failed to initialize ISPC\n";
        return 1;
    }

    std::cout << "Compiling simple.ispc using library mode...\n";

    std::vector<std::string> args1 = {"ispc",          "simple.ispc", "--target=host", "-O2", "-o",
                                      "simple_ispc.o", "-h",          "simple_ispc.h"};

    int result = ispc::CompileFromArgs(args1);

    if (result == 0) {
        std::cout << "ISPC compilation successful!\n";
    } else {
        std::cerr << "ISPC compilation failed with code: " << result << "\n";
        ispc::Shutdown();
        return result;
    }

    std::cout << "Compiling simple.ispc using library mode with different options...\n";

    // Set up a second compilation with different options
    std::vector<std::string> args2 = {"ispc",           "simple.ispc", "--target=host",  "-O0", "--emit-asm", "-o",
                                      "simple_debug.s", "-h",          "simple_debug.h", "-g"};

    // Execute second compilation
    int result2 = ispc::CompileFromArgs(args2);

    if (result2 == 0) {
        std::cout << "Second compilation successful with different options\n";

        // Verify that files with different extensions were created
        if (std::filesystem::exists("simple_ispc.o") && std::filesystem::exists("simple_debug.s")) {
            std::cout << "SUCCESS: Both compilations produced different output files:\n";
            std::cout << "  - First compilation:  simple_ispc.o\n";
            std::cout << "  - Second compilation: simple_debug.s\n";
        } else {
            std::cout << "Note: Output files may not be visible in current directory\n";
        }
    } else {
        std::cerr << "Second ISPC compilation failed with code: " << result2 << "\n";
    }

    // Test global state corruption with multiple engines using different targets
    std::cout << "\nTesting global state management with multiple engines...\n";

    // Create multiple engines with different targets that would conflict
    std::vector<std::string> engineArgs1 = {"ispc",          "simple.ispc", "--target=sse2-i32x4", "-O2", "-o",
                                            "simple_sse2.o", "-h",          "simple_sse2.h"};

    std::vector<std::string> engineArgs2 = {"ispc",          "simple.ispc", "--target=avx2-i32x8", "-O0", "-o",
                                            "simple_avx2.o", "-h",          "simple_avx2.h"};

    std::vector<std::string> engineArgs3 = {"ispc",          "simple.ispc", "--target=host", "--emit-asm", "-o",
                                            "simple_host.s", "-h",          "simple_host.h"};

    // Create engines but don't execute yet - this tests target state isolation
    auto engine1 = ispc::ISPCEngine::CreateFromArgs(engineArgs1);
    auto engine2 = ispc::ISPCEngine::CreateFromArgs(engineArgs2);
    auto engine3 = ispc::ISPCEngine::CreateFromArgs(engineArgs3);

    if (!engine1 || !engine2 || !engine3) {
        std::cerr << "Failed to create one or more ISPC engines\n";
    } else {
        std::cout << "Created 3 engines with different targets\n";

        // Execute engines in sequence - this will expose global state conflicts
        std::cout << "Executing engine 1 (SSE2)...\n";
        int result1 = engine1->Execute();

        std::cout << "Executing engine 2 (AVX2)...\n";
        int result2 = engine2->Execute();

        std::cout << "Executing engine 3 (Host ASM)...\n";
        int result3 = engine3->Execute();

        if (result1 == 0 && result2 == 0 && result3 == 0) {
            std::cout << "SUCCESS: All engines executed without global state corruption!\n";
            std::cout << "  - SSE2 compilation: simple_sse2.o\n";
            std::cout << "  - AVX2 compilation: simple_avx2.o\n";
            std::cout << "  - Host ASM compilation: simple_host.s\n";
        } else {
            std::cerr << "FAILURE: Global state corruption detected!\n";
            std::cerr << "  Engine 1 result: " << result1 << "\n";
            std::cerr << "  Engine 2 result: " << result2 << "\n";
            std::cerr << "  Engine 3 result: " << result3 << "\n";
        }
    }

    // Additional stress test: rapid consecutive compilations
    std::cout << "\nStress testing with rapid consecutive compilations...\n";
    bool stress_success = true;
    for (int i = 0; i < 5; i++) {
        std::vector<std::string> stressArgs = {"ispc",
                                               "simple.ispc",
                                               "--target=host",
                                               "-O" + std::to_string(i % 3),
                                               "-o",
                                               "stress_" + std::to_string(i) + ".o"};

        int stress_result = ispc::CompileFromArgs(stressArgs);
        if (stress_result != 0) {
            std::cerr << "Stress test iteration " << i << " failed with code: " << stress_result << "\n";
            stress_success = false;
            break;
        }
    }

    if (stress_success) {
        std::cout << "Stress test passed: 5 rapid compilations successful\n";
    } else {
        std::cout << "Stress test failed: Global state corruption during rapid compilations\n";
    }

    // Extended test: Mixed output formats and preprocessor options
    std::cout << "\nTesting mixed output formats and preprocessor options...\n";

    std::vector<std::vector<std::string>> mixed_tests = {
        // Test 1: LLVM bitcode output with debug info
        {"ispc", "simple.ispc", "--target=host", "-O1", "--emit-llvm", "-g", "-o", "mixed_1.bc", "-h", "mixed_1.h"},

        // Test 2: Assembly with preprocessor defines
        {"ispc", "simple.ispc", "--target=sse4-i32x4", "--emit-asm", "-DDEBUG=1", "-DVERSION=2", "-o", "mixed_2.s"},

        // Test 3: Object with math library and optimization
        {"ispc", "simple.ispc", "--target=avx2-i32x8", "-O3", "--math-lib=fast", "-o", "mixed_3.o"},

        // Test 4: Header-only output with different preprocessor options
        {"ispc", "simple.ispc", "--target=host", "-h", "mixed_4.h", "--no-omit-frame-pointer"},

        // Test 5: Multiple defines and includes simulation
        {"ispc", "simple.ispc", "--target=avx2-i32x8", "-DUSE_DOUBLES=1", "-DMAX_SIZE=1024", "-O2", "-o", "mixed_5.o"},

        // Test 6: Different architecture with PIC
        {"ispc", "simple.ispc", "--target=sse2-i32x8", "--pic", "-O0", "-o", "mixed_6.o"},

        // Test 7: Assembly with line comments
        {"ispc", "simple.ispc", "--target=host", "--emit-asm", "-g", "-o", "mixed_7.s"},

        // Test 8: LLVM assembly (text format)
        {"ispc", "simple.ispc", "--target=avx2-i32x4", "--emit-llvm-text", "-O2", "-o", "mixed_8.ll"},
    };

    bool mixed_success = true;
    for (size_t i = 0; i < mixed_tests.size(); i++) {
        std::cout << "Mixed test " << (i + 1) << "/" << mixed_tests.size() << ": ";

        // Print test description
        std::string desc = "target=" + mixed_tests[i][3];
        if (std::find(mixed_tests[i].begin(), mixed_tests[i].end(), std::string("--emit-asm")) !=
            mixed_tests[i].end()) {
            desc += ", asm output";
        } else if (std::find(mixed_tests[i].begin(), mixed_tests[i].end(), std::string("--emit-llvm")) !=
                   mixed_tests[i].end()) {
            desc += ", llvm bitcode";
        } else if (std::find(mixed_tests[i].begin(), mixed_tests[i].end(), std::string("--emit-llvm-text")) !=
                   mixed_tests[i].end()) {
            desc += ", llvm assembly";
        } else {
            desc += ", object output";
        }
        std::cout << desc << "\n";

        int mixed_result = ispc::CompileFromArgs(mixed_tests[i]);
        if (mixed_result != 0) {
            std::cerr << "Mixed test " << (i + 1) << " failed with code: " << mixed_result << "\n";
            mixed_success = false;
            break;
        }
    }

    if (mixed_success) {
        std::cout << "All mixed format tests passed!\n";
    } else {
        std::cout << "Mixed format tests failed - potential state corruption\n";
    }

    // Engine-based mixed tests with different configurations
    std::cout << "\nTesting engine-based mixed configurations...\n";

    struct EngineTest {
        std::vector<std::string> args;
        std::string description;
    };

    std::vector<EngineTest> engine_tests = {
        {{"ispc", "simple.ispc", "--target=avx2-i32x4", "-O3", "-o", "engine_1.o"}, "AVX2 optimized"},
        {{"ispc", "simple.ispc", "--target=sse2-i32x8", "--emit-asm", "-DFAST_PATH=1", "-o", "engine_2.s"},
         "SSE2 assembly with defines"},
        {{"ispc", "simple.ispc", "--target=host", "-O1", "-o", "engine_3.o"}, "Host target optimized"},
    };

    std::vector<std::unique_ptr<ispc::ISPCEngine>> engines;
    bool engine_creation_success = true;

    // Create all engines first
    for (const auto &test : engine_tests) {
        auto engine = ispc::ISPCEngine::CreateFromArgs(test.args);
        if (!engine) {
            std::cerr << "Failed to create engine for: " << test.description << "\n";
            engine_creation_success = false;
            break;
        }
        engines.push_back(std::move(engine));
    }

    if (engine_creation_success) {
        std::cout << "Created " << engines.size() << " engines with different configurations\n";

        // Execute engines in sequence
        bool engine_execution_success = true;
        for (size_t i = 0; i < engines.size(); i++) {
            std::cout << "Executing: " << engine_tests[i].description << " ... ";
            std::cout.flush();
            int result = engines[i]->Execute();
            if (result != 0) {
                std::cout << "FAILED (code: " << result << ")\n";
                std::cerr << "Engine execution failed for: " << engine_tests[i].description << " (code: " << result
                          << ")\n";

                // For SVML test, try again in isolation to see if it's a state issue
                if (engine_tests[i].description.find("SVML") != std::string::npos) {
                    std::cout << "Retrying SVML test in isolation...\n";
                    auto retry_engine = ispc::ISPCEngine::CreateFromArgs(engine_tests[i].args);
                    if (retry_engine) {
                        int retry_result = retry_engine->Execute();
                        std::cout << "SVML retry result: " << retry_result << "\n";
                    }
                }

                engine_execution_success = false;
                break;
            } else {
                std::cout << "SUCCESS\n";
            }
        }

        if (engine_execution_success) {
            std::cout << "All engine configurations executed successfully!\n";
        } else {
            std::cout << "Engine execution tests failed\n";
        }
    }

    std::cout << "\nCleaning up...\n";
    ispc::Shutdown();
    std::cout << "ISPC shutdown complete\n";

    return 0;
}