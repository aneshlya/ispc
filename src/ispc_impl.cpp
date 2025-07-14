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
#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/TargetParser/Host.h>

#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <unistd.h>

namespace ispc {

class ISPCEngine::Impl {
  public:
    Impl() {
        m_output.type = Module::OutputType::Object; // Default output type
    }

    // Fields populated by ParseCommandLineArgs
    char *m_file = nullptr;
    Arch m_arch{Arch::none};
    const char *m_cpu{nullptr};
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
            ret = Module::CompileAndOutput(m_file, m_arch, m_cpu, m_targets, m_output);
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
        return Module::LinkAndOutput(m_linkFileNames, m_output.type, filename);
    }

    int Execute() {
        if (m_isLinkMode) {
            return Link();
        } else if (m_isHelpMode) {
            return 0;
        } else {
            // Validate input file before compilation
            if (!ValidateInputFile(m_file)) {
                return 1;
            }
            return Compile();
        }
    }

    bool ValidateInputFile(const char *filename, bool allowStdin = true) {
        if (filename == nullptr) {
            if (allowStdin) {
                Error(SourcePos(), "No input file were specified. To read text from stdin use \"-\" as file name.");
            } else {
                Error(SourcePos(), "No input file specified.");
            }
            return false;
        }

        if (strcmp(filename, "-") != 0) {
            // If the input is not stdin then check that the file exists and it is
            // not a directory.
            if (!llvm::sys::fs::exists(filename)) {
                Error(SourcePos(), "File \"%s\" does not exist.", filename);
                return false;
            }

            if (llvm::sys::fs::is_directory(filename)) {
                Error(SourcePos(), "File \"%s\" is a directory.", filename);
                return false;
            }
        }
        return true;
    }

    int CompileFromFileToJit(const char *filename) {
        if (!ValidateInputFile(filename, false)) { // Don't allow stdin for JIT
            return 1;
        }

        // Initialize JIT if needed
        if (!InitializeJit()) {
            return 1;
        }

        // Compile ISPC file to LLVM module
        auto llvmModule = Module::CompileToLLVMModule(filename, m_arch, m_cpu, m_targets);
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

    void *GetJitFunction(const char *functionName) {
        if (!m_isJitMode) {
            Error(SourcePos(), "JIT mode is not active.");
            return nullptr;
        }

        if (functionName == nullptr) {
            Error(SourcePos(), "Function name cannot be null.");
            return nullptr;
        }

        // Look up the function symbol in the JIT
        auto symbolOrError = m_jit->lookup(functionName);
        if (!symbolOrError) {
            Error(SourcePos(), "Function '%s' not found in JIT: %s", functionName,
                  llvm::toString(symbolOrError.takeError()).c_str());
            return nullptr;
        }

        // Get the function address - ExecutorAddr can be converted to uintptr_t
        auto address = symbolOrError->getValue();
        return reinterpret_cast<void *>(static_cast<uintptr_t>(address));
    }

    void ClearJitCode() {
        if (m_isJitMode) {
            // Clear JIT code by resetting the JIT engine
            m_jit.reset();
            m_jitContext.reset();
            m_isJitMode = false;
        }
    }

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

ISPCEngine::~ISPCEngine() {}

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

int ISPCEngine::CompileFromFile(const char *filename) { return pImpl->CompileFromFileToJit(filename); }

void *ISPCEngine::GetFunction(const char *functionName) { return pImpl->GetJitFunction(functionName); }

bool ISPCEngine::IsJitMode() const { return pImpl->m_isJitMode; }

void ISPCEngine::ClearJitCode() { pImpl->ClearJitCode(); }

// RAII helper for global state management
class GlobalStateGuard {
  public:
    GlobalStateGuard() : savedModule(m), savedTarget(g->target) {}
    ~GlobalStateGuard() {
        m = savedModule;
        g->target = savedTarget;
    }

  private:
    Module *savedModule;
    Target *savedTarget;
};

// Function that uses C-style argc/argv interface
int CompileFromCArgs(int argc, char *argv[]) {
    // Check if library is initialized
    if (g == nullptr) {
        return 1;
    }

    // Automatically save and restore global state
    GlobalStateGuard guard;

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

    // CompileFromCArgs already handles global state management
    return CompileFromCArgs(argc, argv.data());
}

} // namespace ispc

// C API implementations
extern "C" {

int ispc_initialize(void) { return ispc::Initialize() ? 1 : 0; }

void ispc_shutdown(void) { ispc::Shutdown(); }

int ispc_compile_from_args(int argc, char *argv[]) { return ispc::CompileFromCArgs(argc, argv); }

ispc_engine_t *ispc_engine_create_from_args(int argc, char *argv[]) {
    auto engine = ispc::ISPCEngine::CreateFromCArgs(argc, argv);
    if (!engine) {
        return nullptr;
    }
    // Transfer ownership to C handle
    return reinterpret_cast<ispc_engine_t *>(engine.release());
}

void ispc_engine_destroy(ispc_engine_t *engine) {
    if (engine) {
        auto *cppEngine = reinterpret_cast<ispc::ISPCEngine *>(engine);
        delete cppEngine;
    }
}

int ispc_engine_execute(ispc_engine_t *engine) {
    if (!engine) {
        return 1;
    }

    // Automatically save and restore global state
    ispc::GlobalStateGuard guard;

    auto *cppEngine = reinterpret_cast<ispc::ISPCEngine *>(engine);
    return cppEngine->Execute();
}

int ispc_engine_compile_from_file(ispc_engine_t *engine, const char *filename) {
    if (!engine) {
        return 1;
    }
    auto *cppEngine = reinterpret_cast<ispc::ISPCEngine *>(engine);
    return cppEngine->CompileFromFile(filename);
}

void *ispc_engine_get_function(ispc_engine_t *engine, const char *function_name) {
    if (!engine) {
        return nullptr;
    }
    auto *cppEngine = reinterpret_cast<ispc::ISPCEngine *>(engine);
    return cppEngine->GetFunction(function_name);
}

int ispc_engine_is_jit_mode(ispc_engine_t *engine) {
    if (!engine) {
        return 0;
    }
    auto *cppEngine = reinterpret_cast<ispc::ISPCEngine *>(engine);
    return cppEngine->IsJitMode() ? 1 : 0;
}

void ispc_engine_clear_jit_code(ispc_engine_t *engine) {
    if (!engine) {
        return;
    }
    auto *cppEngine = reinterpret_cast<ispc::ISPCEngine *>(engine);
    cppEngine->ClearJitCode();
}

} // extern "C"