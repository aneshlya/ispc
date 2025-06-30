/*
  Copyright (c) 2025, Intel Corporation

  SPDX-License-Identifier: BSD-3-Clause
*/

/** @file options.h
    @brief Command line argument parsing functionality for ispc
*/

#pragma once

#include "module.h"
#include "target_enums.h"
#include "target_registry.h"

#include <string>
#include <vector>

namespace ispc {
enum class ArgsParseResult : int { success = 0, failure = 1, help_requested = 2 };

/** Parse all command line arguments from various sources (command line, environment, files)
 *  and merge them into a single argv vector.
 *
 *  @param Argc Count of command line arguments
 *  @param Argv Array of command line argument strings
 *  @param argv Output vector to store all merged arguments
 */
void GetAllArgs(int Argc, char *Argv[], std::vector<char *> &argv);

/** Free memory allocated for arguments in the argv vector
 *
 *  @param argv Vector of argument strings to free
 */
void FreeArgv(std::vector<char *> &argv);

/** Parse command line arguments and set global options
 *
 *  @param argc Number of arguments
 *  @param argv Array of argument strings
 *  @param file Output parameter for input source file name
 *  @param arch Output parameter for target architecture
 *  @param cpu Output parameter for target CPU type
 *  @param targets Output parameter for vector of ISPC targets
 *  @param output Output parameter for compilation output settings
 *  @param linkFileNames Output parameter for files to link
 *  @param isLinkMode Output parameter indicating if in link mode
 *  @return Result of argument parsing
 */
ArgsParseResult ParseCommandLineArgs(int argc, char *argv[], char *&file, Arch &arch, const char *&cpu,
                                     std::vector<ISPCTarget> &targets, Module::Output &output,
                                     std::vector<std::string> &linkFileNames, bool &isLinkMode);

} // namespace ispc
