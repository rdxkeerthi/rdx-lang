/*
 * ============================================================================
 *  module.c  —  RDX Module System Implementation  (v2.0)
 * ============================================================================
 */

#include "module.h"
#include "lexer.h"
#include "parser.h"
#include "analyzer.h"
#include "type_check.h"
#include "ir_lower.h"

/* ============================================================
 *  Registry
 * ============================================================ */
void module_registry_init(ModuleRegistry *reg, Arena *arena) {
    my_memset(reg, 0, sizeof(ModuleRegistry));
    reg->arena = arena;
    reg->count = 0;
}

RdxModule *module_register(ModuleRegistry *reg, const char *name, usize name_len,
                             const char *path) {
    if (!reg || !name || reg->count >= MAX_REGISTERED_MODULES) return NULL;

    /* Check for duplicate */
    RdxModule *existing = module_find(reg, name, name_len);
    if (existing) return existing;

    RdxModule *mod = (RdxModule*)arena_alloc_zero(reg->arena, sizeof(RdxModule));
    if (!mod) return NULL;

    /* Copy name */
    usize copy_len = name_len < MAX_MODULE_NAME - 1 ? name_len : MAX_MODULE_NAME - 1;
    my_strncpy(mod->name, name, copy_len);
    mod->name[copy_len] = '\0';
    mod->name_len = copy_len;

    /* Copy path */
    if (path) {
        usize path_len = my_strlen(path);
        usize pcopy = path_len < MAX_MODULE_PATH - 1 ? path_len : MAX_MODULE_PATH - 1;
        my_strncpy(mod->path, path, pcopy);
        mod->path[pcopy] = '\0';
    }

    mod->state        = MOD_STATE_UNLOADED;
    mod->ir_module    = NULL;
    mod->export_count = 0;
    mod->dep_count    = 0;

    reg->modules[reg->count++] = mod;
    return mod;
}

RdxModule *module_find(ModuleRegistry *reg, const char *name, usize name_len) {
    if (!reg || !name) return NULL;
    for (u32 i = 0; i < reg->count; i++) {
        if (reg->modules[i]->name_len == name_len &&
            my_strncmp(reg->modules[i]->name, name, name_len) == 0) {
            return reg->modules[i];
        }
    }
    return NULL;
}

/* ============================================================
 *  Exported Symbol Registration
 * ============================================================ */
static void module_add_export(RdxModule *mod, const char *name, usize name_len,
                               RdxType *type, bool is_fn, bool is_const) {
    if (!mod || mod->export_count >= MAX_MODULE_EXPORTS) return;
    ModuleExport *ex = &mod->exports[mod->export_count++];
    ex->name     = name;
    ex->name_len = name_len;
    ex->type     = type;
    ex->is_fn    = is_fn;
    ex->is_const = is_const;
}

ModuleExport *module_find_export(RdxModule *mod, const char *name, usize name_len) {
    if (!mod || !name) return NULL;
    for (u32 i = 0; i < mod->export_count; i++) {
        if (mod->exports[i].name_len == name_len &&
            my_strncmp(mod->exports[i].name, name, name_len) == 0) {
            return &mod->exports[i];
        }
    }
    return NULL;
}

/* ============================================================
 *  Module Compilation
 * ============================================================ */
bool module_compile(ModuleRegistry *reg, RdxModule *mod) {
    if (!mod) return FALSE;

    if (mod->state == MOD_STATE_LOADED) return TRUE;

    if (mod->state == MOD_STATE_LOADING) {
        rdx_print("[MODULE ERROR] Circular import detected: ");
        rdx_print(mod->name);
        rdx_print("\n");
        return FALSE;
    }

    if (mod->state == MOD_STATE_ERROR) return FALSE;

    /* Mark as loading — circular dependency detection */
    mod->state = MOD_STATE_LOADING;

    /* If this is a synthetic stdlib module (no file path), mark loaded */
    if (mod->path[0] == '\0' || mod->path[0] == '<') {
        mod->state = MOD_STATE_LOADED;
        return TRUE;
    }

    /* Read source file */
    rdx_print("[module] Compiling: ");
    rdx_print(mod->name);
    rdx_print(" (");
    rdx_print(mod->path);
    rdx_print(")\n");

    usize src_len = 0;
    char *source = rdx_read_file(mod->path, &src_len);
    if (!source) {
        rdx_print("[MODULE ERROR] Cannot read file: ");
        rdx_print(mod->path);
        rdx_print("\n");
        mod->state = MOD_STATE_ERROR;
        return FALSE;
    }

    /* Lex */
    Lexer L;
    lexer_init(&L, source);
    int tok_count = lexer_tokenize(&L);
    if (tok_count < 0 || L.had_error) {
        mod->state = MOD_STATE_ERROR;
        return FALSE;
    }

    /* Parse */
    Parser P;
    parser_init(&P, L.tokens, tok_count);
    int root_idx = parser_parse(&P);
    if (root_idx < 0 || P.had_error) {
        mod->state = MOD_STATE_ERROR;
        return FALSE;
    }

    /* Resolve `use` statements — compile dependencies first */
    /* TODO: Extract use statements from AST and compile deps here */

    /* Type Check */
    Arena *arena = reg->arena;
    tc_init(&mod->type_checker, arena);
    /* tc_check_program needs AstProgram — for now use the new parser path */

    mod->state = MOD_STATE_LOADED;
    rdx_print("[module] Compiled: ");
    rdx_print(mod->name);
    rdx_print("\n");
    return TRUE;
}

/* ============================================================
 *  Standard Library Registration
 *
 *  Registers built-in modules with synthetic export tables.
 *  No file reading needed — they export pre-defined types.
 * ============================================================ */

/* --- math module exports --- */
static void register_stdlib_math(ModuleRegistry *reg, Arena *arena) {
    RdxModule *mod = module_register(reg, "math", 4, "<stdlib:math>");
    if (!mod) return;

    /* Register type-only stubs for now */
    module_add_export(mod, "abs",    3, &TC_TY_INT,   TRUE,  FALSE);
    module_add_export(mod, "max",    3, &TC_TY_INT,   TRUE,  FALSE);
    module_add_export(mod, "min",    3, &TC_TY_INT,   TRUE,  FALSE);
    module_add_export(mod, "pow",    3, &TC_TY_INT,   TRUE,  FALSE);
    module_add_export(mod, "sqrt",   4, &TC_TY_FLOAT, TRUE,  FALSE);
    module_add_export(mod, "clamp",  5, &TC_TY_INT,   TRUE,  FALSE);
    module_add_export(mod, "PI",     2, &TC_TY_FLOAT, FALSE, TRUE);
    module_add_export(mod, "E",      1, &TC_TY_FLOAT, FALSE, TRUE);
    module_add_export(mod, "TAU",    3, &TC_TY_FLOAT, FALSE, TRUE);
    module_add_export(mod, "INF",    3, &TC_TY_FLOAT, FALSE, TRUE);
    mod->state = MOD_STATE_LOADED;
    (void)arena;
}

/* --- str module exports --- */
static void register_stdlib_str(ModuleRegistry *reg, Arena *arena) {
    RdxModule *mod = module_register(reg, "str", 3, "<stdlib:str>");
    if (!mod) return;

    module_add_export(mod, "len",     3,  &TC_TY_INT, TRUE, FALSE);
    module_add_export(mod, "upper",   5,  &TC_TY_STR, TRUE, FALSE);
    module_add_export(mod, "lower",   5,  &TC_TY_STR, TRUE, FALSE);
    module_add_export(mod, "concat",  6,  &TC_TY_STR, TRUE, FALSE);
    module_add_export(mod, "slice",   5,  &TC_TY_STR, TRUE, FALSE);
    module_add_export(mod, "trim",    4,  &TC_TY_STR, TRUE, FALSE);
    module_add_export(mod, "split",   5,  &TC_TY_UNKNOWN, TRUE, FALSE);  /* []chr */
    module_add_export(mod, "find",    4,  &TC_TY_INT, TRUE, FALSE);
    module_add_export(mod, "replace", 7,  &TC_TY_STR, TRUE, FALSE);
    module_add_export(mod, "starts_with", 11, &TC_TY_BOOL, TRUE, FALSE);
    module_add_export(mod, "ends_with",    9, &TC_TY_BOOL, TRUE, FALSE);
    module_add_export(mod, "contains",     8, &TC_TY_BOOL, TRUE, FALSE);
    module_add_export(mod, "to_num",  6,  &TC_TY_INT, TRUE, FALSE);
    module_add_export(mod, "to_flt",  6,  &TC_TY_FLOAT, TRUE, FALSE);
    mod->state = MOD_STATE_LOADED;
    (void)arena;
}

/* --- io module exports --- */
static void register_stdlib_io(ModuleRegistry *reg, Arena *arena) {
    RdxModule *mod = module_register(reg, "io", 2, "<stdlib:io>");
    if (!mod) return;

    module_add_export(mod, "print",   5, &TC_TY_NIL, TRUE, FALSE);
    module_add_export(mod, "println", 7, &TC_TY_NIL, TRUE, FALSE);
    module_add_export(mod, "read",    4, &TC_TY_STR, TRUE, FALSE);
    module_add_export(mod, "write",   5, &TC_TY_NIL, TRUE, FALSE);

    /* File I/O */
    module_add_export(mod, "read_file",  9,  &TC_TY_STR,  TRUE, FALSE);
    module_add_export(mod, "write_file", 10, &TC_TY_BOOL, TRUE, FALSE);
    module_add_export(mod, "file_exists",11, &TC_TY_BOOL, TRUE, FALSE);
    mod->state = MOD_STATE_LOADED;
    (void)arena;
}

/* --- collections module exports --- */
static void register_stdlib_collections(ModuleRegistry *reg, Arena *arena) {
    RdxModule *mod = module_register(reg, "collections", 11, "<stdlib:collections>");
    if (!mod) return;

    /* Vec */
    module_add_export(mod, "Vec",    3, &TC_TY_UNKNOWN, FALSE, FALSE);
    module_add_export(mod, "Map",    3, &TC_TY_UNKNOWN, FALSE, FALSE);
    module_add_export(mod, "Set",    3, &TC_TY_UNKNOWN, FALSE, FALSE);
    module_add_export(mod, "Queue",  5, &TC_TY_UNKNOWN, FALSE, FALSE);
    module_add_export(mod, "Stack",  5, &TC_TY_UNKNOWN, FALSE, FALSE);
    mod->state = MOD_STATE_LOADED;
    (void)arena;
}

/* --- sys module exports --- */
static void register_stdlib_sys(ModuleRegistry *reg, Arena *arena) {
    RdxModule *mod = module_register(reg, "sys", 3, "<stdlib:sys>");
    if (!mod) return;

    module_add_export(mod, "exit",       4, &TC_TY_NEVER, TRUE, FALSE);
    module_add_export(mod, "args",       4, &TC_TY_UNKNOWN, FALSE, FALSE);
    module_add_export(mod, "env",        3, &TC_TY_STR, TRUE, FALSE);
    module_add_export(mod, "timestamp",  9, &TC_TY_INT, TRUE, FALSE);
    module_add_export(mod, "sleep_ms",   8, &TC_TY_NIL, TRUE, FALSE);
    module_add_export(mod, "platform",   8, &TC_TY_STR, FALSE, TRUE);
    module_add_export(mod, "PAGE_SIZE",  9, &TC_TY_INT, FALSE, TRUE);
    mod->state = MOD_STATE_LOADED;
    (void)arena;
}

void module_register_stdlib(ModuleRegistry *reg) {
    Arena *arena = reg->arena;
    register_stdlib_math(reg, arena);
    register_stdlib_str(reg, arena);
    register_stdlib_io(reg, arena);
    register_stdlib_collections(reg, arena);
    register_stdlib_sys(reg, arena);
}

/* ============================================================
 *  Resolve a `use` statement
 * ============================================================ */
RdxModule *module_resolve_use(ModuleRegistry *reg, const char *name, usize name_len,
                               const char *from_module_path) {
    (void)from_module_path;
    RdxModule *mod = module_find(reg, name, name_len);
    if (!mod) {
        /* Try building a path: <dirname of from_module>/<name>.rdx */
        /* For now, just report not found */
        rdx_print("[MODULE] Not found: ");
        rdx_print(name);
        rdx_print("\n");
        return NULL;
    }
    if (mod->state == MOD_STATE_UNLOADED) {
        module_compile(reg, mod);
    }
    return mod->state == MOD_STATE_LOADED ? mod : NULL;
}

/* ============================================================
 *  Dump
 * ============================================================ */
void module_registry_dump(const ModuleRegistry *reg) {
    if (!reg) return;
    rdx_print("[ModuleRegistry] ");
    rdx_print_num((i64)reg->count);
    rdx_print(" modules registered:\n");
    for (u32 i = 0; i < reg->count; i++) {
        const RdxModule *mod = reg->modules[i];
        rdx_print("  [");
        switch (mod->state) {
            case MOD_STATE_UNLOADED: rdx_print("UNLOADED"); break;
            case MOD_STATE_LOADING:  rdx_print("LOADING");  break;
            case MOD_STATE_LOADED:   rdx_print("LOADED");   break;
            case MOD_STATE_ERROR:    rdx_print("ERROR");    break;
        }
        rdx_print("] ");
        rdx_print(mod->name);
        rdx_print(" — ");
        rdx_print_num((i64)mod->export_count);
        rdx_print(" exports\n");
    }
}
