set ARTIFACTORY_BASE_URL=%1
call %SCRIPTS_DIR%\install-gfx-driver.bat %ARTIFACTORY_BASE_URL% || goto :error

set ISPC_DIR=%GITHUB_WORKSPACE%\artifacts\install
set PATH=%ISPC_DIR%\bin;%PATH%

cd %GITHUB_WORKSPACE%\examples\xpu && mkdir build && cd build
cmake .. -DLEVEL_ZERO_ROOT=%ISPC_DIR%  || goto :error

REM Build ISPC examples
MSBuild XeExamples.sln /p:Configuration=Release /p:Platform=x64 /m || goto :error

REM Run examples
ctest -C Release -V --timeout 30 || goto :error

goto :EOF

:error
echo Failed - error #%errorlevel%
exit /b %errorlevel%
