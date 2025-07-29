/*
  Copyright (c) 2025, Intel Corporation

  SPDX-License-Identifier: BSD-3-Clause
*/

// ISPC headers
#include "args.h"
#include "binary_type.h"
#include "ispc.h"
#include "ispc/ispc.h"
#include "target_registry.h"
#include "type.h"
#include "util.h"

// LLVM core headers
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Support/DynamicLibrary.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/ToolOutputFile.h>
#include <llvm/TargetParser/Host.h>

// LLVM JIT headers
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/JITSymbol.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/ExecutionEngine/Orc/RTDyldObjectLinkingLayer.h>
#include <llvm/ExecutionEngine/Orc/Shared/ExecutorAddress.h>
#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>

// Standard C++ headers
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <sstream>

// Platform-specific headers
#include <unistd.h>
#if defined(__linux__) || defined(__FreeBSD__)
#include <dlfcn.h>
#include <malloc.h>
#endif
#ifdef _WIN32
#include <windows.h>
#endif

// Forward declaration
extern void initializeBinaryType(const char *ISPCExecutableAbsPath);

// Get the path of the current shared library (JIT-appropriate method)
static std::string __getISPCLibraryPath() {
    // Try multiple approaches to find the library path

#if defined(__linux__) || defined(__FreeBSD__) || defined(__APPLE__)
    Dl_info dl_info;
    // Use the address of this function to find the current library
    if (dladdr((void *)__getISPCLibraryPath, &dl_info) && dl_info.dli_fname && strlen(dl_info.dli_fname) > 0) {
        std::string libPath(dl_info.dli_fname);
        // Sanity check: ensure path is not empty and exists
        if (!libPath.empty() && llvm::sys::fs::exists(libPath)) {
            // Additional check: make sure this looks like a library file
            if (libPath.find("libispc") != std::string::npos || libPath.find(".so") != std::string::npos ||
                libPath.find(".dylib") != std::string::npos) {
                return libPath;
            }
        }
    }
#elif defined(_WIN32)
    HMODULE hModule = NULL;
    if (GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                          (LPCTSTR)__getISPCLibraryPath, &hModule)) {
        char path[MAX_PATH];
        DWORD result = GetModuleFileName(hModule, path, MAX_PATH);
        if (result > 0 && result < MAX_PATH) {
            std::string libPath(path);
            if (llvm::sys::fs::exists(libPath) &&
                (libPath.find("ispc") != std::string::npos || libPath.find(".dll") != std::string::npos)) {
                return libPath;
            }
        }
    }
#endif
    return "";
}

static void initializePaths(const char *ISPCLibraryPath) {
    llvm::SmallString<128> includeDir(ISPCLibraryPath);
    llvm::sys::path::remove_filename(includeDir); // Remove libispc.so -> /path/to/lib
    llvm::sys::path::remove_filename(includeDir); // Remove lib -> /path/to
    llvm::sys::path::append(includeDir, "include", "stdlib");
    ispc::g->includePath.push_back(std::string(includeDir.c_str()));
}

namespace ispc {

// Class for managing global state during compilation
class GlobalStateGuard {
  public:
    GlobalStateGuard() : savedModule(m), savedTarget(g->target) {}

    ~GlobalStateGuard() {
        // Restore global state
        m = savedModule;
        g->target = savedTarget;
    }

  private:
    Module *savedModule;
    Target *savedTarget;
};

/**
 * @brief Implementation class for ISPCEngine using the PIMPL pattern
 *
 * This class handles both traditional ISPC compilation and JIT compilation.
 * It manages LLVM JIT state, user-provided runtime functions, and compilation
 * configuration. The class is designed to be used through the ISPCEngine
 * public interface.
 *
 * For JIT compilation, users must provide runtime functions via SetJitRuntimeFunctions()
 * before calling CompileFromFileToJit(). The JIT engine will resolve ISPC runtime
 * calls (ISPCLaunch, ISPCSync, ISPCAlloc) to these user-provided implementations.
 */
class ISPCEngine::Impl {
  public:
    Impl() {
        m_output.type = Module::OutputType::Object; // Default output type
    }

    // Fields populated by ParseCommandLineArgs
    std::string m_file;
    Arch m_arch{Arch::none};
    std::string m_cpu;
    std::vector<ISPCTarget> m_targets;
    Module::Output m_output;
    std::vector<std::string> m_linkFileNames;
    bool m_isHelpMode{false};
    bool m_isLinkMode{false};

    // JIT-related fields
    bool m_isJitMode{false};
    std::unique_ptr<llvm::orc::LLJIT> m_jit;

    // User-provided runtime functions storage
    // Maps function name to function pointer for runtime functions
    std::map<std::string, void *> m_runtimeFunctions;

    bool IsLinkMode() const { return m_isLinkMode; }

    int Compile() {
        if (g->enableTimeTrace) {
            llvm::timeTraceProfilerInitialize(g->timeTraceGranularity, "ispc");
        }

        int ret = 0;
        {
            llvm::TimeTraceScope TimeScope("ExecuteISPCEngine");
            ret = Module::CompileAndOutput(m_file.c_str(), m_arch, m_cpu.empty() ? nullptr : m_cpu.c_str(), m_targets,
                                           m_output);
        }

        if (g->enableTimeTrace) {
            // Write to file only if compilation is successful.
            if ((ret == 0) && (!m_output.out.empty())) {
                writeCompileTimeFile(m_output.out.c_str());
            }
            llvm::timeTraceProfilerCleanup();
        }
        return ret;
    }

    int Link() {
        std::string filename = !m_output.out.empty() ? m_output.out : "";
        return Module::LinkAndOutput(m_linkFileNames, m_output.type, std::move(filename));
    }

    int Execute() {
        // TODO!: remove the global state.
        GlobalStateGuard guard; // The guard to protect global state

        if (m_isLinkMode) {
            return Link();
        } else if (m_isHelpMode) {
            return 0;
        } else {
            // Validate input file before compilation
            if (!ValidateInputFile(m_file)) {
                return 1;
            }
            // Validate output files (skipped in JIT mode)
            ValidateOutputFiles(m_output);
            return Compile();
        }
    }

    bool ValidateInputFile(const std::string &filename, bool allowStdin = true) {
        if (filename.empty()) {
            if (allowStdin) {
                Error(SourcePos(), "No input file were specified. To read text from stdin use \"-\" as file name.");
            } else {
                Error(SourcePos(), "No input file specified.");
            }
            return false;
        }

        if (filename != "-") {
            // If the input is not stdin then check that the file exists and it is
            // not a directory.
            if (!llvm::sys::fs::exists(filename)) {
                Error(SourcePos(), "File \"%s\" does not exist.", filename.c_str());
                return false;
            }

            if (llvm::sys::fs::is_directory(filename)) {
                Error(SourcePos(), "File \"%s\" is a directory.", filename.c_str());
                return false;
            }
        }
        return true;
    }

    void ValidateOutputFiles(const Module::Output &output) {
        // Skip validation in JIT mode - output files not needed for in-memory compilation
        if (g->isJitMode && output.out.empty()) {
            return;
        }

        if (output.out.empty() && output.header.empty() && (output.deps.empty() && !output.flags.isDepsToStdout()) &&
            output.hostStub.empty() && output.devStub.empty() && output.nbWrap.empty()) {
            Warning(SourcePos(), "No output file or header file name specified. "
                                 "Program will be compiled and warnings/errors will "
                                 "be issued, but no output will be generated.");
        }
    }

    int CompileFromFileToJit(const std::string &filename) {
        if (!ValidateInputFile(filename, false)) { // Don't allow stdin for JIT
            return 1;
        }

        // JIT compilation only supports single target compilation
        if (m_targets.size() != 1) {
            if (m_targets.empty()) {
                Error(SourcePos(), "JIT compilation requires exactly one target to be specified");
            } else {
                Error(SourcePos(),
                      "JIT compilation only supports single target compilation, but %zu targets were specified",
                      m_targets.size());
            }
            return 1;
        }

        // Initialize JIT if needed
        if (!InitializeJit()) {
            return 1;
        }

        // Compile ISPC file to LLVM module
        auto llvmModule =
            Module::CompileToLLVMModule(filename.c_str(), m_arch, m_cpu.empty() ? nullptr : m_cpu.c_str(), m_targets);
        if (!llvmModule) {
            return 1;
        }

        // Create a fresh context for this module
        auto context = std::make_unique<llvm::LLVMContext>();
        auto tsm = llvm::orc::ThreadSafeModule(std::move(llvmModule), llvm::orc::ThreadSafeContext(std::move(context)));

        auto addResult = m_jit->addIRModule(std::move(tsm));
        if (addResult) {
            Error(SourcePos(), "Failed to add module to JIT: %s", llvm::toString(std::move(addResult)).c_str());
            return 1;
        }

        return 0;
    }

    void *GetJitFunction(const std::string &functionName) {
        if (!m_isJitMode) {
            Error(SourcePos(), "JIT mode is not active.");
            return nullptr;
        }

        if (functionName.empty()) {
            Error(SourcePos(), "Function name cannot be empty.");
            return nullptr;
        }

        // Look up the function symbol in the JIT
        // Now that we have unmangled aliases, this should work directly
        auto symbolOrError = m_jit->lookup(functionName);
        if (!symbolOrError) {
            Error(SourcePos(), "Function '%s' not found in JIT: %s", functionName.c_str(),
                  llvm::toString(symbolOrError.takeError()).c_str());
            return nullptr;
        }

        // Get the function address - ExecutorAddr can be converted to uintptr_t
        auto address = symbolOrError->getValue();
        return reinterpret_cast<void *>(static_cast<uintptr_t>(address));
    }

    void ClearJitCode() {
        if (m_isJitMode && m_jit) {
            // Clear JIT code by resetting the JIT engine
            // Set mode to false first to prevent re-entry
            m_isJitMode = false;

            // Clear all dylibs first to clean up modules properly
            // LLVM's clear() method returns an Error, but for cleanup we ignore failures
            auto &MainJD = m_jit->getMainJITDylib();
            if (auto err = MainJD.clear()) {
                // Log the error but continue with cleanup - this is a best-effort operation
                // During shutdown, some cleanup operations may fail due to destruction order
                llvm::consumeError(std::move(err));
            }

            // Reset the JIT engine - this should not throw in normal circumstances
            m_jit.reset();
        }
    }

    bool IsJitMode() const { return m_isJitMode; }

    bool SetJitRuntimeFunction(const std::string &functionName, void *functionPtr) {
        if (functionName.empty()) {
            Error(SourcePos(), "Runtime function name cannot be empty");
            return false;
        }

        if (!functionPtr) {
            Error(SourcePos(), "Runtime function pointer cannot be null");
            return false;
        }

        // Validate that this is a known runtime function
        static const std::set<std::string> validFunctions = {"ISPCLaunch", "ISPCSync", "ISPCAlloc"};
        if (validFunctions.find(functionName) == validFunctions.end()) {
            Error(SourcePos(), "Unknown runtime function '%s'. Valid functions: ISPCLaunch, ISPCSync, ISPCAlloc",
                  functionName.c_str());
            return false;
        }

        m_runtimeFunctions[functionName] = functionPtr;
        return true;
    }

    void ClearJitRuntimeFunction(const std::string &functionName) {
        // Clear specific function
        m_runtimeFunctions.erase(functionName);
    }

    void ClearJitRuntimeFunctions() {
        // Clear all functions
        m_runtimeFunctions.clear();
    }

    void ReleaseJitForShutdown() {
        // Release JIT pointer to avoid destruction order issues during shutdown
        // This prevents LLVM from trying to clean up contexts/modules that may
        // have already been destroyed, which can cause segmentation faults
        if (m_jit) {
            m_jit.release();
        }
    }

    bool InitializeJit() {
        if (m_isJitMode) {
            return true; // Already initialized
        }

        // Initialize library-specific paths for JIT compilation
        // This is only needed for JIT mode, not for the main executable
        std::string libPath = __getISPCLibraryPath();
        initializePaths(libPath.c_str());

        // Create LLJIT instance - it will manage its own context
        auto jitBuilder = llvm::orc::LLJITBuilder();
        auto jitOrError = jitBuilder.create();

        if (!jitOrError) {
            Error(SourcePos(), "Failed to create JIT engine: %s", llvm::toString(jitOrError.takeError()).c_str());
            return false;
        }

        m_jit = std::move(*jitOrError);

        // Add process symbols generator to find runtime functions from the current process
        auto processSymbolsGenerator =
            llvm::orc::DynamicLibrarySearchGenerator::GetForCurrentProcess(m_jit->getDataLayout().getGlobalPrefix());

        if (!processSymbolsGenerator) {
            Error(SourcePos(), "Failed to create process symbols generator: %s",
                  llvm::toString(processSymbolsGenerator.takeError()).c_str());
            return false;
        }

        m_jit->getMainJITDylib().addGenerator(std::move(*processSymbolsGenerator));

        // Define user-provided runtime symbols if they are available
        if (!m_runtimeFunctions.empty()) {
            auto &JD = m_jit->getMainJITDylib();
            llvm::orc::SymbolMap symbols;

            for (const auto &[name, ptr] : m_runtimeFunctions) {
                symbols[m_jit->mangleAndIntern(name)] = {llvm::orc::ExecutorAddr::fromPtr(ptr),
                                                         llvm::JITSymbolFlags::Exported};
            }

            auto materializer = llvm::orc::absoluteSymbols(symbols);
            if (auto err = JD.define(materializer)) {
                Error(SourcePos(), "Failed to define runtime symbols: %s", llvm::toString(std::move(err)).c_str());
                return false;
            }
        }

        m_isJitMode = true;
        return true;
    }

  private:
    static void writeCompileTimeFile(const char *outFileName) {
        llvm::SmallString<128> jsonFileName(outFileName);
        jsonFileName.append(".json");
        llvm::sys::fs::OpenFlags flags = llvm::sys::fs::OF_Text;
        std::error_code error;
        std::unique_ptr<llvm::ToolOutputFile> of(new llvm::ToolOutputFile(jsonFileName.c_str(), error, flags));

        if (error) {
            Error(SourcePos(), "Cannot open json file \"%s\".\n", jsonFileName.c_str());
            return;
        }

        llvm::raw_fd_ostream &fos(of->os());
        llvm::timeTraceProfilerWrite(fos);
        of->keep();
    }
};

bool Initialize() {
    // Check if already initialized
    if (g != nullptr) {
        return true;
    }

    // Initialize available LLVM targets
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

    // Initialize globals
    g = new Globals;

    return true;
}

std::unique_ptr<ISPCEngine> ISPCEngine::CreateFromCArgs(int argc, char *argv[]) {
    // Check if library is initialized
    if (g == nullptr) {
        return nullptr;
    }

    auto instance = std::unique_ptr<ISPCEngine>(new ISPCEngine());

    // Parse command line options
    ArgsParseResult parseResult =
        ParseCommandLineArgs(argc, argv, instance->pImpl->m_file, instance->pImpl->m_arch, instance->pImpl->m_cpu,
                             instance->pImpl->m_targets, instance->pImpl->m_output, instance->pImpl->m_linkFileNames,
                             instance->pImpl->m_isLinkMode);

    if (parseResult == ArgsParseResult::failure) {
        return nullptr;
    }

    instance->pImpl->m_isHelpMode = (parseResult == ArgsParseResult::help_requested);

    return instance;
}

std::unique_ptr<ISPCEngine> ISPCEngine::CreateFromArgs(const std::vector<std::string> &args) {
    if (args.empty()) {
        return nullptr;
    }

    // Convert vector to argc/argv format
    int argc = args.size();
    std::vector<std::string> argsCopy(args); // Keep a copy to ensure lifetime
    std::vector<char *> argv(argc);

    for (int i = 0; i < argc; ++i) {
        argv[i] = const_cast<char *>(argsCopy[i].c_str());
    }

    return ISPCEngine::CreateFromCArgs(argc, argv.data());
}

ISPCEngine::ISPCEngine() : pImpl(std::make_unique<Impl>()) {}

ISPCEngine::~ISPCEngine() {
    // Skip JIT cleanup to avoid LLVM destruction order issues
    // Release the JIT pointer to prevent automatic cleanup
    if (pImpl) {
        pImpl->ReleaseJitForShutdown();
    }
    // The OS will clean up memory on process exit
}

void Shutdown() {
    // Free all bookkept objects.
    BookKeeper::in().freeAll();

    // Clean up global state
    if (g != nullptr) {
        delete g;
        g = nullptr;
    }
}

int ISPCEngine::Execute() { return pImpl->Execute(); }

int ISPCEngine::CompileFromFileToJit(const std::string &filename) { return pImpl->CompileFromFileToJit(filename); }

void *ISPCEngine::GetJitFunction(const std::string &functionName) { return pImpl->GetJitFunction(functionName); }

bool ISPCEngine::IsJitMode() const { return pImpl->IsJitMode(); }

void ISPCEngine::ClearJitCode() { pImpl->ClearJitCode(); }

bool ISPCEngine::SetJitRuntimeFunction(const std::string &functionName, void *functionPtr) {
    return pImpl->SetJitRuntimeFunction(functionName, functionPtr);
}

void ISPCEngine::ClearJitRuntimeFunction(const std::string &functionName) {
    pImpl->ClearJitRuntimeFunction(functionName);
}

void ISPCEngine::ClearJitRuntimeFunctions() { pImpl->ClearJitRuntimeFunctions(); }

// Function that uses C-style argc/argv interface
int CompileFromCArgs(int argc, char *argv[]) {
    // Check if library is initialized
    if (g == nullptr) {
        return 1;
    }

    auto instance = ISPCEngine::CreateFromCArgs(argc, argv);
    if (!instance) {
        return 1;
    }

    return instance->Execute();
}

int CompileFromArgs(const std::vector<std::string> &args) {
    if (args.empty()) {
        return 1;
    }

    // Convert vector to argc/argv format
    int argc = args.size();
    std::vector<std::string> argsCopy(args); // Keep a copy to ensure lifetime
    std::vector<char *> argv(argc);

    for (int i = 0; i < argc; ++i) {
        argv[i] = const_cast<char *>(argsCopy[i].c_str());
    }

    return CompileFromCArgs(argc, argv.data());
}

} // namespace ispc