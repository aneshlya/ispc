/*
  Copyright (c) 2010-2025, Intel Corporation

  SPDX-License-Identifier: BSD-3-Clause
*/

/** @file test_ispc_engine.cpp
    @brief Simple test program for ISPCEngine library interface
*/

#include "ispc_engine.h"
#include <iostream>
#include <fstream>
#include <sstream>

int main() {
    std::cout << "Testing ISPCEngine library interface..." << std::endl;
    
    // Initialize ISPC library
    if (!ispc::initializeISPC()) {
        std::cerr << "Failed to initialize ISPC library" << std::endl;
        return 1;
    }
    
    // Create engine instance
    ispc::ISPCEngine engine;
    
    // Test basic functionality
    std::cout << "ISPC Version: " << engine.getVersion() << std::endl;
    
    // Get supported targets
    auto targets = engine.getSupportedTargets();
    std::cout << "Supported targets (" << targets.size() << "): ";
    for (size_t i = 0; i < targets.size(); ++i) {
        std::cout << targets[i];
        if (i < targets.size() - 1) std::cout << ", ";
    }
    std::cout << std::endl;
    
    // Get supported architectures
    auto archs = engine.getSupportedArchitectures();
    std::cout << "Supported architectures (" << archs.size() << "): ";
    for (size_t i = 0; i < archs.size(); ++i) {
        std::cout << archs[i];
        if (i < archs.size() - 1) std::cout << ", ";
    }
    std::cout << std::endl;
    
    // Test simple ISPC code compilation
    std::string simpleKernel = R"(
        export void simple_add(uniform float a[], uniform float b[],
                              uniform float result[], uniform int count) {
            foreach (i = 0 ... count) {
                result[i] = a[i] + b[i];
            }
        }
    )";
    
    std::cout << "\nTesting string compilation..." << std::endl;
    
    // Set up compilation options
    ispc::ISPCEngine::Options options;
    options.target = "host";
    options.optimizationLevel = 2;
    options.quiet = true; // Suppress compiler output for clean test output
    
    // Test compilation
    auto result = engine.compileString(simpleKernel, options);
    
    if (result.success) {
        std::cout << "✓ String compilation succeeded!" << std::endl;
        std::cout << "  Object code size: " << result.objectCode.size() << " bytes" << std::endl;
        std::cout << "  Header size: " << result.headerContent.size() << " bytes" << std::endl;
        
        // Show a snippet of the generated header
        if (!result.headerContent.empty()) {
            std::cout << "  Header snippet:" << std::endl;
            std::istringstream headerStream(result.headerContent);
            std::string line;
            int lineCount = 0;
            while (std::getline(headerStream, line) && lineCount < 5) {
                std::cout << "    " << line << std::endl;
                lineCount++;
            }
            if (lineCount == 5) {
                std::cout << "    ..." << std::endl;
            }
        }
    } else {
        std::cout << "✗ String compilation failed: " << result.errorMessage << std::endl;
    }
    
    // Test target support checking
    std::cout << "\nTesting target support..." << std::endl;
    std::vector<std::string> testTargets = {"host", "sse4", "avx2", "invalid_target"};
    for (const auto& target : testTargets) {
        bool supported = engine.isTargetSupported(target);
        std::cout << "  " << target << ": " << (supported ? "✓ supported" : "✗ not supported") << std::endl;
    }
    
    // Test file compilation if we have a test file
    std::cout << "\nTesting file compilation..." << std::endl;
    
    // Create a temporary ISPC file
    std::string testFileName = "test_kernel.ispc";
    std::ofstream testFile(testFileName);
    if (testFile.is_open()) {
        testFile << simpleKernel;
        testFile.close();
        
        auto fileResult = engine.compileFile(testFileName, options);
        
        if (fileResult.success) {
            std::cout << "✓ File compilation succeeded!" << std::endl;
            std::cout << "  Object code size: " << fileResult.objectCode.size() << " bytes" << std::endl;
        } else {
            std::cout << "✗ File compilation failed: " << fileResult.errorMessage << std::endl;
        }
        
        // Clean up test file
        std::remove(testFileName.c_str());
    } else {
        std::cout << "✗ Could not create test file" << std::endl;
    }
    
    // Test multi-target compilation
    if (!targets.empty()) {
        std::cout << "\nTesting multi-target compilation..." << std::endl;
        
        // Select first few available targets for testing
        std::vector<std::string> multiTargets;
        for (size_t i = 0; i < std::min(size_t(3), targets.size()); ++i) {
            multiTargets.push_back(targets[i]);
        }
        
        auto multiResult = engine.compileMultiTarget(simpleKernel, multiTargets, options);
        
        if (multiResult.success) {
            std::cout << "✓ Multi-target compilation succeeded!" << std::endl;
            std::cout << "  Main object code size: " << multiResult.objectCode.size() << " bytes" << std::endl;
            std::cout << "  Target-specific code for " << multiResult.targetSpecificCode.size() << " targets:" << std::endl;
            for (const auto& pair : multiResult.targetSpecificCode) {
                std::cout << "    " << pair.first << ": " << pair.second.size() << " bytes" << std::endl;
            }
        } else {
            std::cout << "✗ Multi-target compilation failed: " << multiResult.errorMessage << std::endl;
        }
    }
    
    // Test error handling
    std::cout << "\nTesting error handling..." << std::endl;
    
    std::string invalidKernel = R"(
        export void invalid_function() {
            this_function_does_not_exist();
        }
    )";
    
    auto errorResult = engine.compileString(invalidKernel, options);
    if (!errorResult.success) {
        std::cout << "✓ Error handling works (expected failure)" << std::endl;
        std::cout << "  Error message: " << errorResult.errorMessage << std::endl;
    } else {
        std::cout << "✗ Error handling failed (unexpected success)" << std::endl;
    }
    
    // Test with different optimization levels
    std::cout << "\nTesting different optimization levels..." << std::endl;
    for (int optLevel = 0; optLevel <= 3; ++optLevel) {
        ispc::ISPCEngine::Options optOptions = options;
        optOptions.optimizationLevel = optLevel;
        
        auto optResult = engine.compileString(simpleKernel, optOptions);
        if (optResult.success) {
            std::cout << "  -O" << optLevel << ": ✓ (" << optResult.objectCode.size() << " bytes)" << std::endl;
        } else {
            std::cout << "  -O" << optLevel << ": ✗ " << optResult.errorMessage << std::endl;
        }
    }
    
    std::cout << "\nAll tests completed!" << std::endl;
    
    // Shutdown ISPC library
    ispc::shutdownISPC();
    
    return 0;
}