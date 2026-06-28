/*
 * test_lexer.c — RDX Lexer Smoke Test
 *
 * Compile (PowerShell / VS Code Terminal):
 *   gcc -nostdlib -O2 -o test_lexer.exe src/test_lexer.c src/lexer.c -lkernel32
 *   .\test_lexer.exe
 *
 * The test tokenises a realistic RDX source snippet and
 * asserts that specific tokens appear in the right positions.
 */

#define RDX_DEFINE_ENTRY
#include "lexer.h"

/* -------------------------------------------------------
 *  The RDX source we will tokenise
 * ------------------------------------------------------- */
static const char *TEST_SOURCE =
    "// A complete RDX program\n"
    "num age : 25;\n"
    "str name : \"Arthur\";\n"
    "\n"
    "fn calculate(x: num) -> num {\n"
    "    return x + 1;\n"
    "}\n"
    "\n"
    "blueprint Player {\n"
    "    num health;\n"
    "    num ammo;\n"
    "}\n"
    "\n"
    "// Allocate and drop a Player instance\n"
    "Player p : alloc Player();\n"
    "p.health : 100;\n"
    "drop p;\n"
    "\n"
    "// Control flow\n"
    "num count : 0;\n"
    "loop (count < 10) {\n"
    "    count : count + 1;\n"
    "}\n"
    "\n"
    "if (age == 25) {\n"
    "    num x : 1;\n"
    "} else {\n"
    "    num x : 0;\n"
    "}\n"
    "\n"
    "sys.rdx {\n"
    "    call(\"Hello Windows!\");\n"
    "}\n";

/* -------------------------------------------------------
 *  Simple assertion helper (prints PASS / FAIL)
 * ------------------------------------------------------- */
static int g_pass = 0;
static int g_fail = 0;

static void assert_tok(const Lexer *L, int idx,
                       TokenType expected_type,
                       const char *expected_val)
{
    if (idx >= L->token_count) {
        rdx_print("  FAIL  index ");
        rdx_print_num((i64)idx);
        rdx_print(" out of range\r\n");
        g_fail++;
        return;
    }
    const Token *t = &L->tokens[idx];
    bool type_ok = (t->type == expected_type);
    bool val_ok  = (expected_val == NULL) ||
                   (my_strcmp(t->value, expected_val) == 0);

    if (type_ok && val_ok) {
        g_pass++;
    } else {
        rdx_print("  FAIL  tok[");
        rdx_print_num((i64)idx);
        rdx_print("]  got ");
        rdx_print(token_type_name(t->type));
        rdx_print(" \"");
        rdx_print(t->value);
        rdx_print("\"  expected ");
        rdx_print(token_type_name(expected_type));
        if (expected_val) { rdx_print(" \""); rdx_print(expected_val); rdx_print("\""); }
        rdx_print("\r\n");
        g_fail++;
    }
}

/* -------------------------------------------------------
 *  rdx_compiler_main — test entry (called by mainCRTStartup)
 * ------------------------------------------------------- */
int rdx_compiler_main(void) {
    rdx_print("=== RDX Lexer Smoke Test ===\r\n\r\n");

    /* 1. Initialise & tokenise */
    Lexer L;
    lexer_init(&L, TEST_SOURCE);
    int count = lexer_tokenize(&L);

    if (count < 0) {
        rdx_print("FATAL: tokeniser reported errors — aborting test\r\n");
        return 1;
    }

    rdx_print("Tokenised ");
    rdx_print_num((i64)count);
    rdx_print(" tokens from test source.\r\n\r\n");

    /* 2. Full dump so the user can inspect the stream */
    lexer_dump(&L);
    rdx_print("\r\n");

    /* 3. Spot-check individual tokens by index
     *
     *  The comment on line 1 is skipped, so token[0] starts at line 2.
     *
     *  Expected token stream (ignoring whitespace / comments):
     *  [0]  TOK_NUM         "num"
     *  [1]  TOK_IDENT       "age"
     *  [2]  TOK_ASSIGN      ":"
     *  [3]  TOK_NUM_LIT     "25"
     *  [4]  TOK_SEMICOLON   ";"
     *  [5]  TOK_STR         "str"
     *  [6]  TOK_IDENT       "name"
     *  [7]  TOK_ASSIGN      ":"
     *  [8]  TOK_STR_LIT     "Arthur"
     *  [9]  TOK_SEMICOLON   ";"
     *  [10] TOK_FN          "fn"
     *  [11] TOK_IDENT       "calculate"
     *  [12] TOK_LPAREN      "("
     *  [13] TOK_IDENT       "x"
     *  [14] TOK_ASSIGN      ":"
     *  [15] TOK_NUM         "num"
     *  [16] TOK_RPAREN      ")"
     *  [17] TOK_ARROW       "->"
     *  [18] TOK_NUM         "num"
     *  [19] TOK_LBRACE      "{"
     *  [20] TOK_RETURN      "return"
     *  [21] TOK_IDENT       "x"
     *  [22] TOK_PLUS        "+"
     *  [23] TOK_NUM_LIT     "1"
     *  [24] TOK_SEMICOLON   ";"
     *  [25] TOK_RBRACE      "}"
     *  [26] TOK_BLUEPRINT   "blueprint"
     *  [27] TOK_IDENT       "Player"
     *  ...
     */
    rdx_print("--- Spot Checks ---\r\n");
    assert_tok(&L,  0, TOK_NUM,       "num");
    assert_tok(&L,  1, TOK_IDENT,     "age");
    assert_tok(&L,  2, TOK_ASSIGN,    ":");
    assert_tok(&L,  3, TOK_NUM_LIT,   "25");
    assert_tok(&L,  4, TOK_SEMICOLON, ";");
    assert_tok(&L,  5, TOK_STR,       "str");
    assert_tok(&L,  6, TOK_IDENT,     "name");
    assert_tok(&L,  7, TOK_ASSIGN,    ":");
    assert_tok(&L,  8, TOK_STR_LIT,   "Arthur");
    assert_tok(&L,  9, TOK_SEMICOLON, ";");
    assert_tok(&L, 10, TOK_FN,        "fn");
    assert_tok(&L, 11, TOK_IDENT,     "calculate");
    assert_tok(&L, 12, TOK_LPAREN,    "(");
    assert_tok(&L, 13, TOK_IDENT,     "x");
    assert_tok(&L, 14, TOK_ASSIGN,    ":");
    assert_tok(&L, 15, TOK_NUM,       "num");
    assert_tok(&L, 16, TOK_RPAREN,    ")");
    assert_tok(&L, 17, TOK_ARROW,     "->");
    assert_tok(&L, 18, TOK_NUM,       "num");
    assert_tok(&L, 19, TOK_LBRACE,    "{");
    assert_tok(&L, 20, TOK_RETURN,    "return");
    assert_tok(&L, 21, TOK_IDENT,     "x");
    assert_tok(&L, 22, TOK_PLUS,      "+");
    assert_tok(&L, 23, TOK_NUM_LIT,   "1");
    assert_tok(&L, 24, TOK_SEMICOLON, ";");
    assert_tok(&L, 25, TOK_RBRACE,    "}");
    assert_tok(&L, 26, TOK_BLUEPRINT, "blueprint");
    assert_tok(&L, 27, TOK_IDENT,     "Player");
    assert_tok(&L, 28, TOK_LBRACE,    "{");

    /* Verify 'sys' keyword and '.' are both produced */
    bool found_sys = FALSE;
    bool found_call = FALSE;
    bool found_lt   = FALSE;
    bool found_loop = FALSE;
    bool found_dot  = FALSE;
    bool found_drop = FALSE;
    bool found_alloc = FALSE;
    bool found_arrow = FALSE;
    for (int i = 0; i < L.token_count; i++) {
        TokenType tt = L.tokens[i].type;
        if (tt == TOK_SYS)   found_sys   = TRUE;
        if (tt == TOK_CALL)  found_call  = TRUE;
        if (tt == TOK_LT)    found_lt    = TRUE;
        if (tt == TOK_LOOP)  found_loop  = TRUE;
        if (tt == TOK_DOT)   found_dot   = TRUE;
        if (tt == TOK_DROP)  found_drop  = TRUE;
        if (tt == TOK_ALLOC) found_alloc = TRUE;
        if (tt == TOK_ARROW) found_arrow = TRUE;
    }
    if (found_sys)   { g_pass++; } else { rdx_print("  FAIL: TOK_SYS not found\r\n");   g_fail++; }
    if (found_call)  { g_pass++; } else { rdx_print("  FAIL: TOK_CALL not found\r\n");  g_fail++; }
    if (found_lt)    { g_pass++; } else { rdx_print("  FAIL: TOK_LT not found\r\n");    g_fail++; }
    if (found_loop)  { g_pass++; } else { rdx_print("  FAIL: TOK_LOOP not found\r\n");  g_fail++; }
    if (found_dot)   { g_pass++; } else { rdx_print("  FAIL: TOK_DOT not found\r\n");   g_fail++; }
    if (found_drop)  { g_pass++; } else { rdx_print("  FAIL: TOK_DROP not found\r\n");  g_fail++; }
    if (found_alloc) { g_pass++; } else { rdx_print("  FAIL: TOK_ALLOC not found\r\n"); g_fail++; }
    if (found_arrow) { g_pass++; } else { rdx_print("  FAIL: TOK_ARROW not found\r\n"); g_fail++; }

    /* Verify last token is EOF */
    assert_tok(&L, L.token_count - 1, TOK_EOF, "");

    /* Verify no lex errors */
    if (!L.had_error) { g_pass++; }
    else { rdx_print("  FAIL: lexer reported errors on valid source\r\n"); g_fail++; }

    /* 4. Results */
    rdx_print("\r\n--- Results ---\r\n");
    rdx_print("  PASS: "); rdx_print_num((i64)g_pass); rdx_print("\r\n");
    rdx_print("  FAIL: "); rdx_print_num((i64)g_fail); rdx_print("\r\n");

    if (g_fail == 0) {
        rdx_print("\r\n=== ALL LEXER TESTS PASSED ===\r\n");
    } else {
        rdx_print("\r\n=== SOME TESTS FAILED ===\r\n");
    }

    return g_fail > 0 ? 1 : 0;
}
