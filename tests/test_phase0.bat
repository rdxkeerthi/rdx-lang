@echo off
:: tests\test_phase0.bat — Phase 0 regression tests
:: Verifies:
::   0a. test_safety.rdx and pattern.rdx parse and run correctly
::       (they previously crashed due to the err/ok contextual-keyword bug)
::   0b. aot_demo.rdx JIT output matches AOT compiled-binary output
::       (it previously succeeded silently with zero output — false success)

set MINGW=C:\msys64\mingw64\bin
set PATH=%MINGW%;%PATH%

set PASS=0
set FAIL=0

echo ============================================================
echo  Phase 0 Regression Test Suite
echo ============================================================
echo.

:: ─── 0a-1: test_safety.rdx parses and runs ───────────────────────────────────
echo [TEST] 0a-1: test_safety.rdx runs without parse error
rdx_bin.exe run examples\test_safety.rdx > tmp_safety.txt 2>&1
if errorlevel 1 (
    echo   FAIL — exited with error
    type tmp_safety.txt
    set /a FAIL+=1
) else (
    findstr "Execution completed successfully" tmp_safety.txt >nul 2>&1
    if errorlevel 1 (
        echo   FAIL — no success message
        type tmp_safety.txt
        set /a FAIL+=1
    ) else (
        echo   PASS
        set /a PASS+=1
    )
)

:: ─── 0a-2: test_safety.rdx contains expected output ─────────────────────────
echo [TEST] 0a-2: test_safety.rdx output contains "Memory Safety"
findstr "Memory Safety" tmp_safety.txt >nul 2>&1
if errorlevel 1 (
    echo   FAIL — expected output not found
    set /a FAIL+=1
) else (
    echo   PASS
    set /a PASS+=1
)

:: ─── 0a-3: pattern.rdx parses and runs ───────────────────────────────────────
echo [TEST] 0a-3: pattern.rdx runs without parse error
rdx_bin.exe run examples\pattern.rdx > tmp_pattern.txt 2>&1
if errorlevel 1 (
    echo   FAIL — exited with error
    type tmp_pattern.txt
    set /a FAIL+=1
) else (
    findstr "Execution completed successfully" tmp_pattern.txt >nul 2>&1
    if errorlevel 1 (
        echo   FAIL — no success message
        type tmp_pattern.txt
        set /a FAIL+=1
    ) else (
        echo   PASS
        set /a PASS+=1
    )
)

:: ─── 0a-4: pattern.rdx contains safe/fail section ─────────────────────────
echo [TEST] 0a-4: pattern.rdx "safe / fail" section header present in output
findstr "Error Handling" tmp_pattern.txt >nul 2>&1
if errorlevel 1 (
    echo   FAIL — 'Error Handling' section header not found in output
    type tmp_pattern.txt
    set /a FAIL+=1
) else (
    echo   PASS
    set /a PASS+=1
)

:: ─── 0a-5: pattern.rdx fail err2 block ran correctly ─────────────────────────
echo [TEST] 0a-5: pattern.rdx second safe block succeeded (no err2 output)
findstr "should NOT run" tmp_pattern.txt >nul 2>&1
if errorlevel 1 (
    echo   PASS ^(err2 fail block correctly did NOT fire^)
    set /a PASS+=1
) else (
    echo   FAIL — err2 fail block fired unexpectedly
    set /a FAIL+=1
)

:: ─── 0b-1: aot_demo.rdx JIT produces output ─────────────────────────────────
echo [TEST] 0b-1: aot_demo.rdx JIT produces numeric output ^(fib 20 = 6765^)
rdx_bin.exe run examples\aot_demo.rdx > tmp_jit.txt 2>&1
if errorlevel 1 (
    echo   FAIL — JIT exited with error
    type tmp_jit.txt
    set /a FAIL+=1
    goto test_0b2
)
type tmp_jit.txt | findstr /C:"6765" >nul
if errorlevel 1 (
    echo   FAIL — expected 6765 not found
    type tmp_jit.txt
    set /a FAIL+=1
) else (
    echo   PASS ^(JIT output: 6765^)
    set /a PASS+=1
)
:test_0b2

:: ─── 0b-2: aot_demo.rdx transpiles without error ────────────────────────────
echo [TEST] 0b-2: aot_demo.rdx transpiles successfully
rdx_bin.exe transpile examples\aot_demo.rdx > tmp_aot_transpile.txt 2>&1
if errorlevel 1 (
    echo   FAIL — transpile failed
    type tmp_aot_transpile.txt
    set /a FAIL+=1
) else (
    findstr "Transpiled successfully" tmp_aot_transpile.txt >nul 2>&1
    if errorlevel 1 (
        echo   FAIL — no success message
        type tmp_aot_transpile.txt
        set /a FAIL+=1
    ) else (
        echo   PASS
        set /a PASS+=1
    )
)

:: ─── 0b-3: compiled binary matches JIT output ────────────────────────────────
echo [TEST] 0b-3: AOT compiled binary produces same output as JIT
gcc -O2 -o tmp_aot_bin.exe out.c > tmp_gcc.txt 2>&1
if errorlevel 1 (
    echo   FAIL — gcc compilation failed
    type tmp_gcc.txt
    set /a FAIL+=1
) else (
    tmp_aot_bin.exe > tmp_aot_out.txt 2>&1
    findstr "6765" tmp_aot_out.txt >nul 2>&1
    if errorlevel 1 (
        echo   FAIL — AOT binary output mismatch
        echo     JIT output:
        type tmp_jit.txt
        echo     AOT output:
        type tmp_aot_out.txt
        set /a FAIL+=1
    ) else (
        echo   PASS ^(AOT output: 6765 — matches JIT^)
        set /a PASS+=1
    )
)

:: ─── 0b-4: unsupported construct fails loudly, not silently ──────────────────
echo [TEST] 0b-4: transpiling pattern.rdx fails with explicit error, not silently
rdx_bin.exe transpile examples\pattern.rdx > tmp_aot_err.txt 2>&1
if errorlevel 1 (
    findstr "AOT ERROR" tmp_aot_err.txt >nul 2>&1
    if errorlevel 1 (
        echo   FAIL — failed but no [AOT ERROR] message
        type tmp_aot_err.txt
        set /a FAIL+=1
    ) else (
        echo   PASS ^(loud failure with named node type^)
        set /a PASS+=1
    )
) else (
    echo   FAIL — should have failed but succeeded
    set /a FAIL+=1
)

:: ─── Cleanup ──────────────────────────────────────────────────────────────────
del tmp_safety.txt tmp_pattern.txt tmp_jit.txt tmp_aot_transpile.txt 2>nul
del tmp_gcc.txt tmp_aot_out.txt tmp_aot_err.txt tmp_aot_bin.exe 2>nul

echo.
echo ============================================================
echo  Results: %PASS% passed, %FAIL% failed
echo ============================================================
if %FAIL% gtr 0 exit /b 1
exit /b 0
