/*
 * ============================================================================
 *  token.h  —  RDX Token Definitions
 * ============================================================================
 */

#ifndef RDX_TOKEN_H
#define RDX_TOKEN_H

#include "core.h"

typedef enum {
    /* Special */
    TOK_EOF = 0,
    TOK_ERROR,

    /* Literals */
    TOK_INT_LIT,       /* 42, 0xFF, 0b1010       */
    TOK_FLOAT_LIT,     /* 3.14159                */
    TOK_STR_LIT,       /* "hello world"          */
    TOK_CHAR_LIT,      /* 'c'                    */
    TOK_TRUE,          /* true                   */
    TOK_FALSE,         /* false                  */
    TOK_NIL,           /* nil / none             */

    /* Keywords */
    TOK_FN,            /* fn / make              */
    TOK_LET,           /* let                    */
    TOK_MUT,           /* mut                    */
    TOK_FIX,           /* fix                    */
    TOK_IF,            /* if                     */
    TOK_ELIF,          /* elif                   */
    TOK_ELSE,          /* else                   */
    TOK_LOOP,          /* loop                   */
    TOK_WHILE,         /* while                  */
    TOK_FOR,           /* for                    */
    TOK_IN,            /* in                     */
    TOK_BREAK,         /* break                  */
    TOK_CONTINUE,      /* continue               */
    TOK_RETURN,        /* return / ret           */
    TOK_YIELD,         /* yield                  */
    TOK_SPAWN,         /* spawn                  */
    TOK_CAP,           /* cap                    */
    TOK_SHAPE,         /* shape                  */
    TOK_IMPL,          /* impl                   */
    TOK_SELF,          /* self                   */
    TOK_PUB,           /* pub                    */
    TOK_LOAD,          /* load                   */
    TOK_EXTERN,        /* extern                 */
    TOK_PRINT,         /* print / call           */

    /* Embedded & Systems Keywords */
    TOK_VOLATILE,      /* volatile               */
    TOK_REG,           /* reg (hardware register)*/
    TOK_PACKED,        /* packed                 */
    TOK_STRUCT,        /* struct                 */
    TOK_COMPTIME,      /* comptime               */
    TOK_ASM,           /* asm                    */
    TOK_ISR,           /* isr (interrupt)        */
    TOK_CAPABILITY,    /* capability             */

    /* Fixed-Width Embedded Types */
    TOK_U8,            /* u8                     */
    TOK_U16,           /* u16                    */
    TOK_U32,           /* u32                    */
    TOK_U64,           /* u64                    */
    TOK_I8,            /* i8                     */
    TOK_I16,           /* i16                    */
    TOK_I32,           /* i32                    */
    TOK_I64,           /* i64                    */
    TOK_USIZE,         /* usize                  */
    TOK_ISIZE,         /* isize                  */
    TOK_F32,           /* f32                    */
    TOK_F64,           /* f64                    */

    /* Legacy Type aliases */
    TOK_TY_INT,        /* int / num              */
    TOK_TY_FLOAT,      /* float / flt / dob      */
    TOK_TY_BOOL,       /* bool / blo             */
    TOK_TY_STR,        /* str / chr              */
    TOK_TY_BYTE,       /* byte / byt             */
    TOK_TY_VOID,       /* void                   */

    /* Identifiers */
    TOK_IDENT,

    /* Operators */
    TOK_PLUS,          /* +                      */
    TOK_MINUS,         /* -                      */
    TOK_STAR,          /* *                      */
    TOK_SLASH,         /* /                      */
    TOK_PERCENT,       /* %                      */
    TOK_ASSIGN,        /* =                      */
    TOK_PLUS_ASSIGN,   /* +=                     */
    TOK_MINUS_ASSIGN,  /* -=                     */
    TOK_STAR_ASSIGN,   /* *=                     */
    TOK_SLASH_ASSIGN,  /* /=                     */

    /* Comparison */
    TOK_EQ,            /* ==                     */
    TOK_NEQ,           /* !=                     */
    TOK_LT,            /* <                      */
    TOK_LTE,           /* <=                     */
    TOK_GT,            /* >                      */
    TOK_GTE,           /* >=                     */

    /* Logical */
    TOK_AND,           /* and / &&               */
    TOK_OR,            /* or  / ||               */
    TOK_NOT,           /* not / !                */

    /* Bitwise */
    TOK_AMP,           /* &                      */
    TOK_PIPE,          /* |                      */
    TOK_CARET,         /* ^                      */
    TOK_TILDE,         /* ~                      */
    TOK_SHL,           /* <<                     */
    TOK_SHR,           /* >>                     */

    /* Delimiters & Punctuation */
    TOK_LPAREN,        /* (                      */
    TOK_RPAREN,        /* )                      */
    TOK_LBRACE,        /* {                      */
    TOK_RBRACE,        /* }                      */
    TOK_LBRACKET,      /* [                      */
    TOK_RBRACKET,      /* ]                      */
    TOK_COMMA,         /* ,                      */
    TOK_DOT,           /* .                      */
    TOK_DOTDOT,        /* ..                     */
    TOK_COLON,         /* :                      */
    TOK_SEMICOLON,     /* ;                      */
    TOK_ARROW,         /* ->                     */
    TOK_FAT_ARROW,     /* =>                     */
    TOK_QUESTION,      /* ?                      */
    TOK_AT,            /* @                      */
} TokenType;

typedef struct {
    TokenType   type;
    const char *start;
    usize       length;
    u32         line;
    u32         col;
    union {
        i64     int_val;
        double  float_val;
        char    char_val;
    };
} Token;

const char *token_type_name(TokenType type);

#endif /* RDX_TOKEN_H */
