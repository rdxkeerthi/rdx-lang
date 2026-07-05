@echo off
echo [test_step0_diagnostics] Testing Diagnostics Engine...

echo [test_step0_diagnostics] Compiling compiler...
call build.bat
if errorlevel 1 exit /b 1

echo [test_step0_diagnostics] Testing Lexer Error...
rdx_bin.exe tests\diag_lex.rdx > tests\diag_lex_out.txt
fc tests\diag_lex_expected.txt tests\diag_lex_out.txt
if errorlevel 1 (
    echo [test_step0_diagnostics] FAILED: Lexer diagnostic mismatch.
    exit /b 1
)

echo [test_step0_diagnostics] Testing Parser Error...
rdx_bin.exe tests\diag_parse.rdx > tests\diag_parse_out.txt
fc tests\diag_parse_expected.txt tests\diag_parse_out.txt
if errorlevel 1 (
    echo [test_step0_diagnostics] FAILED: Parser diagnostic mismatch.
    exit /b 1
)

echo [test_step0_diagnostics] Testing Semantic Error...
rdx_bin.exe tests\diag_semantic.rdx > tests\diag_semantic_out.txt
fc tests\diag_semantic_expected.txt tests\diag_semantic_out.txt
if errorlevel 1 (
    echo [test_step0_diagnostics] FAILED: Semantic diagnostic mismatch.
    exit /b 1
)

echo [test_step0_diagnostics] SUCCESS! All diagnostics matched byte-for-byte!
exit /b 0
