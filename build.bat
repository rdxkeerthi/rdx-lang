@echo off
:: ===========================================================================
::  build.bat — RDX Language Build Script
::  Requires: MinGW-w64 GCC (C:\msys64\mingw64\bin) + NASM
:: ===========================================================================
set MINGW=C:\msys64\mingw64\bin
set PATH=%MINGW%;%PATH%

:: Compiler flags
set CFLAGS=-std=c11 -nostdlib -fno-builtin -mno-stack-arg-probe -O2
set LDFLAGS=-lkernel32
set SRCS=src\main.c src\lexer.c src\parser.c src\codegen.c

echo [build.bat] Toolchain check...
gcc --version | findstr "gcc"
nasm --version

:: ---------------------------------------------------------------------------
::  BUILD: rdxc.exe (the RDX compiler/interpreter)
:: ---------------------------------------------------------------------------
if "%1"=="" goto build_compiler
if "%1"=="compiler" goto build_compiler
if "%1"=="test_core" goto build_test_core
if "%1"=="test_lexer" goto build_test_lexer
if "%1"=="hello" goto build_hello
if "%1"=="all" goto build_all
goto build_compiler

:build_compiler
echo.
echo [rdx] Building compiler...
gcc %CFLAGS% -o rdx_bin.exe %SRCS% %LDFLAGS%
if errorlevel 1 (
    echo [rdx] BUILD FAILED
    exit /b 1
)
echo [rdx] Compiler built: rdx_bin.exe
goto :eof

:: ---------------------------------------------------------------------------
::  TEST: core smoke test
:: ---------------------------------------------------------------------------
:build_test_core
echo.
echo [test_core] Building...
gcc %CFLAGS% -DRDX_DEFINE_ENTRY -o test_core.exe src\test_core.c %LDFLAGS%
if errorlevel 1 (
    echo [test_core] BUILD FAILED
    exit /b 1
)
echo [test_core] Running...
test_core.exe
goto :eof

:: ---------------------------------------------------------------------------
::  TEST: lexer smoke test
:: ---------------------------------------------------------------------------
:build_test_lexer
echo.
echo [test_lexer] Building...
gcc %CFLAGS% -DRDX_DEFINE_ENTRY -o test_lexer.exe src\test_lexer.c src\lexer.c %LDFLAGS%
if errorlevel 1 (
    echo [test_lexer] BUILD FAILED
    exit /b 1
)
echo [test_lexer] Running...
test_lexer.exe
goto :eof

:: ---------------------------------------------------------------------------
::  DEMO: compile + assemble + link hello.rdx end-to-end
:: ---------------------------------------------------------------------------
:build_hello
call :build_compiler
if %errorlevel% neq 0 exit /b %errorlevel%
echo [rdxc] Compiling and running examples\hello.rdx...
call rdx.bat run examples\hello.rdx
exit /b %errorlevel%
if errorlevel 1 (
    echo [hello] Compile FAILED
    exit /b 1
)
if not exist hello.asm (
    echo [hello] No output file generated
    exit /b 1
)
echo [hello] Assembling hello.asm...
nasm -f win64 hello.asm -o hello.obj
if errorlevel 1 (
    echo [hello] NASM FAILED
    exit /b 1
)
echo [hello] Linking hello.exe...
ld hello.obj -lkernel32 -o hello.exe
if errorlevel 1 (
    echo [hello] Linker FAILED — trying gcc fallback...
    gcc -nostdlib -o hello.exe hello.obj -lkernel32
)
echo [hello] Running hello.exe...
hello.exe
goto :eof

:: ---------------------------------------------------------------------------
::  ALL: build + test everything
:: ---------------------------------------------------------------------------
:build_all
call :build_compiler
call build.bat test_core
call build.bat test_lexer
call build.bat hello
goto :eof
