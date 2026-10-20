@echo off
echo [test_diag_engine] Testing Diagnostics Engine caret formatting...

echo [test_diag_engine] Compiling compiler...
call build.bat
if errorlevel 1 exit /b 1

echo [test_diag_engine] Testing Lexer Error...
rdx_bin.exe tests\diag_lex.rdx > tests\diag_lex_out.txt 2>&1
findstr /C:"LEX ERROR line 1, col 1: unexpected character: '@'" tests\diag_lex_out.txt > nul
if errorlevel 1 (
    echo [test_diag_engine] FAILED: Lexer diagnostic mismatched!
    type tests\diag_lex_out.txt
    exit /b 1
)

echo [test_diag_engine] Testing Parser Error...
rdx_bin.exe tests\diag_parse.rdx > tests\diag_parse_out.txt 2>&1
findstr /C:"[RDX PARSE ERROR] line 2, col 10: expected '," tests\diag_parse_out.txt > nul
if errorlevel 1 (
    echo [test_diag_engine] FAILED: Parser diagnostic mismatched!
    type tests\diag_parse_out.txt
    exit /b 1
)

echo [test_diag_engine] Testing Semantic Error...
rdx_bin.exe broken_semantic.rdx > tests\diag_semantic_out.txt 2>&1
findstr /C:"[RDX MEMORY ERROR] line 4, col 11: You are trying to use the variable 'x' whose ownership was already moved" tests\diag_semantic_out.txt > nul
if errorlevel 1 (
    echo [test_diag_engine] FAILED: Semantic diagnostic mismatched!
    type tests\diag_semantic_out.txt
    exit /b 1
)

echo [test_diag_engine] SUCCESS! All diagnostics carets matched exact positions!
exit /b 0
