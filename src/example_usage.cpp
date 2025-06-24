/*
  Copyright (c) 2010-2025, Intel Corporation

  SPDX-License-Identifier: BSD-3-Clause
*/

/** @file example_usage.cpp
    @brief Practical usage example for ISPCEngine library
*/

#include "ispc_engine.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <chrono>

/**
 * Example showing how to use ISPCEngine to compile and use ISPC kernels
 */
int main() {
    std::cout << "ISPCEngine Library Usage Example" << std::endl;
    std::cout << "=================================" << std::endl;
    
    // Initialize ISPC library
    if (!ispc::initializeISPC()) {
        std::cerr << "Error: Failed to initialize ISPC library" << std::endl;
        return 1;
    }
    
    // Create engine instance
    ispc::ISPCEngine engine;
    
    std::cout << "Using ISPC version: " << engine.getVersion() << std::endl;
    
    // Example 1: Basic vector addition kernel
    std::cout << "\n1. Compiling vector addition kernel..." << std::endl;
    
    std::string vectorAddKernel = R"(
        export void vector_add(uniform float a[], uniform float b[],
                              uniform float result[], uniform int count) {
            foreach (i = 0 ... count) {
                result[i] = a[i] + b[i];
            }
        }
        
        export void vector_multiply(uniform float a[], uniform float b[],
                                  uniform float result[], uniform int count) {
            foreach (i = 0 ... count) {
                result[i] = a[i] * b[i];
            }
        }
    )";
    
    // Configure compilation options
    ispc::ISPCEngine::Options options;
    options.target = "host";
    options.optimizationLevel = 2;
    options.generateDebugInfo = false;
    options.quiet = true;
    
    auto start = std::chrono::high_resolution_clock::now();
    auto result = engine.compileString(vectorAddKernel, options);
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    if (result.success) {
        std::cout << "✓ Compilation successful in " << duration.count() << "ms" << std::endl;
        std::cout << "  Generated object code: " << result.objectCode.size() << " bytes" << std::endl;
        std::cout << "  Generated header: " << result.headerContent.size() << " bytes" << std::endl;
        
        // Save object file
        std::ofstream objFile("vector_kernels.o", std::ios::binary);
        objFile.write(reinterpret_cast<const char*>(result.objectCode.data()), result.objectCode.size());
        objFile.close();
        std::cout << "  Saved object file: vector_kernels.o" << std::endl;
        
        // Save header file
        std::ofstream headerFile("vector_kernels.h");
        headerFile << result.headerContent;
        headerFile.close();
        std::cout << "  Saved header file: vector_kernels.h" << std::endl;
        
    } else {
        std::cout << "✗ Compilation failed: " << result.errorMessage << std::endl;
    }
    
    // Example 2: Mandelbrot kernel with different optimization levels
    std::cout << "\n2. Testing optimization levels with Mandelbrot kernel..." << std::endl;
    
    std::string mandelbrotKernel = R"(
        export void mandelbrot_ispc(uniform float x0, uniform float y0,
                                   uniform float x1, uniform float y1,
                                   uniform int width, uniform int height,
                                   uniform int maxIterations,
                                   uniform int output[]) {
            float dx = (x1 - x0) / width;
            float dy = (y1 - y0) / height;
            
            foreach (j = 0 ... height, i = 0 ... width) {
                float x = x0 + i * dx;
                float y = y0 + j * dy;
                
                int iter = 0;
                float zx = x, zy = y;
                
                while (iter < maxIterations && (zx*zx + zy*zy) < 4.0f) {
                    float new_zx = zx*zx - zy*zy + x;
                    zy = 2.0f * zx * zy + y;
                    zx = new_zx;
                    iter++;
                }
                
                output[j * width + i] = iter;
            }
        }
    )";
    
    for (int optLevel = 0; optLevel <= 3; ++optLevel) {
        ispc::ISPCEngine::Options optOptions = options;
        optOptions.optimizationLevel = optLevel;
        
        auto start = std::chrono::high_resolution_clock::now();
        auto result = engine.compileString(mandelbrotKernel, optOptions);
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        if (result.success) {
            std::cout << "  -O" << optLevel << ": ✓ " << result.objectCode.size() << " bytes (" 
                      << duration.count() << "ms)" << std::endl;
        } else {
            std::cout << "  -O" << optLevel << ": ✗ " << result.errorMessage << std::endl;
        }
    }
    
    // Example 3: Multi-target compilation for performance portability
    std::cout << "\n3. Multi-target compilation for different architectures..." << std::endl;
    
    auto targets = engine.getSupportedTargets();
    std::vector<std::string> selectedTargets;
    
    // Select a few interesting targets if available
    std::vector<std::string> preferredTargets = {"sse4", "avx2", "avx512skx"};
    for (const auto& preferred : preferredTargets) {
        if (engine.isTargetSupported(preferred)) {
            selectedTargets.push_back(preferred);
        }
    }
    
    // Fallback to first available target
    if (selectedTargets.empty() && !targets.empty()) {
        selectedTargets.push_back(targets[0]);
    }
    
    if (!selectedTargets.empty()) {
        std::cout << "  Compiling for targets: ";
        for (size_t i = 0; i < selectedTargets.size(); ++i) {
            std::cout << selectedTargets[i];
            if (i < selectedTargets.size() - 1) std::cout << ", ";
        }
        std::cout << std::endl;
        
        auto start = std::chrono::high_resolution_clock::now();
        auto multiResult = engine.compileMultiTarget(mandelbrotKernel, selectedTargets, options);
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        if (multiResult.success) {
            std::cout << "  ✓ Multi-target compilation successful (" << duration.count() << "ms)" << std::endl;
            std::cout << "    Dispatch code: " << multiResult.objectCode.size() << " bytes" << std::endl;
            
            for (const auto& pair : multiResult.targetSpecificCode) {
                std::cout << "    " << pair.first << " target: " << pair.second.size() << " bytes" << std::endl;
            }
            
            // Save multi-target files
            std::ofstream dispatchFile("mandelbrot_dispatch.o", std::ios::binary);
            dispatchFile.write(reinterpret_cast<const char*>(multiResult.objectCode.data()), 
                              multiResult.objectCode.size());
            dispatchFile.close();
            
            for (const auto& pair : multiResult.targetSpecificCode) {
                std::string filename = "mandelbrot_" + pair.first + ".o";
                std::ofstream targetFile(filename, std::ios::binary);
                targetFile.write(reinterpret_cast<const char*>(pair.second.data()), pair.second.size());
                targetFile.close();
                std::cout << "    Saved: " << filename << std::endl;
            }
            
        } else {
            std::cout << "  ✗ Multi-target compilation failed: " << multiResult.errorMessage << std::endl;
        }
    } else {
        std::cout << "  No suitable targets available for multi-target compilation" << std::endl;
    }
    
    // Example 4: Compilation options demonstration
    std::cout << "\n4. Advanced compilation options..." << std::endl;
    
    std::string mathKernel = R"(
        export void math_operations(uniform float input[], uniform float output[], 
                                   uniform int count) {
            foreach (i = 0 ... count) {
                float x = input[i];
                output[i] = sin(x) * cos(x) + sqrt(abs(x));
            }
        }
    )";
    
    // Test different math libraries and options
    std::vector<std::pair<std::string, ispc::ISPCEngine::Options>> testConfigs = {
        {"Standard math", options},
        {"Fast math", [&]() {
            auto opts = options;
            opts.fastMath = true;
            return opts;
        }()},
        {"SVML math", [&]() {
            auto opts = options;
            opts.mathLibrary = ispc::ISPCEngine::Options::MathLib::SVML;
            return opts;
        }()},
        {"With debug info", [&]() {
            auto opts = options;
            opts.generateDebugInfo = true;
            return opts;
        }()},
        {"Position independent", [&]() {
            auto opts = options;
            opts.generatePIC = true;
            return opts;
        }()}
    };
    
    for (const auto& config : testConfigs) {
        auto start = std::chrono::high_resolution_clock::now();
        auto result = engine.compileString(mathKernel, config.second);
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        if (result.success) {
            std::cout << "  " << config.first << ": ✓ " << result.objectCode.size() 
                      << " bytes (" << duration.count() << "ms)" << std::endl;
        } else {
            std::cout << "  " << config.first << ": ✗ " << result.errorMessage << std::endl;
        }
    }
    
    // Example 5: File compilation
    std::cout << "\n5. File-based compilation..." << std::endl;
    
    std::string filename = "example_kernel.ispc";
    std::ofstream sourceFile(filename);
    sourceFile << R"(
        // Example ISPC kernel demonstrating various features
        
        struct Point3D {
            float x, y, z;
        };
        
        export void transform_points(uniform Point3D input[], 
                                   uniform Point3D output[],
                                   uniform int count,
                                   uniform float scale) {
            foreach (i = 0 ... count) {
                output[i].x = input[i].x * scale;
                output[i].y = input[i].y * scale;
                output[i].z = input[i].z * scale;
            }
        }
        
        export void dot_product(uniform float a[], uniform float b[],
                               uniform float result[], uniform int count) {
            varying float sum = 0.0f;
            foreach (i = 0 ... count) {
                sum += a[i] * b[i];
            }
            result[programIndex] = sum;
        }
    )";
    sourceFile.close();
    
    auto start = std::chrono::high_resolution_clock::now();
    auto fileResult = engine.compileFile(filename, options);
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    if (fileResult.success) {
        std::cout << "  ✓ File compilation successful (" << duration.count() << "ms)" << std::endl;
        std::cout << "    Object code: " << fileResult.objectCode.size() << " bytes" << std::endl;
        std::cout << "    Header content: " << fileResult.headerContent.size() << " bytes" << std::endl;
        
        // Show part of the generated header
        std::cout << "    Generated header preview:" << std::endl;
        std::istringstream headerStream(fileResult.headerContent);
        std::string line;
        int lineCount = 0;
        while (std::getline(headerStream, line) && lineCount < 10) {
            if (!line.empty() && line.find("//") != 0) { // Skip empty lines and comments
                std::cout << "      " << line << std::endl;
                lineCount++;
            }
        }
        
    } else {
        std::cout << "  ✗ File compilation failed: " << fileResult.errorMessage << std::endl;
    }
    
    // Clean up
    std::remove(filename.c_str());
    
    std::cout << "\n6. System information..." << std::endl;
    std::cout << "  Available targets: " << engine.getSupportedTargets().size() << std::endl;
    std::cout << "  Available architectures: " << engine.getSupportedArchitectures().size() << std::endl;
    std::cout << "  Available OS targets: " << engine.getSupportedOS().size() << std::endl;
    
    std::cout << "\nExample completed successfully!" << std::endl;
    std::cout << "\nGenerated files:" << std::endl;
    std::cout << "  - vector_kernels.o (vector addition object code)" << std::endl;
    std::cout << "  - vector_kernels.h (corresponding header)" << std::endl;
    std::cout << "  - mandelbrot_dispatch.o (multi-target dispatch code)" << std::endl;
    std::cout << "  - mandelbrot_*.o (target-specific object files)" << std::endl;
    
    std::cout << "\nThese files can be linked with your C/C++ application." << std::endl;
    
    // Shutdown ISPC library
    ispc::shutdownISPC();
    
    return 0;
}