/* analyzer.h — RDX Semantic Ownership Analyzer: Public API
 *
 * Phase 3b of the RDX compiler pipeline.
 * Executes after parsing (Phase 3) and before code generation (Phase 4).
 *
 * Enforces Rust-like single-ownership memory safety at compile-time,
 * completely eliminating Use-After-Free and Double-Free vulnerabilities
 * before a single byte of assembly is emitted.
 *
 * Deps: parser.h (ASTNode, rdx_nodes, NodeType)
 *       core.h   (rdx_print_err, rdx_print_num, i64)
 */

#ifndef RDX_ANALYZER_H
#define RDX_ANALYZER_H

#include "parser.h"  /* ASTNode, NodeType, rdx_nodes, rdx_node_count */

/* ============================================================
 *  Ownership State Machine
 *
 *  Every tracked variable lives in exactly one of these states.
 *  State transitions are the core of the safety guarantee:
 *
 *    OWN_VALID  ──move──►  OWN_MOVED   (read after this = error)
 *    OWN_VALID  ──drop──►  OWN_DROPPED (any access after = error)
 *    OWN_MOVED  ──assign─► OWN_VALID   (re-assignment revives)
 *    OWN_DROPPED──assign─► OWN_VALID   (re-assignment revives)
 *    OWN_DROPPED──drop───► error       (Double-Free)
 * ============================================================ */
typedef enum {
    OWN_VALID   = 0,   /* variable is live and usable            */
    OWN_MOVED   = 1,   /* ownership was transferred — cannot use */
    OWN_DROPPED = 2,   /* memory was freed — cannot use or free  */
} OwnershipState;

/* ============================================================
 *  TrackedVariable — one slot in the ownership table
 *
 *  var_name  : the identifier as it appears in source (sval)
 *  state     : current ownership state
 *  line      : source line where the state last changed
 *              (used in error messages for better diagnostics)
 * ============================================================ */
typedef struct {
    char           var_name[64];
    OwnershipState state;
    int            line;        /* line of last state change */
    int            col;         /* column of last state change */
} TrackedVariable;

/* ============================================================
 *  SemanticAnalyzer — top-level analyzer context
 *
 *  tracked   : flat array of all variables seen so far
 *  count     : number of entries in `tracked`
 *  had_error : set to 1 on any ownership violation;
 *              compilation must be aborted when this is true
 * ============================================================ */
#define SA_MAX_VARS 128

typedef struct {
    TrackedVariable tracked[SA_MAX_VARS];
    int             count;
    int             had_error;
} SemanticAnalyzer;

/* ============================================================
 *  Public API
 *
 *  rdx_analyze_ownership(root_node)
 *    - Traverses the entire AST rooted at `root_node`.
 *    - Enforces single-ownership rules on every variable.
 *    - Emits human-readable error messages to stderr via
 *      rdx_print_err / rdx_print_num (no printf, no stdlib).
 *    - Returns 0 if the program is memory-safe.
 *    - Returns 1 if any ownership violation was detected.
 * ============================================================ */
int rdx_analyze_ownership(int root_node);

#endif /* RDX_ANALYZER_H */
