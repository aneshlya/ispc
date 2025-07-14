/*
  Copyright (c) 2025, Intel Corporation
  SPDX-License-Identifier: BSD-3-Clause
*/

#pragma once

#ifdef __cplusplus
#include <memory>
#include <string>
#include <vector>
extern "C" {
#endif

/* C API - compatible with both C and C++ */

/**
 * @brief Initializes the ISPC library.
 * This must be called once before creating any ISPCEngine instances.
 * Initializes LLVM targets and creates global state.
 *
 * @return 1 on success, 0 on failure.
 */
int ispc_initialize(void);

/**
 * @brief Shuts down the ISPC library and releases global resources.
 * This should be called once when the consumer is finished using the library.
 * After calling this, ispc_initialize() must be called again before creating new instances.
 */
void ispc_shutdown(void);

/**
 * @brief Compiles ISPC code from command-line arguments.
 * This function parses command-line arguments and executes the appropriate action
 * (compile, link, or help). ispc_initialize() must be called successfully before using this function.
 *
 * @param argc Argument count.
 * @param argv Argument vector. The first argument should be the program name
 *             or any dummy string (it will be ignored eventually).
 * @return 0 on success, non-zero on failure.
 */
int ispc_compile_from_args(int argc, char *argv[]);

/* Opaque handle for ISPCEngine in C API */
typedef struct ispc_engine ispc_engine_t;

/**
 * @brief Creates a ISPCEngine instance from command-line arguments.
 * ispc_initialize() must be called successfully before using this function.
 *
 * @param argc Argument count.
 * @param argv Argument vector. The first argument should be the program name
 *             or any dummy string (it will be ignored eventually).
 * @return Pointer to engine instance, or NULL on failure.
 */
ispc_engine_t *ispc_engine_create_from_args(int argc, char *argv[]);

/**
 * @brief Destroys a ISPCEngine instance.
 * @param engine Pointer to engine instance.
 */
void ispc_engine_destroy(ispc_engine_t *engine);

/**
 * @brief Executes the appropriate action based on the driver state (link, help, or compile).
 * @param engine Pointer to engine instance.
 * @return 0 on success, non-zero on failure.
 */
int ispc_engine_execute(ispc_engine_t *engine);

#ifdef __cplusplus
} /* extern "C" */

/* C++ API - only available when compiled as C++ */
namespace ispc {

/**
 * @brief Initializes the ISPC library.
 * This must be called once before creating any ISPCEngine instances.
 * Initializes LLVM targets and creates global state.
 *
 * @return true on success, false on failure.
 */
bool Initialize();

/**
 * @brief Shuts down the ISPC library and releases global resources.
 * This should be called once when the consumer is finished using the library.
 * After calling this, Initialize() must be called again before creating new instances.
 */
void Shutdown();

/**
 * @brief Compiles ISPC code from command-line arguments.
 * This function parses command-line arguments and executes the appropriate action
 * (compile, link, or help). Initialize() must be called successfully before using this function.
 *
 * @param args Vector of command-line arguments. The first argument should be the program name
 *             or any dummy string (it will be ignored eventually).
 * @return 0 on success, non-zero on failure.
 */
int CompileFromArgs(const std::vector<std::string> &args);

/**
 * @brief Compiles ISPC code from command-line arguments.
 * This function parses command-line arguments and executes the appropriate action
 * (compile, link, or help). Initialize() must be called successfully before using this function.
 *
 * @param argc Argument count.
 * @param argv Argument vector. The first argument should be the program name
 *             or any dummy string (it will be ignored eventually).
 * @return 0 on success, non-zero on failure.
 */
int CompileFromCArgs(int argc, char *argv[]);

class ISPCEngine {
  public:
    /**
     * @brief Factory method to create a ISPCEngine instance from command-line arguments.
     * Initialize() must be called successfully before using this method.
     *
     * @param args Vector of command-line arguments. The first argument should be the program name
     *             or any dummy string (it will be ignored eventually).
     * @return A unique_ptr to a ISPCEngine instance, or nullptr on failure.
     */
    static std::unique_ptr<ISPCEngine> CreateFromArgs(const std::vector<std::string> &args);

    /**
     * @brief Factory method for C-style argc/argv arguments.
     * @param argc Argument count.
     * @param argv Argument vector. The first argument should be the program name
     *             or any dummy string (it will be ignored eventually).
     * @return A unique_ptr to a ISPCEngine instance, or nullptr on failure.
     */
    static std::unique_ptr<ISPCEngine> CreateFromCArgs(int argc, char *argv[]);

    /**
     * @brief Executes the appropriate action based on the driver state (link, help, or compile).
     * @return 0 on success, non-zero on failure.
     */
    int Execute();

    ~ISPCEngine();

  private:
    ISPCEngine();

    class Impl;
    std::unique_ptr<Impl> pImpl;
};

} // namespace ispc

#endif /* __cplusplus */