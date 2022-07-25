REM Copyright (c) 2023, Intel Corporation
REM SPDX-License-Identifier: BSD-3-Clause

set BUILD_EMBARGO=%1
set ARTIFACTORY_BASE_URL=%2
set DEPS_PIPELINE_ID=%3
set LLVM_HOME=C:\llvm

set disable_assertions=%LLVM_NO_ASSERTIONS%
set signing_required=%SIGNING_REQUIRED%

set LLVM_ASSERT_SUFFIX

if defined disable_assertions (
    set "LLVM_ASSERT_SUFFIX=.noasserts"
)

if defined signing_required (
    set "SIGNING_ARG=-DISPC_SIGN_KEY=%SIGNING_HASH%"
)

REM preset type doesn't really matter for ISPC only build
if not defined PRESET_TYPE (
    set "PRESET_TYPE=os"
)

set LLVM_VER_WITH_SUFFIX=%LTO_SUFFIX%%LLVM_VER%%LLVM_ASSERT_SUFFIX%

if defined DEPS_PIPELINE_ID (
    call %SCRIPTS_DIR%\install-llvm.bat %DEPS_PIPELINE_ID% || goto :error
) else (
    call %SCRIPTS_DIR%\install-llvm.bat || goto :error
)

set ARTIFACTORY_GFX_BASE_URL=%ARTIFACTORY_ISPC_URL%/ispc-deps

cd %GITHUB_WORKSPACE%
call %SCRIPTS_DIR%\download-file.bat %ARTIFACTORY_BASE_URL% xe-deps.zip %ARTIFACTORY_ISPC_API_KEY% || goto :error
unzip xe-deps.zip || goto :error

rm -rf build
REM Build ISPC package ready for release
cmake superbuild -B build --preset %PRESET_TYPE% -G "NMake Makefiles" -DLTO=%LTO% -DPREBUILT_XE_STAGE2_PATH=%GITHUB_WORKSPACE%\xe-deps -DGNUWIN32=C:\gnuwin32 -D__INTEL_EMBARGO__=%BUILD_EMBARGO% -DPREBUILT_STAGE2_PATH=%LLVM_HOME%\%LLVM_VER_WITH_SUFFIX%\bin-%LLVM_VER% %SIGNING_ARG% -DINSTALL_ISPC=ON -DCMAKE_INSTALL_PREFIX=%GITHUB_WORKSPACE%\build\install || goto :error

mv %GITHUB_WORKSPACE%\build\build-ispc-stage2\src\ispc-stage2-build\_CPack_Packages\win64\ZIP\*.zip %GITHUB_WORKSPACE%

REM Build ISPC
cmake --build build || goto :error

REM Run lit tests
cmake --build build --target ispc-stage2-check-all || goto :error

cp -R xe-deps/* %GITHUB_WORKSPACE%/build/install/

goto :EOF

:error
echo Failed - error #%errorlevel%
exit /b %errorlevel%
