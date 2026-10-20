/*
 * ============================================================================
 *  test_embedded.c  —  RDX Embedded & Bare-Metal Compiler Pipeline Tests
 * ============================================================================
 */

#define RDX_DEFINE_ENTRY
#include "core.h"
#include "arena.h"
#include "token.h"
#include "lexer.h"
#include "parser.h"
#include "comptime.h"
#include "sem_embed.h"
#include "embed_codegen.h"

static u32 tests_run    = 0;
static u32 tests_passed = 0;
static bool current_test_failed = FALSE;

#define TEST(name) \
    static void test_##name(void); \
    static void run_##name(void) { \
        tests_run++; \
        rdx_print("  [TEST] " #name " ... "); \
        current_test_failed = FALSE; \
        test_##name(); \
        if (!current_test_failed) { \
            tests_passed++; \
            rdx_print("PASSED\n"); \
        } else { \
            rdx_print("FAILED\n"); \
        } \
    } \
    static void test_##name(void)

#define ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            rdx_print("\n[ASSERTION FAILED] " msg "\n"); \
            current_test_failed = TRUE; \
            return; \
        } \
    } while (0)

TEST(embedded_token_scanning) {
    const char *source = "volatile reg UART0 @ 0x40004000 : cap UART0_CAP; packed struct Pkt { b: u8, w: u16, d: u32, q: u64 }; isr(16) fn handler() {}";
    Lexer lexer;
    lexer_init(&lexer, source, my_strlen(source), "test_embed.rdx");

    Token t1 = lexer_next(&lexer);
    ASSERT(t1.type == TOK_VOLATILE, "Expected TOK_VOLATILE");

    Token t2 = lexer_next(&lexer);
    ASSERT(t2.type == TOK_REG, "Expected TOK_REG");

    Token t3 = lexer_next(&lexer);
    ASSERT(t3.type == TOK_IDENT, "Expected TOK_IDENT UART0");

    Token t4 = lexer_next(&lexer);
    ASSERT(t4.type == TOK_AT, "Expected TOK_AT");
}

TEST(comptime_constant_evaluation) {
    Arena arena = arena_create(4096);
    ComptimeContext comptime;
    comptime_init(&comptime, &arena);

    // Compute (72000000 / (16 * 115200)) = 72000000 / 1843200 = 39
    AstExpr *left  = ast_make_int(&arena, 72000000, 1, 1);
    AstExpr *r1    = ast_make_int(&arena, 16, 1, 1);
    AstExpr *r2    = ast_make_int(&arena, 115200, 1, 1);
    AstExpr *right = ast_make_binary(&arena, TOK_STAR, r1, r2, 1, 1);
    AstExpr *div   = ast_make_binary(&arena, TOK_SLASH, left, right, 1, 1);

    i64 result = 0;
    bool ok = comptime_eval_int(&comptime, div, &result);
    ASSERT(ok, "Comptime evaluation must succeed");
    ASSERT(result == 39, "Baudrate BRR divisor must equal 39");

    arena_free(&arena);
}

TEST(peripheral_capability_security_pass) {
    const char *source = "reg UART0 @ 0x40004000 : cap UART0_CAP {\n"
                         "    volatile CR1: u32 @ 0x00,\n"
                         "}\n"
                         "fn init_uart(c: UART0_CAP) {\n"
                         "    UART0.CR1 = 1;\n"
                         "}\n";
    Arena arena = arena_create(8192);
    Parser parser;
    parser_init(&parser, source, my_strlen(source), "uart_pass.rdx", &arena);

    AstProgram *prog = parser_parse_program(&parser);
    ASSERT(!parser.had_error, "Parser must succeed");

    EmbeddedAnalyzer sem;
    sem_embed_init(&sem, &arena);
    bool ok = sem_embed_analyze_program(&sem, prog);
    ASSERT(ok, "Semantic analysis with valid capability must pass");

    arena_free(&arena);
}

TEST(peripheral_capability_security_reject) {
    // Attempting to access UART0 without declaring UART0_CAP parameter
    const char *source = "reg UART0 @ 0x40004000 : cap UART0_CAP {\n"
                         "    volatile CR1: u32 @ 0x00,\n"
                         "}\n"
                         "fn rogue_driver() {\n"
                         "    UART0.CR1 = 1;\n"
                         "}\n";
    Arena arena = arena_create(8192);
    Parser parser;
    parser_init(&parser, source, my_strlen(source), "uart_fail.rdx", &arena);

    AstProgram *prog = parser_parse_program(&parser);
    ASSERT(!parser.had_error, "Parser must succeed");

    EmbeddedAnalyzer sem;
    sem_embed_init(&sem, &arena);
    bool ok = sem_embed_analyze_program(&sem, prog);
    ASSERT(!ok, "Semantic analysis must REJECT rogue peripheral access");
    ASSERT(sem.error_count > 0, "Error count must be > 0");

    arena_free(&arena);
}

TEST(bit_range_slicing_ast) {
    const char *source = "let val = CR1[15..8];\n";
    Arena arena = arena_create(4096);
    Parser parser;
    parser_init(&parser, source, my_strlen(source), "bit_range.rdx", &arena);

    AstProgram *prog = parser_parse_program(&parser);
    ASSERT(!parser.had_error, "Parser must succeed");
    ASSERT(prog->decl_count == 1, "Must have 1 declaration");

    AstStmt *stmt = prog->decls[0]->stmt;
    ASSERT(stmt->kind == STMT_VAR, "Must be variable declaration");
    ASSERT(stmt->var_decl.init->kind == EXPR_BIT_RANGE, "Init must be EXPR_BIT_RANGE");

    arena_free(&arena);
}

TEST(freestanding_c_codegen_stm32_uart) {
    usize len = 0;
    char *source = rdx_read_file("examples/stm32_uart.rdx", &len);
    if (!source) source = rdx_read_file("examples\\stm32_uart.rdx", &len);
    ASSERT(source != NULL, "examples/stm32_uart.rdx must be found");

    Arena arena = arena_create(16 * 1024);
    Parser parser;
    parser_init(&parser, source, len, "stm32_uart.rdx", &arena);

    AstProgram *prog = parser_parse_program(&parser);
    ASSERT(!parser.had_error, "Parser must succeed on stm32_uart.rdx");

    EmbeddedAnalyzer sem;
    sem_embed_init(&sem, &arena);
    bool ok = sem_embed_analyze_program(&sem, prog);
    ASSERT(ok, "Semantic analysis must pass on stm32_uart.rdx");

    EmbedCodeGen codegen;
    embed_codegen_init(&codegen, &arena);
    const char *c_code = embed_codegen_generate(&codegen, prog);
    ASSERT(c_code != NULL && my_strlen(c_code) > 0, "C Code must be generated");

    // Verify key C components in output
    ASSERT(my_strlen(c_code) > 100, "Generated code must have substantial size");
    rdx_print("\n--- Generated STM32 Bare-Metal C ---\n");
    rdx_print(c_code);
    rdx_print("------------------------------------\n");

    arena_free(&arena);
}

TEST(freestanding_c_codegen_riscv_gpio) {
    usize len = 0;
    char *source = rdx_read_file("examples/riscv_gpio.rdx", &len);
    if (!source) source = rdx_read_file("examples\\riscv_gpio.rdx", &len);
    ASSERT(source != NULL, "examples/riscv_gpio.rdx must be found");

    Arena arena = arena_create(16 * 1024);
    Parser parser;
    parser_init(&parser, source, len, "riscv_gpio.rdx", &arena);

    AstProgram *prog = parser_parse_program(&parser);
    ASSERT(!parser.had_error, "Parser must succeed on riscv_gpio.rdx");

    EmbedCodeGen codegen;
    embed_codegen_init(&codegen, &arena);
    const char *c_code = embed_codegen_generate(&codegen, prog);
    ASSERT(c_code != NULL && my_strlen(c_code) > 0, "C Code must be generated");

    rdx_print("\n--- Generated RISC-V Bare-Metal C ---\n");
    rdx_print(c_code);
    rdx_print("-------------------------------------\n");

    arena_free(&arena);
}

TEST(comprehensive_test_suite_pipeline) {
    usize len = 0;
    char *source = rdx_read_file("examples/test_suite.rdx", &len);
    if (!source) source = rdx_read_file("examples\\test_suite.rdx", &len);
    ASSERT(source != NULL, "examples/test_suite.rdx must be found");

    Arena arena = arena_create(32 * 1024);
    Parser parser;
    parser_init(&parser, source, len, "test_suite.rdx", &arena);

    AstProgram *prog = parser_parse_program(&parser);
    ASSERT(!parser.had_error, "Parser must succeed on test_suite.rdx");

    EmbeddedAnalyzer sem;
    sem_embed_init(&sem, &arena);
    bool ok = sem_embed_analyze_program(&sem, prog);
    ASSERT(ok, "Semantic analysis must pass on test_suite.rdx");

    EmbedCodeGen codegen;
    embed_codegen_init(&codegen, &arena);
    const char *c_code = embed_codegen_generate(&codegen, prog);
    ASSERT(c_code != NULL && my_strlen(c_code) > 0, "C Code must be generated");

    rdx_print("\n--- Generated MISRA-C Output for test_suite.rdx ---\n");
    rdx_print(c_code);
    rdx_print("---------------------------------------------------\n");

    arena_free(&arena);
}

TEST(can_packet_reader_pipeline) {
    usize len = 0;
    char *source = rdx_read_file("examples/can_packet_reader.rdx", &len);
    if (!source) source = rdx_read_file("examples\\can_packet_reader.rdx", &len);
    ASSERT(source != NULL, "examples/can_packet_reader.rdx must be found");

    Arena arena = arena_create(32 * 1024);
    Parser parser;
    parser_init(&parser, source, len, "can_packet_reader.rdx", &arena);

    AstProgram *prog = parser_parse_program(&parser);
    ASSERT(!parser.had_error, "Parser must succeed on can_packet_reader.rdx");

    EmbeddedAnalyzer sem;
    sem_embed_init(&sem, &arena);
    bool ok = sem_embed_analyze_program(&sem, prog);
    ASSERT(ok, "Semantic analysis must pass on can_packet_reader.rdx");

    EmbedCodeGen codegen;
    embed_codegen_init(&codegen, &arena);
    const char *c_code = embed_codegen_generate(&codegen, prog);
    ASSERT(c_code != NULL && my_strlen(c_code) > 0, "C Code must be generated");

    rdx_print("\n--- Generated CAN Driver MISRA-C Output ---\n");
    rdx_print(c_code);
    rdx_print("-------------------------------------------\n");

    arena_free(&arena);
}

int rdx_compiler_main(void) {
    rdx_print("====================================================\n");
    rdx_print("  rdx-lang Embedded & Bare-Metal Test Suite\n");
    rdx_print("====================================================\n");

    run_embedded_token_scanning();
    run_comptime_constant_evaluation();
    run_peripheral_capability_security_pass();
    run_peripheral_capability_security_reject();
    run_bit_range_slicing_ast();
    run_freestanding_c_codegen_stm32_uart();
    run_freestanding_c_codegen_riscv_gpio();
    run_comprehensive_test_suite_pipeline();
    run_can_packet_reader_pipeline();

    rdx_print("====================================================\n");
    rdx_print("  Summary: ");
    rdx_print_num(tests_passed);
    rdx_print(" / ");
    rdx_print_num(tests_run);
    rdx_print(" tests passed.\n");
    rdx_print("====================================================\n");
    return 0;
}
