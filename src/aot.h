/*
 * aot.h — RDX AST-to-C Transpiler
 */
#ifndef RDX_AOT_H
#define RDX_AOT_H

#include "parser.h"

int aot_transpile(int root_node, const char *out_path);

#endif
