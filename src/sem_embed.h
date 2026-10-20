/*
 * ============================================================================
 *  sem_embed.h  —  RDX Embedded Semantic Analyzer & Peripheral Ownership Safety
 * ============================================================================
 */

#ifndef RDX_SEM_EMBED_H
#define RDX_SEM_EMBED_H

#include "core.h"
#include "ast.h"
#include "arena.h"

#define MAX_PERIPHERALS 64
#define MAX_CAPABILITIES 64

typedef struct {
    const char   *name;
    usize         name_len;
    u64           base_address;
    const char   *cap_token;
    usize         cap_len;
    AstRegMember *registers;
    u32           reg_count;
} PeripheralSymbol;

typedef struct {
    Arena            *arena;
    PeripheralSymbol  peripherals[MAX_PERIPHERALS];
    u32               peripheral_count;
    const char       *granted_caps[MAX_CAPABILITIES];
    u32               granted_cap_count;
    bool              had_error;
    const char       *current_fn_name;
    u32               error_count;
} EmbeddedAnalyzer;

void sem_embed_init(EmbeddedAnalyzer *sem, Arena *arena);
bool sem_embed_analyze_program(EmbeddedAnalyzer *sem, AstProgram *prog);
bool sem_embed_check_peripheral_access(EmbeddedAnalyzer *sem, const char *periph_name, usize periph_len, u32 line, u32 col);

#endif /* RDX_SEM_EMBED_H */
