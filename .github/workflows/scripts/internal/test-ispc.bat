set TESTS_TARGET=%1
set ISPC_OUTPUT=%2
set FAIL_DB_PATH=%3
set ARTIFACTORY_BASE_URL=%4
set CPU_TARGET=%5

IF not "%CPU_TARGET%"=="" (
  set EXTRA_ARGS=--device=%CPU_TARGET%
)

call %SCRIPTS_DIR%\install-gfx-driver.bat %ARTIFACTORY_BASE_URL% || goto :error

set ISPC_DIR=%GITHUB_WORKSPACE%\artifacts\install
set PATH=%ISPC_DIR%\bin;%PATH%

cd %GITHUB_WORKSPACE%
python run_tests.py -u FP -a xe64 -t %TESTS_TARGET% --l0loader=%ISPC_DIR% --ispc_output=%ISPC_OUTPUT% --fail_db=%FAIL_DB_PATH% --test_time 60 -j 8 %EXTRA_ARGS% || goto :error

goto :EOF

:error
echo Failed - error #%errorlevel%
exit /b %errorlevel%
