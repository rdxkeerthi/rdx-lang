/*
 * ============================================================================
 *  value.c  —  RDX Bytecode Value Implementation  (v2.0)
 * ============================================================================
 */

#include "value.h"

/* ============================================================
 *  String Objects
 * ============================================================ */
ObjString *obj_string_create(Arena *arena, const char *chars, usize length) {
    if (!arena) return NULL;
    usize total_size = sizeof(ObjString) + length;
    ObjString *s = (ObjString*)arena_alloc(arena, total_size);
    if (s) {
        s->length = length;
        if (chars && length > 0) {
            my_memcpy(s->chars, chars, length);
        }
        s->chars[length] = '\0';
    }
    return s;
}

ObjString *obj_string_create_unescaped(Arena *arena, const char *chars, usize length) {
    if (!arena) return NULL;
    char *buf = (char*)arena_alloc(arena, length + 1);
    usize out_len = 0;
    for (usize i = 0; i < length; i++) {
        if (chars[i] == '\\' && i + 1 < length) {
            i++;
            switch (chars[i]) {
                case 'n':  buf[out_len++] = '\n'; break;
                case 't':  buf[out_len++] = '\t'; break;
                case 'r':  buf[out_len++] = '\r'; break;
                case '\\': buf[out_len++] = '\\'; break;
                case '"':  buf[out_len++] = '"';  break;
                case '\'': buf[out_len++] = '\''; break;
                case '0':  buf[out_len++] = '\0'; break;
                default:
                    buf[out_len++] = '\\';
                    buf[out_len++] = chars[i];
                    break;
            }
        } else {
            buf[out_len++] = chars[i];
        }
    }
    buf[out_len] = '\0';
    return obj_string_create(arena, buf, out_len);
}

/* ============================================================
 *  Array Objects
 * ============================================================ */
ObjArray *obj_array_create(Arena *arena, u32 initial_capacity) {
    ObjArray *arr = (ObjArray*)arena_alloc(arena, sizeof(ObjArray));
    if (!arr) return NULL;
    if (initial_capacity < 4) initial_capacity = 4;
    arr->elements = (RdxValue*)arena_alloc(arena, sizeof(RdxValue) * initial_capacity);
    arr->count    = 0;
    arr->capacity = initial_capacity;
    arr->arena    = arena;
    return arr;
}

void obj_array_push(ObjArray *arr, RdxValue val) {
    if (!arr) return;
    if (arr->count >= arr->capacity) {
        u32 new_cap = arr->capacity * 2;
        RdxValue *new_elements = (RdxValue*)arena_alloc(arr->arena, sizeof(RdxValue) * new_cap);
        my_memcpy(new_elements, arr->elements, sizeof(RdxValue) * arr->count);
        arr->elements = new_elements;
        arr->capacity = new_cap;
    }
    arr->elements[arr->count++] = val;
}

/* ============================================================
 *  Struct Objects
 * ============================================================ */
ObjStruct *obj_struct_create(Arena *arena, const char *type_name, usize name_len,
                              u32 field_count,
                              const char **field_names, usize *field_name_lens) {
    ObjStruct *s = (ObjStruct*)arena_alloc(arena, sizeof(ObjStruct));
    if (!s) return NULL;
    s->type_name      = type_name;
    s->type_name_len  = name_len;
    s->field_count    = field_count;
    s->arena          = arena;
    s->fields         = (RdxValue*)arena_alloc(arena, sizeof(RdxValue) * field_count);
    s->field_names    = field_names;
    s->field_name_lens = field_name_lens;
    /* Zero-initialise all fields to nil */
    for (u32 i = 0; i < field_count; i++) {
        s->fields[i] = val_nil();
    }
    return s;
}

/* ============================================================
 *  Enum Objects
 * ============================================================ */
ObjEnum *obj_enum_create(Arena *arena,
                          const char *enum_name, usize enum_name_len,
                          const char *variant_name, usize variant_name_len,
                          u32 tag,
                          RdxValue *payload, u32 payload_count) {
    ObjEnum *e = (ObjEnum*)arena_alloc(arena, sizeof(ObjEnum));
    if (!e) return NULL;
    e->enum_name         = enum_name;
    e->enum_name_len     = enum_name_len;
    e->variant_name      = variant_name;
    e->variant_name_len  = variant_name_len;
    e->tag               = tag;
    e->payload_count     = payload_count;
    e->arena             = arena;
    if (payload_count > 0 && payload) {
        e->payload = (RdxValue*)arena_alloc(arena, sizeof(RdxValue) * payload_count);
        my_memcpy(e->payload, payload, sizeof(RdxValue) * payload_count);
    } else {
        e->payload = NULL;
    }
    return e;
}

/* ============================================================
 *  Tuple Objects
 * ============================================================ */
ObjTuple *obj_tuple_create(Arena *arena, RdxValue *elements, u32 count) {
    ObjTuple *t = (ObjTuple*)arena_alloc(arena, sizeof(ObjTuple));
    if (!t) return NULL;
    t->count    = count;
    t->elements = (RdxValue*)arena_alloc(arena, sizeof(RdxValue) * count);
    if (elements && count > 0) {
        my_memcpy(t->elements, elements, sizeof(RdxValue) * count);
    }
    return t;
}

/* ============================================================
 *  Closure Objects
 * ============================================================ */
ObjClosure *obj_closure_create(Arena *arena, ObjFunction *fn) {
    ObjClosure *cl = (ObjClosure*)arena_alloc(arena, sizeof(ObjClosure));
    if (!cl) return NULL;
    cl->function      = fn;
    cl->upvalue_count = fn ? fn->upvalue_count : 0;
    if (cl->upvalue_count > 0) {
        cl->upvalues = (ObjUpvalue**)arena_alloc(arena, sizeof(ObjUpvalue*) * cl->upvalue_count);
        my_memset(cl->upvalues, 0, sizeof(ObjUpvalue*) * cl->upvalue_count);
    } else {
        cl->upvalues = NULL;
    }
    return cl;
}

/* ============================================================
 *  Channel Objects
 * ============================================================ */
ObjChan *obj_chan_create(Arena *arena) {
    ObjChan *ch = (ObjChan*)arena_alloc(arena, sizeof(ObjChan));
    if (!ch) return NULL;
    ch->head  = 0;
    ch->tail  = 0;
    ch->count = 0;
    return ch;
}

bool obj_chan_send(ObjChan *ch, RdxValue val) {
    if (!ch || ch->count >= CHAN_BUFFER_MAX) return FALSE;
    ch->buffer[ch->tail] = val;
    ch->tail = (ch->tail + 1) % CHAN_BUFFER_MAX;
    ch->count++;
    return TRUE;
}

bool obj_chan_recv(ObjChan *ch, RdxValue *out) {
    if (!ch || ch->count == 0) return FALSE;
    *out = ch->buffer[ch->head];
    ch->head = (ch->head + 1) % CHAN_BUFFER_MAX;
    ch->count--;
    return TRUE;
}

/* ============================================================
 *  Value Array (constant pool)
 * ============================================================ */
void value_array_init(ValueArray *arr, Arena *arena) {
    arr->arena    = arena;
    arr->count    = 0;
    arr->capacity = 8;
    arr->values   = (RdxValue*)arena_alloc(arena, sizeof(RdxValue) * arr->capacity);
}

void value_array_write(ValueArray *arr, RdxValue val) {
    if (arr->count >= arr->capacity) {
        u32 new_cap = arr->capacity * 2;
        RdxValue *new_vals = (RdxValue*)arena_alloc(arr->arena, sizeof(RdxValue) * new_cap);
        my_memcpy(new_vals, arr->values, sizeof(RdxValue) * arr->capacity);
        arr->values   = new_vals;
        arr->capacity = new_cap;
    }
    arr->values[arr->count++] = val;
}

/* ============================================================
 *  value_type_name
 * ============================================================ */
const char *value_type_name(ValueType t) {
    switch (t) {
        case VAL_NIL:     return "nil";
        case VAL_BOOL:    return "blo";
        case VAL_INT:     return "num";
        case VAL_FLOAT:   return "flt";
        case VAL_STR:     return "chr";
        case VAL_ARRAY:   return "[]";
        case VAL_FN:      return "fn";
        case VAL_CLOSURE: return "closure";
        case VAL_STRUCT:  return "struct";
        case VAL_ENUM:    return "enum";
        case VAL_TUPLE:   return "tuple";
        case VAL_CHAN:    return "Channel";
        case VAL_FIBER:   return "Fiber";
        case VAL_CAP:     return "CapToken";
        case VAL_GENREF:  return "GenRef";
    }
    return "unknown";
}

/* ============================================================
 *  value_print
 * ============================================================ */
void value_print(RdxValue val) {
    switch (val.type) {
        case VAL_NIL:
            rdx_print("nil");
            break;
        case VAL_BOOL:
            rdx_print(val.as.bool_val ? "true" : "false");
            break;
        case VAL_INT:
            rdx_print_num(val.as.int_val);
            break;
        case VAL_FLOAT: {
            double f = val.as.float_val;
            if (f < 0) { rdx_print("-"); f = -f; }
            i64 ipart = (i64)f;
            rdx_print_num(ipart);
            rdx_print(".");
            double frac = f - (double)ipart;
            for (int i = 0; i < 6; i++) {
                frac *= 10.0;
                int digit = (int)frac;
                rdx_print_num(digit);
                frac -= (double)digit;
            }
            break;
        }
        case VAL_STR:
            if (val.as.str_val) rdx_print(val.as.str_val->chars);
            else rdx_print("\"\"");
            break;
        case VAL_ARRAY: {
            rdx_print("[");
            if (val.as.array_val) {
                for (u32 i = 0; i < val.as.array_val->count; i++) {
                    if (i > 0) rdx_print(", ");
                    value_print(val.as.array_val->elements[i]);
                }
            }
            rdx_print("]");
            break;
        }
        case VAL_FN:
            rdx_print("<fn ");
            if (val.as.fn_val && val.as.fn_val->name) rdx_print(val.as.fn_val->name);
            else rdx_print("anonymous");
            rdx_print(">");
            break;
        case VAL_CLOSURE:
            rdx_print("<closure ");
            if (val.as.closure_val && val.as.closure_val->function && val.as.closure_val->function->name)
                rdx_print(val.as.closure_val->function->name);
            else rdx_print("anonymous");
            rdx_print(">");
            break;
        case VAL_STRUCT: {
            ObjStruct *s = val.as.struct_val;
            if (!s) { rdx_print("<null struct>"); break; }
            rdx_print(s->type_name ? s->type_name : "?");
            rdx_print(" { ");
            for (u32 i = 0; i < s->field_count; i++) {
                if (i > 0) rdx_print(", ");
                if (s->field_names && s->field_names[i]) rdx_print(s->field_names[i]);
                rdx_print(": ");
                value_print(s->fields[i]);
            }
            rdx_print(" }");
            break;
        }
        case VAL_ENUM: {
            ObjEnum *e = val.as.enum_val;
            if (!e) { rdx_print("<null enum>"); break; }
            if (e->enum_name) rdx_print(e->enum_name);
            rdx_print(".");
            if (e->variant_name) rdx_print(e->variant_name);
            if (e->payload_count > 0) {
                rdx_print("(");
                for (u32 i = 0; i < e->payload_count; i++) {
                    if (i > 0) rdx_print(", ");
                    value_print(e->payload[i]);
                }
                rdx_print(")");
            }
            break;
        }
        case VAL_TUPLE: {
            ObjTuple *t = val.as.tuple_val;
            rdx_print("(");
            if (t) {
                for (u32 i = 0; i < t->count; i++) {
                    if (i > 0) rdx_print(", ");
                    value_print(t->elements[i]);
                }
            }
            rdx_print(")");
            break;
        }
        case VAL_CHAN:
            rdx_print("<Channel count=");
            rdx_print_num(val.as.chan_val ? (i64)val.as.chan_val->count : 0);
            rdx_print(">");
            break;
        case VAL_FIBER:
            rdx_print("<Fiber id=");
            rdx_print_num((i64)val.as.fiber_id);
            rdx_print(">");
            break;
        case VAL_CAP:
            rdx_print("<CapToken 0x");
            rdx_print_num((i64)val.as.cap_token);
            rdx_print(">");
            break;
        case VAL_GENREF:
            rdx_print("<GenRef slot=");
            rdx_print_num(val.as.gen_ref.slot_index);
            rdx_print(" gen=");
            rdx_print_num(val.as.gen_ref.generation);
            rdx_print(">");
            break;
    }
}

/* ============================================================
 *  value_is_truthy
 * ============================================================ */
bool value_is_truthy(RdxValue val) {
    switch (val.type) {
        case VAL_NIL:   return FALSE;
        case VAL_BOOL:  return val.as.bool_val;
        case VAL_INT:   return val.as.int_val != 0;
        case VAL_FLOAT: return val.as.float_val != 0.0;
        case VAL_STR:   return val.as.str_val && val.as.str_val->length > 0;
        case VAL_ENUM: {
            /* Option::None is falsy; everything else is truthy */
            ObjEnum *e = val.as.enum_val;
            if (e && e->variant_name && my_strcmp(e->variant_name, "None") == 0) return FALSE;
            return TRUE;
        }
        default: return TRUE;
    }
}

/* ============================================================
 *  value_equals
 * ============================================================ */
bool value_equals(RdxValue a, RdxValue b) {
    if (a.type != b.type) return FALSE;
    switch (a.type) {
        case VAL_NIL:   return TRUE;
        case VAL_BOOL:  return a.as.bool_val == b.as.bool_val;
        case VAL_INT:   return a.as.int_val  == b.as.int_val;
        case VAL_FLOAT: return a.as.float_val == b.as.float_val;
        case VAL_STR: {
            if (!a.as.str_val || !b.as.str_val) return a.as.str_val == b.as.str_val;
            if (a.as.str_val->length != b.as.str_val->length) return FALSE;
            return my_strncmp(a.as.str_val->chars, b.as.str_val->chars, a.as.str_val->length) == 0;
        }
        case VAL_FN:
            return a.as.fn_val == b.as.fn_val;
        case VAL_CLOSURE:
            return a.as.closure_val == b.as.closure_val;
        case VAL_ENUM: {
            ObjEnum *ea = a.as.enum_val, *eb = b.as.enum_val;
            if (!ea || !eb) return ea == eb;
            if (ea->tag != eb->tag) return FALSE;
            if (ea->payload_count != eb->payload_count) return FALSE;
            for (u32 i = 0; i < ea->payload_count; i++) {
                if (!value_equals(ea->payload[i], eb->payload[i])) return FALSE;
            }
            return TRUE;
        }
        case VAL_CAP:
            return a.as.cap_token == b.as.cap_token;
        case VAL_GENREF:
            return a.as.gen_ref.slot_index == b.as.gen_ref.slot_index &&
                   a.as.gen_ref.generation  == b.as.gen_ref.generation;
        default:
            return FALSE;
    }
}
