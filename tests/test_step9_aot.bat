@echo off
set MINGW=C:\msys64\mingw64\bin
set PATH=%MINGW%;%PATH%

echo [test_step9_aot] Compiling compiler...
call build.bat compiler
if errorlevel 1 exit /b 1

echo [test_step9_aot] Transpiling examples\aot_demo.rdx...
rdx_bin.exe transpile examples\aot_demo.rdx
if errorlevel 1 (
    echo [test_step9_aot] Transpilation failed!
    exit /b 1
)

echo [test_step9_aot] Compiling out.c with GCC...
gcc -O2 -o aot_out.exe out.c
if errorlevel 1 (
    echo [test_step9_aot] GCC compilation failed!
    exit /b 1
)

echo [test_step9_aot] Running aot_out.exe...
aot_out.exe
if errorlevel 1 (
    echo [test_step9_aot] Execution failed!
    exit /b 1
)

echo [test_step9_aot] SUCCESS!
