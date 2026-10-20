/*
 * ============================================================================
 *  module.h  —  RDX Module System  (v2.0)
 * ============================================================================
 *
 *  Manages multi-file compilation.
 *  Each .rdx file is an independent module.
 *  Symbols are private by default; exported with `pub`.
 *  Imported with `use <module_name>;`
 *
 *  Features:
 *    - Module registry (name → path → compiled IrModule)
 *    - Circular dependency detection via topological sort
 *    - Symbol export/import table
 *    - Namespace resolution (module.symbol)
 *    - Support for stdlib/ built-in modules
 */

#ifndef RDX_MODULE_H
#define RDX_MODULE_H

#include "core.h"
#include "arena.h"
#include "rdx_ir.h"
#include "type_check.h"

/* ============================================================
 *  Module State
 * ============================================================ */
typedef enum {
    MOD_STATE_UNLOADED,    /* not yet discovered     */
    MOD_STATE_LOADING,     /* currently being parsed (cycle detection) */
    MOD_STATE_LOADED,      /* fully compiled         */
    MOD_STATE_ERROR,       /* failed to compile      */
} ModuleState;

/* ============================================================
 *  Exported Symbol
 * ============================================================ */
typedef struct {
    const char  *name;
    usize        name_len;
    RdxType     *type;
    bool         is_fn;
    bool         is_const;
} ModuleExport;

/* ============================================================
 *  Module Descriptor
 * ============================================================ */
#define MAX_MODULE_EXPORTS  256
#define MAX_MODULE_DEPS      32
#define MAX_MODULE_NAME     128
#define MAX_MODULE_PATH    1024

typedef struct RdxModule {
    char         name[MAX_MODULE_NAME];   /* logical name, e.g. "math"      */
    usize        name_len;
    char         path[MAX_MODULE_PATH];   /* file system path               */
    ModuleState  state;

    /* Compiled artifacts */
    IrModule    *ir_module;               /* lowered IR (may be NULL pre-compile) */
    TypeChecker  type_checker;            /* type table for this module     */

    /* Public exports */
    ModuleExport exports[MAX_MODULE_EXPORTS];
    u32          export_count;

    /* Dependencies (modules this one imports) */
    struct RdxModule *deps[MAX_MODULE_DEPS];
    u32               dep_count;
} RdxModule;

/* ============================================================
 *  Module Registry
 * ============================================================ */
#define MAX_REGISTERED_MODULES 128

typedef struct {
    RdxModule *modules[MAX_REGISTERED_MODULES];
    u32        count;
    Arena     *arena;
} ModuleRegistry;

/* ============================================================
 *  Public API
 * ============================================================ */

/* Initialize a module registry */
void module_registry_init(ModuleRegistry *reg, Arena *arena);

/* Register a module by name and file path.
 * Returns the module descriptor (pre-creation, not compiled yet). */
RdxModule *module_register(ModuleRegistry *reg, const char *name, usize name_len,
                            const char *path);

/* Find a registered module by name */
RdxModule *module_find(ModuleRegistry *reg, const char *name, usize name_len);

/* Compile a module and all its dependencies (recursive).
 * Performs cycle detection.  Returns TRUE on success.  */
bool module_compile(ModuleRegistry *reg, RdxModule *mod);

/* Look up an exported symbol from a compiled module */
ModuleExport *module_find_export(RdxModule *mod, const char *name, usize name_len);

/* Register the standard library modules into the registry */
void module_register_stdlib(ModuleRegistry *reg);

/* Resolve a `use` statement: find/compile the named module and
 * return a pointer to it (or NULL on error).                          */
RdxModule *module_resolve_use(ModuleRegistry *reg, const char *name, usize name_len,
                               const char *from_module_path);

/* Dump all registered modules to stdout */
void module_registry_dump(const ModuleRegistry *reg);

#endif /* RDX_MODULE_H */
