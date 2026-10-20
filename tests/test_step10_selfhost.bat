@echo off
echo [test_step10_selfhost] Testing Self-Hosting Lexer Prototype...

echo [test_step10_selfhost] Compiling compiler and tools...
call build.bat
gcc -O2 tests\trim_output.c -o tests\trim_output.exe
if errorlevel 1 exit /b 1

echo [test_step10_selfhost] Generating test.rdx...
echo make main(num x) { ret x + 10.5 == "hello" } > test.rdx

echo [test_step10_selfhost] Running C Lexer...
test_lexer.exe test.rdx > c_lex_raw.txt

echo [test_step10_selfhost] Running RDX Lexer...
rdx_bin.exe examples\lexer.rdx > rdx_lex_raw.txt

echo [test_step10_selfhost] Formatting outputs...
tests\trim_output.exe c_lex_raw.txt c_lex.txt "=== Tokens ==="
tests\trim_output.exe rdx_lex_raw.txt rdx_lex.txt "=== Tokens ===" "[rdx] Execution completed successfully."

echo [test_step10_selfhost] Comparing outputs...
fc c_lex.txt rdx_lex.txt
if errorlevel 1 (
    echo [test_step10_selfhost] FAILED: Lexer outputs do not match.
    exit /b 1
)

echo [test_step10_selfhost] SUCCESS! Lexers match byte-for-byte!
exit /b 0
