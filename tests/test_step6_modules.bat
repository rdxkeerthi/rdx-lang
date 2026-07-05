@echo off
setlocal enabledelayedexpansion

echo [test] Building rdx_bin.exe...
call build.bat > nul 2>&1
if %errorlevel% neq 0 (
    echo [FAIL] Build failed!
    exit /b 1
)

echo [test] Running module_demo/main.rdx ...
.\rdx_bin.exe examples\module_demo\main.rdx > tests\out_modules.txt 2>&1

>nul findstr /C:"square(5): 25" tests\out_modules.txt
if %errorlevel% neq 0 (
    echo [FAIL] square function from module failed
    type tests\out_modules.txt
    exit /b 1
) else (
    echo [PASS] square function
)

>nul findstr /C:"cube(3): 27" tests\out_modules.txt
if %errorlevel% neq 0 (
    echo [FAIL] cube function from module failed
    type tests\out_modules.txt
    exit /b 1
) else (
    echo [PASS] cube function
)

echo [RESULT] ALL PASSED  2 tests
del tests\out_modules.txt
exit /b 0
