/*
  Copyright (c) 2010-2024, Intel Corporation

  SPDX-License-Identifier: BSD-3-Clause
*/

/** @file builtins.cpp
    @brief Definitions of functions related to setting up the standard library
           and other builtins.
*/

#include "builtins.h"
#include "ctx.h"
#include "expr.h"
#include "llvmutil.h"
#include "module.h"
#include "sym.h"
#include "type.h"
#include "util.h"

#include <math.h>
#include <stdlib.h>

#include <unordered_set>
#include <unordered_map>

#include <llvm/ADT/StringRef.h>
#include <llvm/ADT/StringSet.h>
#include <llvm/Bitcode/BitcodeReader.h>
#include <llvm/IR/Attributes.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/Linker/Linker.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Transforms/IPO.h>
#include <llvm/Transforms/IPO/GlobalDCE.h>
#if ISPC_LLVM_VERSION >= ISPC_LLVM_17_0
#include <llvm/TargetParser/Triple.h>
#else
#include <llvm/ADT/Triple.h>
#endif

#ifdef ISPC_XE_ENABLED
#include <llvm/GenXIntrinsics/GenXIntrinsics.h>
#endif

using namespace ispc;
using namespace ispc::builtin;

/** Given an LLVM type, try to find the equivalent ispc type.  Note that
    this is an under-constrained problem due to LLVM's type representations
    carrying less information than ispc's.  (For example, LLVM doesn't
    distinguish between signed and unsigned integers in its types.)

    Because this function is only used for generating ispc declarations of
    functions defined in LLVM bitcode in the builtins-*.ll files, in practice
    we can get enough of what we need for the relevant cases to make things
    work, partially with the help of the intAsUnsigned parameter, which
    indicates whether LLVM integer types should be treated as being signed
    or unsigned.

 */
static const Type *lLLVMTypeToISPCType(const llvm::Type *t, bool intAsUnsigned) {
    if (t == LLVMTypes::VoidType) {
        return AtomicType::Void;
    }

    // uniform
    else if (t == LLVMTypes::BoolType) {
        return AtomicType::UniformBool;
    } else if (t == LLVMTypes::Int8Type) {
        return intAsUnsigned ? AtomicType::UniformUInt8 : AtomicType::UniformInt8;
    } else if (t == LLVMTypes::Int16Type) {
        return intAsUnsigned ? AtomicType::UniformUInt16 : AtomicType::UniformInt16;
    } else if (t == LLVMTypes::Int32Type) {
        return intAsUnsigned ? AtomicType::UniformUInt32 : AtomicType::UniformInt32;
    } else if (t == LLVMTypes::Float16Type) {
        return AtomicType::UniformFloat16;
    } else if (t == LLVMTypes::FloatType) {
        return AtomicType::UniformFloat;
    } else if (t == LLVMTypes::DoubleType) {
        return AtomicType::UniformDouble;
    } else if (t == LLVMTypes::Int64Type) {
        return intAsUnsigned ? AtomicType::UniformUInt64 : AtomicType::UniformInt64;
    }

    // varying
    if (t == LLVMTypes::Int8VectorType) {
        return intAsUnsigned ? AtomicType::VaryingUInt8 : AtomicType::VaryingInt8;
    } else if (t == LLVMTypes::Int16VectorType) {
        return intAsUnsigned ? AtomicType::VaryingUInt16 : AtomicType::VaryingInt16;
    } else if (t == LLVMTypes::Int32VectorType) {
        return intAsUnsigned ? AtomicType::VaryingUInt32 : AtomicType::VaryingInt32;
    } else if (t == LLVMTypes::Float16VectorType) {
        return AtomicType::VaryingFloat16;
    } else if (t == LLVMTypes::FloatVectorType) {
        return AtomicType::VaryingFloat;
    } else if (t == LLVMTypes::DoubleVectorType) {
        return AtomicType::VaryingDouble;
    } else if (t == LLVMTypes::Int64VectorType) {
        return intAsUnsigned ? AtomicType::VaryingUInt64 : AtomicType::VaryingInt64;
    } else if (t == LLVMTypes::MaskType) {
        return AtomicType::VaryingBool;
    } else if (t == LLVMTypes::Int1VectorType) {
        return AtomicType::VaryingInt1;
    }

    // pointers to uniform
    else if (t == LLVMTypes::Int8PointerType) {
        return PointerType::GetUniform(intAsUnsigned ? AtomicType::UniformUInt8 : AtomicType::UniformInt8);
    } else if (t == LLVMTypes::Int16PointerType) {
        return PointerType::GetUniform(intAsUnsigned ? AtomicType::UniformUInt16 : AtomicType::UniformInt16);
    } else if (t == LLVMTypes::Int32PointerType) {
        return PointerType::GetUniform(intAsUnsigned ? AtomicType::UniformUInt32 : AtomicType::UniformInt32);
    } else if (t == LLVMTypes::Int64PointerType) {
        return PointerType::GetUniform(intAsUnsigned ? AtomicType::UniformUInt64 : AtomicType::UniformInt64);
    } else if (t == LLVMTypes::Float16PointerType) {
        return PointerType::GetUniform(AtomicType::UniformFloat16);
    } else if (t == LLVMTypes::FloatPointerType) {
        return PointerType::GetUniform(AtomicType::UniformFloat);
    } else if (t == LLVMTypes::DoublePointerType) {
        return PointerType::GetUniform(AtomicType::UniformDouble);
    }

    // pointers to varying
    else if (t == LLVMTypes::Int8VectorPointerType) {
        return PointerType::GetUniform(intAsUnsigned ? AtomicType::VaryingUInt8 : AtomicType::VaryingInt8);
    } else if (t == LLVMTypes::Int16VectorPointerType) {
        return PointerType::GetUniform(intAsUnsigned ? AtomicType::VaryingUInt16 : AtomicType::VaryingInt16);
    } else if (t == LLVMTypes::Int32VectorPointerType) {
        return PointerType::GetUniform(intAsUnsigned ? AtomicType::VaryingUInt32 : AtomicType::VaryingInt32);
    } else if (t == LLVMTypes::Int64VectorPointerType) {
        return PointerType::GetUniform(intAsUnsigned ? AtomicType::VaryingUInt64 : AtomicType::VaryingInt64);
    } else if (t == LLVMTypes::Float16VectorPointerType) {
        return PointerType::GetUniform(AtomicType::VaryingFloat16);
    } else if (t == LLVMTypes::FloatVectorPointerType) {
        return PointerType::GetUniform(AtomicType::VaryingFloat);
    } else if (t == LLVMTypes::DoubleVectorPointerType) {
        return PointerType::GetUniform(AtomicType::VaryingDouble);
    }

    // vector of pointers
    else if (t == LLVMTypes::PtrVectorType) {
        return AtomicType::VaryingUInt64;
    }

    // vector with length different from TARGET_WIDTH can be repsented as uniform TYPE<N>
    else if (const llvm::VectorType *vt = llvm::dyn_cast<llvm::VectorType>(t)) {
        // check if vector length is equal to TARGET_WIDTH
        unsigned int vectorWidth = vt->getElementCount().getKnownMinValue();
        if (vectorWidth == g->target->getVectorWidth()) {
            // we should never hit this case, because it should be handled by the cases above
            return nullptr;
        } else {
            const Type *elementType = lLLVMTypeToISPCType(vt->getElementType(), intAsUnsigned);
            if (elementType == nullptr) {
                return nullptr;
            }
            return new VectorType(elementType, vectorWidth);
        }
    }

    // struct of varying
    else if (const llvm::StructType *st = llvm::dyn_cast<llvm::StructType>(t)) {
        llvm::SmallVector<const Type *, 8> elementTypes;
        llvm::SmallVector<std::string, 8> elementNames;
        llvm::SmallVector<SourcePos, 8> elementPositions;
        std::string structName = "__ispcstub_";
        for (unsigned int i = 0; i < st->getNumElements(); ++i) {
            const Type *elementType = lLLVMTypeToISPCType(st->getElementType(i), intAsUnsigned);
            if (elementType == nullptr) {
                return nullptr;
            }
            elementTypes.push_back(elementType);
            elementNames.push_back("element" + std::to_string(i));
            elementPositions.push_back(SourcePos());
            structName += elementType->Mangle();
        }
        // This struct name has to be same
        return new StructType(structName, elementTypes, elementNames, elementPositions, false, Variability::Uniform,
                              true, SourcePos());
    }

    return nullptr;
}

Symbol *ispc::CreateISPCSymbolForLLVMIntrinsic(llvm::Function *func, SymbolTable *symbolTable) {
    Symbol *existingSym = symbolTable->LookupIntrinsics(func);
    if (existingSym != nullptr) {
        return existingSym;
    }
    SourcePos noPos;
    noPos.name = "LLVM Intrinsic";
    const llvm::FunctionType *ftype = func->getFunctionType();
    std::string name = std::string(func->getName());
    const Type *returnType = lLLVMTypeToISPCType(ftype->getReturnType(), false);
    if (returnType == nullptr) {
        Error(SourcePos(),
              "Return type not representable for "
              "Intrinsic %s.",
              name.c_str());
        // return type not representable in ispc -> not callable from ispc
        return nullptr;
    }
    llvm::SmallVector<const Type *, 8> argTypes;
    for (unsigned int j = 0; j < ftype->getNumParams(); ++j) {
        const llvm::Type *llvmArgType = ftype->getParamType(j);
        const Type *type = lLLVMTypeToISPCType(llvmArgType, false);
        if (type == nullptr) {
            Error(SourcePos(),
                  "Type of parameter %d not "
                  "representable for Intrinsic %s",
                  j, name.c_str());
            return nullptr;
        }
        argTypes.push_back(type);
    }
    FunctionType *funcType = new FunctionType(returnType, argTypes, noPos);
    Debug(noPos, "Created Intrinsic symbol \"%s\" [%s]\n", name.c_str(), funcType->GetString().c_str());
    Symbol *sym = new Symbol(name, noPos, Symbol::SymbolKind::Function, funcType);
    sym->function = func;
    symbolTable->AddIntrinsics(sym);
    return sym;
}

/** In many of the builtins-*.ll files, we have declarations of various LLVM
    intrinsics that are then used in the implementation of various target-
    specific functions.  This function loops over all of the intrinsic
    declarations and makes sure that the signature we have in our .ll file
    matches the signature of the actual intrinsic.
*/
static void lCheckModuleIntrinsics(llvm::Module *module) {
    llvm::Module::iterator iter;
    for (iter = module->begin(); iter != module->end(); ++iter) {
        llvm::Function *func = &*iter;
        if (!func->isIntrinsic()) {
            continue;
        }

        const std::string funcName = func->getName().str();
        const std::string llvm_x86 = "llvm.x86.";
        // Work around http://llvm.org/bugs/show_bug.cgi?id=10438; only
        // check the llvm.x86.* intrinsics for now...
        if (funcName.length() >= llvm_x86.length() && !funcName.compare(0, llvm_x86.length(), llvm_x86)) {
            llvm::Intrinsic::ID id = (llvm::Intrinsic::ID)func->getIntrinsicID();
            if (id == 0) {
                std::string error_message = "Intrinsic is not found: ";
                error_message += funcName;
                FATAL(error_message.c_str());
            }
            llvm::Type *intrinsicType = llvm::Intrinsic::getType(*g->ctx, id);
            intrinsicType = llvm::PointerType::get(intrinsicType, 0);
            Assert(func->getType() == intrinsicType);
        }
    }
}

static void lUpdateIntrinsicsAttributes(llvm::Module *module) {
#ifdef ISPC_XE_ENABLED
    for (auto F = module->begin(), E = module->end(); F != E; ++F) {
        llvm::Function *Fn = &*F;
        // WA for isGenXIntrinsic(Fn) and getGenXIntrinsicID(Fn)
        // There are crashes if intrinsics is not supported on some platforms
        if (Fn && Fn->getName().contains("prefetch")) {
            continue;
        }
        if (Fn && llvm::GenXIntrinsic::isGenXIntrinsic(Fn)) {
            Fn->setAttributes(
                llvm::GenXIntrinsic::getAttributes(Fn->getContext(), llvm::GenXIntrinsic::getGenXIntrinsicID(Fn)));

            // ReadNone, ReadOnly and WriteOnly are not supported for intrinsics anymore:
            FixFunctionAttribute(*Fn, llvm::Attribute::ReadNone, llvm::MemoryEffects::none());
            FixFunctionAttribute(*Fn, llvm::Attribute::ReadOnly, llvm::MemoryEffects::readOnly());
            FixFunctionAttribute(*Fn, llvm::Attribute::WriteOnly, llvm::MemoryEffects::writeOnly());
        }
    }
#endif
}

static void lSetAsInternal(llvm::Module *module, llvm::StringSet<> &functions) {
    for (llvm::Function &F : module->functions()) {
        if (!F.isDeclaration() && functions.find(F.getName()) != functions.end()) {
            F.setLinkage(llvm::GlobalValue::InternalLinkage);
        }
    }
}

void lSetInternalLinkageGlobal(llvm::Module *module, const char *name) {
    llvm::GlobalValue *GV = module->getNamedGlobal(name);
    if (GV) {
        GV->setLinkage(llvm::GlobalValue::InternalLinkage);
    }
}

void lSetInternalLinkageGlobals(llvm::Module *module) {
    lSetInternalLinkageGlobal(module, "__fast_masked_vload");
    lSetInternalLinkageGlobal(module, "__math_lib");
    lSetInternalLinkageGlobal(module, "__memory_alignment");
}

void lAddBitcodeToModule(llvm::Module *bcModule, llvm::Module *module) {
    if (!bcModule) {
        Error(SourcePos(), "Error library module is nullptr");
    } else {
        if (g->target->isXeTarget()) {
            // Maybe we will use it for other targets in future,
            // but now it is needed only by Xe. We need
            // to update attributes because Xe intrinsics are
            // separated from the others and it is not done by default
            lUpdateIntrinsicsAttributes(bcModule);
        }

        for (llvm::Function &f : *bcModule) {
            if (f.isDeclaration()) {
                // Declarations with uses will be moved by Linker.
                if (f.getNumUses() > 0) {
                    continue;
                }
                // Declarations with 0 uses are moved by hands.
                module->getOrInsertFunction(f.getName(), f.getFunctionType(), f.getAttributes());
            }
        }

        // Remove clang ID metadata from the bitcode module, as we don't need it.
        llvm::NamedMDNode *identMD = bcModule->getNamedMetadata("llvm.ident");
        if (identMD) {
            identMD->eraseFromParent();
        }

        std::unique_ptr<llvm::Module> M(bcModule);
        if (llvm::Linker::linkModules(*module, std::move(M), llvm::Linker::Flags::LinkOnlyNeeded)) {
            Error(SourcePos(), "Error linking stdlib bitcode.");
        }
    }
}

void lAddDeclarationsToModule(llvm::Module *bcModule, llvm::Module *module) {
    if (!bcModule) {
        Error(SourcePos(), "Error library module is nullptr");
    } else {
        // FIXME: this feels like a bad idea, but the issue is that when we
        // set the llvm::Module's target triple in the ispc Module::Module
        // constructor, we start by calling llvm::sys::getHostTriple() (and
        // then change the arch if needed).  Somehow that ends up giving us
        // strings like 'x86_64-apple-darwin11.0.0', while the stuff we
        // compile to bitcode with clang has module triples like
        // 'i386-apple-macosx10.7.0'.  And then LLVM issues a warning about
        // linking together modules with incompatible target triples..
        llvm::Triple mTriple(m->module->getTargetTriple());
        llvm::Triple bcTriple(bcModule->getTargetTriple());
        Debug(SourcePos(), "module triple: %s\nbitcode triple: %s\n", mTriple.str().c_str(), bcTriple.str().c_str());

        bcModule->setTargetTriple(mTriple.str());
        bcModule->setDataLayout(module->getDataLayout());

        if (g->target->isXeTarget()) {
            // Maybe we will use it for other targets in future,
            // but now it is needed only by Xe. We need
            // to update attributes because Xe intrinsics are
            // separated from the others and it is not done by default
            lUpdateIntrinsicsAttributes(bcModule);
        }

        for (llvm::Function &f : *bcModule) {
            // TODO! do we really try to define already included symbol?
            if (!module->getFunction(f.getName())) {
                module->getOrInsertFunction(f.getName(), f.getFunctionType(), f.getAttributes());
            }
        }
    }
}

llvm::Constant *lFuncAsConstInt8Ptr(llvm::Module &M, const char *name) {
    llvm::LLVMContext &Context = M.getContext();
    llvm::Function *F = M.getFunction(name);
#if ISPC_LLVM_VERSION >= ISPC_LLVM_17_0
    llvm::Type *type = llvm::PointerType::getUnqual(Context);
#else
    llvm::Type *type = llvm::Type::getInt8PtrTy(Context);
#endif
    if (F) {
        return llvm::ConstantExpr::getBitCast(F, type);
    }
    return nullptr;
}

void lRemoveUnused(llvm::Module *M) {
    llvm::FunctionAnalysisManager FAM;
    llvm::ModuleAnalysisManager MAM;
    llvm::ModulePassManager PM;
    llvm::PassBuilder PB;
    PB.registerModuleAnalyses(MAM);
    PM.addPass(llvm::GlobalDCEPass());
    PM.run(*M, MAM);
}

// Extract functions from llvm.compiler.used that are currently used in the module.
void lExtractUsedFunctions(llvm::GlobalVariable *llvmUsed, std::unordered_set<llvm::Function *> &usedFunctions) {
    llvm::ConstantArray *initList = llvm::cast<llvm::ConstantArray>(llvmUsed->getInitializer());
    for (unsigned i = 0; i < initList->getNumOperands(); i++) {
        auto *C = initList->getOperand(i);
        llvm::ConstantExpr *CE = llvm::dyn_cast<llvm::ConstantExpr>(C);
        // Bitcast as ConstExpr when opaque pointer is not used, otherwise C is just an opaque pointer.
        llvm::Value *val = CE ? CE->getOperand(0) : C;
        Assert(val);
        if (val->getNumUses() > 1) {
            Assert(llvm::isa<llvm::Function>(val));
            usedFunctions.insert(llvm::cast<llvm::Function>(val));
        }
    }
}

// Find persistent groups that are used in the module.
void lFindUsedPersistentGroups(llvm::Module *M, std::unordered_set<llvm::Function *> &usedFunctions,
                               std::unordered_set<const builtin::PersistentGroup *> &usedPersistentGroups) {
    for (auto const &[group, functions] : builtin::persistentGroups) {
        for (auto const &name : functions) {
            llvm::Function *F = M->getFunction(name);
            if (usedFunctions.find(F) != usedFunctions.end()) {
                usedPersistentGroups.insert(&group);
                break;
            }
        }
    }
}

// Collect functions that should be preserved in the module based on the used persistent groups.
void lCollectPreservedFunctions(llvm::Module *M, std::vector<llvm::Constant *> &newElements,
                                std::unordered_set<const builtin::PersistentGroup *> &usedPersistentGroups) {
    for (auto const &[group, functions] : builtin::persistentGroups) {
        if (usedPersistentGroups.find(&group) != usedPersistentGroups.end()) {
            for (auto const &name : functions) {
                if (llvm::Constant *C = lFuncAsConstInt8Ptr(*M, name)) {
                    newElements.push_back(C);
                }
            }
        }
    }
    // Extend the list of preserved functions with the functions from corresponding persistent groups.
    for (auto const &[name, val] : builtin::persistentFuncs) {
        if (llvm::Constant *C = lFuncAsConstInt8Ptr(*M, name.c_str())) {
            newElements.push_back(C);
        }
    }
}

void lCreateLLVMUsed(llvm::Module &M, std::vector<llvm::Constant *> &ConstPtrs) {
    llvm::LLVMContext &Context = M.getContext();

    // Create the array of i8* that llvm.used will hold
#if ISPC_LLVM_VERSION >= ISPC_LLVM_17_0
    llvm::Type *type = llvm::PointerType::getUnqual(Context);
#else
    llvm::Type *type = llvm::Type::getInt8PtrTy(Context);
#endif
    llvm::ArrayType *ATy = llvm::ArrayType::get(type, ConstPtrs.size());
    llvm::Constant *ArrayInit = llvm::ConstantArray::get(ATy, ConstPtrs);

    // Create llvm.used and initialize it with the functions
    llvm::GlobalVariable *llvmUsed = new llvm::GlobalVariable(
        M, ArrayInit->getType(), false, llvm::GlobalValue::AppendingLinkage, ArrayInit, "llvm.compiler.used");
    llvmUsed->setSection("llvm.metadata");
}

// Update llvm.compiler.used with the new list of preserved functions.
void lUpdateLLVMUsed(llvm::Module *M, llvm::GlobalVariable *llvmUsed, std::vector<llvm::Constant *> &newElements) {
    llvmUsed->eraseFromParent();
    lCreateLLVMUsed(*M, newElements);
}

void lRemoveUnusedPersistentFunctions(llvm::Module *M) {
    // The idea here is to preserve only the needed subset of persistent functions.
    // Inspect llvm.compiler.used and find all functions that are used in the
    // module and re-create it with only those functions (and corresponding
    // persistent groups).
    llvm::GlobalVariable *llvmUsed = M->getNamedGlobal("llvm.compiler.used");
    if (llvmUsed) {
        std::unordered_set<llvm::Function *> usedFunctions;
        lExtractUsedFunctions(llvmUsed, usedFunctions);

        std::unordered_set<const builtin::PersistentGroup *> usedPersistentGroups;
        lFindUsedPersistentGroups(M, usedFunctions, usedPersistentGroups);

        std::vector<llvm::Constant *> newElements;
        lCollectPreservedFunctions(M, newElements, usedPersistentGroups);

        lUpdateLLVMUsed(M, llvmUsed, newElements);
        lRemoveUnused(M);
    }
}

void ispc::debugDumpModule(llvm::Module *module, std::string name, int stage) {
    if (!(g->off_stages.find(stage) == g->off_stages.end() && g->debug_stages.find(stage) != g->debug_stages.end())) {
        return;
    }

    SourcePos noPos;
    name = std::string("pre_") + std::to_string(stage) + "_" + name + ".ll";
    if (g->dumpFile && !g->dumpFilePath.empty()) {
        std::error_code EC;
        llvm::SmallString<128> path(g->dumpFilePath);

        if (!llvm::sys::fs::exists(path)) {
            EC = llvm::sys::fs::create_directories(g->dumpFilePath);
            if (EC) {
                Error(noPos, "Error creating directory '%s': %s", g->dumpFilePath.c_str(), EC.message().c_str());
                return;
            }
        }

        if (g->isMultiTargetCompilation) {
            name += std::string("_") + g->target->GetISAString();
        }
        llvm::sys::path::append(path, name);
        llvm::raw_fd_ostream OS(path, EC);

        if (EC) {
            Error(noPos, "Error opening file '%s': %s", path.c_str(), EC.message().c_str());
            return;
        }

        module->print(OS, nullptr);
        OS.flush();
    } else {
        // dump to stdout
        module->print(llvm::outs(), nullptr);
    }
}

void ispc::LinkDispatcher(llvm::Module *module) {
    const BitcodeLib *dispatch = g->target_registry->getDispatchLib(g->target_os);
    Assert(dispatch);
    llvm::Module *dispatchBCModule = dispatch->getLLVMModule();
    lAddDeclarationsToModule(dispatchBCModule, module);
    lAddBitcodeToModule(dispatchBCModule, module);
}

void lLinkCommonBuiltins(llvm::Module *module) {
    const BitcodeLib *builtins = g->target_registry->getBuiltinsCLib(g->target_os, g->target->getArch());
    Assert(builtins);
    llvm::Module *builtinsBCModule = builtins->getLLVMModule();

    // Suppress LLVM warnings about incompatible target triples and data layout when linking.
    // It is OK until we have different platform agnostic bc libraries.
    builtinsBCModule->setDataLayout(g->target->getDataLayout()->getStringRepresentation());
    builtinsBCModule->setTargetTriple(module->getTargetTriple());

    // Unlike regular builtins and dispatch module, which don't care about mangling of external functions,
    // so they only differentiate Windows/Unix and 32/64 bit, builtins-c need to take care about mangling.
    // Hence, different version for all potentially supported OSes.
    lAddBitcodeToModule(builtinsBCModule, module);

    llvm::StringSet<> commonBuiltins = {builtin::__do_print, builtin::__num_cores};
    lSetAsInternal(module, commonBuiltins);
}

void lAddPersistentToLLVMUsed(llvm::Module &M) {
    // Bitcast all function pointer to i8*
    std::vector<llvm::Constant *> ConstPtrs;
    for (auto const &[group, functions] : persistentGroups) {
        bool isGroupUsed = false;
        for (auto const &name : functions) {
            llvm::Function *F = M.getFunction(name);
            if (F && F->getNumUses() > 0) {
                isGroupUsed = true;
                break;
            }
        }
        // TODO! comment that we don't need to preserve unused symbols chains
        if (isGroupUsed) {
            for (auto const &name : functions) {
                if (llvm::Constant *C = lFuncAsConstInt8Ptr(M, name)) {
                    ConstPtrs.push_back(C);
                }
            }
        }
    }

    for (auto const &[name, val] : persistentFuncs) {
        if (llvm::Constant *C = lFuncAsConstInt8Ptr(M, name.c_str())) {
            ConstPtrs.push_back(C);
        }
    }

    if (ConstPtrs.empty()) {
        return;
    }

    lCreateLLVMUsed(M, ConstPtrs);
}

bool lStartsWithLLVM(llvm::StringRef name) {
#if ISPC_LLVM_VERSION >= ISPC_LLVM_17_0
    return name.starts_with("llvm.");
#else
    return name.startswith("llvm.");
#endif
}

// bool lIsInPersistentGroups(const std::string& name) {
//     for (const auto& pair : builtin::persistentGroups) {
//         const auto& charVector = pair.second;
//         for (const char* str : charVector) {
//             if (name == str) {
//                 return true;
//             }
//         }
//     }
//     return false;
// }

std::unordered_set<std::string> targetBuiltinFuncs = {
// builtins.isph
"__acos_uniform_double",
"__acos_uniform_float",
"__acos_uniform_half",
"__acos_varying_double",
"__acos_varying_float",
"__acos_varying_half",
"__asin_uniform_double",
"__asin_uniform_float",
"__asin_uniform_half",
"__asin_varying_double",
"__asin_varying_float",
"__asin_varying_half",
"__atan2_uniform_double",
"__atan2_uniform_float",
"__atan2_uniform_half",
"__atan2_varying_double",
"__atan2_varying_float",
"__atan2_varying_half",
"__atan_uniform_double",
"__atan_uniform_float",
"__atan_uniform_half",
"__atan_varying_double",
"__atan_varying_float",
"__atan_varying_half",
"__cos_uniform_double",
"__cos_uniform_float",
"__cos_uniform_half",
"__cos_varying_double",
"__cos_varying_float",
"__cos_varying_half",
"__sincos_uniform_double",
"__sincos_uniform_float",
"__sincos_uniform_half",
"__sincos_varying_double",
"__sincos_varying_float",
"__sincos_varying_half",
"__sin_uniform_double",
"__sin_uniform_float",
"__sin_uniform_half",
"__sin_varying_double",
"__sin_varying_float",
"__sin_varying_half",
"__tan_uniform_double",
"__tan_uniform_float",
"__tan_uniform_half",
"__tan_varying_double",
"__tan_varying_float",
"__tan_varying_half",
"__aos_to_soa2_double64",
"__aos_to_soa2_double",
"__aos_to_soa2_float64",
"__aos_to_soa2_float",
"__aos_to_soa3_double64",
"__aos_to_soa3_double",
"__aos_to_soa3_float64",
"__aos_to_soa3_float",
"__aos_to_soa4_double64",
"__aos_to_soa4_double",
"__aos_to_soa4_float64",
"__aos_to_soa4_float",
"__soa_to_aos2_double64",
"__soa_to_aos2_double",
"__soa_to_aos2_float64",
"__soa_to_aos2_float",
"__soa_to_aos3_double64",
"__soa_to_aos3_double",
"__soa_to_aos3_float64",
"__soa_to_aos3_float",
"__soa_to_aos4_double64",
"__soa_to_aos4_double",
"__soa_to_aos4_float64",
"__soa_to_aos4_float",
"__atomic_add_int32_global",
"__atomic_add_int64_global",
"__atomic_add_uniform_int32_global",
"__atomic_add_uniform_int64_global",
"__atomic_add_varying_int32_global",
"__atomic_add_varying_int64_global",
"__atomic_and_int32_global",
"__atomic_and_int64_global",
"__atomic_and_uniform_int32_global",
"__atomic_and_uniform_int64_global",
"__atomic_and_varying_int32_global",
"__atomic_and_varying_int64_global",
"__atomic_compare_exchange_double_global",
"__atomic_compare_exchange_float_global",
"__atomic_compare_exchange_int32_global",
"__atomic_compare_exchange_int64_global",
"__atomic_compare_exchange_uniform_double_global",
"__atomic_compare_exchange_uniform_float_global",
"__atomic_compare_exchange_uniform_int32_global",
"__atomic_compare_exchange_uniform_int64_global",
"__atomic_compare_exchange_varying_double_global",
"__atomic_compare_exchange_varying_float_global",
"__atomic_compare_exchange_varying_int32_global",
"__atomic_compare_exchange_varying_int64_global",
"__atomic_fadd_double_global",
"__atomic_fadd_double_global",
"__atomic_fadd_float_global",
"__atomic_fadd_float_global",
"__atomic_fadd_uniform_double_global",
"__atomic_fadd_uniform_double_global",
"__atomic_fadd_uniform_float_global",
"__atomic_fadd_uniform_float_global",
"__atomic_fadd_varying_double_global",
"__atomic_fadd_varying_double_global",
"__atomic_fadd_varying_float_global",
"__atomic_fadd_varying_float_global",
"__atomic_fmax_double_global",
"__atomic_fmax_float_global",
"__atomic_fmax_uniform_double_global",
"__atomic_fmax_uniform_double_global",
"__atomic_fmax_uniform_float_global",
"__atomic_fmax_uniform_float_global",
"__atomic_fmax_varying_double_global",
"__atomic_fmax_varying_double_global",
"__atomic_fmax_varying_float_global",
"__atomic_fmax_varying_float_global",
"__atomic_fmin_double_global",
"__atomic_fmin_float_global",
"__atomic_fmin_uniform_double_global",
"__atomic_fmin_uniform_double_global",
"__atomic_fmin_uniform_float_global",
"__atomic_fmin_uniform_float_global",
"__atomic_fmin_varying_double_global",
"__atomic_fmin_varying_double_global",
"__atomic_fmin_varying_float_global",
"__atomic_fmin_varying_float_global",
"__atomic_fsub_double_global",
"__atomic_fsub_double_global",
"__atomic_fsub_float_global",
"__atomic_fsub_float_global",
"__atomic_fsub_uniform_double_global",
"__atomic_fsub_uniform_double_global",
"__atomic_fsub_uniform_float_global",
"__atomic_fsub_uniform_float_global",
"__atomic_fsub_varying_double_global",
"__atomic_fsub_varying_double_global",
"__atomic_fsub_varying_float_global",
"__atomic_fsub_varying_float_global",
"__atomic_max_uniform_int32_global",
"__atomic_max_uniform_int64_global",
"__atomic_max_varying_int32_global",
"__atomic_max_varying_int64_global",
"__atomic_min_uniform_int32_global",
"__atomic_min_uniform_int64_global",
"__atomic_min_varying_int32_global",
"__atomic_min_varying_int64_global",
"__atomic_or_int32_global",
"__atomic_or_int64_global",
"__atomic_or_uniform_int32_global",
"__atomic_or_uniform_int64_global",
"__atomic_or_varying_int32_global",
"__atomic_or_varying_int64_global",
"__atomic_sub_int32_global",
"__atomic_sub_int64_global",
"__atomic_sub_uniform_int32_global",
"__atomic_sub_uniform_int64_global",
"__atomic_sub_varying_int32_global",
"__atomic_sub_varying_int64_global",
"__atomic_swap_uniform_double_global",
"__atomic_swap_uniform_float_global",
"__atomic_swap_uniform_int32_global",
"__atomic_swap_uniform_int64_global",
"__atomic_swap_varying_double_global",
"__atomic_swap_varying_float_global",
"__atomic_swap_varying_int32_global",
"__atomic_swap_varying_int64_global",
"__atomic_umax_uniform_uint32_global",
"__atomic_umax_uniform_uint64_global",
"__atomic_umax_varying_uint32_global",
"__atomic_umax_varying_uint64_global",
"__atomic_umin_uniform_uint32_global",
"__atomic_umin_uniform_uint64_global",
"__atomic_umin_varying_uint32_global",
"__atomic_umin_varying_uint64_global",
"__atomic_xor_int32_global",
"__atomic_xor_int64_global",
"__atomic_xor_uniform_int32_global",
"__atomic_xor_uniform_int64_global",
"__atomic_xor_varying_int32_global",
"__atomic_xor_varying_int64_global",
"__memory_barrier",
"__abs_ui64",
"__abs_vi64",
"__padds_ui16",
"__padds_ui32",
"__padds_ui64",
"__padds_ui8",
"__padds_vi16",
"__padds_vi32",
"__padds_vi64",
"__padds_vi8",
"__paddus_ui16",
"__paddus_ui32",
"__paddus_ui64",
"__paddus_ui8",
"__paddus_vi16",
"__paddus_vi32",
"__paddus_vi64",
"__paddus_vi8",
"__pdivs_ui16",
"__pdivs_ui32",
"__pdivs_ui64",
"__pdivs_ui8",
"__pdivs_vi16",
"__pdivs_vi32",
"__pdivs_vi64",
"__pdivs_vi8",
"__pdivus_ui16",
"__pdivus_ui32",
"__pdivus_ui64",
"__pdivus_ui8",
"__pdivus_vi16",
"__pdivus_vi32",
"__pdivus_vi64",
"__pdivus_vi8",
"__pmuls_ui16",
"__pmuls_ui32",
"__pmuls_ui64",
"__pmuls_ui8",
"__pmuls_vi16",
"__pmuls_vi32",
"__pmuls_vi64",
"__pmuls_vi8",
"__pmulus_ui16",
"__pmulus_ui32",
"__pmulus_ui64",
"__pmulus_ui8",
"__pmulus_vi16",
"__pmulus_vi32",
"__pmulus_vi64",
"__pmulus_vi8",
"__psubs_ui16",
"__psubs_ui32",
"__psubs_ui64",
"__psubs_ui8",
"__psubs_vi16",
"__psubs_vi32",
"__psubs_vi64",
"__psubs_vi8",
"__psubus_ui16",
"__psubus_ui32",
"__psubus_ui64",
"__psubus_ui8",
"__psubus_vi16",
"__psubus_vi32",
"__psubus_vi64",
"__psubus_vi8",
"__ceil_uniform_double",
"__ceil_uniform_float",
"__ceil_uniform_half",
"__ceil_varying_double",
"__ceil_varying_float",
"__ceil_varying_half",
"__exp_uniform_double",
"__exp_uniform_float",
"__exp_uniform_half",
"__exp_varying_double",
"__exp_varying_float",
"__exp_varying_half",
"__floor_uniform_double",
"__floor_uniform_float",
"__floor_uniform_half",
"__floor_varying_double",
"__floor_varying_float",
"__floor_varying_half",
"__log_uniform_double",
"__log_uniform_float",
"__log_uniform_half",
"__log_varying_double",
"__log_varying_float",
"__log_varying_half",
"__pow_uniform_double",
"__pow_uniform_float",
"__pow_uniform_half",
"__pow_varying_double",
"__pow_varying_float",
"__pow_varying_half",
"__rcp_fast_uniform_double",
"__rcp_fast_uniform_float",
"__rcp_fast_varying_double",
"__rcp_fast_varying_float",
"__rcp_uniform_double",
"__rcp_uniform_float",
"__rcp_uniform_half",
"__rcp_varying_double",
"__rcp_varying_float",
"__rcp_varying_half",
"__round_uniform_double",
"__round_uniform_float",
"__round_uniform_half",
"__round_varying_double",
"__round_varying_float",
"__round_varying_half",
"__rsqrt_fast_uniform_double",
"__rsqrt_fast_uniform_float",
"__rsqrt_fast_varying_double",
"__rsqrt_fast_varying_float",
"__rsqrt_uniform_double",
"__rsqrt_uniform_float",
"__rsqrt_uniform_half",
"__rsqrt_varying_double",
"__rsqrt_varying_float",
"__rsqrt_varying_half",
"__sqrt_uniform_double",
"__sqrt_uniform_float",
"__sqrt_uniform_half",
"__sqrt_varying_double",
"__sqrt_varying_float",
"__sqrt_varying_half",
"__trunc_uniform_double",
"__trunc_uniform_float",
"__trunc_uniform_half",
"__trunc_varying_double",
"__trunc_varying_float",
"__trunc_varying_half",
"__popcnt_int32",
"__popcnt_int64",
"__clock",
"__exclusive_scan_add_double",
"__exclusive_scan_add_float",
"__exclusive_scan_add_half",
"__exclusive_scan_add_i32",
"__exclusive_scan_add_i64",
"__exclusive_scan_and_i32",
"__exclusive_scan_and_i64",
"__exclusive_scan_or_i32",
"__exclusive_scan_or_i64",
"__extract_bool",
"__extract_int16",
"__extract_int32",
"__extract_int64",
"__extract_int8",
"__insert_bool",
"__insert_int16",
"__insert_int32",
"__insert_int64",
"__insert_int8",
"__dot2add_i16packed_sat",
"__dot2add_i16packed",
"__dot4add_u8i8packed_sat",
"__dot4add_u8i8packed",
"__doublebits_uniform_int64",
"__doublebits_varying_int64",
"__floatbits_uniform_int32",
"__floatbits_varying_int32",
"__float_to_half_uniform",
"__float_to_half_varying",
"__halfbits_uniform_int16",
"__halfbits_varying_int16",
"__half_to_float_uniform",
"__half_to_float_varying",
"__intbits_uniform_double",
"__intbits_uniform_float",
"__intbits_uniform_half",
"__intbits_varying_double",
"__intbits_varying_float",
"__intbits_varying_half",
"__undef_uniform",
"__undef_varying",
"__fastmath",
"__gather_elt32_double",
"__gather_elt32_float",
"__gather_elt32_half",
"__gather_elt32_i16",
"__gather_elt32_i32",
"__gather_elt32_i64",
"__gather_elt32_i8",
"__gather_elt64_double",
"__gather_elt64_float",
"__gather_elt64_half",
"__gather_elt64_i16",
"__gather_elt64_i32",
"__gather_elt64_i64",
"__gather_elt64_i8",
"__scatter_elt32_double",
"__scatter_elt32_float",
"__scatter_elt32_half",
"__scatter_elt32_i16",
"__scatter_elt32_i32",
"__scatter_elt32_i64",
"__scatter_elt32_i8",
"__scatter_elt64_double",
"__scatter_elt64_float",
"__scatter_elt64_half",
"__scatter_elt64_i16",
"__scatter_elt64_i32",
"__scatter_elt64_i64",
"__scatter_elt64_i8",
"__idiv_int16",
"__idiv_int32",
"__idiv_int8",
"__idiv_uint16",
"__idiv_uint32",
"__idiv_uint8",
"__masked_load_private_double",
"__masked_load_private_float",
"__masked_load_private_i16",
"__masked_load_private_i32",
"__masked_load_private_i64",
"__masked_load_private_i8",
"__max_uniform_double",
"__max_uniform_float",
"__max_uniform_half",
"__max_uniform_int32",
"__max_uniform_int64",
"__max_uniform_uint32",
"__max_uniform_uint64",
"__max_varying_double",
"__max_varying_float",
"__max_varying_half",
"__max_varying_int32",
"__max_varying_int64",
"__max_varying_uint32",
"__max_varying_uint64",
"__min_uniform_double",
"__min_uniform_float",
"__min_uniform_half",
"__min_uniform_int32",
"__min_uniform_int64",
"__min_uniform_uint32",
"__min_uniform_uint64",
"__min_varying_double",
"__min_varying_float",
"__min_varying_half",
"__min_varying_int32",
"__min_varying_int64",
"__min_varying_uint32",
"__min_varying_uint64",
"__memcpy32",
"__memcpy64",
"__memmove32",
"__memmove64",
"__memset32",
"__memset64",
"__reduce_add_double",
"__reduce_add_float",
"__reduce_add_half",
"__reduce_add_int16",
"__reduce_add_int32",
"__reduce_add_int64",
"__reduce_add_int8",
"__reduce_equal_double",
"__reduce_equal_float",
"__reduce_equal_half",
"__reduce_equal_int32",
"__reduce_equal_int64",
"__reduce_max_double",
"__reduce_max_float",
"__reduce_max_half",
"__reduce_max_int32",
"__reduce_max_int64",
"__reduce_max_uint32",
"__reduce_max_uint64",
"__reduce_min_double",
"__reduce_min_float",
"__reduce_min_half",
"__reduce_min_int32",
"__reduce_min_int64",
"__reduce_min_uint32",
"__reduce_min_uint64",
"__rdrand_i16",
"__rdrand_i32",
"__rdrand_i64",
"__packed_load_activei32",
"__packed_load_activei64",
"__packed_store_active2i32",
"__packed_store_active2i64",
"__packed_store_activei32",
"__packed_store_activei64",
"__prefetch_read_sized_uniform_1",
"__prefetch_read_sized_uniform_2",
"__prefetch_read_sized_uniform_3",
"__prefetch_read_sized_uniform_nt",
"__prefetch_read_sized_varying_1",
"__prefetch_read_sized_varying_2",
"__prefetch_read_sized_varying_3",
"__prefetch_read_sized_varying_nt",
"__prefetch_read_uniform_1",
"__prefetch_read_uniform_2",
"__prefetch_read_uniform_3",
"__prefetch_read_uniform_nt",
"__prefetch_write_uniform_1",
"__prefetch_write_uniform_2",
"__prefetch_write_uniform_3",
"__sext_uniform_bool",
"__sext_varying_bool",
"__streaming_load_uniform_double",
"__streaming_load_uniform_float",
"__streaming_load_uniform_half",
"__streaming_load_uniform_i16",
"__streaming_load_uniform_i32",
"__streaming_load_uniform_i64",
"__streaming_load_uniform_i8",
"__streaming_load_varying_double",
"__streaming_load_varying_float",
"__streaming_load_varying_half",
"__streaming_load_varying_i16",
"__streaming_load_varying_i32",
"__streaming_load_varying_i64",
"__streaming_load_varying_i8",
"__streaming_store_uniform_double",
"__streaming_store_uniform_float",
"__streaming_store_uniform_half",
"__streaming_store_uniform_i16",
"__streaming_store_uniform_i32",
"__streaming_store_uniform_i64",
"__streaming_store_uniform_i8",
"__streaming_store_varying_double",
"__streaming_store_varying_float",
"__streaming_store_varying_half",
"__streaming_store_varying_i16",
"__streaming_store_varying_i32",
"__streaming_store_varying_i64",
"__streaming_store_varying_i8",
"__stdlib_acosf",
"__stdlib_asinf",
"__stdlib_asin",
"__stdlib_atan2f",
"__stdlib_atan2",
"__stdlib_atanf",
"__stdlib_atan",
"__stdlib_cosf",
"__stdlib_cos",
"__stdlib_expf",
"__stdlib_exp",
"__stdlib_logf",
"__stdlib_log",
"__stdlib_powf",
"__stdlib_pow",
"__stdlib_sincosf",
"__stdlib_sincos",
"__stdlib_sinf",
"__stdlib_sin",
"__stdlib_tanf",
"__stdlib_tan",
"__broadcast_double",
"__broadcast_float",
"__broadcast_half",
"__broadcast_i16",
"__broadcast_i32",
"__broadcast_i64",
"__broadcast_i8",
"__cast_mask_to_i64",
"__rotate_double",
"__rotate_float",
"__rotate_half",
"__rotate_i16",
"__rotate_i32",
"__rotate_i64",
"__rotate_i8",
"__shift_double",
"__shift_float",
"__shift_half",
"__shift_i16",
"__shift_i32",
"__shift_i64",
"__shift_i8",
"__shuffle2_double",
"__shuffle2_float",
"__shuffle2_half",
"__shuffle2_i16",
"__shuffle2_i32",
"__shuffle2_i64",
"__shuffle2_i8",
"__shuffle_double",
"__shuffle_float",
"__shuffle_half",
"__shuffle_i16",
"__shuffle_i32",
"__shuffle_i64",
"__shuffle_i8",
// core.isph
"__masked_load_i8",
"__masked_load_i16",
"__masked_load_half",
"__masked_load_i32",
"__masked_load_float",
"__masked_load_i64",
"__masked_load_double",
"__masked_store_i8",
"__masked_store_i16",
"__masked_store_i32",
"__masked_store_i64",
"__masked_store_float",
"__masked_store_double",
"__masked_store_blend_float",
"__masked_store_blend_double",
"__masked_store_half",
"__masked_store_blend_half",
"__masked_store_blend_i8",
"__masked_store_blend_i16",
"__masked_store_blend_i32",
"__masked_store_blend_i64",
"__gather32_i8",
"__gather64_i8",
"__gather32_i16",
"__gather64_i16",
"__gather32_half",
"__gather64_half",
"__gather32_i32",
"__gather64_i32",
"__gather32_float",
"__gather64_float",
"__gather32_i64",
"__gather64_i64",
"__gather32_double",
"__gather64_double",
"__scatter32_i8",
"__scatter64_i8",
"__scatter32_i16",
"__scatter64_i16",
"__scatter32_half",
"__scatter64_half",
"__scatter32_i32",
"__scatter64_i32",
"__scatter32_float",
"__scatter64_float",
"__scatter32_i64",
"__scatter64_i64",
"__scatter32_double",
"__scatter64_double",
"__prefetch_read_varying_1",
"__prefetch_read_varying_2",
"__prefetch_read_varying_3",
"__prefetch_read_varying_nt",
"__prefetch_write_varying_1",
"__prefetch_write_varying_2",
"__prefetch_write_varying_3",
"__avg_up_uint8",
"__avg_up_int8",
"__avg_up_uint16",
"__avg_up_int16",
"__avg_down_uint8",
"__avg_down_int8",
"__avg_down_uint16",
"__avg_down_int16",
"__set_ftz_daz_flags",
"__restore_ftz_daz_flags",
"__set_ftz_daz_flags",
"__restore_ftz_daz_flags",
"__new_uniform_32rt",
"__new_varying32_32rt",
"__delete_uniform_32rt",
"__delete_varying_32rt",
"__new_uniform_64rt",
"__new_varying32_64rt",
"__new_varying64_64rt",
"__delete_uniform_64rt",
"__delete_varying_64rt",
"__do_assume_uniform",
"__do_assert_uniform",
"__do_assert_varying",
"__do_assert_uniform",
"__do_assert_varying",
"__count_leading_zeros_i32",
"__count_leading_zeros_i64",
"__count_trailing_zeros_i32",
"__count_trailing_zeros_i64",
"__movmsk",
"__any",
"__all",
"__none",
"__do_print",
"__num_cores",
"__set_system_isa",
"__task_index0",
"__task_index1",
"__task_index2",
"__task_index",
"__task_count0",
"__task_count1",
"__task_count2",
"__task_count",
"__wasm_cmp_msk_eq",
};
std::unordered_set<std::string> pseudoFuncs = {
// builtins.isph
"__masked_load_blend_double",
"__masked_load_blend_float",
"__masked_load_blend_half",
"__masked_load_blend_i16",
"__masked_load_blend_i32",
"__masked_load_blend_i64",
"__masked_load_blend_i8",
// core.isph
"__pseudo_masked_store_i8",
"__pseudo_masked_store_i16",
"__pseudo_masked_store_half",
"__pseudo_masked_store_i32",
"__pseudo_masked_store_float",
"__pseudo_masked_store_i64",
"__pseudo_masked_store_double",
"__pseudo_gather32_i8",
"__pseudo_gather32_i16",
"__pseudo_gather32_half",
"__pseudo_gather32_i32",
"__pseudo_gather32_float",
"__pseudo_gather32_i64",
"__pseudo_gather32_double",
"__pseudo_gather64_i8",
"__pseudo_gather64_i16",
"__pseudo_gather64_half",
"__pseudo_gather64_i32",
"__pseudo_gather64_float",
"__pseudo_gather64_i64",
"__pseudo_gather64_double",
"__pseudo_gather_factored_base_offsets32_i8",
"__pseudo_gather_factored_base_offsets32_i16",
"__pseudo_gather_factored_base_offsets32_half",
"__pseudo_gather_factored_base_offsets32_i32",
"__pseudo_gather_factored_base_offsets32_float",
"__pseudo_gather_factored_base_offsets32_i64",
"__pseudo_gather_factored_base_offsets32_double",
"__pseudo_gather_factored_base_offsets64_i8",
"__pseudo_gather_factored_base_offsets64_i16",
"__pseudo_gather_factored_base_offsets64_half",
"__pseudo_gather_factored_base_offsets64_i32",
"__pseudo_gather_factored_base_offsets64_float",
"__pseudo_gather_factored_base_offsets64_i64",
"__pseudo_gather_factored_base_offsets64_double",
"__pseudo_gather_base_offsets32_i8",
"__pseudo_gather_base_offsets32_i16",
"__pseudo_gather_base_offsets32_half",
"__pseudo_gather_base_offsets32_i32",
"__pseudo_gather_base_offsets32_float",
"__pseudo_gather_base_offsets32_i64",
"__pseudo_gather_base_offsets32_double",
"__pseudo_gather_base_offsets64_i8",
"__pseudo_gather_base_offsets64_i16",
"__pseudo_gather_base_offsets64_half",
"__pseudo_gather_base_offsets64_i32",
"__pseudo_gather_base_offsets64_float",
"__pseudo_gather_base_offsets64_i64",
"__pseudo_gather_base_offsets64_double",
"__pseudo_scatter32_i8",
"__pseudo_scatter32_i16",
"__pseudo_scatter32_half",
"__pseudo_scatter32_i32",
"__pseudo_scatter32_float",
"__pseudo_scatter32_i64",
"__pseudo_scatter32_double",
"__pseudo_scatter64_i8",
"__pseudo_scatter64_i16",
"__pseudo_scatter64_half",
"__pseudo_scatter64_i32",
"__pseudo_scatter64_float",
"__pseudo_scatter64_i64",
"__pseudo_scatter64_double",
"__pseudo_scatter_factored_base_offsets32_i8",
"__pseudo_scatter_factored_base_offsets32_i16",
"__pseudo_scatter_factored_base_offsets32_half",
"__pseudo_scatter_factored_base_offsets32_i32",
"__pseudo_scatter_factored_base_offsets32_float",
"__pseudo_scatter_factored_base_offsets32_i64",
"__pseudo_scatter_factored_base_offsets32_double",
"__pseudo_scatter_factored_base_offsets64_i8",
"__pseudo_scatter_factored_base_offsets64_i16",
"__pseudo_scatter_factored_base_offsets64_half",
"__pseudo_scatter_factored_base_offsets64_i32",
"__pseudo_scatter_factored_base_offsets64_float",
"__pseudo_scatter_factored_base_offsets64_i64",
"__pseudo_scatter_factored_base_offsets64_double",
"__pseudo_scatter_base_offsets32_i8",
"__pseudo_scatter_base_offsets32_i16",
"__pseudo_scatter_base_offsets32_half",
"__pseudo_scatter_base_offsets32_i32",
"__pseudo_scatter_base_offsets32_float",
"__pseudo_scatter_base_offsets32_i64",
"__pseudo_scatter_base_offsets32_double",
"__pseudo_scatter_base_offsets64_i8",
"__pseudo_scatter_base_offsets64_i16",
"__pseudo_scatter_base_offsets64_half",
"__pseudo_scatter_base_offsets64_i32",
"__pseudo_scatter_base_offsets64_float",
"__pseudo_scatter_base_offsets64_i64",
"__pseudo_scatter_base_offsets64_double",
"__pseudo_prefetch_read_varying_1",
"__pseudo_prefetch_read_varying_1_native",
"__pseudo_prefetch_read_varying_2",
"__pseudo_prefetch_read_varying_2_native",
"__pseudo_prefetch_read_varying_3",
"__pseudo_prefetch_read_varying_3_native",
"__pseudo_prefetch_read_varying_nt",
"__pseudo_prefetch_read_varying_nt_native",
"__pseudo_prefetch_write_varying_1",
"__pseudo_prefetch_write_varying_1_native",
"__pseudo_prefetch_write_varying_2",
"__pseudo_prefetch_write_varying_2_native",
"__pseudo_prefetch_write_varying_3",
"__pseudo_prefetch_write_varying_3_native",
"__gather32_generic_i8",
"__gather64_generic_i8",
"__gather32_generic_i16",
"__gather64_generic_i16",
"__gather32_generic_half",
"__gather64_generic_half",
"__gather32_generic_i32",
"__gather64_generic_i32",
"__gather32_generic_float",
"__gather64_generic_float",
"__gather32_generic_i64",
"__gather64_generic_i64",
"__gather32_generic_double",
"__gather64_generic_double",
"__scatter32_generic_i8",
"__scatter64_generic_i8",
"__scatter32_generic_i16",
"__scatter64_generic_i16",
"__scatter32_generic_half",
"__scatter64_generic_half",
"__scatter32_generic_i32",
"__scatter64_generic_i32",
"__scatter32_generic_float",
"__scatter64_generic_float",
"__scatter32_generic_i64",
"__scatter64_generic_i64",
"__scatter32_generic_double",
"__scatter64_generic_double",
"__gather_factored_base_offsets32_i8",
"__gather_factored_base_offsets64_i8",
"__gather_factored_base_offsets32_i16",
"__gather_factored_base_offsets64_i16",
"__gather_factored_base_offsets32_half",
"__gather_factored_base_offsets64_half",
"__gather_factored_base_offsets32_i32",
"__gather_factored_base_offsets64_i32",
"__gather_factored_base_offsets32_float",
"__gather_factored_base_offsets64_float",
"__gather_factored_base_offsets32_i64",
"__gather_factored_base_offsets64_i64",
"__gather_factored_base_offsets32_double",
"__gather_factored_base_offsets64_double",
"__scatter_factored_base_offsets32_i8",
"__scatter_factored_base_offsets64_i8",
"__scatter_factored_base_offsets32_i16",
"__scatter_factored_base_offsets64_i16",
"__scatter_factored_base_offsets32_half",
"__scatter_factored_base_offsets64_half",
"__scatter_factored_base_offsets32_i32",
"__scatter_factored_base_offsets64_i32",
"__scatter_factored_base_offsets32_float",
"__scatter_factored_base_offsets64_float",
"__scatter_factored_base_offsets32_i64",
"__scatter_factored_base_offsets64_i64",
"__scatter_factored_base_offsets32_double",
"__scatter_factored_base_offsets64_double",
"__gather_base_offsets32_i8",
"__gather_base_offsets64_i8",
"__gather_base_offsets32_i16",
"__gather_base_offsets64_i16",
"__gather_base_offsets32_half",
"__gather_base_offsets64_half",
"__gather_base_offsets32_i32",
"__gather_base_offsets64_i32",
"__gather_base_offsets32_float",
"__gather_base_offsets64_float",
"__gather_base_offsets32_i64",
"__gather_base_offsets64_i64",
"__gather_base_offsets32_double",
"__gather_base_offsets64_double",
"__scatter_base_offsets32_i8",
"__scatter_base_offsets64_i8",
"__scatter_base_offsets32_i16",
"__scatter_base_offsets64_i16",
"__scatter_base_offsets32_half",
"__scatter_base_offsets64_half",
"__scatter_base_offsets32_i32",
"__scatter_base_offsets64_i32",
"__scatter_base_offsets32_float",
"__scatter_base_offsets64_float",
"__scatter_base_offsets32_i64",
"__scatter_base_offsets64_i64",
"__scatter_base_offsets32_double",
"__scatter_base_offsets64_double",
"__prefetch_read_varying_nt_native",
"__prefetch_write_varying_1_native",
"__prefetch_write_varying_2_native",
"__prefetch_read_varying_2_native",
"__prefetch_read_varying_1_native",
"__prefetch_write_varying_3_native",
"__prefetch_read_varying_3_native",
"ISPCAlloc",
"ISPCLaunch",
"ISPCSync",
"ISPCInstrument",
"__is_compile_time_constant_mask",
"__is_compile_time_constant_uniform_int32",
"__is_compile_time_constant_varying_int32",
};

// Mapping from each target to its parent target
std::unordered_map<ISPCTarget, ISPCTarget> targetParentMap = {
    {ISPCTarget::avx512spr_x4, ISPCTarget::avx512icl_x4},
    {ISPCTarget::avx512icl_x4, ISPCTarget::avx512skx_x4},
    //{ISPCTarget::avx512skx_x4, ISPCTarget::common_x4},

    {ISPCTarget::avx2vnni_i32x4, ISPCTarget::avx2_i32x4},
    {ISPCTarget::avx2_i32x4, ISPCTarget::avx1_i32x4},
    {ISPCTarget::avx1_i32x4, ISPCTarget::sse4_i32x4},
    {ISPCTarget::sse4_i32x4, ISPCTarget::sse2_i32x4},

    {ISPCTarget::avx512spr_x8, ISPCTarget::avx512icl_x8},
    {ISPCTarget::avx512icl_x8, ISPCTarget::avx512skx_x8},
    //{ISPCTarget::avx512skx_x8, ISPCTarget::common_x8},

    {ISPCTarget::avx2vnni_i32x8, ISPCTarget::avx2_i32x8},
    {ISPCTarget::avx2_i32x8, ISPCTarget::avx1_i32x8},
    {ISPCTarget::avx1_i32x8, ISPCTarget::sse4_i32x8},
    {ISPCTarget::sse4_i32x8, ISPCTarget::sse2_i32x8},

    {ISPCTarget::avx512spr_x16, ISPCTarget::avx512icl_x16},
    {ISPCTarget::avx512icl_x16, ISPCTarget::avx512skx_x16},
    {ISPCTarget::avx512skx_x8, ISPCTarget::avx2vnni_i32x16},
    {ISPCTarget::avx2vnni_i32x16, ISPCTarget::avx2_i32x16},
    {ISPCTarget::avx2_i32x16, ISPCTarget::avx1_i32x16},
    {ISPCTarget::avx1_i32x16, ISPCTarget::sse4_i8x16},

    {ISPCTarget::avx512spr_x32, ISPCTarget::avx512icl_x32},
    {ISPCTarget::avx512icl_x32, ISPCTarget::avx512skx_x32},
    {ISPCTarget::avx512skx_x32, ISPCTarget::avx2_i8x32},

    {ISPCTarget::avx512spr_x64, ISPCTarget::avx512icl_x64},
    {ISPCTarget::avx512icl_x64, ISPCTarget::avx512skx_x64},
};

ISPCTarget GetParentTarget(ISPCTarget target) {
    auto it = targetParentMap.find(target);
    if (it != targetParentMap.end()) {
        return it->second;
    }
    return ISPCTarget::none;
}

bool lHasUnresolvedSymbols(llvm::Module *module) {
    for (llvm::Function &F : module->functions()) {
        auto name = F.getName().str();
        if (F.isDeclaration() && !F.use_empty() && targetBuiltinFuncs.count(name) && !pseudoFuncs.count(name)) {
            printf("Unresolved symbol: %s\n", name.c_str());
            for (auto *User : F.users()) {
                if (llvm::Instruction *Inst = llvm::dyn_cast<llvm::Instruction>(User)) {
                    llvm::errs() << "  Used in instruction: ";
                    Inst->print(llvm::errs());
                    // Inst->getFunction()->print(llvm::errs());
                    llvm::errs() << "\n";
                } else if (llvm::ConstantExpr *ConstExpr = llvm::dyn_cast<llvm::ConstantExpr>(User)) {
                    llvm::errs() << "  Used in constant expression: ";
                    ConstExpr->print(llvm::errs());
                    // Inst->getFunction()->print(llvm::errs());
                    llvm::errs() << "\n";
                } else {
                    llvm::errs() << "  Used in: ";
                    User->print(llvm::errs());
                    // Inst->getFunction()->print(llvm::errs());
                    llvm::errs() << "\n";
                }
            }
            return true;
        }
    }
    return false;
}

void lFillTargetBuiltins(llvm::Module *bcModule, llvm::StringSet<> &targetBuiltins) {
    for (llvm::Function &F : bcModule->functions()) {
        auto name = F.getName();
        if (!lStartsWithLLVM(name)) {
            targetBuiltins.insert(name);
        }
    }
}

void lLinkTargetBuiltins(llvm::Module *module, int &debug_num) {
    auto tgtReg = g->target_registry;
    ISPCTarget target = g->target->getISPCTarget();
    const BitcodeLib *targetBCLib = tgtReg->getISPCTargetLib(target, g->target_os, g->target->getArch());
    Assert(targetBCLib);
    llvm::Module *targetBCModule = targetBCLib->getLLVMModule();

    llvm::StringSet<> targetBuiltins;
    lFillTargetBuiltins(targetBCModule, targetBuiltins);

    targetBCModule->setDataLayout(g->target->getDataLayout()->getStringRepresentation());
    targetBCModule->setTargetTriple(module->getTargetTriple());

    // Next, add the target's custom implementations of the various needed
    // builtin functions (e.g. __masked_store_32(), etc).
    lAddBitcodeToModule(targetBCModule, module);

    // Check for unresolved symbols after adding targetBCModule and
    // hierarchically link with the parent (more common) target's bitcode if
    // needed.
    while (lHasUnresolvedSymbols(module)) {
        target = GetParentTarget(target);
        printf("Parent target: %s\n", ISPCTargetToString(target).c_str());
        if (target == ISPCTarget::none) {
            // Error(SourcePos(), "Unresolved symbols in target bitcode.");
            break;
        }
        const BitcodeLib *parentLib = tgtReg->getISPCTargetLib(target, g->target_os, g->target->getArch());
        printf("Bitcode lib: %s\n", parentLib->getFilename().c_str());
        Assert(parentLib);
        llvm::Module *parentBCModule = parentLib->getLLVMModule();
        lFillTargetBuiltins(parentBCModule, targetBuiltins);
        parentBCModule->setDataLayout(g->target->getDataLayout()->getStringRepresentation());
        lAddBitcodeToModule(parentBCModule, module);
        // dump module
        debugDumpModule(module, "Parent", debug_num++);
    }

    llvm::GlobalVariable *llvmUsed = module->getNamedGlobal("llvm.compiler.used");
    if (llvmUsed) {
        llvmUsed->eraseFromParent();
    }
    // new/delete use __pseudo_masked_store functions which we need to add in persistent and link again
    // TODO: redo to avoid duplication
    lAddPersistentToLLVMUsed(*module);
    // lRemoveUnused(module);
    // lRemoveUnusedPersistentFunctions(module);
    while (lHasUnresolvedSymbols(module)) {
        target = GetParentTarget(target);
        printf("Parent target: %s\n", ISPCTargetToString(target).c_str());
        if (target == ISPCTarget::none) {
            // Error(SourcePos(), "Unresolved symbols in target bitcode.");
            break;
        }
        const BitcodeLib *parentLib = tgtReg->getISPCTargetLib(target, g->target_os, g->target->getArch());
        printf("Bitcode lib: %s\n", parentLib->getFilename().c_str());
        Assert(parentLib);
        llvm::Module *parentBCModule = parentLib->getLLVMModule();
        lFillTargetBuiltins(parentBCModule, targetBuiltins);
        parentBCModule->setDataLayout(g->target->getDataLayout()->getStringRepresentation());
        lAddBitcodeToModule(parentBCModule, module);
        // dump module
        debugDumpModule(module, "Parent", debug_num++);
    }

    lSetAsInternal(module, targetBuiltins);
}

void lLinkStdlib(llvm::Module *module) {
    const BitcodeLib *stdlib =
        g->target_registry->getISPCStdLib(g->target->getISPCTarget(), g->target_os, g->target->getArch());
    Assert(stdlib);
    llvm::Module *stdlibBCModule = stdlib->getLLVMModule();

    if (g->isMultiTargetCompilation) {
        for (llvm::Function &F : stdlibBCModule->functions()) {
            if (!F.isDeclaration() && !lStartsWithLLVM(F.getName())) {
                F.setName(F.getName() + g->target->GetTargetSuffix());
            }
        }
    }

    llvm::StringSet<> stdlibFunctions;
    for (llvm::Function &F : stdlibBCModule->functions()) {
        stdlibFunctions.insert(F.getName());
    }

    stdlibBCModule->setDataLayout(g->target->getDataLayout()->getStringRepresentation());
    stdlibBCModule->setTargetTriple(module->getTargetTriple());

    lAddBitcodeToModule(stdlibBCModule, module);
    lSetAsInternal(module, stdlibFunctions);
}

void ispc::LinkStandardLibraries(llvm::Module *module, int &debug_num) {
    if (g->includeStdlib) {
        lLinkStdlib(module);
        // Remove from module here only function definitions that unused (or
        // cannot be used) in module.
        lAddPersistentToLLVMUsed(*module);
        lRemoveUnused(module);
        // debugDumpModule(module, "PreRemove", debug_num++);
        // lRemoveUnusedPersistentFunctions(module);
        debugDumpModule(module, "LinkStdlib", debug_num++);
    } else {
        lAddPersistentToLLVMUsed(*module);
    }

    lLinkCommonBuiltins(module);
    lRemoveUnused(module);
    debugDumpModule(module, "LinkCommonBuiltins", debug_num++);

    lLinkTargetBuiltins(module, debug_num);
    // lRemoveUnused(module);
    debugDumpModule(module, "LinkTargetBuiltins", debug_num++);

    lSetInternalLinkageGlobals(module);
    lCheckModuleIntrinsics(module);
}
