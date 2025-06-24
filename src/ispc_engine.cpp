/*
  Copyright (c) 2010-2025, Intel Corporation

  SPDX-License-Identifier: BSD-3-Clause
*/

/** @file ispc_engine.cpp
    @brief Implementation of ISPCEngine library interface
*/

#include "ispc_engine.h"
#include "ispc.h"
#include "module.h"
#include "target_registry.h"
#include "target_enums.h"
#include "util.h"

#include <memory>
#include <sstream>
#include <fstream>
#include <cstdio>
#include <cstdlib>

#ifdef ISPC_HOST_IS_WINDOWS
#include <io.h>
#include <fcntl.h>
#else
#include <unistd.h>
#endif

#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/FileSystem.h>

namespace ispc {

// Forward declarations for LLVM initialization functions
#ifdef ISPC_X86_ENABLED
extern "C" {
void LLVMInitializeX86TargetInfo();
void LLVMInitializeX86Target();
void LLVMInitializeX86AsmPrinter();
void LLVMInitializeX86AsmParser();
void LLVMInitializeX86Disassembler();
void LLVMInitializeX86TargetMC();
}
#endif

#ifdef ISPC_ARM_ENABLED
extern "C" {
void LLVMInitializeARMTargetInfo();
void LLVMInitializeARMTarget();
void LLVMInitializeARMAsmPrinter();
void LLVMInitializeARMAsmParser();
void LLVMInitializeARMDisassembler();
void LLVMInitializeARMTargetMC();
void LLVMInitializeAArch64TargetInfo();
void LLVMInitializeAArch64Target();
void LLVMInitializeAArch64AsmPrinter();
void LLVMInitializeAArch64AsmParser();
void LLVMInitializeAArch64Disassembler();
void LLVMInitializeAArch64TargetMC();
}
#endif

#ifdef ISPC_WASM_ENABLED
extern "C" {
void LLVMInitializeWebAssemblyAsmParser();
void LLVMInitializeWebAssemblyAsmPrinter();
void LLVMInitializeWebAssemblyDisassembler();
void LLVMInitializeWebAssemblyTarget();
void LLVMInitializeWebAssemblyTargetInfo();
void LLVMInitializeWebAssemblyTargetMC();
}
#endif

/**
 * Implementation class for ISPCEngine
 * 
 * This class manages the global state and provides the actual compilation functionality.
 * It wraps the existing ISPC Module class and handles global initialization.
 */
class ISPCEngine::Implementation {
public:
    Implementation() {
        // Store original global state if it exists
        originalGlobals_ = g;
        originalModule_ = m;
        
        // Initialize LLVM targets once
        static bool llvmInitialized = false;
        if (!llvmInitialized) {
            initializeLLVMTargets();
            llvmInitialized = true;
        }
    }
    
    ~Implementation() {
        // Restore original global state
        if (g != originalGlobals_) {
            delete g;
        }
        g = originalGlobals_;
        m = originalModule_;
    }
    
    ISPCEngine::Result compileString(const std::string& sourceCode, const ISPCEngine::Options& options) {
        // Create temporary file for string compilation
        std::string tempFileName = createTempFile(sourceCode);
        if (tempFileName.empty()) {
            ISPCEngine::Result result;
            result.success = false;
            result.errorMessage = "Failed to create temporary file for string compilation";
            return result;
        }
        
        // Compile the temporary file
        ISPCEngine::Result result = compileFile(tempFileName, options);
        
        // Clean up temporary file
        std::remove(tempFileName.c_str());
        
        return result;
    }
    
    ISPCEngine::Result compileFile(const std::string& filename, const ISPCEngine::Options& options) {
        ISPCEngine::Result result;
        
        try {
            // Create and configure globals for this compilation
            std::unique_ptr<Globals> tempGlobals = std::make_unique<Globals>();
            Globals* savedGlobals = g;
            g = tempGlobals.get();
            
            // Configure globals from options
            configureGlobals(options);
            
            // Parse target and architecture
            std::vector<ISPCTarget> targets;
            auto parseResult = ParseISPCTargets(options.target.c_str());
            targets = parseResult.first;
            if (!parseResult.second.empty()) {
                result.success = false;
                result.errorMessage = "Invalid target: " + parseResult.second;
                g = savedGlobals;
                return result;
            }
            
            Arch arch = ParseArch(options.arch.c_str());
            if (arch == Arch::error) {
                result.success = false;
                result.errorMessage = "Invalid architecture: " + options.arch;
                g = savedGlobals;
                return result;
            }
            
            // Set up output configuration
            Module::Output output;
            setupOutput(output, options);
            
            // Compile using existing Module::CompileAndOutput
            int errorCount = Module::CompileAndOutput(filename.c_str(), arch, 
                                                     options.cpu.c_str(), targets, output);
            
            if (errorCount == 0) {
                result.success = true;
                extractResults(result, output, targets[0]);
            } else {
                result.success = false;
                result.errorMessage = "Compilation failed with " + std::to_string(errorCount) + " errors";
            }
            
            // Restore globals
            g = savedGlobals;
            
        } catch (const std::exception& e) {
            result.success = false;
            result.errorMessage = std::string("Exception during compilation: ") + e.what();
        }
        
        return result;
    }
    
    ISPCEngine::Result compileMultiTarget(const std::string& sourceCode,
                                         const std::vector<std::string>& targets,
                                         const ISPCEngine::Options& options) {
        ISPCEngine::Result result;
        
        // Create temporary file for string compilation
        std::string tempFileName = createTempFile(sourceCode);
        if (tempFileName.empty()) {
            result.success = false;
            result.errorMessage = "Failed to create temporary file for multi-target compilation";
            return result;
        }
        
        try {
            // Create and configure globals
            std::unique_ptr<Globals> tempGlobals = std::make_unique<Globals>();
            Globals* savedGlobals = g;
            g = tempGlobals.get();
            
            configureGlobals(options);
            
            // Parse targets
            std::vector<ISPCTarget> ispcTargets;
            for (const auto& targetStr : targets) {
                auto parseResult = ParseISPCTargets(targetStr.c_str());
                if (!parseResult.second.empty()) {
                    result.success = false;
                    result.errorMessage = "Invalid target: " + parseResult.second;
                    g = savedGlobals;
                    std::remove(tempFileName.c_str());
                    return result;
                }
                ispcTargets.insert(ispcTargets.end(), parseResult.first.begin(), parseResult.first.end());
            }
            
            // Set multi-target compilation flag
            g->isMultiTargetCompilation = true;
            
            Arch arch = ParseArch(options.arch.c_str());
            if (arch == Arch::error) {
                result.success = false;
                result.errorMessage = "Invalid architecture: " + options.arch;
                g = savedGlobals;
                std::remove(tempFileName.c_str());
                return result;
            }
            
            // Set up output configuration
            Module::Output output;
            setupOutput(output, options);
            
            // Compile for multiple targets
            int errorCount = Module::CompileAndOutput(tempFileName.c_str(), arch, 
                                                     options.cpu.c_str(), ispcTargets, output);
            
            if (errorCount == 0) {
                result.success = true;
                extractMultiTargetResults(result, output, ispcTargets);
            } else {
                result.success = false;
                result.errorMessage = "Multi-target compilation failed with " + std::to_string(errorCount) + " errors";
            }
            
            // Restore globals
            g = savedGlobals;
            
        } catch (const std::exception& e) {
            result.success = false;
            result.errorMessage = std::string("Exception during multi-target compilation: ") + e.what();
        }
        
        // Clean up temporary file
        std::remove(tempFileName.c_str());
        
        return result;
    }
    
    std::vector<std::string> getSupportedTargets() const {
        if (!g || !g->target_registry) {
            return {};
        }
        
        std::string targetsStr = g->target_registry->getSupportedTargets();
        return parseTargetList(targetsStr);
    }
    
    std::vector<std::string> getSupportedArchitectures() const {
        if (!g || !g->target_registry) {
            return {};
        }
        
        std::string archStr = g->target_registry->getSupportedArchs();
        return parseTargetList(archStr);
    }
    
    std::vector<std::string> getSupportedOS() const {
        if (!g || !g->target_registry) {
            return {};
        }
        
        std::string osStr = g->target_registry->getSupportedOSes();
        return parseTargetList(osStr);
    }
    
    std::string getVersion() const {
        return ISPC_VERSION_STRING;
    }
    
    bool isTargetSupported(const std::string& target) const {
        auto targets = getSupportedTargets();
        return std::find(targets.begin(), targets.end(), target) != targets.end();
    }

private:
    Globals* originalGlobals_ = nullptr;
    Module* originalModule_ = nullptr;
    
    void initializeLLVMTargets() {
        // Initialize LLVM targets (extracted from main.cpp)
#ifdef ISPC_X86_ENABLED
        LLVMInitializeX86TargetInfo();
        LLVMInitializeX86Target();
        LLVMInitializeX86AsmPrinter();
        LLVMInitializeX86AsmParser();
        LLVMInitializeX86Disassembler();
        LLVMInitializeX86TargetMC();
#endif

#ifdef ISPC_ARM_ENABLED
        LLVMInitializeARMTargetInfo();
        LLVMInitializeARMTarget();
        LLVMInitializeARMAsmPrinter();
        LLVMInitializeARMAsmParser();
        LLVMInitializeARMDisassembler();
        LLVMInitializeARMTargetMC();

        LLVMInitializeAArch64TargetInfo();
        LLVMInitializeAArch64Target();
        LLVMInitializeAArch64AsmPrinter();
        LLVMInitializeAArch64AsmParser();
        LLVMInitializeAArch64Disassembler();
        LLVMInitializeAArch64TargetMC();
#endif

#ifdef ISPC_WASM_ENABLED
        LLVMInitializeWebAssemblyAsmParser();
        LLVMInitializeWebAssemblyAsmPrinter();
        LLVMInitializeWebAssemblyDisassembler();
        LLVMInitializeWebAssemblyTarget();
        LLVMInitializeWebAssemblyTargetInfo();
        LLVMInitializeWebAssemblyTargetMC();
#endif
    }
    
    void configureGlobals(const ISPCEngine::Options& options) {
        // Set optimization level
        g->opt.level = options.optimizationLevel;
        if (options.optimizationLevel == 0) {
            g->codegenOptLevel = Globals::CodegenOptLevel::None;
        } else if (options.optimizationLevel == 1) {
            g->codegenOptLevel = Globals::CodegenOptLevel::Default;
            g->opt.disableCoherentControlFlow = true;
        } else {
            g->codegenOptLevel = Globals::CodegenOptLevel::Aggressive;
        }
        
        // Set optimization flags
        g->opt.fastMath = options.fastMath;
        g->opt.disableAsserts = options.disableAsserts;
        g->opt.forceAlignedMemory = options.forceAlignedMemory;
        
        // Set debug info
        g->generateDebuggingSymbols = options.generateDebugInfo;
        if (options.generateDebugInfo) {
            g->generateDWARFVersion = options.dwarfVersion;
            g->debugInfoType = Globals::DebugInfoType::DWARF;
        }
        
        // Set library options
        g->includeStdlib = options.includeStdlib;
        g->quiet = options.quiet;
        g->warningsAsErrors = options.warnAsError;
        g->disableWarnings = options.disableWarnings;
        
        // Set math library
        switch (options.mathLibrary) {
            case ISPCEngine::Options::MathLib::Default:
                g->mathLib = Globals::MathLib::Math_ISPC;
                break;
            case ISPCEngine::Options::MathLib::Fast:
                g->mathLib = Globals::MathLib::Math_ISPCFast;
                break;
            case ISPCEngine::Options::MathLib::SVML:
                g->mathLib = Globals::MathLib::Math_SVML;
                break;
            case ISPCEngine::Options::MathLib::System:
                g->mathLib = Globals::MathLib::Math_System;
                break;
        }
        
        // Set target OS
        if (!options.targetOS.empty()) {
            g->target_os = ParseOS(options.targetOS);
        }
        
        // Set include paths
        g->includePath = options.includePaths;
        
        // Set defines
        g->cppArgs.clear();
        for (const auto& define : options.defines) {
            g->cppArgs.push_back("-D" + define);
        }
    }
    
    void setupOutput(Module::Output& output, const ISPCEngine::Options& options) {
        // Configure output settings for in-memory compilation
        Module::OutputFlags flags;
        
        if (options.generatePIC) {
            flags.setPICLevel(PICLevel::SmallPIC);
        }
        
        // Set output type to object by default
        output.type = Module::Object;
        output.flags = flags;
        
        // Use temporary files for output
        output.out = getTempFileName("ispc_engine_out");
        output.header = getTempFileName("ispc_engine_header");
    }
    
    void extractResults(ISPCEngine::Result& result, const Module::Output& output, ISPCTarget target) {
        // Read object file
        readFileToVector(output.out, result.objectCode);
        
        // Read header file
        readFileToString(output.header, result.headerContent);
        
        // TODO: Add support for extracting other output formats (bitcode, assembly, etc.)
        // This would involve modifying the Module::Output setup to generate those formats
        
        // Clean up temporary files
        if (!output.out.empty()) {
            std::remove(output.out.c_str());
        }
        if (!output.header.empty()) {
            std::remove(output.header.c_str());
        }
    }
    
    void extractMultiTargetResults(ISPCEngine::Result& result, const Module::Output& output, 
                                  const std::vector<ISPCTarget>& targets) {
        // For multi-target, we need to read the dispatch module and target-specific files
        readFileToVector(output.out, result.objectCode);
        readFileToString(output.header, result.headerContent);
        
        // Read target-specific object files
        for (const auto& target : targets) {
            std::string targetStr = ISPCTargetToString(target);
            std::string targetFile = output.out + "_" + targetStr + ".o";
            
            std::vector<uint8_t> targetCode;
            if (readFileToVector(targetFile, targetCode)) {
                result.targetSpecificCode[targetStr] = std::move(targetCode);
            }
            
            // Clean up target-specific file
            std::remove(targetFile.c_str());
        }
        
        // Clean up main files
        if (!output.out.empty()) {
            std::remove(output.out.c_str());
        }
        if (!output.header.empty()) {
            std::remove(output.header.c_str());
        }
    }
    
    std::string createTempFile(const std::string& content) {
        // Create a temporary file with unique name
        std::string tempFileName = getTempFileName("ispc_engine_src", ".ispc");
        
        std::ofstream tempFile(tempFileName, std::ios::binary);
        if (!tempFile.is_open()) {
            return "";
        }
        
        tempFile.write(content.c_str(), content.size());
        tempFile.close();
        
        return tempFileName;
    }
    
    std::string getTempFileName(const std::string& prefix, const std::string& suffix = "") {
        // Generate unique temporary file name
        static int counter = 0;
        std::string tempDir;
        
#ifdef ISPC_HOST_IS_WINDOWS
        char* tempPath = getenv("TEMP");
        if (tempPath) {
            tempDir = tempPath;
        } else {
            tempDir = ".";
        }
#else
        tempDir = "/tmp";
#endif
        
        std::ostringstream oss;
        oss << tempDir << "/" << prefix << "_" << getpid() << "_" << (++counter) << suffix;
        return oss.str();
    }
    
    bool readFileToVector(const std::string& filename, std::vector<uint8_t>& data) {
        std::ifstream file(filename, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            return false;
        }
        
        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);
        
        data.resize(size);
        return file.read(reinterpret_cast<char*>(data.data()), size).good();
    }
    
    bool readFileToString(const std::string& filename, std::string& content) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            return false;
        }
        
        std::ostringstream oss;
        oss << file.rdbuf();
        content = oss.str();
        return true;
    }
    
    std::vector<std::string> parseTargetList(const std::string& targetStr) const {
        std::vector<std::string> targets;
        std::istringstream iss(targetStr);
        std::string token;
        
        while (std::getline(iss, token, ',')) {
            // Trim whitespace
            size_t start = token.find_first_not_of(" \t");
            size_t end = token.find_last_not_of(" \t");
            if (start != std::string::npos && end != std::string::npos) {
                targets.push_back(token.substr(start, end - start + 1));
            }
        }
        
        return targets;
    }
};

// ISPCEngine public interface implementation

ISPCEngine::ISPCEngine() : impl_(std::make_unique<Implementation>()) {}

ISPCEngine::~ISPCEngine() = default;

ISPCEngine::Result ISPCEngine::compileString(const std::string& sourceCode, const Options& options) {
    return impl_->compileString(sourceCode, options);
}

ISPCEngine::Result ISPCEngine::compileString(const std::string& sourceCode) {
    Options defaultOptions;
    return impl_->compileString(sourceCode, defaultOptions);
}

ISPCEngine::Result ISPCEngine::compileFile(const std::string& filename, const Options& options) {
    return impl_->compileFile(filename, options);
}

ISPCEngine::Result ISPCEngine::compileFile(const std::string& filename) {
    Options defaultOptions;
    return impl_->compileFile(filename, defaultOptions);
}

ISPCEngine::Result ISPCEngine::compileMultiTarget(const std::string& sourceCode,
                                                 const std::vector<std::string>& targets,
                                                 const Options& options) {
    return impl_->compileMultiTarget(sourceCode, targets, options);
}

ISPCEngine::Result ISPCEngine::compileMultiTarget(const std::string& sourceCode,
                                                 const std::vector<std::string>& targets) {
    Options defaultOptions;
    return impl_->compileMultiTarget(sourceCode, targets, defaultOptions);
}

std::vector<std::string> ISPCEngine::getSupportedTargets() const {
    return impl_->getSupportedTargets();
}

std::vector<std::string> ISPCEngine::getSupportedArchitectures() const {
    return impl_->getSupportedArchitectures();
}

std::vector<std::string> ISPCEngine::getSupportedOS() const {
    return impl_->getSupportedOS();
}

std::string ISPCEngine::getVersion() const {
    return impl_->getVersion();
}

bool ISPCEngine::isTargetSupported(const std::string& target) const {
    return impl_->isTargetSupported(target);
}

void ISPCEngine::addIncludePath(const std::string& path) {
    // This would require modifying the implementation to store per-instance options
    // For now, users should pass include paths via Options struct
}

void ISPCEngine::addDefine(const std::string& define) {
    // This would require modifying the implementation to store per-instance options
    // For now, users should pass defines via Options struct
}

void ISPCEngine::setOptimizationLevel(int level) {
    // This would require modifying the implementation to store per-instance options
    // For now, users should pass optimization level via Options struct
}

// Global initialization functions

bool initializeISPC() {
    if (!g) {
        g = new Globals;
        return true;
    }
    return true; // Already initialized
}

void shutdownISPC() {
    delete g;
    g = nullptr;
    
    // Free all bookkeeping objects
    BookKeeper::in().freeAll();
}

std::string getISPCVersion() {
    return ISPC_VERSION_STRING;
}

std::vector<std::string> getAvailableTargets() {
    if (!g || !g->target_registry) {
        return {};
    }
    
    std::string targetsStr = g->target_registry->getSupportedTargets();
    std::vector<std::string> targets;
    std::istringstream iss(targetsStr);
    std::string token;
    
    while (std::getline(iss, token, ',')) {
        // Trim whitespace
        size_t start = token.find_first_not_of(" \t");
        size_t end = token.find_last_not_of(" \t");
        if (start != std::string::npos && end != std::string::npos) {
            targets.push_back(token.substr(start, end - start + 1));
        }
    }
    
    return targets;
}

} // namespace ispc