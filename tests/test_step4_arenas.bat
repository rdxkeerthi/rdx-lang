@echo off
setlocal
echo ========================================================
echo RUNNING STEP 4 TESTS: ARENA MEMORY MODEL
echo ========================================================

echo.
echo Building RDX...
call build.bat
if %errorlevel% neq 0 (
    echo [FAIL] Build failed.
    exit /b %errorlevel%
)

echo.
echo Running examples\arena_demo.rdx...
.\rdx_bin.exe examples\arena_demo.rdx > tests\out_step4.txt
if %errorlevel% neq 0 (
    echo [FAIL] Execution failed.
    exit /b %errorlevel%
)

echo.
echo Expected Output vs Actual Output:
findstr "Capacity: 1024" tests\out_step4.txt >nul
if %errorlevel% neq 0 ( echo [FAIL] Capacity check failed & exit /b 1 )

findstr "Used: 128" tests\out_step4.txt >nul
if %errorlevel% neq 0 ( echo [FAIL] Alloc 1 check failed & exit /b 1 )

findstr "Used: 384" tests\out_step4.txt >nul
if %errorlevel% neq 0 ( echo [FAIL] Alloc 2 check failed & exit /b 1 )

findstr "Used after reset: 0" tests\out_step4.txt >nul
if %errorlevel% neq 0 ( echo [FAIL] Reset check failed & exit /b 1 )

echo.
echo [PASS] All Step 4 Arena tests passed!
exit /b 0
