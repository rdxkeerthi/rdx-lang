/*
 * diag.h — RDX Diagnostics Engine
 */
#ifndef RDX_DIAG_H
#define RDX_DIAG_H

#include "core.h"

/* Initialize diagnostic engine with a pointer to the source text */
void rdx_diag_init(const char *source);

/* Print an error with caret pointing to the specific line/col */
void rdx_diag_error(int line, int col, int len, const char *prefix, const char *msg);
void rdx_diag_errorf(int line, int col, int len, const char *prefix, const char *msg, const char *arg);

#endif /* RDX_DIAG_H */
