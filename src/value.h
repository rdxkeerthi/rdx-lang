/*
 * ============================================================================
 *  value.h  —  RDX Bytecode Value Representation  (v2.0)
 * ============================================================================
 *
 *  v2 adds:
 *    VAL_STRUCT   — heap-allocated struct object
 *    VAL_ENUM     — tagged union (enum variant)
 *    VAL_TUPLE    — fixed-size heterogeneous tuple
 *    VAL_CLOSURE  — function + captured upvalues
 *    VAL_CHAN     — channel for inter-fiber message passing
 *    VAL_FIBER    — spawned fiber handle (for await)
 */

#ifndef RDX_VALUE_H
#define RDX_VALUE_H

#include "core.h"
#include "arena.h"
#include "gen_ref.h"

/* ============================================================
 *  Forward Declarations
 * ============================================================ */
typedef struct RdxValue    RdxValue;
typedef struct ObjString   ObjString;
typedef struct ObjArray    ObjArray;
typedef struct ObjFunction ObjFunction;
typedef struct ObjClosure  ObjClosure;
typedef struct ObjUpvalue  ObjUpvalue;
typedef struct ObjStruct   ObjStruct;
typedef struct ObjEnum     ObjEnum;
typedef struct ObjTuple    ObjTuple;
typedef struct ObjChan     ObjChan;

/* ============================================================
 *  Value Type Tag
 * ============================================================ */
typedef enum {
    VAL_NIL,
    VAL_BOOL,
    VAL_INT,
    VAL_FLOAT,
    VAL_STR,
    VAL_ARRAY,
    VAL_FN,
    VAL_CLOSURE,    /* v2: first-class closure with upvalues  */
    VAL_STRUCT,     /* v2: struct instance                    */
    VAL_ENUM,       /* v2: tagged union / enum variant        */
    VAL_TUPLE,      /* v2: fixed-size tuple                   */
    VAL_CHAN,       /* v2: channel handle                     */
    VAL_FIBER,      /* v2: fiber/task handle                  */
    VAL_CAP,
    VAL_GENREF,
} ValueType;

/* ============================================================
 *  Object Definitions
 * ============================================================ */

struct ObjString {
    usize length;
    char  chars[1];   /* flexible array member */
};

struct ObjArray {
    RdxValue *elements;
    u32       count;
    u32       capacity;
    Arena    *arena;
};

struct ObjFunction {
    const char   *name;
    usize         name_len;
    u32           arity;
    u32           upvalue_count;   /* v2: number of captured upvalues */
    struct Chunk *chunk;
};

/* ObjUpvalue — heap-allocated cell for a captured local variable.
 * While the captured variable is still on the stack, `location` points
 * directly into the stack slot.  When the enclosing scope exits
 * (OP_CLOSE_UPVALUE), the value is copied into `closed` and `location`
 * is redirected to `&closed`.                                          */
struct ObjUpvalue {
    RdxValue  *location;        /* points to stack slot OR to `closed`  */
    RdxValue   closed;          /* heap-resident copy after close        */
    ObjUpvalue *next;           /* intrusive linked list (open upvalues) */
};

/* ObjClosure — a function together with its captured environment.     */
struct ObjClosure {
    ObjFunction  *function;
    ObjUpvalue  **upvalues;
    u32           upvalue_count;
};

/* ObjStruct — a runtime struct instance.
 * Field names and field count are stored in the type descriptor
 * (a constant in the chunk's pool).  Values are stored inline.         */
struct ObjStruct {
    const char  *type_name;     /* e.g. "Point"   */
    usize        type_name_len;
    RdxValue    *fields;        /* indexed by field declaration order    */
    const char **field_names;   /* parallel array of field name pointers */
    usize       *field_name_lens;
    u32          field_count;
    Arena       *arena;
};

/* ObjEnum — a runtime enum variant.                                    */
struct ObjEnum {
    const char *enum_name;      /* e.g. "Status"   */
    usize       enum_name_len;
    const char *variant_name;   /* e.g. "Ok"       */
    usize       variant_name_len;
    u32         tag;            /* numeric tag index (0-based)           */
    RdxValue   *payload;        /* payload values (can be NULL)          */
    u32         payload_count;
    Arena      *arena;
};

/* ObjTuple — heterogeneous fixed-size tuple.                           */
struct ObjTuple {
    RdxValue *elements;
    u32       count;
};

/* ObjChan — FIFO message channel for fiber communication.              */
#define CHAN_BUFFER_MAX 256
struct ObjChan {
    RdxValue   buffer[CHAN_BUFFER_MAX];
    u32        head;
    u32        tail;
    u32        count;
};

/* ============================================================
 *  Tagged Value Union
 * ============================================================ */
struct RdxValue {
    ValueType type;
    union {
        bool         bool_val;
        i64          int_val;
        double       float_val;
        ObjString   *str_val;
        ObjArray    *array_val;
        ObjFunction *fn_val;
        ObjClosure  *closure_val;
        ObjStruct   *struct_val;
        ObjEnum     *enum_val;
        ObjTuple    *tuple_val;
        ObjChan     *chan_val;
        u64          fiber_id;       /* VAL_FIBER: numeric fiber ID       */
        u64          cap_token;
        GenRef       gen_ref;
    } as;
};

/* ============================================================
 *  Value Constructors (inline)
 * ============================================================ */
static inline RdxValue val_nil(void) {
    RdxValue v;
    v.type = VAL_NIL;
    v.as.int_val = 0;
    return v;
}
static inline RdxValue val_bool(bool b) {
    RdxValue v;
    v.type = VAL_BOOL;
    v.as.bool_val = b;
    return v;
}
static inline RdxValue val_int(i64 i) {
    RdxValue v;
    v.type = VAL_INT;
    v.as.int_val = i;
    return v;
}
static inline RdxValue val_float(double f) {
    RdxValue v;
    v.type = VAL_FLOAT;
    v.as.float_val = f;
    return v;
}
static inline RdxValue val_str(ObjString *s) {
    RdxValue v;
    v.type = VAL_STR;
    v.as.str_val = s;
    return v;
}
static inline RdxValue val_fn(ObjFunction *fn) {
    RdxValue v;
    v.type = VAL_FN;
    v.as.fn_val = fn;
    return v;
}
static inline RdxValue val_closure(ObjClosure *cl) {
    RdxValue v;
    v.type = VAL_CLOSURE;
    v.as.closure_val = cl;
    return v;
}
static inline RdxValue val_struct(ObjStruct *s) {
    RdxValue v;
    v.type = VAL_STRUCT;
    v.as.struct_val = s;
    return v;
}
static inline RdxValue val_enum(ObjEnum *e) {
    RdxValue v;
    v.type = VAL_ENUM;
    v.as.enum_val = e;
    return v;
}
static inline RdxValue val_tuple(ObjTuple *t) {
    RdxValue v;
    v.type = VAL_TUPLE;
    v.as.tuple_val = t;
    return v;
}
static inline RdxValue val_chan(ObjChan *c) {
    RdxValue v;
    v.type = VAL_CHAN;
    v.as.chan_val = c;
    return v;
}
static inline RdxValue val_fiber(u64 id) {
    RdxValue v;
    v.type = VAL_FIBER;
    v.as.fiber_id = id;
    return v;
}
static inline RdxValue val_cap(u64 cap_token) {
    RdxValue v;
    v.type = VAL_CAP;
    v.as.cap_token = cap_token;
    return v;
}
static inline RdxValue val_genref(GenRef ref) {
    RdxValue v;
    v.type = VAL_GENREF;
    v.as.gen_ref = ref;
    return v;
}

/* ============================================================
 *  Value Array (constant pool)
 * ============================================================ */
typedef struct {
    RdxValue *values;
    u32       count;
    u32       capacity;
    Arena    *arena;
} ValueArray;

/* ============================================================
 *  Public API
 * ============================================================ */
void        value_array_init(ValueArray *arr, Arena *arena);
void        value_array_write(ValueArray *arr, RdxValue val);
void        value_print(RdxValue val);
bool        value_is_truthy(RdxValue val);
bool        value_equals(RdxValue a, RdxValue b);
const char *value_type_name(ValueType t);

ObjString  *obj_string_create(Arena *arena, const char *chars, usize length);
ObjString  *obj_string_create_unescaped(Arena *arena, const char *chars, usize length);

/* v2 object constructors */
ObjArray   *obj_array_create(Arena *arena, u32 initial_capacity);
void        obj_array_push(ObjArray *arr, RdxValue val);

ObjStruct  *obj_struct_create(Arena *arena, const char *type_name, usize name_len,
                               u32 field_count,
                               const char **field_names, usize *field_name_lens);

ObjEnum    *obj_enum_create(Arena *arena,
                             const char *enum_name, usize enum_name_len,
                             const char *variant_name, usize variant_name_len,
                             u32 tag,
                             RdxValue *payload, u32 payload_count);

ObjTuple   *obj_tuple_create(Arena *arena, RdxValue *elements, u32 count);

ObjClosure *obj_closure_create(Arena *arena, ObjFunction *fn);

ObjChan    *obj_chan_create(Arena *arena);
bool        obj_chan_send(ObjChan *ch, RdxValue val);
bool        obj_chan_recv(ObjChan *ch, RdxValue *out);

#endif /* RDX_VALUE_H */
