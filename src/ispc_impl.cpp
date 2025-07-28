/*
  Copyright (c) 2025, Intel Corporation

  SPDX-License-Identifier: BSD-3-Clause
*/

#include "args.h"
#include "binary_type.h"
#include "ispc.h"
#include "ispc/ispc.h"
#include "target_registry.h"
#include "type.h"
#include "util.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/ToolOutputFile.h"

#include <cstring>
#include <llvm/MC/TargetRegistry.h>

// LLVM JIT includes
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/JITSymbol.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/ExecutionEngine/Orc/RTDyldObjectLinkingLayer.h>
#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/DynamicLibrary.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/TargetParser/Host.h>

#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
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
static std::string getCurrentLibraryPath() {
    // Try multiple approaches to find the library path

#if defined(__linux__) || defined(__FreeBSD__) || defined(__APPLE__)
    Dl_info dl_info;
    // Use the address of this function to find the current library
    if (dladdr((void *)getCurrentLibraryPath, &dl_info) && dl_info.dli_fname && strlen(dl_info.dli_fname) > 0) {
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
                          (LPCTSTR)getCurrentLibraryPath, &hModule)) {
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

    // Fallback approach: use executable path and try to infer library location
    char dummy;
    std::string execPath = llvm::sys::fs::getMainExecutable(nullptr, &dummy);

    // If executable path contains hints about the build structure, try to construct library path
    if (execPath.find("/build/") != std::string::npos) {
        size_t buildPos = execPath.find("/build/");
        std::string buildRoot = execPath.substr(0, buildPos + 7); // Include "/build/"
        std::string possibleLibPath = buildRoot + "lib";
        if (llvm::sys::fs::exists(possibleLibPath)) {
            return possibleLibPath; // Return directory path for lib directory case
        }
    }

    return execPath;
}

// Library-specific path initialization for libispc
static void initializePaths(const char *ISPCLibraryPath) {
    if (!ISPCLibraryPath || strlen(ISPCLibraryPath) == 0) {
        // Fallback to original behavior if path is invalid
        char dummy;
        std::string fallbackPath = llvm::sys::fs::getMainExecutable(nullptr, &dummy);
        initializeBinaryType(fallbackPath.c_str());
        return;
    }

    llvm::SmallString<128> includeDir(ISPCLibraryPath);

    // Check if the path is a directory (lib directory) or a file (library file)
    bool isDirectory = llvm::sys::fs::is_directory(includeDir);

    if (!isDirectory) {
        // Remove library filename (e.g., libispc.so) if it's a file path
        llvm::sys::path::remove_filename(includeDir);
    }

    // Handle case where lib is in subdirectory like lib/x86/
    // Keep going up until we find a directory that contains include/stdlib
    llvm::SmallString<128> testPath(includeDir);

    // Try current directory first (for cases like lib/x86/ or when we already have lib/)
    llvm::sys::path::append(testPath, "include", "stdlib", "short_vec.isph");
    if (!llvm::sys::fs::exists(testPath)) {
        // Go up one level (lib/ -> root case)
        llvm::sys::path::remove_filename(includeDir);
        testPath = includeDir;
        llvm::sys::path::append(testPath, "include", "stdlib", "short_vec.isph");
        if (!llvm::sys::fs::exists(testPath)) {
            // Go up another level (typical installed case)
            llvm::sys::path::remove_filename(includeDir);
        }
    }

    // Add the final include path
    llvm::SmallString<128> finalPath(includeDir);
    llvm::sys::path::append(finalPath, "include", "stdlib");

    // Verify the final path exists before adding it
    if (llvm::sys::fs::exists(finalPath)) {
        ispc::g->includePath.push_back(std::string(finalPath.c_str()));
    } else {
        // Ultimate fallback: use the original binary type initialization
        char dummy;
        std::string fallbackPath = llvm::sys::fs::getMainExecutable(nullptr, &dummy);
        initializeBinaryType(fallbackPath.c_str());
    }
}

// ISPC runtime functions for JIT - exposed with C linkage globally
extern "C" {
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
#ifdef ISPC_HOST_IS_WINDOWS
    return _aligned_malloc(size, alignment);
#elif defined(__linux__) || defined(__FreeBSD__)
    return memalign(alignment, size);
#elif defined(__APPLE__)
    void *mem = malloc(size + (alignment - 1) + sizeof(void *));
    char *amem = ((char *)mem) + sizeof(void *);
    amem = amem + uint32_t(alignment - (reinterpret_cast<uint64_t>(amem) & (alignment - 1)));
    ((void **)amem)[-1] = mem;
    return amem;
#else
#error "Host OS was not detected"
#endif
}
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
    std::unique_ptr<llvm::LLVMContext> m_jitContext;

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
        if (g->isJitMode) {
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

        // Add the module to the JIT
        auto tsm =
            llvm::orc::ThreadSafeModule(std::move(llvmModule), llvm::orc::ThreadSafeContext(std::move(m_jitContext)));

        // Create a new context for future compilations
        // Note: ThreadSafeModule takes ownership of the context, so we need a fresh one
        // for subsequent JIT compilations to work correctly
        m_jitContext = std::make_unique<llvm::LLVMContext>();

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
            // Reset in reverse order of initialization to avoid cleanup dependencies
            try {
                m_jit.reset();
            } catch (...) {
                // Ignore exceptions during JIT cleanup
            }

            try {
                m_jitContext.reset();
            } catch (...) {
                // Ignore exceptions during context cleanup
            }

            m_isJitMode = false;
        }
    }

    bool IsJitMode() const { return m_isJitMode; }

    bool InitializeJit() {
        if (m_isJitMode) {
            return true; // Already initialized
        }

        // Create a new LLVM context for JIT compilation
        m_jitContext = std::make_unique<llvm::LLVMContext>();

        // Create LLJIT instance
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

    // Initialize paths to set up stdlib include paths for library usage
    // Use JIT-appropriate method to find current library path
    std::string libPath = getCurrentLibraryPath();
    initializePaths(libPath.c_str());

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
    // Clear JIT code on destruction
    // Check if global state still exists before JIT cleanup
    if (pImpl && pImpl->IsJitMode() && g != nullptr) {
        ClearJitCode();
    }
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