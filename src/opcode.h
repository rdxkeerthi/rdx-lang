/*
 * ============================================================================
 *  opcode.h  —  RDX Dense 8-bit Bytecode Instructions  (v2.0)
 * ============================================================================
 *
 *  Every opcode that reads operands from the bytecode stream documents
 *  its operand layout in brackets.  All multi-byte operands are
 *  little-endian (low byte first).
 */

#ifndef RDX_OPCODE_H
#define RDX_OPCODE_H

#include "core.h"

typedef enum {
    /* ------------------------------------------------------------------ */
    /* Stack / Literals                                                     */
    /* ------------------------------------------------------------------ */
    OP_CONSTANT,        /* [u16 const_index] Push constant from chunk pool   */
    OP_NIL,             /* Push nil onto stack                               */
    OP_TRUE,            /* Push true onto stack                              */
    OP_FALSE,           /* Push false onto stack                             */
    OP_POP,             /* Pop top value from stack                          */
    OP_DUP,             /* Duplicate top value on stack                      */
    OP_SWAP,            /* Swap top two values: [a,b] → [b,a]               */

    /* ------------------------------------------------------------------ */
    /* Local Variables                                                      */
    /* ------------------------------------------------------------------ */
    OP_GET_LOCAL,       /* [u16 slot] Push local variable onto stack         */
    OP_SET_LOCAL,       /* [u16 slot] Store top of stack into local slot     */

    /* ------------------------------------------------------------------ */
    /* Global Variables                                                     */
    /* ------------------------------------------------------------------ */
    OP_GET_GLOBAL,      /* [u16 name_const_idx] Push global value            */
    OP_SET_GLOBAL,      /* [u16 name_const_idx] Store global value           */
    OP_DEFINE_GLOBAL,   /* [u16 name_const_idx] Define new global value      */

    /* ------------------------------------------------------------------ */
    /* Upvalue / Closure Variables                                          */
    /* ------------------------------------------------------------------ */
    OP_GET_UPVALUE,     /* [u8 upvalue_index] Push captured upvalue          */
    OP_SET_UPVALUE,     /* [u8 upvalue_index] Write into captured upvalue    */
    OP_CLOSE_UPVALUE,   /* Hoist top-of-stack local to heap upvalue          */

    /* ------------------------------------------------------------------ */
    /* Arithmetic                                                           */
    /* ------------------------------------------------------------------ */
    OP_ADD,             /* Pop b, pop a, push (a + b)                        */
    OP_SUB,             /* Pop b, pop a, push (a - b)                        */
    OP_MUL,             /* Pop b, pop a, push (a * b)                        */
    OP_DIV,             /* Pop b, pop a, push (a / b)                        */
    OP_MOD,             /* Pop b, pop a, push (a % b)                        */
    OP_NEG,             /* Pop a, push (-a)                                  */

    /* ------------------------------------------------------------------ */
    /* Logical & Bitwise                                                    */
    /* ------------------------------------------------------------------ */
    OP_NOT,             /* Pop a, push (!a)                                  */
    OP_BIT_NOT,         /* Pop a, push (~a)                                  */
    OP_BIT_AND,         /* Pop b, pop a, push (a & b)                        */
    OP_BIT_OR,          /* Pop b, pop a, push (a | b)                        */
    OP_BIT_XOR,         /* Pop b, pop a, push (a ^ b)                        */
    OP_SHL,             /* Pop b, pop a, push (a << b)                       */
    OP_SHR,             /* Pop b, pop a, push (a >> b) arithmetic            */

    /* ------------------------------------------------------------------ */
    /* Comparisons                                                          */
    /* ------------------------------------------------------------------ */
    OP_EQ,              /* Pop b, pop a, push (a == b)                       */
    OP_NEQ,             /* Pop b, pop a, push (a != b)                       */
    OP_LT,              /* Pop b, pop a, push (a < b)                        */
    OP_LTE,             /* Pop b, pop a, push (a <= b)                       */
    OP_GT,              /* Pop b, pop a, push (a > b)                        */
    OP_GTE,             /* Pop b, pop a, push (a >= b)                       */

    /* ------------------------------------------------------------------ */
    /* Control Flow                                                         */
    /* ------------------------------------------------------------------ */
    OP_JUMP,            /* [u16 offset] Unconditional jump forward           */
    OP_JUMP_IF_FALSE,   /* [u16 offset] Jump forward if top of stack false   */
    OP_JUMP_IF_TRUE,    /* [u16 offset] Jump forward if top of stack truthy  */
    OP_LOOP,            /* [u16 offset] Unconditional jump backward          */

    /* ------------------------------------------------------------------ */
    /* Struct Operations                                                    */
    /* ------------------------------------------------------------------ */
    /*  OP_MAKE_STRUCT  [u16 type_const_idx] [u8 field_count]
     *  Pops `field_count` values (in declaration order, last pushed last).
     *  Pushes an ObjStruct onto the stack.                                 */
    OP_MAKE_STRUCT,

    /*  OP_GET_FIELD    [u16 field_name_const_idx]
     *  Pops struct, pushes struct.field value.                             */
    OP_GET_FIELD,

    /*  OP_SET_FIELD    [u16 field_name_const_idx]
     *  Stack: [..., struct, value].  Sets struct.field = value.
     *  Leaves struct on stack.                                             */
    OP_SET_FIELD,

    /* ------------------------------------------------------------------ */
    /* Enum / Tagged-Union Operations                                       */
    /* ------------------------------------------------------------------ */
    /*  OP_MAKE_ENUM    [u16 enum_type_const_idx] [u16 variant_const_idx] [u8 payload_count]
     *  Pops `payload_count` payload values.
     *  Pushes an ObjEnum with variant tag + payload.                       */
    OP_MAKE_ENUM,

    /*  OP_ENUM_TAG                                                         */
    /*  Pops enum, pushes its variant tag index (num).                      */
    OP_ENUM_TAG,

    /*  OP_ENUM_PAYLOAD [u8 payload_index]                                  */
    /*  Pops enum, pushes payload[payload_index].                           */
    OP_ENUM_PAYLOAD,

    /* ------------------------------------------------------------------ */
    /* Tuple Operations                                                     */
    /* ------------------------------------------------------------------ */
    /*  OP_MAKE_TUPLE   [u8 element_count]
     *  Pops `element_count` values, pushes ObjTuple.                      */
    OP_MAKE_TUPLE,

    /*  OP_TUPLE_GET    [u8 index]
     *  Pops tuple, pushes tuple.index.                                     */
    OP_TUPLE_GET,

    /* ------------------------------------------------------------------ */
    /* Array / Slice Operations                                             */
    /* ------------------------------------------------------------------ */
    /*  OP_MAKE_ARRAY   [u16 element_count]
     *  Pops element_count values (first pushed = index 0).
     *  Pushes ObjArray.                                                    */
    OP_MAKE_ARRAY,

    /*  OP_ARRAY_GET
     *  Stack: [..., array, index].  Pushes array[index] (bounds checked). */
    OP_ARRAY_GET,

    /*  OP_ARRAY_SET
     *  Stack: [..., array, index, value].  array[index] = value.           */
    OP_ARRAY_SET,

    /*  OP_ARRAY_LEN
     *  Pops array, pushes length as num.                                   */
    OP_ARRAY_LEN,

    /*  OP_ARRAY_PUSH
     *  Stack: [..., array, value]. Appends value to array (mut arrays).    */
    OP_ARRAY_PUSH,

    /* ------------------------------------------------------------------ */
    /* Pattern Matching                                                     */
    /* ------------------------------------------------------------------ */
    /*  OP_MATCH_INT    [i64 imm — 8 bytes LE]
     *  Peeks top. If top == imm pushes true, else pushes false.           */
    OP_MATCH_INT,

    /*  OP_MATCH_STR    [u16 const_idx]
     *  Peeks top. If string equal pushes true, else pushes false.          */
    OP_MATCH_STR,

    /*  OP_MATCH_ENUM   [u16 variant_const_idx]
     *  Peeks enum. If tag matches variant_idx pushes true, else false.     */
    OP_MATCH_ENUM,

    /*  OP_MATCH_RANGE  [i64 lo — 8 bytes] [i64 hi — 8 bytes]
     *  Peeks top (int). Pushes true if lo <= top <= hi.                    */
    OP_MATCH_RANGE,

    /*  OP_MATCH_WILDCARD
     *  Always pushes true (wildcard `_` arm).                              */
    OP_MATCH_WILDCARD,

    /* ------------------------------------------------------------------ */
    /* Closures                                                             */
    /* ------------------------------------------------------------------ */
    /*  OP_CLOSURE      [u16 const_idx (ObjFunction)] [u8 upvalue_count]
     *  Followed by upvalue_count pairs of [u8 is_local] [u8 index].
     *  Wraps the function in an ObjClosure capturing upvalues.             */
    OP_CLOSURE,

    /* ------------------------------------------------------------------ */
    /* Functions & Calls                                                    */
    /* ------------------------------------------------------------------ */
    OP_CALL,            /* [u8 arg_count] Call function or closure           */
    OP_TAIL_CALL,       /* [u8 arg_count] Tail-call optimised call           */
    OP_RETURN,          /* Return from current call frame                    */

    /* ------------------------------------------------------------------ */
    /* Type Checking / Casting                                              */
    /* ------------------------------------------------------------------ */
    /*  OP_IS_TYPE      [u16 type_const_idx]
     *  Pops value, pushes bool: true if value is of named type.            */
    OP_IS_TYPE,

    /*  OP_CAST         [u16 target_type_const_idx]
     *  Pops value, pushes converted value (num↔flt, num→chr, etc.).       */
    OP_CAST,

    /* ------------------------------------------------------------------ */
    /* Option / Result Constructors                                         */
    /* ------------------------------------------------------------------ */
    OP_MAKE_SOME,       /* Pop v, push Option::Some(v)                       */
    OP_MAKE_NONE,       /* Push Option::None                                 */
    OP_MAKE_OK,         /* Pop v, push Result::Ok(v)                         */
    OP_MAKE_ERR,        /* Pop v (chr), push Result::Err(v)                  */
    OP_UNWRAP_OK,       /* Peek Result — if Err, panic; else push Ok payload  */
    OP_UNWRAP_SOME,     /* Peek Option — if None, panic; else push Some payload*/
    OP_PROPAGATE_ERR,   /* ? operator: if Err, return it from current fn      */

    /* ------------------------------------------------------------------ */
    /* I/O & Builtins                                                       */
    /* ------------------------------------------------------------------ */
    OP_PRINT,           /* [u8 count] Print values from stack + newline      */
    OP_INPUT,           /* [u16 prompt_const_idx] Read line from stdin       */

    /* ------------------------------------------------------------------ */
    /* Module / Namespaces                                                  */
    /* ------------------------------------------------------------------ */
    /*  OP_GET_MODULE   [u16 module_const_idx] [u16 symbol_const_idx]
     *  Push a symbol from an imported module.                              */
    OP_GET_MODULE,

    /* ------------------------------------------------------------------ */
    /* Structured Concurrency (Green Threads / Fibers)                      */
    /* ------------------------------------------------------------------ */
    OP_SPAWN,           /* [u8 arg_count] Spawn green thread fiber           */
    OP_AWAIT,           /* Pop fiber handle, suspend until completion        */
    OP_YIELD,           /* Yield current fiber execution back to scheduler   */
    OP_CHAN_SEND,        /* Pop channel, pop value — send into channel        */
    OP_CHAN_RECV,        /* Pop channel — push received value (blocks fiber)  */

    /* ------------------------------------------------------------------ */
    /* Zero-Trust Capability Security                                       */
    /* ------------------------------------------------------------------ */
    OP_CAP_REQ,         /* [u16 name_const_idx] Request capability token     */
    OP_CAP_INVOKE,      /* [u8 cap_id, u8 arg_count] Perform capability call */

    /* ------------------------------------------------------------------ */
    /* Resource Lifecycle                                                   */
    /* ------------------------------------------------------------------ */
    OP_RESOURCE_OPEN,   /* [u8 res_kind] Open a tracked resource             */
    OP_RESOURCE_CLOSE,  /* Close top-of-stack resource handle                */

    /* ------------------------------------------------------------------ */
    /* Compile-Time Assertions                                              */
    /* ------------------------------------------------------------------ */
    OP_ASSERT,          /* [u16 msg_const_idx] Pop bool; panic if false      */
    OP_PANIC,           /* [u16 msg_const_idx] Always panic with message     */

    /* ------------------------------------------------------------------ */
    /* Termination                                                          */
    /* ------------------------------------------------------------------ */
    OP_HALT,            /* Terminate VM execution                            */

    OP_COUNT,           /* Sentinel — number of opcodes                      */
} OpCode;

const char *opcode_name(OpCode op);

#endif /* RDX_OPCODE_H */
