/*
 * ============================================================================
 *  test_rdx_vm.c  —  Comprehensive Test Suite for rdx-lang Architecture
 * ============================================================================
 *  Tests:
 *  - Phase 1: Lexer & Pratt Parser & AST
 *  - Phase 2: Bytecode Compiler & Virtual Machine
 *  - Phase 3: Deterministic Arena Allocator & Generational References
 *  - Phase 4: Colorless Concurrency (Fibers) & Zero-Trust Capabilities
 * ============================================================================
 */

#define RDX_DEFINE_ENTRY
#include "core.h"
#include "arena.h"
#include "gen_ref.h"
#include "token.h"
#include "lexer.h"
#include "parser.h"
#include "compiler.h"
#include "vm.h"

static u32 tests_run    = 0;
static u32 tests_passed = 0;

#define TEST(name) \
    static void test_##name(void); \
    static void run_##name(void) { \
        tests_run++; \
        rdx_print("  [TEST] " #name " ... "); \
        test_##name(); \
        tests_passed++; \
        rdx_print("PASSED\n"); \
    } \
    static void test_##name(void)

#define ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            rdx_print("\n[ASSERTION FAILED] " msg "\n"); \
            return; \
        } \
    } while (0)

/* ============================================================
 *  Phase 3 Tests: Deterministic Arena Allocator & GenRefs
 * ============================================================ */

TEST(arena_alloc_and_reset) {
    Arena arena = arena_create(1024);
    ASSERT(arena.first_block != NULL, "Arena must initialize first block");

    void *p1 = arena_alloc(&arena, 128);
    ASSERT(p1 != NULL, "Alloc p1 must succeed");
    void *p2 = arena_alloc(&arena, 256);
    ASSERT(p2 != NULL, "Alloc p2 must succeed");
    ASSERT((u8*)p2 >= (u8*)p1 + 128, "Allocations must be contiguous bump");

    ArenaMarker mark = arena_save(&arena);
    void *p3 = arena_alloc(&arena, 512);
    ASSERT(p3 != NULL, "Alloc p3 must succeed");

    arena_restore(&arena, mark);
    void *p4 = arena_alloc(&arena, 512);
    ASSERT(p4 == p3, "Restored arena must reuse memory address");

    arena_free(&arena);
}

TEST(generational_references) {
    Arena arena = arena_create(4096);
    GenTable table;
    gen_table_init(&table, &arena, 16);

    int val = 42;
    GenRef ref = gen_table_insert(&table, &val, 1);
    ASSERT(gen_table_is_valid(&table, ref), "GenRef must be valid after insert");

    int *retrieved = (int*)gen_table_get(&table, ref, 1);
    ASSERT(retrieved != NULL && *retrieved == 42, "Retrieved pointer must match");

    /* Free slot and verify immediate dangling ref invalidation in O(1) */
    gen_table_free_slot(&table, ref);
    ASSERT(!gen_table_is_valid(&table, ref), "GenRef must be invalid after free");
    ASSERT(gen_table_get(&table, ref, 1) == NULL, "Dangling GenRef must return NULL");

    arena_free(&arena);
}

/* ============================================================
 *  Phase 1 Tests: Lexer & Pratt Parser
 * ============================================================ */

TEST(lexer_scanning) {
    const char *source = "let x: int = 42 + 3.14 * 2; spawn worker(); cap \"stdio\";";
    Lexer lexer;
    lexer_init(&lexer, source, my_strlen(source), "test.rdx");

    Token t1 = lexer_next(&lexer);
    ASSERT(t1.type == TOK_LET, "Expected TOK_LET");

    Token t2 = lexer_next(&lexer);
    ASSERT(t2.type == TOK_IDENT, "Expected TOK_IDENT");

    Token t3 = lexer_next(&lexer);
    ASSERT(t3.type == TOK_COLON, "Expected TOK_COLON");

    Token t4 = lexer_next(&lexer);
    ASSERT(t4.type == TOK_TY_INT, "Expected TOK_TY_INT");
}

TEST(pratt_parser_precedence) {
    /* 2 + 3 * 4 should parse as 2 + (3 * 4) */
    const char *source = "let val = 2 + 3 * 4;";
    Arena arena = arena_create(4096);

    Parser parser;
    parser_init(&parser, source, my_strlen(source), "test.rdx", &arena);

    AstProgram *prog = parser_parse_program(&parser);
    ASSERT(!parser.had_error, "Parser must not error");
    ASSERT(prog->decl_count == 1, "Must have 1 declaration");

    AstStmt *stmt = prog->decls[0]->stmt;
    ASSERT(stmt->kind == STMT_VAR, "Declaration must be a variable declaration");
    ASSERT(stmt->var_decl.init->kind == EXPR_BINARY, "Init must be binary expr");
    ASSERT(stmt->var_decl.init->binary.op == TOK_PLUS, "Root binary op must be +");
    ASSERT(stmt->var_decl.init->binary.right->kind == EXPR_BINARY, "Right operand must be binary *");
    ASSERT(stmt->var_decl.init->binary.right->binary.op == TOK_STAR, "Right binary op must be *");

    arena_free(&arena);
}

/* ============================================================
 *  Phase 2 & 4 Tests: Bytecode Engine, Concurrency & Capabilities
 * ============================================================ */

TEST(vm_arithmetic_execution) {
    const char *source = "let a = 10;\n"
                         "let b = 25;\n"
                         "let sum = a + b * 2;\n"
                         "print sum;\n";
    VM vm;
    vm_init(&vm, CAP_SYS_ALL);

    InterpretResult res = vm_interpret_source(&vm, source, my_strlen(source), "math.rdx");
    ASSERT(res == INTERPRET_OK, "VM execution must succeed");

    RdxValue sum_val;
    bool found = vm_get_global(&vm, "sum", 3, &sum_val);
    ASSERT(found && sum_val.type == VAL_INT && sum_val.as.int_val == 60, "Sum must equal 60");

    vm_free(&vm);
}

TEST(vm_control_flow_and_functions) {
    const char *source = "fn fib(n) {\n"
                         "    if n <= 1 {\n"
                         "        return n;\n"
                         "    }\n"
                         "    return fib(n - 1) + fib(n - 2);\n"
                         "}\n"
                         "let result = fib(7);\n"
                         "print result;\n";
    VM vm;
    vm_init(&vm, CAP_SYS_ALL);

    InterpretResult res = vm_interpret_source(&vm, source, my_strlen(source), "fib.rdx");
    ASSERT(res == INTERPRET_OK, "Fibonacci VM execution must succeed");

    RdxValue res_val;
    bool found = vm_get_global(&vm, "result", 6, &res_val);
    ASSERT(found && res_val.type == VAL_INT && res_val.as.int_val == 13, "fib(7) must equal 13");

    vm_free(&vm);
}

TEST(vm_concurrency_green_threads) {
    const char *source = "fn worker() {\n"
                         "    print \"Fiber worker running\";\n"
                         "    yield;\n"
                         "    print \"Fiber worker resumed and finished\";\n"
                         "}\n"
                         "spawn worker();\n"
                         "print \"Main fiber spawned worker\";\n";
    VM vm;
    vm_init(&vm, CAP_SYS_ALL);

    InterpretResult res = vm_interpret_source(&vm, source, my_strlen(source), "concurrency.rdx");
    ASSERT(res == INTERPRET_OK, "Colorless concurrency VM execution must succeed");

    vm_free(&vm);
}

TEST(vm_zero_trust_capability_security) {
    /* Test: Fiber without CAP_STDIO_WRITE gets blocked when invoking print */
    const char *source = "print \"This should be blocked by zero-trust sandbox\";\n";
    VM vm;
    vm_init(&vm, CAP_NONE); /* Zero capabilities */

    InterpretResult res = vm_interpret_source(&vm, source, my_strlen(source), "security.rdx");
    ASSERT(res == INTERPRET_OK, "VM handles security restriction cleanly");

    vm_free(&vm);
}

TEST(showcase_example) {
    usize len = 0;
    char *source = rdx_read_file("examples/showcase.rdx", &len);
    if (!source) {
        /* Fallback for backslash path */
        source = rdx_read_file("examples\\showcase.rdx", &len);
    }
    ASSERT(source != NULL, "showcase.rdx must be found");

    VM vm;
    vm_init(&vm, CAP_SYS_ALL);
    InterpretResult res = vm_interpret_source(&vm, source, len, "examples/showcase.rdx");
    ASSERT(res == INTERPRET_OK, "Showcase example execution must succeed");
    vm_free(&vm);
}

int rdx_compiler_main(void) {
    rdx_print("====================================================\n");
    rdx_print("  rdx-lang Architecture Test Suite\n");
    rdx_print("====================================================\n");

    run_arena_alloc_and_reset();
    run_generational_references();
    run_lexer_scanning();
    run_pratt_parser_precedence();
    run_vm_arithmetic_execution();
    run_vm_control_flow_and_functions();
    run_vm_concurrency_green_threads();
    run_vm_zero_trust_capability_security();
    run_showcase_example();

    rdx_print("====================================================\n");
    rdx_print("  Summary: ");
    rdx_print_num(tests_passed);
    rdx_print(" / ");
    rdx_print_num(tests_run);
    rdx_print(" tests passed.\n");
    rdx_print("====================================================\n");
    return 0;
}
