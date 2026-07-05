@echo off
setlocal
set PASS=0
set FAIL=0
set PATH=C:\msys64\mingw64\bin;%PATH%

echo [test] Building rdx_bin.exe...
gcc -std=c11 -nostdlib -fno-builtin -mno-stack-arg-probe -O2 -o rdx_bin.exe src\main.c src\lexer.c src\parser.c src\analyzer.c src\codegen.c src\aot.c src\diag.c -lkernel32
if errorlevel 1 ( echo [FAIL] Build failed & exit /b 1 )

echo [test] Running result_handling.rdx ...
rdx_bin.exe "examples\result_handling.rdx" > __rdx_res.txt 2>&1

findstr "Is OK: 1" __rdx_res.txt >nul && ( echo [PASS] is_ok & set /a PASS+=1 ) || ( echo [FAIL] is_ok & set /a FAIL+=1 )
findstr "Unwrapped: 42" __rdx_res.txt >nul && ( echo [PASS] unwrap & set /a PASS+=1 ) || ( echo [FAIL] unwrap & set /a FAIL+=1 )
findstr "Is Err: 1" __rdx_res.txt >nul && ( echo [PASS] is_err & set /a PASS+=1 ) || ( echo [FAIL] is_err & set /a FAIL+=1 )
findstr "Unwrap or default: -1" __rdx_res.txt >nul && ( echo [PASS] unwrap_or & set /a PASS+=1 ) || ( echo [FAIL] unwrap_or & set /a FAIL+=1 )
findstr "Caught error successfully." __rdx_res.txt >nul && ( echo [PASS] safe block & set /a PASS+=1 ) || ( echo [FAIL] safe block & set /a FAIL+=1 )
findstr "Final: 999" __rdx_res.txt >nul && ( echo [PASS] safe final & set /a PASS+=1 ) || ( echo [FAIL] safe final & set /a FAIL+=1 )

del __rdx_res.txt >nul 2>&1

if %FAIL% gtr 0 (
    echo [RESULT] FAILED  %PASS% passed  %FAIL% failed
    exit /b 1
)
echo [RESULT] ALL PASSED  %PASS% tests
exit /b 0
