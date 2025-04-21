@echo on
setlocal enabledelayedexpansion

cd Scripts

:: Run GenerateProjectFiles script
call GenerateProjectFiles.bat

:: Define targets and file type
set "targets=release avx2 sse2 sse4"

:: Build targets
pushd ..\Build\ISPCTest\vs2019\msvc
for %%t in (%targets%) do (
    set "target=%%t"
    msbuild ISPCTest.sln /t:clean /p:Configuration=!target!
    msbuild ISPCTest.sln /p:Configuration=!target! /m
    if errorlevel 1 (
        echo "Build failed for target !target!"
        exit /b 1
    )
)
popd

:: Run tests for one configuration only to save time
pushd ..\x86_64
ISPCTest-release.exe --benchmark_min_time=0.1s --benchmark_repetitions=1 --benchmark_out=validate.json --benchmark_out_format=json --benchmark_report_aggregates_only=true
popd
