/* Copyright  (C) 2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rslc_share.h).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef __LIBRETRO_SDK_RSLC_SHARE_H__
#define __LIBRETRO_SDK_RSLC_SHARE_H__

/* Internal to the three rslc translation units.  Not installed, not
 * part of the ABI.
 *
 * Memory model: one bump arena per compile holds every intermediate
 * (tokens, IR, intern table, reflection staging).  Nothing in it is
 * freed individually; it dies with the compile.  The only other
 * allocation is the output blob, which the caller owns.  The arena is
 * either a block the caller handed in (fixed: fail when it runs out)
 * or a chain of RSLC_ARENA_CHUNK chunks from the caller's allocator
 * under opts->arena_cap.  The implementations live in rslc_ir.c so
 * each TU carries one copy of the code, not three. */

#include <gfx/shaders/rslc.h>

RETRO_BEGIN_DECLS

/* The largest alignment any arena object needs. */
#define RSLC_ARENA_ALIGN 8

struct rslc_chunk
{
   struct rslc_chunk *next;
   size_t             len;    /* usable bytes after this header */
};

struct rslc_arena
{
   struct rslc_chunk *chunks; /* newest first; NULL with a fixed block */
   struct rslc_chunk *spare;  /* released chunks kept for reuse (scratch) */
   uint8_t           *cur;
   uint8_t           *end;
   size_t             used;   /* bytes handed out, all chunks */
   size_t             held;   /* bytes allocated from the caller */
   size_t             cap;
   bool               fixed;
};

/* A point in a scratch arena to release back to. */
struct rslc_mark
{
   struct rslc_chunk *chunk;
   uint8_t           *cur;
   size_t             used;
};

/* Interned identifier.  One record per distinct spelling, allocated
 * once in the arena and never moved, so a pointer to it is the
 * identifier: tokens, macros and symbols compare by address.  kw is
 * the keyword code when the spelling is one, RSLC_KW_NONE otherwise;
 * macro is the current #define, if any. */
struct rslc_sym
{
   const char        *str;
   struct rslc_macro *macro;
   struct rslc_decl  *decl;    /* innermost visible declaration */
   uint32_t           hash;
   uint32_t           len;
   uint32_t           index;   /* position in ctx->syms */
   uint16_t           kw;
   uint16_t           flags;   /* RSLC_SYM_* */
   uint16_t           bfn;     /* enum rslc_bfn when a builtin function name */
   uint16_t           pad;
};

#define RSLC_SYM_BUSY     1u   /* macro being expanded: do not re-expand */

struct rslc_intern
{
   struct rslc_sym **slots;
   uint32_t          mask;   /* slot count - 1 */
   uint32_t          count;
};

/* Output buffer that becomes the result blob.  The backend reserves
 * the header and reflection section by starting len at that size and
 * sizes the whole buffer from the IR up front, so the blob is one
 * allocation and the buffer is the blob: no final copy.  Growth by
 * doubling exists for a misestimate only; it counts as a defect when
 * it fires on a corpus shader. */
struct rslc_pbuf
{
   uint8_t *data;
   size_t   len;
   size_t   cap;
   unsigned grows;
};

/* Tokens.  Sixteen bytes: a stage expands to hundreds of thousands of
 * them in the worst corpus shaders.  ref is the sym index for an
 * identifier, else the byte offset of the spelling in the source, or
 * in ctx->synth when RSLC_TF_SYNTH is set (text the preprocessor made
 * up: __LINE__, defined()).  rslc_tok_text / rslc_tok_sym resolve it.
 * The preprocessor leaves the stream in 4096-token chunks
 * (ctx->tok_chunks); rslc_tok_at indexes them. */
enum rslc_tk
{
   RSLC_TK_EOF = 0,
   RSLC_TK_IDENT,
   RSLC_TK_NUMBER,
   RSLC_TK_STRING,     /* "..." - only #line uses it */
   RSLC_TK_PUNCT,      /* kind in punct, see RSLC_P_* */
   RSLC_TK_NEWLINE,    /* preprocessing only; never reaches the parser */
   RSLC_TK_OTHER       /* a byte the language does not use */
};

/* Punctuators, longest match first in the lexer table. */
enum rslc_punct
{
   RSLC_P_NONE = 0,
   RSLC_P_LSHIFT_ASSIGN, RSLC_P_RSHIFT_ASSIGN,
   RSLC_P_LSHIFT, RSLC_P_RSHIFT, RSLC_P_INC, RSLC_P_DEC,
   RSLC_P_LE, RSLC_P_GE, RSLC_P_EQ, RSLC_P_NE,
   RSLC_P_AND, RSLC_P_OR, RSLC_P_XOR,
   RSLC_P_ADD_ASSIGN, RSLC_P_SUB_ASSIGN, RSLC_P_MUL_ASSIGN,
   RSLC_P_DIV_ASSIGN, RSLC_P_MOD_ASSIGN, RSLC_P_BAND_ASSIGN,
   RSLC_P_BOR_ASSIGN, RSLC_P_BXOR_ASSIGN,
   RSLC_P_LPAREN, RSLC_P_RPAREN, RSLC_P_LBRACKET, RSLC_P_RBRACKET,
   RSLC_P_LBRACE, RSLC_P_RBRACE, RSLC_P_DOT, RSLC_P_COMMA,
   RSLC_P_COLON, RSLC_P_SEMI, RSLC_P_ASSIGN, RSLC_P_PLUS, RSLC_P_MINUS,
   RSLC_P_STAR, RSLC_P_SLASH, RSLC_P_PERCENT, RSLC_P_LT, RSLC_P_GT,
   RSLC_P_BANG, RSLC_P_TILDE, RSLC_P_AMP, RSLC_P_PIPE, RSLC_P_CARET,
   RSLC_P_QUESTION, RSLC_P_HASH, RSLC_P_PASTE,
   RSLC_P_COUNT
};

#define RSLC_TF_BOL      1u  /* first token on its line */
#define RSLC_TF_SPACE    2u  /* whitespace precedes it */
#define RSLC_TF_NOEXPAND 4u  /* identifier painted: never a macro again */

#define RSLC_TF_SYNTH    8u  /* spelling lives in ctx->synth, not the source */

struct rslc_tok
{
   uint32_t ref;
   uint32_t line;
   uint16_t len;
   uint8_t  kind;    /* enum rslc_tk */
   uint8_t  punct;   /* enum rslc_punct when kind is PUNCT */
   uint8_t  flags;   /* RSLC_TF_* */
   uint8_t  file;    /* index into ctx files, from #line */
   uint16_t pad;
};

#define RSLC_TOK_CHUNK_SHIFT 12
#define RSLC_TOK_CHUNK       (1u << RSLC_TOK_CHUNK_SHIFT)
#define rslc_tok_at(c, i) \
   (&(c)->tok_chunks[(i) >> RSLC_TOK_CHUNK_SHIFT][(i) & (RSLC_TOK_CHUNK - 1)])

/* A #define.  Parameters and body are token arrays in the arena; a
 * parameter reference in the body is an IDENT whose sym is one of the
 * params. */
struct rslc_macro
{
   struct rslc_sym  *name;
   struct rslc_sym **params;
   struct rslc_tok  *body;
   uint32_t          nparams;
   uint32_t          nbody;
   bool              function_like;
};

/* Keyword codes, assigned to syms when the context is seeded. */
enum rslc_kw
{
   RSLC_KW_NONE = 0,
   /* storage and qualifiers */
   RSLC_KW_CONST, RSLC_KW_IN, RSLC_KW_OUT, RSLC_KW_INOUT, RSLC_KW_UNIFORM,
   RSLC_KW_FLAT, RSLC_KW_SMOOTH, RSLC_KW_NOPERSPECTIVE, RSLC_KW_CENTROID,
   RSLC_KW_SAMPLE, RSLC_KW_INVARIANT, RSLC_KW_PRECISE, RSLC_KW_LAYOUT,
   RSLC_KW_PRECISION, RSLC_KW_LOWP, RSLC_KW_MEDIUMP, RSLC_KW_HIGHP,
   RSLC_KW_STRUCT,
   /* control */
   RSLC_KW_IF, RSLC_KW_ELSE, RSLC_KW_FOR, RSLC_KW_WHILE, RSLC_KW_DO,
   RSLC_KW_SWITCH, RSLC_KW_CASE, RSLC_KW_DEFAULT, RSLC_KW_BREAK,
   RSLC_KW_CONTINUE, RSLC_KW_RETURN, RSLC_KW_DISCARD,
   /* literals */
   RSLC_KW_TRUE, RSLC_KW_FALSE,
   /* types: the order within each family is width 1..4 */
   RSLC_KW_VOID, RSLC_KW_BOOL, RSLC_KW_INT, RSLC_KW_UINT, RSLC_KW_FLOAT,
   RSLC_KW_BVEC2, RSLC_KW_BVEC3, RSLC_KW_BVEC4,
   RSLC_KW_IVEC2, RSLC_KW_IVEC3, RSLC_KW_IVEC4,
   RSLC_KW_UVEC2, RSLC_KW_UVEC3, RSLC_KW_UVEC4,
   RSLC_KW_VEC2, RSLC_KW_VEC3, RSLC_KW_VEC4,
   RSLC_KW_MAT2, RSLC_KW_MAT3, RSLC_KW_MAT4,
   RSLC_KW_MAT2X2, RSLC_KW_MAT2X3, RSLC_KW_MAT2X4,
   RSLC_KW_MAT3X2, RSLC_KW_MAT3X3, RSLC_KW_MAT3X4,
   RSLC_KW_MAT4X2, RSLC_KW_MAT4X3, RSLC_KW_MAT4X4,
   RSLC_KW_SAMPLER2D, RSLC_KW_ISAMPLER2D, RSLC_KW_USAMPLER2D,
   RSLC_KW_COUNT
};

/* ------------------------------------------------------------------ */
/* IR                                                                  */
/* ------------------------------------------------------------------ */

/* Types are interned: equal shapes are one record, compared by
 * address.  Structs are distinct per definition.  rows is the vector
 * width (1 for a scalar) or the matrix row count; cols the matrix
 * column count (1 otherwise).  GLSL matN is N columns of vecN; matCxR
 * is C columns of vecR. */
enum rslc_tkind
{
   RSLC_TY_VOID = 0,
   RSLC_TY_SCALAR,
   RSLC_TY_VEC,
   RSLC_TY_MAT,
   RSLC_TY_SAMPLER2D,
   RSLC_TY_ARRAY,
   RSLC_TY_STRUCT
};

struct rslc_type;

struct rslc_field
{
   const struct rslc_type *type;
   struct rslc_sym        *name;
   uint32_t                line;
   uint8_t                 precision;   /* RSLC_PREC_* */
   uint8_t                 pad[3];
};

struct rslc_type
{
   const struct rslc_type  *elem;      /* ARRAY element */
   const struct rslc_field *fields;    /* STRUCT */
   struct rslc_sym         *name;      /* STRUCT */
   struct rslc_type        *next;      /* intern chain */
   uint32_t                 id;
   uint32_t                 array_len; /* ARRAY; 0 = unsized */
   uint32_t                 nfields;
   uint8_t                  kind;      /* enum rslc_tkind */
   uint8_t                  base;      /* enum rslc_base */
   uint8_t                  rows;
   uint8_t                  cols;
};

#define RSLC_PREC_NONE   0
#define RSLC_PREC_LOW    1
#define RSLC_PREC_MEDIUM 2
#define RSLC_PREC_HIGH   3

enum rslc_storage
{
   RSLC_ST_LOCAL = 0,
   RSLC_ST_GLOBAL,
   RSLC_ST_CONST,       /* global or local const with a constant init */
   RSLC_ST_IN,
   RSLC_ST_OUT,
   RSLC_ST_PARAM,       /* function parameter; dir says in/out/inout */
   RSLC_ST_SAMPLER,     /* uniform sampler2D */
   RSLC_ST_BLOCK,       /* the instance of a named uniform / push block */
   RSLC_ST_MEMBER,      /* a member of an unnamed block, visible bare */
   RSLC_ST_BUILTIN      /* gl_Position and friends */
};

#define RSLC_DIR_IN    0
#define RSLC_DIR_OUT   1
#define RSLC_DIR_INOUT 2

#define RSLC_INTERP_NONE          0
#define RSLC_INTERP_FLAT          1
#define RSLC_INTERP_NOPERSPECTIVE 2
#define RSLC_INTERP_SMOOTH        3

#define RSLC_VQ_CENTROID  1u
#define RSLC_VQ_SAMPLE    2u
#define RSLC_VQ_INVARIANT 4u
#define RSLC_VQ_PRECISE   8u

#define RSLC_UNSET 0xffffffffu

struct rslc_var
{
   const struct rslc_type *type;
   struct rslc_sym        *name;
   uint32_t                id;
   uint32_t                init;       /* node, 0 = none */
   uint32_t                line;
   uint32_t                location;   /* RSLC_UNSET when absent */
   uint32_t                binding;
   uint32_t                set;
   uint32_t                block;      /* RSLC_ST_MEMBER / BLOCK: block index */
   uint32_t                member;     /* RSLC_ST_MEMBER: field index */
   uint8_t                 storage;    /* enum rslc_storage */
   uint8_t                 dir;        /* RSLC_DIR_* for params */
   uint8_t                 interp;
   uint8_t                 precision;
   uint8_t                 quals;      /* RSLC_VQ_* */
   uint8_t                 builtin;    /* RSLC_BI_* for RSLC_ST_BUILTIN */
   uint8_t                 global;     /* declared at file scope */
   uint8_t                 pad;
};

/* Builtin variables. */
#define RSLC_BI_NONE         0
#define RSLC_BI_POSITION     1
#define RSLC_BI_FRAGCOORD    2
#define RSLC_BI_FRAGDEPTH    3
#define RSLC_BI_VERTEXINDEX  4
#define RSLC_BI_POINTSIZE    5
#define RSLC_BI_FRONTFACING  6
#define RSLC_BI_INSTANCEINDEX 7

/* A uniform block or push-constant block. */
struct rslc_blockdecl
{
   const struct rslc_type *type;      /* the struct of its members */
   struct rslc_sym        *instance;  /* NULL for an unnamed block */
   uint32_t                binding;   /* RSLC_UNSET when absent */
   uint32_t                set;
   uint32_t                line;
   bool                    push;
   bool                    std140;    /* layout; push blocks default std430 */
};

struct rslc_func
{
   struct rslc_sym        *name;
   const struct rslc_type *ret;
   struct rslc_var       **params;
   struct rslc_func       *next;      /* next overload of the same name */
   uint32_t                nparams;
   uint32_t                body;      /* BLOCK node; 0 for a prototype */
   uint32_t                id;
   uint32_t                line;
};

/* Nodes.  Expressions carry their type after typecheck; statements
 * carry NULL.  a, b, c are node ids or list ids as the op says; 0 is
 * "none" (node 0 is a sentinel).  Lists are in ctx->lists: lists[id]
 * is the count, followed by the items. */
enum rslc_op
{
   RSLC_OP_NONE = 0,
   /* expressions */
   RSLC_OP_CONST,      /* aux unused; a = value bits (int/uint/float/bool) */
   RSLC_OP_VAR,        /* a = var id */
   RSLC_OP_MEMBER,     /* a = struct expr, b = field index */
   RSLC_OP_INDEX,      /* a = aggregate expr, b = index expr */
   RSLC_OP_SWIZZLE,    /* a = expr, aux = count | idx0<<2 | idx1<<4 | ... */
   RSLC_OP_CALL,       /* a = func id, b = arg list */
   RSLC_OP_BUILTIN,    /* a = builtin fn id, b = arg list */
   RSLC_OP_CONSTRUCT,  /* type = target, b = arg list */
   RSLC_OP_UNARY,      /* aux = RSLC_UN_*, a = operand */
   RSLC_OP_BINARY,     /* aux = RSLC_BIN_*, a, b */
   RSLC_OP_ASSIGN,     /* aux = RSLC_BIN_* or RSLC_BIN_NONE, a = lvalue, b = rhs */
   RSLC_OP_TERNARY,    /* a = cond, b, c */
   RSLC_OP_SEQ,        /* a, b (comma) */
   RSLC_OP_CONVERT,    /* type = target, a = expr (implicit conversion) */
   /* statements */
   RSLC_OP_BLOCK,      /* b = stmt list; aux RSLC_BLOCK_GROUP: no scope of its own */
   RSLC_OP_DECL,       /* a = var id (init in the var) */
   RSLC_OP_EXPR,       /* a = expr */
   RSLC_OP_IF,         /* a = cond, b = then, c = else or 0 */
   RSLC_OP_FOR,        /* b = list [init stmt|0, cond|0, iter|0, body] */
   RSLC_OP_WHILE,      /* a = cond, b = body */
   RSLC_OP_DO,         /* a = body, b = cond */
   RSLC_OP_SWITCH,     /* a = expr, b = stmt list with CASE nodes inside */
   RSLC_OP_CASE,       /* a = const expr, or 0 for default */
   RSLC_OP_BREAK,
   RSLC_OP_CONTINUE,
   RSLC_OP_RETURN,     /* a = expr or 0 */
   RSLC_OP_DISCARD
};

#define RSLC_BLOCK_GROUP 1   /* BLOCK aux: several declarators of one declaration */

#define RSLC_UN_NEG     1
#define RSLC_UN_NOT     2
#define RSLC_UN_BITNOT  3
#define RSLC_UN_PREINC  4
#define RSLC_UN_PREDEC  5
#define RSLC_UN_POSTINC 6
#define RSLC_UN_POSTDEC 7

#define RSLC_BIN_NONE   0
#define RSLC_BIN_ADD    1
#define RSLC_BIN_SUB    2
#define RSLC_BIN_MUL    3
#define RSLC_BIN_DIV    4
#define RSLC_BIN_MOD    5
#define RSLC_BIN_LSHIFT 6
#define RSLC_BIN_RSHIFT 7
#define RSLC_BIN_LT     8
#define RSLC_BIN_GT     9
#define RSLC_BIN_LE     10
#define RSLC_BIN_GE     11
#define RSLC_BIN_EQ     12
#define RSLC_BIN_NE     13
#define RSLC_BIN_AND    14
#define RSLC_BIN_OR     15
#define RSLC_BIN_XOR    16
#define RSLC_BIN_BAND   17
#define RSLC_BIN_BOR    18
#define RSLC_BIN_BXOR   19

struct rslc_node
{
   const struct rslc_type *type;
   uint32_t                a;
   uint32_t                b;
   uint32_t                c;
   uint32_t                line;
   uint16_t                op;    /* enum rslc_op */
   uint16_t                aux;
   uint8_t                 prec;  /* RSLC_PREC_* of an expression's result */
   uint8_t                 nonconstant;
   uint8_t                 pad[2];
};

#define RSLC_NODE_CHUNK_SHIFT 12
#define RSLC_NODE_CHUNK       (1u << RSLC_NODE_CHUNK_SHIFT)
#define rslc_node_at(c, i) \
   (&(c)->node_chunks[(i) >> RSLC_NODE_CHUNK_SHIFT][(i) & (RSLC_NODE_CHUNK - 1)])

/* Builtin functions, by id.  The signature table is in rslc_front.c;
 * the emitters map each to GLSL.std.450 or a core op. */
enum rslc_bfn
{
   RSLC_BFN_NONE = 0,
   RSLC_BFN_RADIANS, RSLC_BFN_DEGREES, RSLC_BFN_SIN, RSLC_BFN_COS, RSLC_BFN_TAN,
   RSLC_BFN_ASIN, RSLC_BFN_ACOS, RSLC_BFN_ATAN, RSLC_BFN_SINH, RSLC_BFN_COSH,
   RSLC_BFN_TANH, RSLC_BFN_ASINH, RSLC_BFN_ACOSH, RSLC_BFN_ATANH,
   RSLC_BFN_POW, RSLC_BFN_EXP, RSLC_BFN_LOG, RSLC_BFN_EXP2, RSLC_BFN_LOG2,
   RSLC_BFN_SQRT, RSLC_BFN_INVERSESQRT,
   RSLC_BFN_ABS, RSLC_BFN_SIGN, RSLC_BFN_FLOOR, RSLC_BFN_TRUNC, RSLC_BFN_ROUND,
   RSLC_BFN_ROUNDEVEN, RSLC_BFN_CEIL, RSLC_BFN_FRACT, RSLC_BFN_MOD, RSLC_BFN_MODF,
   RSLC_BFN_MIN, RSLC_BFN_MAX, RSLC_BFN_CLAMP, RSLC_BFN_MIX, RSLC_BFN_STEP,
   RSLC_BFN_SMOOTHSTEP, RSLC_BFN_ISNAN, RSLC_BFN_ISINF,
   RSLC_BFN_FLOATBITSTOINT, RSLC_BFN_FLOATBITSTOUINT, RSLC_BFN_INTBITSTOFLOAT,
   RSLC_BFN_UINTBITSTOFLOAT, RSLC_BFN_FMA, RSLC_BFN_FREXP, RSLC_BFN_LDEXP,
   RSLC_BFN_PACKUNORM2X16, RSLC_BFN_PACKSNORM2X16, RSLC_BFN_PACKUNORM4X8,
   RSLC_BFN_PACKSNORM4X8, RSLC_BFN_UNPACKUNORM2X16, RSLC_BFN_UNPACKSNORM2X16,
   RSLC_BFN_UNPACKUNORM4X8, RSLC_BFN_UNPACKSNORM4X8, RSLC_BFN_PACKHALF2X16,
   RSLC_BFN_UNPACKHALF2X16,
   RSLC_BFN_LENGTH, RSLC_BFN_DISTANCE, RSLC_BFN_DOT, RSLC_BFN_CROSS,
   RSLC_BFN_NORMALIZE, RSLC_BFN_FACEFORWARD, RSLC_BFN_REFLECT, RSLC_BFN_REFRACT,
   RSLC_BFN_MATRIXCOMPMULT, RSLC_BFN_OUTERPRODUCT, RSLC_BFN_TRANSPOSE,
   RSLC_BFN_DETERMINANT, RSLC_BFN_INVERSE,
   RSLC_BFN_LESSTHAN, RSLC_BFN_LESSTHANEQUAL, RSLC_BFN_GREATERTHAN,
   RSLC_BFN_GREATERTHANEQUAL, RSLC_BFN_EQUAL, RSLC_BFN_NOTEQUAL, RSLC_BFN_ANY,
   RSLC_BFN_ALL, RSLC_BFN_NOT,
   RSLC_BFN_UADDCARRY, RSLC_BFN_USUBBORROW, RSLC_BFN_UMULEXTENDED,
   RSLC_BFN_IMULEXTENDED, RSLC_BFN_BITFIELDEXTRACT, RSLC_BFN_BITFIELDINSERT,
   RSLC_BFN_BITFIELDREVERSE, RSLC_BFN_BITCOUNT, RSLC_BFN_FINDLSB, RSLC_BFN_FINDMSB,
   RSLC_BFN_TEXTURESIZE, RSLC_BFN_TEXTUREQUERYLOD, RSLC_BFN_TEXTUREQUERYLEVELS,
   RSLC_BFN_TEXTURE, RSLC_BFN_TEXTUREPROJ, RSLC_BFN_TEXTURELOD,
   RSLC_BFN_TEXTUREOFFSET, RSLC_BFN_TEXELFETCH, RSLC_BFN_TEXELFETCHOFFSET,
   RSLC_BFN_TEXTUREPROJOFFSET, RSLC_BFN_TEXTURELODOFFSET, RSLC_BFN_TEXTUREPROJLOD,
   RSLC_BFN_TEXTUREPROJLODOFFSET, RSLC_BFN_TEXTUREGRAD, RSLC_BFN_TEXTUREGRADOFFSET,
   RSLC_BFN_TEXTUREPROJGRAD, RSLC_BFN_TEXTUREPROJGRADOFFSET, RSLC_BFN_TEXTUREGATHER,
   RSLC_BFN_TEXTUREGATHEROFFSET,
   RSLC_BFN_DFDX, RSLC_BFN_DFDY, RSLC_BFN_FWIDTH, RSLC_BFN_DFDXFINE, RSLC_BFN_DFDYFINE,
   RSLC_BFN_FWIDTHFINE, RSLC_BFN_DFDXCOARSE, RSLC_BFN_DFDYCOARSE, RSLC_BFN_FWIDTHCOARSE,
   RSLC_BFN_COUNT
};

/* The parsed stage. */
struct rslc_prog
{
   struct rslc_var       **vars;      /* every variable, by id (1-based) */
   struct rslc_func      **funcs;     /* every function, by id (1-based) */
   struct rslc_type      **structs;   /* struct types in declaration order */
   struct rslc_var       **samplers;
   struct rslc_var       **inputs;    /* stage in, by declaration order */
   struct rslc_var       **outputs;
   struct rslc_func       *main_fn;
   struct rslc_blockdecl   blocks[2]; /* [0] uniform block, [1] push block */
   uint32_t                nvars, vars_cap;
   uint32_t                nfuncs, funcs_cap;
   uint32_t                nstructs, structs_cap;
   uint32_t                nsamplers, samplers_cap;
   uint32_t                ninputs, inputs_cap;
   uint32_t                noutputs, outputs_cap;
   bool                    has_block[2];
};

/* Source file names seen through #line, for diagnostics. */
#define RSLC_MAX_FILES 64

/* Per-compile state shared by the three TUs. */
struct rslc_ctx
{
   const struct rslc_opts *opts;
   struct rslc_arena       arena;      /* lives for the compile */
   struct rslc_arena       scratch;    /* mark / release transients */
   struct rslc_intern      intern;
   struct rslc_sym       **syms;       /* dense, by sym->index */
   struct rslc_tok       **tok_chunks; /* preprocessed stream, EOF-terminated */
   struct rslc_node      **node_chunks;
   uint32_t               *lists;      /* node lists, see enum rslc_op */
   struct rslc_type       *type_chain; /* interned types */
   struct rslc_prog        prog;
   const char             *src;        /* the stage source, for token text */
   char                   *synth;      /* preprocessor-made spellings */
   const char             *files[RSLC_MAX_FILES];
   uint32_t                nsyms;
   uint32_t                syms_cap;
   uint32_t                ntoks;
   uint32_t                ntok_chunks;
   uint32_t                nnodes;
   uint32_t                nnode_chunks;
   uint32_t                nlists;
   uint32_t                lists_cap;
   uint32_t                ntypes;
   uint32_t                nsynth;
   uint32_t                synth_cap;
   uint32_t                nfiles;
   uint32_t                version;    /* from #version */
   uint8_t                 err_file;   /* files[] index named in the next error */
   enum rslc_stage         stage;
   struct rslc_parser     *parser;     /* the front end's, for the remap */
   enum rslc_remap_result  remap;      /* what opts.remap did */
   bool                    es;         /* #version ... es */
   bool                    failed;
};

/* Allocator wrappers: opts->alloc / free_fn or malloc / free. */
void *rslc_mem_alloc(const struct rslc_opts *opts, size_t len);
void  rslc_mem_free(const struct rslc_opts *opts, void *ptr);

/* Context lifetime.  rslc_ctx_init validates opts, attaches or opens
 * the arena and seeds the intern table with the keywords;
 * rslc_ctx_close releases arena chunks (never a caller-supplied
 * block). */
bool  rslc_ctx_init(struct rslc_ctx *c, const struct rslc_opts *opts,
      enum rslc_stage stage);
void  rslc_ctx_close(struct rslc_ctx *c);

/* Arena.  Returns NULL and marks the compile failed when the cap or
 * the fixed block is exhausted; callers propagate NULL upward.  The
 * scratch arena shares the cap with the main one and gives memory
 * back on release, keeping its chunks for the next mark. */
void *rslc_arena_alloc(struct rslc_ctx *c, size_t len);
void *rslc_arena_zalloc(struct rslc_ctx *c, size_t len);
void *rslc_scratch_alloc(struct rslc_ctx *c, size_t len);
void  rslc_scratch_mark(struct rslc_ctx *c, struct rslc_mark *m);
void  rslc_scratch_release(struct rslc_ctx *c, const struct rslc_mark *m);

/* Token text and symbol. */
const char      *rslc_tok_text(const struct rslc_ctx *c, const struct rslc_tok *t);
struct rslc_sym *rslc_tok_sym(const struct rslc_ctx *c, const struct rslc_tok *t);

/* Intern.  rslc_sym returns the record for a spelling, creating it on
 * first sight; rslc_intern is the string of that record.  NULL on
 * arena exhaustion. */
struct rslc_sym *rslc_sym(struct rslc_ctx *c, const char *s, size_t len);
const char *rslc_intern(struct rslc_ctx *c, const char *s, size_t len);
const char *rslc_intern_cstr(struct rslc_ctx *c, const char *s);
uint32_t    rslc_hash(const char *s, size_t len);

/* Print buffer.  rslc_pbuf_init allocates prefix + hint bytes once;
 * rslc_pbuf_reserve guarantees extra free bytes; rslc_pbuf_put /
 * _putc append.  All return false and mark the compile failed when
 * the allocator refuses.  The buffer is released by rslc_pbuf_free, or
 * detached into a result by the backend.  grows counts the times the
 * hint was wrong. */
bool  rslc_pbuf_init(struct rslc_ctx *c, struct rslc_pbuf *b,
      size_t prefix, size_t hint);
bool  rslc_pbuf_reserve(struct rslc_ctx *c, struct rslc_pbuf *b, size_t extra);
bool  rslc_pbuf_put(struct rslc_ctx *c, struct rslc_pbuf *b,
      const void *src, size_t len);
bool  rslc_pbuf_puts(struct rslc_ctx *c, struct rslc_pbuf *b, const char *s);
bool  rslc_pbuf_putc(struct rslc_ctx *c, struct rslc_pbuf *b, char ch);
bool  rslc_pbuf_putu(struct rslc_ctx *c, struct rslc_pbuf *b, uint32_t v);
void  rslc_pbuf_free(struct rslc_ctx *c, struct rslc_pbuf *b);

/* Error sink.  Writes "a" then "b" then, when line is non-zero,
 * " (line N)" into opts->err, truncating to err_cap, and marks the
 * compile failed.  Only the first message of a compile is kept.  No
 * stdio anywhere in the compiler. */
void  rslc_err(struct rslc_ctx *c, const char *a, const char *b,
      unsigned line);

/* Preprocessor (TU 1).  Lexes src and runs the directives and macros
 * over it, leaving c->toks / c->ntoks as the parser's input: IDENT,
 * NUMBER, PUNCT and a final EOF, with the #version and #extension
 * lines consumed into c->version / c->es and the extension list.
 * Returns false with the error set. */
bool  rslc_pp_run(struct rslc_ctx *c, const char *src, size_t len);

/* IR construction (TU 2).  Types are interned; node 0 and list 0 are
 * reserved sentinels. */
const struct rslc_type *rslc_type_basic(struct rslc_ctx *c, enum rslc_tkind kind,
      enum rslc_base base, unsigned rows, unsigned cols);
const struct rslc_type *rslc_type_array(struct rslc_ctx *c,
      const struct rslc_type *elem, uint32_t len);
struct rslc_type *rslc_type_struct(struct rslc_ctx *c, struct rslc_sym *name,
      const struct rslc_field *fields, uint32_t nfields);
const struct rslc_type *rslc_type_sampler2d(struct rslc_ctx *c, enum rslc_base base);
uint32_t rslc_node_new(struct rslc_ctx *c, enum rslc_op op,
      const struct rslc_type *type, uint32_t a, uint32_t b, uint32_t c_,
      uint16_t aux, uint32_t line);
uint32_t rslc_list_new(struct rslc_ctx *c, const uint32_t *items, uint32_t n);
/* Constant folding over the IR, for TU 2: out->v holds component
 * bits, out->type the folded type.  False when not a constant. */
struct rslc_cval
{
   const struct rslc_type *type;
   uint32_t                v[16];   /* component bits, floats rounded */
   double                  d[16];   /* float components, as folded */
};
bool     rslc_const_eval(struct rslc_ctx *c, uint32_t node, struct rslc_cval *out);
unsigned rslc_ty_components(const struct rslc_type *t);
const char *rslc_bfn_name(unsigned id);

/* The frame-rectangle remap (TU 3, opts.remap): run by the front end
 * on the parsed stage, it sets c->remap and is false only when the
 * compile fails.  The front end lends it three things: a global
 * variable bound by name, an unbound copy of a variable, and more
 * declarations parsed from text at file scope. */
bool rslc_remap_run(struct rslc_ctx *c);
struct rslc_var *rslc_front_global(struct rslc_ctx *c, struct rslc_sym *name,
      const struct rslc_type *type);
struct rslc_var *rslc_front_var_copy(struct rslc_ctx *c,
      const struct rslc_var *src);
bool rslc_front_parse(struct rslc_ctx *c, const char *src, size_t len);
#define rslc_list_len(c, id)     ((c)->lists[(id)])
#define rslc_list_item(c, id, i) ((c)->lists[(id) + 1 + (i)])

/* Entry points across the TUs.  rslc_front_compile (TU 1) builds the
 * IR; rslc_emit_dst (TU 2) fills result for opts->dst, handling SPIR-V
 * and reflection itself and calling rslc_emit_text (TU 3) for the
 * printed languages unless RSLC_NO_TEXT is defined. */
bool  rslc_front_compile(struct rslc_ctx *c, const char *src, size_t len);
bool  rslc_emit_dst(struct rslc_ctx *c, struct rslc_result *result);
bool  rslc_emit_text(struct rslc_ctx *c, struct rslc_result *result);

/* Shared by the backends (TU 2): which functions main reaches, the
 * reflection section, and the blob around a payload. */
void     rslc_mark_reach(struct rslc_ctx *c, uint32_t node, uint8_t *reach);
void     rslc_reach_funcs(struct rslc_ctx *c, uint8_t *reach);
void     rslc_reach_vars(struct rslc_ctx *c, const uint8_t *reach, uint8_t *used);
uint32_t rslc_reflect_len(struct rslc_ctx *c);
/* Base alignment, size and (array / matrix) stride of a type under
 * std140 or std430. */
void rslc_layout_of_type(const struct rslc_type *t, bool std140,
      uint32_t *align, uint32_t *size, uint32_t *stride);
uint32_t rslc_layout_field_offset(const struct rslc_type *t, uint32_t field,
      bool std140);
uint32_t rslc_reflect_write(struct rslc_ctx *c, uint8_t *blob, uint32_t off,
      uint8_t *const active[2], const uint8_t *used);
bool     rslc_blob_begin(struct rslc_ctx *c, struct rslc_pbuf *pb, size_t payload_hint,
      uint8_t *const active[2], const uint8_t *used);
void     rslc_blob_end(struct rslc_ctx *c, struct rslc_pbuf *pb, enum rslc_dst dst,
      struct rslc_result *result);

RETRO_END_DECLS

#endif
