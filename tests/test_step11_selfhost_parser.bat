@echo off
echo [test_step11_selfhost] Testing Self-Hosting Parser Prototype...

echo [test_step11_selfhost] Compiling tools...
call build.bat compiler
call build.bat test_parser
gcc -O2 tests\trim_output.c -o tests\trim_output.exe
if errorlevel 1 exit /b 1

echo [test_step11_selfhost] Copying test_parser.rdx...
copy /Y tests\test_parser_input.rdx test_parser.rdx > nul

echo [test_step11_selfhost] Running C Parser...
test_parser.exe test_parser.rdx > c_parse_raw.txt

echo [test_step11_selfhost] Running RDX Parser...
rdx_bin.exe examples\parser.rdx > rdx_parse_raw.txt

echo [test_step11_selfhost] Formatting outputs...
tests\trim_output.exe c_parse_raw.txt c_parse.txt "=== AST ==="
tests\trim_output.exe rdx_parse_raw.txt rdx_parse.txt "=== AST ===" "[rdx] Execution completed successfully."

echo [test_step11_selfhost] Comparing outputs...
fc c_parse.txt rdx_parse.txt
if errorlevel 1 (
    echo [test_step11_selfhost] FAILED: Parser outputs do not match.
    exit /b 1
)

echo [test_step11_selfhost] SUCCESS! Parsers match byte-for-byte!
exit /b 0
