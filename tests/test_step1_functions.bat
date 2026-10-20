@echo off
:: ============================================================
::  tests/test_step1_functions.bat  —  Step 1: User Functions
:: ============================================================
setlocal

set PASS=0
set FAIL=0
set PATH=C:\msys64\mingw64\bin;%PATH%

echo [test] Building rdx_bin.exe...
gcc -std=c11 -nostdlib -fno-builtin -mno-stack-arg-probe -O2 -o rdx_bin.exe src\main.c src\lexer.c src\parser.c src\analyzer.c src\codegen.c src\aot.c src\diag.c -lkernel32 2>nul
if errorlevel 1 ( echo [FAIL] Build failed & exit /b 1 )
echo [test] Build OK

echo.
echo [test] Running fibonacci_recursive.rdx ...
rdx_bin.exe "examples\fibonacci_recursive.rdx" > __rdx_fib.txt 2>&1
type __rdx_fib.txt
echo.

:: Pipe checks — no = in echo labels
findstr "fib(10)" __rdx_fib.txt | findstr "55"  >nul && ( echo [PASS] fib10 is 55 & set /a PASS+=1 ) || ( echo [FAIL] fib10 & set /a FAIL+=1 )
findstr "fib(15)" __rdx_fib.txt | findstr "610" >nul && ( echo [PASS] fib15 is 610 & set /a PASS+=1 ) || ( echo [FAIL] fib15 & set /a FAIL+=1 )
findstr "fact(10)" __rdx_fib.txt | findstr "3628800" >nul && ( echo [PASS] fact10 & set /a PASS+=1 ) || ( echo [FAIL] fact10 & set /a FAIL+=1 )
findstr "fact(5)" __rdx_fib.txt | findstr " 120" >nul && ( echo [PASS] fact5 & set /a PASS+=1 ) || ( echo [FAIL] fact5 & set /a FAIL+=1 )
findstr "add(3, 4)" __rdx_fib.txt | findstr " 7" >nul && ( echo [PASS] add & set /a PASS+=1 ) || ( echo [FAIL] add & set /a FAIL+=1 )
findstr "max2(42, 17)" __rdx_fib.txt | findstr "42" >nul && ( echo [PASS] max2 & set /a PASS+=1 ) || ( echo [FAIL] max2 & set /a FAIL+=1 )
findstr "scoping OK" __rdx_fib.txt >nul && ( echo [PASS] scoping & set /a PASS+=1 ) || ( echo [FAIL] scoping & set /a FAIL+=1 )

del __rdx_fib.txt >nul 2>&1

echo.
echo [test] Regression check ...
rdx_bin.exe "examples\10_functions.rdx" > __rdx_reg.txt 2>&1
findstr "error" __rdx_reg.txt >nul 2>&1
if not errorlevel 1 (
    echo [FAIL] regression
    type __rdx_reg.txt
    set /a FAIL+=1
) else (
    echo [PASS] 10_functions.rdx regression
    set /a PASS+=1
)
del __rdx_reg.txt >nul 2>&1

echo.
if %FAIL% gtr 0 (
    echo [RESULT] FAILED  %PASS% passed  %FAIL% failed
    exit /b 1
)
echo [RESULT] ALL PASSED  %PASS% tests
exit /b 0
