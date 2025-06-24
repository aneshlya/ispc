/*
  Copyright (c) 2010-2025, Intel Corporation

  SPDX-License-Identifier: BSD-3-Clause
*/

/** @file ispc_engine.h
    @brief ISPCEngine library interface for programmatic ISPC compilation
*/

#ifndef ISPC_ENGINE_H
#define ISPC_ENGINE_H

#include <string>
#include <vector>
#include <memory>
#include <map>

namespace ispc {

/**
 * ISPC Compilation Engine
 * 
 * Provides programmatic access to ISPC compilation functionality.
 * Allows compiling ISPC source code from strings or files and
 * retrieving the generated machine code, headers, and other artifacts.
 */
class ISPCEngine {
public:
    /**
     * Compilation options for controlling code generation
     */
    struct Options {
        // Target specification
        std::string target = "host";           // Target ISA (sse4, avx2, avx512, etc.)
        std::string cpu = "";                  // Specific CPU type (core-i7, cortex-a57, etc.)
        std::string arch = "x86-64";           // Target architecture (x86-64, arm64, etc.)
        std::string targetOS = "";             // Target operating system (windows, linux, etc.)
        
        // Optimization settings
        int optimizationLevel = 2;             // Optimization level (0=none, 1=size, 2/3=speed)
        bool fastMath = false;                 // Enable fast math optimizations
        bool disableAsserts = false;           // Remove assertion statements from code
        bool forceAlignedMemory = false;       // Force aligned memory operations
        
        // Code generation options
        bool generatePIC = false;              // Generate position-independent code
        bool generateDebugInfo = false;        // Include debugging information
        int dwarfVersion = 4;                  // DWARF debug format version
        
        // Preprocessor settings
        std::vector<std::string> includePaths; // Additional include directories
        std::vector<std::string> defines;      // Preprocessor definitions
        
        // Compilation behavior
        bool includeStdlib = true;             // Include ISPC standard library
        bool quiet = false;                    // Suppress compiler output
        bool warnAsError = false;              // Treat warnings as errors
        bool disableWarnings = false;          // Suppress all warnings
        
        // Math library selection
        enum class MathLib { Default, Fast, SVML, System };
        MathLib mathLibrary = MathLib::Default; // Math library implementation to use
    };

    /**
     * Result of ISPC compilation containing generated code and metadata
     */
    struct Result {
        bool success = false;                  // Whether compilation succeeded
        std::string errorMessage;              // Error description if compilation failed
        
        // Generated code in various formats
        std::vector<uint8_t> objectCode;       // Native object file data
        std::vector<uint8_t> bitcodeData;      // LLVM bitcode binary data
        std::string bitcodeText;               // LLVM IR in textual form
        std::string headerContent;             // C/C++ header file content
        std::string assemblyCode;              // Assembly language output
        
        // Runtime information for JIT scenarios
        std::map<std::string, void*> exportedSymbols; // Function and variable addresses
        std::vector<std::string> dependencies;        // File dependencies
        
        // Multi-target compilation results
        std::map<std::string, std::vector<uint8_t>> targetSpecificCode; // Per-target object code
    };

    /**
     * Create a new ISPC compilation engine
     */
    ISPCEngine();
    
    /**
     * Destroy the compilation engine and clean up resources
     */
    ~ISPCEngine();
    
    /**
     * Compile ISPC source code from a string
     * 
     * @param sourceCode ISPC source code to compile
     * @param options Compilation options
     * @return Compilation result with generated code or error information
     */
    Result compileString(const std::string& sourceCode, 
                        const Options& options);
    Result compileString(const std::string& sourceCode);
    
    /**
     * Compile ISPC source code from a file
     * 
     * @param filename Path to ISPC source file
     * @param options Compilation options
     * @return Compilation result with generated code or error information
     */
    Result compileFile(const std::string& filename, 
                      const Options& options);
    Result compileFile(const std::string& filename);
    
    /**
     * Compile source code for multiple target architectures
     * 
     * @param sourceCode ISPC source code to compile
     * @param targets List of target ISAs to compile for
     * @param options Base compilation options
     * @return Compilation result with code for all targets
     */
    Result compileMultiTarget(const std::string& sourceCode,
                             const std::vector<std::string>& targets,
                             const Options& options);
    Result compileMultiTarget(const std::string& sourceCode,
                             const std::vector<std::string>& targets);
    
    /**
     * Get list of supported target ISAs
     * 
     * @return Vector of target names (e.g., "sse4", "avx2", "avx512")
     */
    std::vector<std::string> getSupportedTargets() const;
    
    /**
     * Get list of supported architectures
     * 
     * @return Vector of architecture names (e.g., "x86-64", "arm64")
     */
    std::vector<std::string> getSupportedArchitectures() const;
    
    /**
     * Get list of supported operating systems
     * 
     * @return Vector of OS names (e.g., "windows", "linux", "macos")
     */
    std::vector<std::string> getSupportedOS() const;
    
    /**
     * Get ISPC compiler version string
     * 
     * @return Version information
     */
    std::string getVersion() const;
    
    /**
     * Check if a target ISA is supported
     * 
     * @param target Target ISA name
     * @return True if target is supported
     */
    bool isTargetSupported(const std::string& target) const;
    
    /**
     * Add an include directory to search path
     * 
     * @param path Directory path to add
     */
    void addIncludePath(const std::string& path);
    
    /**
     * Add a preprocessor definition
     * 
     * @param define Definition in form "NAME" or "NAME=VALUE"
     */
    void addDefine(const std::string& define);
    
    /**
     * Set the optimization level for subsequent compilations
     * 
     * @param level Optimization level (0-3)
     */
    void setOptimizationLevel(int level);
    
private:
    class Implementation;
    std::unique_ptr<Implementation> impl_;
    
    // Non-copyable
    ISPCEngine(const ISPCEngine&) = delete;
    ISPCEngine& operator=(const ISPCEngine&) = delete;
};

/**
 * Initialize the ISPC library system
 * 
 * Must be called once before using ISPCEngine instances.
 * 
 * @return True if initialization succeeded
 */
bool initializeISPC();

/**
 * Shutdown the ISPC library system
 * 
 * Should be called once when done using ISPC functionality.
 */
void shutdownISPC();

/**
 * Get the version of the ISPC compiler
 * 
 * @return Version string
 */
std::string getISPCVersion();

/**
 * Get list of all available compilation targets
 * 
 * @return Vector of target ISA names
 */
std::vector<std::string> getAvailableTargets();

} // namespace ispc

#endif // ISPC_ENGINE_H