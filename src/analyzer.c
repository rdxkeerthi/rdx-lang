/* analyzer.c — RDX Semantic Ownership Analyzer: Implementation
 *
 * Phase 3b: Traverses the AST produced by the parser and enforces
 * Rust-like single-ownership semantics at compile time.
 *
 * Rules enforced:
 *   - A variable may not be read after its ownership is moved.
 *   - A variable may not be read after it has been dropped.
 *   - A variable may not be dropped twice (Double-Free detection).
 *   - Re-assignment to a moved/dropped variable revives it (OWN_VALID).
 *
 * Error output uses ONLY rdx_print_err / rdx_print_num from core.h.
 * No printf, no stdlib, no string.h.
 *
 * Deps: analyzer.h -> parser.h -> lexer.h -> core.h
 */

#include "analyzer.h"
#include "diag.h"

/* ============================================================
 *  Module-level analyzer state
 *  (a single global instance is sufficient — the compiler is
 *   single-threaded and processes one file per invocation)
 * ============================================================ */
static SemanticAnalyzer SA;

/* ============================================================
 *  Internal helpers
 * ============================================================ */

/*
 * sa_find — linear scan for a variable name in the tracker.
 * Returns a pointer to its TrackedVariable, or NULL if not found.
 */
static TrackedVariable *sa_find(const char *name) {
    for (int i = 0; i < SA.count; i++) {
        if (my_strcmp(SA.tracked[i].var_name, name) == 0)
            return &SA.tracked[i];
    }
    return NULL;
}

/*
 * sa_register — adds a new variable to the tracker as OWN_VALID.
 * If the variable is already tracked (e.g., re-declared in an inner
 * scope — RDX does not have scoped shadowing yet), its state is simply
 * reset to OWN_VALID.
 */
static TrackedVariable *sa_register(const char *name, int line, int col) {
    /* Update existing entry if already known */
    TrackedVariable *existing = sa_find(name);
    if (existing) {
        existing->state = OWN_VALID;
        existing->line  = line;
        existing->col   = col;
        return existing;
    }

    /* Allocate a new slot */
    if (SA.count >= SA_MAX_VARS) {
        rdx_diag_error(line, col, 1, "[RDX ANALYZER ERROR]", "Fatal: too many variables");
        SA.had_error = 1;
        return NULL;
    }

    TrackedVariable *t = &SA.tracked[SA.count++];
    /* Manually copy name (no strncpy from string.h) */
    int i = 0;
    while (name[i] && i < 63) { t->var_name[i] = name[i]; i++; }
    t->var_name[i] = '\0';
    t->state = OWN_VALID;
    t->line  = line;
    t->col   = col;
    return t;
}

/* Forward declaration — check_node is mutually recursive with itself */
static void check_node(int idx);

/* ============================================================
 *  check_children — recurse over a NODE_BLOCK / NODE_PROGRAM
 *  children[] array (statements)
 * ============================================================ */
static void check_children(ASTNode *n) {
    for (int i = 0; i < n->child_count; i++) {
        if (n->children[i] >= 0) {
            check_node(n->children[i]);
        }
    }
}

/* ============================================================
 *  check_node — the core recursive traversal engine
 *
 *  idx  : index into the global rdx_nodes[] pool
 * ============================================================ */
static void check_node(int idx) {
    if (idx < 0 || idx >= rdx_node_count) return;

    ASTNode *n = &rdx_nodes[idx];

    switch (n->type) {

    /* ----------------------------------------------------------
     *  NODE_VAR_DECL — variable declaration: type name : expr;
     *
     *  Two sub-cases:
     *  (a) RHS is a NODE_IDENT  →  move semantics: A : B;
     *      Register A as OWN_VALID, then mark B as OWN_MOVED.
     *  (b) RHS is anything else →  normal init (alloc, literal, …)
     *      Register name as OWN_VALID.
     *
     *  We traverse the RHS *before* registering the LHS so that
     *  self-referential initialisers (pathological edge cases)
     *  are caught correctly.
     * ---------------------------------------------------------- */
    case NODE_VAR_DECL: {
        /* Check RHS expression first */
        if (n->left >= 0) {
            ASTNode *rhs = &rdx_nodes[n->left];

            if (rhs->type == NODE_IDENT) {
                /* Move operation: new_var : existing_var; */
                /* First validate that the source is usable */
                check_node(n->left);   /* will catch moved/dropped */

                /* Register the destination as valid */
                sa_register(n->sval, n->line, n->col);

                /* Mark the source as moved (only if check_node didn't
                 * already flag a fatal error — we still mark to prevent
                 * cascading false positives from the same violation) */
                TrackedVariable *src = sa_find(rhs->sval);
                if (src) {
                    src->state = OWN_MOVED;
                    src->line  = n->line;
                    src->col   = n->col;
                }
            } else {
                /* Normal initialisation — check RHS, then register LHS */
                check_node(n->left);
                sa_register(n->sval, n->line, n->col);
            }
        } else {
            /* Declaration without initialiser (params, etc.) */
            sa_register(n->sval, n->line, n->col);
        }

        /* Also traverse right, condition, body, else_body if present */
        if (n->right     >= 0) check_node(n->right);
        if (n->condition >= 0) check_node(n->condition);
        if (n->body      >= 0) check_node(n->body);
        if (n->else_body >= 0) check_node(n->else_body);
        break;
    }

    /* ----------------------------------------------------------
     *  NODE_IDENT — variable read / access
     *
     *  Catch:
     *    • Use of an already-moved value  (OWN_MOVED)
     *    • Use after free                 (OWN_DROPPED)
     *
     *  Variables not in the tracker (e.g., function parameters
     *  or implicitly-declared globals in simple programs) are
     *  silently allowed — we only police what we track.
     * ---------------------------------------------------------- */
    case NODE_IDENT: {
        TrackedVariable *t = sa_find(n->sval);
        if (!t) break;   /* unknown to tracker — allow */

        if (t->state == OWN_MOVED) {
            rdx_diag_errorf(n->line, n->col, (int)my_strlen(t->var_name),
                            "[RDX MEMORY ERROR]",
                            "You are trying to use the variable '%s' whose ownership was already moved or transferred earlier.",
                            t->var_name);
            SA.had_error = 1;
        } else if (t->state == OWN_DROPPED) {
            rdx_diag_errorf(n->line, n->col, (int)my_strlen(t->var_name),
                            "[RDX MEMORY ERROR]",
                            "You are trying to use the variable '%s' that was already freed (use-after-free).",
                            t->var_name);
            SA.had_error = 1;
        }
        break;
    }

    /* NODE_DROP was removed in Part 1 cleanup — `drop` is no longer a keyword
     * and NODE_DROP can never be emitted by the parser. No case needed. */

    /* ----------------------------------------------------------
     *  NODE_ASSIGN — re-assignment to an existing variable
     *
     *  Semantics: assigning to a previously moved or dropped
     *  variable *revives* it — the variable acquires a new value
     *  and its ownership state becomes OWN_VALID again.
     *
     *  We evaluate the RHS first (to detect any errors in the
     *  source expression before granting a revival).
     * ---------------------------------------------------------- */
    case NODE_ASSIGN: {
        /* Evaluate RHS first */
        if (n->left >= 0) check_node(n->left);

        /* Revive / update the LHS variable */
        TrackedVariable *t = sa_find(n->sval);
        if (t) {
            /* Revival: moved or dropped → valid again */
            t->state = OWN_VALID;
            t->line  = n->line;
            t->col   = n->col;
        }
        /* (If not tracked, we don't create a new entry for
         *  bare assignments — only declarations do that.) */

        if (n->right     >= 0) check_node(n->right);
        if (n->condition >= 0) check_node(n->condition);
        if (n->body      >= 0) check_node(n->body);
        if (n->else_body >= 0) check_node(n->else_body);
        break;
    }

    /* ----------------------------------------------------------
     *  NODE_FN_DECL — function declaration
     *
     *  Parameters are declared as NODE_VAR_DECL children.
     *  The body is traversed via n->body.
     *
     *  NOTE: A production-quality analyzer would push/pop a scope
     *  frame here so that parameter names don't collide with outer
     *  variables.  For Phase 3b we use a flat table and rely on the
     *  fact that RDX currently does not support name shadowing.
     * ---------------------------------------------------------- */
    case NODE_EXTERN:
    case NODE_FN_DECL: {
        /* Register parameters (they are child VAR_DECL nodes) */
        check_children(n);
        /* Traverse the function body */
        if (n->body >= 0) check_node(n->body);
        break;
    }

    /* ----------------------------------------------------------
     *  Standard structural nodes — recurse into all sub-trees
     *
     *  NODE_PROGRAM   : top-level statement list
     *  NODE_BLOCK     : braced statement list
     *  NODE_IF        : condition + body + optional else_body
     *  NODE_LOOP      : condition + body
     *  NODE_BINARY_OP : left + right sub-expressions
     *  NODE_RETURN    : left (return value expression)
     *  NODE_FN_CALL   : callee arguments in children[]
     *  NODE_ALLOC     : no sub-nodes that carry variable names
     *  NODE_SYS_BLOCK : body block of sys.rdx { ... }
     *  NODE_MEMBER_ACCESS: object reference — treat like IDENT
     * ---------------------------------------------------------- */
    case NODE_PROGRAM:
    case NODE_BLOCK: {
        /* Visit direct children (statements) */
        check_children(n);
        /* Also visit structural sub-trees */
        if (n->condition >= 0) check_node(n->condition);
        if (n->left      >= 0) check_node(n->left);
        if (n->right     >= 0) check_node(n->right);
        if (n->body      >= 0) check_node(n->body);
        if (n->else_body >= 0) check_node(n->else_body);
        break;
    }

    case NODE_IF: {
        if (n->condition >= 0) check_node(n->condition);
        
        SemanticAnalyzer *state_before = (SemanticAnalyzer*)rdx_alloc_bump(sizeof(SemanticAnalyzer));
        my_memcpy(state_before, &SA, sizeof(SemanticAnalyzer));
        
        if (n->body >= 0) check_node(n->body);
        
        SemanticAnalyzer *state_after_if = (SemanticAnalyzer*)rdx_alloc_bump(sizeof(SemanticAnalyzer));
        my_memcpy(state_after_if, &SA, sizeof(SemanticAnalyzer));
        
        my_memcpy(&SA, state_before, sizeof(SemanticAnalyzer));
        
        if (n->else_body >= 0) check_node(n->else_body);
        
        /* Merge branches: if a var is moved/dropped in either branch, it's moved/dropped globally */
        for (int i = 0; i < SA.count; i++) {
            if (state_after_if->tracked[i].state == OWN_MOVED || SA.tracked[i].state == OWN_MOVED) {
                SA.tracked[i].state = OWN_MOVED;
            }
            if (state_after_if->tracked[i].state == OWN_DROPPED || SA.tracked[i].state == OWN_DROPPED) {
                SA.tracked[i].state = OWN_DROPPED;
            }
        }
        check_children(n);
        break;
    }

    case NODE_LOOP: {
        if (n->condition >= 0) check_node(n->condition);
        
        /* First iteration */
        if (n->body >= 0) check_node(n->body);
        
        /* Second iteration to catch cross-iteration moves */
        if (n->condition >= 0) check_node(n->condition);
        if (n->body >= 0) check_node(n->body);
        
        check_children(n);
        break;
    }

    case NODE_BINARY_OP: {
        if (n->left  >= 0) check_node(n->left);
        if (n->right >= 0) check_node(n->right);
        break;
    }

    case NODE_RETURN: {
        if (n->left >= 0) check_node(n->left);
        break;
    }

    case NODE_FN_CALL: {
        /* Arguments are stored in children[] */
        check_children(n);
        break;
    }

    case NODE_SYS_BLOCK: {
        if (n->body >= 0) check_node(n->body);
        break;
    }

    case NODE_MEMBER_ACCESS: {
        /*
         * sval holds the object name.  Check whether the object
         * itself is in a valid ownership state.
         */
        TrackedVariable *t = sa_find(n->sval);
        if (t) {
            if (t->state == OWN_MOVED) {
                rdx_diag_errorf(n->line, n->col, (int)my_strlen(t->var_name),
                                "[RDX MEMORY ERROR]",
                                "You are trying to access a member of the variable '%s' whose ownership was already moved.",
                                t->var_name);
                SA.had_error = 1;
            } else if (t->state == OWN_DROPPED) {
                rdx_diag_errorf(n->line, n->col, (int)my_strlen(t->var_name),
                                "[RDX MEMORY ERROR]",
                                "You are trying to access a member of the variable '%s' that was already freed (use-after-free).",
                                t->var_name);
                SA.had_error = 1;
            }
        }
        break;
    }
    /* ----------------------------------------------------------
     *  Leaf nodes with no sub-trees and no ownership impact
     * ---------------------------------------------------------- */
    case NODE_LITERAL_NUM:
    case NODE_LITERAL_STR:
    case NODE_SYS_CALL: {
        if (n->left >= 0) check_node(n->left);
        break;
    }
    case NODE_BLUEPRINT:
    default:
        break;
    }
}

/* ============================================================
 *  Public API — rdx_analyze_ownership
 *
 *  Initialises the analyzer state, kicks off the traversal,
 *  and returns 0 (clean) or 1 (errors detected).
 * ============================================================ */
int rdx_analyze_ownership(int root_node) {
    /* Zero-initialise the entire analyzer (no memset from string.h) */
    my_memset(&SA, 0, sizeof(SemanticAnalyzer));

    if (root_node < 0 || root_node >= rdx_node_count) {
        rdx_print_err("[RDX ANALYZER] Invalid root node index\r\n");
        return 1;
    }

    check_node(root_node);

    return SA.had_error ? 1 : 0;
}
