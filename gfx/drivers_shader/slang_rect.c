/*  RetroArch - A frontend for libretro.
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

/* See slang_rect.h. The stage is read once to learn its types, names,
 * blocks and functions; its functions are then written again, the
 * frame's reads replaced, into a buffer of their own, with whatever new
 * types and constants that needs going into another; and the module is
 * put back together around them. A helper function that is handed the
 * frame at one call and another texture at the next is written twice,
 * once for each. */

#include <stdlib.h>
#include <string.h>

#include "slang_rect.h"

/* The SPIR-V this needs, as numbered in the specification. */
enum
{
   SRECT_OP_NOP                  = 0,
   SRECT_OP_UNDEF                = 1,
   SRECT_OP_SOURCE_CONTINUED     = 2,
   SRECT_OP_SOURCE               = 3,
   SRECT_OP_SOURCE_EXTENSION     = 4,
   SRECT_OP_NAME                 = 5,
   SRECT_OP_MEMBER_NAME          = 6,
   SRECT_OP_STRING               = 7,
   SRECT_OP_LINE                 = 8,
   SRECT_OP_EXTENSION            = 10,
   SRECT_OP_EXT_INST_IMPORT      = 11,
   SRECT_OP_EXT_INST             = 12,
   SRECT_OP_MEMORY_MODEL         = 14,
   SRECT_OP_ENTRY_POINT          = 15,
   SRECT_OP_EXECUTION_MODE       = 16,
   SRECT_OP_CAPABILITY           = 17,
   SRECT_OP_TYPE_BOOL            = 20,
   SRECT_OP_TYPE_INT             = 21,
   SRECT_OP_TYPE_FLOAT           = 22,
   SRECT_OP_TYPE_VECTOR          = 23,
   SRECT_OP_TYPE_MATRIX          = 24,
   SRECT_OP_TYPE_IMAGE           = 25,
   SRECT_OP_TYPE_SAMPLED_IMAGE   = 27,
   SRECT_OP_TYPE_ARRAY           = 28,
   SRECT_OP_TYPE_STRUCT          = 30,
   SRECT_OP_TYPE_POINTER         = 32,
   SRECT_OP_CONSTANT             = 43,
   SRECT_OP_CONSTANT_COMPOSITE   = 44,
   SRECT_OP_FUNCTION             = 54,
   SRECT_OP_FUNCTION_PARAMETER   = 55,
   SRECT_OP_FUNCTION_END         = 56,
   SRECT_OP_FUNCTION_CALL        = 57,
   SRECT_OP_VARIABLE             = 59,
   SRECT_OP_LOAD                 = 61,
   SRECT_OP_STORE                = 62,
   SRECT_OP_COPY_MEMORY          = 63,
   SRECT_OP_ACCESS_CHAIN         = 65,
   SRECT_OP_DECORATE             = 71,
   SRECT_OP_MEMBER_DECORATE      = 72,
   SRECT_OP_VECTOR_EXTRACT_DYN   = 77,
   SRECT_OP_VECTOR_SHUFFLE       = 79,
   SRECT_OP_COMPOSITE_CONSTRUCT  = 80,
   SRECT_OP_COMPOSITE_EXTRACT    = 81,
   SRECT_OP_COMPOSITE_INSERT     = 82,
   SRECT_OP_COPY_OBJECT          = 83,
   SRECT_OP_SAMPLED_IMAGE        = 86,
   SRECT_OP_SAMPLE_IMPLICIT      = 87,
   SRECT_OP_SAMPLE_EXPLICIT      = 88,
   SRECT_OP_SAMPLE_DREF_IMPLICIT = 89,
   SRECT_OP_SAMPLE_DREF_EXPLICIT = 90,
   SRECT_OP_SAMPLE_PROJ_IMPLICIT = 91,
   SRECT_OP_SAMPLE_PROJ_EXPLICIT = 92,
   SRECT_OP_SAMPLE_PROJ_DREF_IMP = 93,
   SRECT_OP_SAMPLE_PROJ_DREF_EXP = 94,
   SRECT_OP_IMAGE_FETCH          = 95,
   SRECT_OP_IMAGE_GATHER         = 96,
   SRECT_OP_IMAGE_DREF_GATHER    = 97,
   SRECT_OP_IMAGE_READ           = 98,
   SRECT_OP_IMAGE_WRITE          = 99,
   SRECT_OP_IMAGE                = 100,
   SRECT_OP_QUERY_SIZE_LOD       = 103,
   SRECT_OP_QUERY_SIZE           = 104,
   SRECT_OP_QUERY_LEVELS         = 106,
   SRECT_OP_CONVERT_F_TO_S       = 110,
   SRECT_OP_CONVERT_S_TO_F       = 111,
   SRECT_OP_IADD                 = 128,
   SRECT_OP_FADD                 = 129,
   SRECT_OP_ISUB                 = 130,
   SRECT_OP_IMUL                 = 132,
   SRECT_OP_FSUB                 = 131,
   SRECT_OP_FMUL                 = 133,
   SRECT_OP_FDIV                 = 136,
   SRECT_OP_ALL                  = 155,
   SRECT_OP_LOGICAL_AND          = 167,
   SRECT_OP_SELECT               = 169,
   SRECT_OP_SGREATER_EQUAL       = 175,
   SRECT_OP_SLESS                = 177,
   SRECT_OP_FORD_LESS_THAN       = 184,
   SRECT_OP_PHI                  = 245,
   SRECT_OP_LOOP_MERGE           = 246,
   SRECT_OP_SELECTION_MERGE      = 247,
   SRECT_OP_LABEL                = 248,
   SRECT_OP_BRANCH               = 249,
   SRECT_OP_BRANCH_CONDITIONAL   = 250,
   SRECT_OP_SWITCH               = 251,
   SRECT_OP_KILL                 = 252,
   SRECT_OP_RETURN               = 253,
   SRECT_OP_RETURN_VALUE         = 254,
   SRECT_OP_UNREACHABLE          = 255,
   SRECT_OP_NO_LINE              = 317,
   SRECT_OP_MODULE_PROCESSED     = 330,
   SRECT_OP_EXECUTION_MODE_ID    = 331,
   SRECT_OP_DECORATE_ID          = 332,
   SRECT_OP_TERMINATE_INVOCATION = 4416,
   SRECT_OP_DEMOTE_TO_HELPER     = 5380,
   SRECT_OP_DECORATE_STRING      = 5632,
   SRECT_OP_MEMBER_DECORATE_STR  = 5633
};

enum
{
   SRECT_SC_UNIFORM_CONSTANT = 0,
   SRECT_SC_UNIFORM          = 2,
   SRECT_SC_PUSH_CONSTANT    = 9
};

enum
{
   SRECT_DEC_BLOCK          = 2,
   SRECT_DEC_ARRAY_STRIDE   = 6,
   SRECT_DEC_MATRIX_STRIDE  = 7,
   SRECT_DEC_BINDING        = 33,
   SRECT_DEC_DESCRIPTOR_SET = 34,
   SRECT_DEC_OFFSET         = 35
};

enum
{
   SRECT_IMG_BIAS          = 0x01,
   SRECT_IMG_LOD           = 0x02,
   SRECT_IMG_GRAD          = 0x04,
   SRECT_IMG_CONST_OFFSET  = 0x08,
   SRECT_IMG_OFFSET        = 0x10,
   SRECT_IMG_CONST_OFFSETS = 0x20,
   SRECT_IMG_SAMPLE        = 0x40,
   SRECT_IMG_MIN_LOD       = 0x80
};

enum
{
   SRECT_GLSL_FLOOR  = 8,
   SRECT_GLSL_FCLAMP = 43,
   SRECT_GLSL_SCLAMP = 45
};

#define SRECT_SPV_MAGIC 0x07230203u

/* The functions one module can have rewritten, and how many ways. */
#define SRECT_RECT_MAX_SPECS 256

typedef struct
{
   uint32_t *w;
   size_t    n;
   size_t    cap;
} srect_rbuf_t;

typedef struct
{
   uint32_t fid;      /* the function in the input */
   uint32_t mask;     /* which of its parameters are the frame */
   uint32_t new_fid;  /* the function written for it */
   bool     clone;    /* written with fresh ids: not the first of fid */
} srect_rect_spec_t;

typedef struct
{
   const uint32_t *in;
   size_t          len;
   uint32_t        bound;
   uint32_t        in_bound;
   uint32_t        version;

   bool            source;
   bool            linear;
   enum slang_rect_wrap wrap;
   bool            fail;
   bool            touched;

   /* Indexed by input id: the defining instruction's word offset. */
   size_t         *def;
   /* Indexed by id, grown with the bound: the value is the frame. */
   uint8_t        *taint;
   size_t          taint_cap;
   /* While a clone is written: input id to the clone's id, 0 for ids
    * that are not the function's own; map_buf when one is. */
   uint32_t       *map;
   uint32_t       *map_buf;

   /* Types, found or made. */
   uint32_t t_bool, t_int, t_float;
   uint32_t t_v2f, t_v4f, t_v2i, t_v2b, t_v4b;
   uint32_t t_ptr_blk_v4f;
   uint32_t glsl450;
   bool     new_import;

   /* Where the three vec4s go: the push block or a uniform block,
    * found or made. */
   struct slang_rect_place place;
   uint32_t sc;
   uint32_t blk_var, blk_struct;
   unsigned blk_members;
   uint32_t blk_end;
   uint32_t blk_base;
   bool     blk_new;
   uint32_t member_idx[3];

   /* Constants made, on first use. */
   uint32_t c_int0, c_v2i0, c_v2i1, c_v2f_half, c_v2f_one, c_v4f0;

   srect_rbuf_t   types;   /* new types, constants, a block's variable */
   srect_rbuf_t   names;   /* new OpName / OpMemberName */
   srect_rbuf_t   annots;  /* new decorations */
   srect_rbuf_t   funcs;   /* every function, as written */

   srect_rect_spec_t specs[SRECT_RECT_MAX_SPECS];
   unsigned    num_specs;
} srect_rect_t;

/* ---- buffers ------------------------------------------------------- */

static bool srect_rb_reserve(srect_rbuf_t *b, size_t extra)
{
   uint32_t *w;
   size_t    cap;
   if (b->n + extra <= b->cap)
      return true;
   cap = b->cap ? b->cap * 2 : 256;
   while (cap < b->n + extra)
      cap *= 2;
   if (!(w = (uint32_t*)realloc(b->w, cap * sizeof(*w))))
      return false;
   b->w   = w;
   b->cap = cap;
   return true;
}

static void srect_rb_inst(srect_rect_t *r, srect_rbuf_t *b, uint32_t op,
      const uint32_t *ops, unsigned n)
{
   unsigned i;
   if (!srect_rb_reserve(b, n + 1))
   {
      r->fail = true;
      return;
   }
   b->w[b->n++] = ((uint32_t)(n + 1) << 16) | op;
   for (i = 0; i < n; i++)
      b->w[b->n++] = ops[i];
}

static void srect_rb_copy(srect_rect_t *r, srect_rbuf_t *b, const uint32_t *w, unsigned n)
{
   if (!srect_rb_reserve(b, n))
   {
      r->fail = true;
      return;
   }
   memcpy(b->w + b->n, w, n * sizeof(*w));
   b->n += n;
}

/* A string literal's words: NUL-terminated, zero-padded. */
static unsigned srect_str_words(const char *s, uint32_t *out, unsigned max)
{
   size_t   len = strlen(s) + 1;
   unsigned n   = (unsigned)((len + 3) / 4);
   if (n > max)
      return 0;
   memset(out, 0, n * sizeof(*out));
   memcpy(out, s, len);
   return n;
}

static uint32_t srect_new_id(srect_rect_t *r)
{
   uint32_t id = r->bound++;
   if (r->bound > r->taint_cap)
   {
      size_t   cap = r->taint_cap * 2;
      uint8_t *t;
      while (cap < r->bound)
         cap *= 2;
      if (!(t = (uint8_t*)realloc(r->taint, cap)))
      {
         r->fail = true;
         r->bound--;
         return 0;
      }
      memset(t + r->taint_cap, 0, cap - r->taint_cap);
      r->taint     = t;
      r->taint_cap = cap;
   }
   return id;
}

/* ---- the input ----------------------------------------------------- */

#define SRECT_INST_LEN(w) ((w) >> 16)
#define SRECT_INST_OP(w)  ((w) & 0xffffu)

/* The word holding an instruction's result id: 2 after a result type,
 * 1 without one, 0 for none. Only opcodes a function body written
 * again may hold are known; 255 for the rest. */
static unsigned srect_result_pos(uint32_t op)
{
   switch (op)
   {
      case SRECT_OP_LABEL:
         return 1;
      case SRECT_OP_UNDEF:
      case SRECT_OP_EXT_INST:
      case SRECT_OP_FUNCTION:
      case SRECT_OP_FUNCTION_PARAMETER:
      case SRECT_OP_FUNCTION_CALL:
      case SRECT_OP_VARIABLE:
      case SRECT_OP_LOAD:
      case SRECT_OP_PHI:
         return 2;
      case SRECT_OP_STORE:
      case SRECT_OP_COPY_MEMORY:
      case SRECT_OP_FUNCTION_END:
      case SRECT_OP_LOOP_MERGE:
      case SRECT_OP_SELECTION_MERGE:
      case SRECT_OP_BRANCH:
      case SRECT_OP_BRANCH_CONDITIONAL:
      case SRECT_OP_SWITCH:
      case SRECT_OP_KILL:
      case SRECT_OP_RETURN:
      case SRECT_OP_RETURN_VALUE:
      case SRECT_OP_UNREACHABLE:
      case SRECT_OP_LINE:
      case SRECT_OP_NO_LINE:
      case SRECT_OP_NOP:
      case SRECT_OP_IMAGE_WRITE:
      case SRECT_OP_TERMINATE_INVOCATION:
      case SRECT_OP_DEMOTE_TO_HELPER:
         return 0;
      default:
         break;
   }
   /* Access chains, vector and composite ops, the image ops, and the
    * conversion, arithmetic, relational, logical, bit and derivative
    * ops: a type, a result, operands. */
   if (     (op >= SRECT_OP_ACCESS_CHAIN && op <= 67)
         || (op >= SRECT_OP_VECTOR_EXTRACT_DYN && op <= 84)
         || (op >= SRECT_OP_SAMPLED_IMAGE && op <= 107 && op != SRECT_OP_IMAGE_WRITE)
         || (op >= 109 && op <= 215))
      return 2;
   return 255;
}

/* Whether word @i (1-based, after the header word) of an instruction is
 * a literal rather than an id. */
static bool srect_word_is_literal(const uint32_t *inst, unsigned i)
{
   uint32_t op = SRECT_INST_OP(inst[0]);
   switch (op)
   {
      case SRECT_OP_VECTOR_SHUFFLE:     return i >= 5;
      case SRECT_OP_COMPOSITE_EXTRACT:  return i >= 4;
      case SRECT_OP_COMPOSITE_INSERT:   return i >= 5;
      case SRECT_OP_EXT_INST:           return i == 4;
      case SRECT_OP_SELECTION_MERGE:    return i == 2;
      case SRECT_OP_LOOP_MERGE:         return i >= 3;
      case SRECT_OP_BRANCH_CONDITIONAL: return i >= 4;
      case SRECT_OP_SWITCH:             return i >= 3 && (i & 1);
      case SRECT_OP_LOAD:               return i >= 4;
      case SRECT_OP_STORE:              return i >= 3;
      case SRECT_OP_COPY_MEMORY:        return i >= 3;
      case SRECT_OP_VARIABLE:           return i == 3;
      case SRECT_OP_FUNCTION:           return i == 3;
      case SRECT_OP_LINE:               return i >= 2;
      case SRECT_OP_SAMPLE_IMPLICIT:
      case SRECT_OP_SAMPLE_EXPLICIT:
      case SRECT_OP_SAMPLE_PROJ_IMPLICIT:
      case SRECT_OP_SAMPLE_PROJ_EXPLICIT:
      case SRECT_OP_IMAGE_FETCH:
      case SRECT_OP_IMAGE_READ:         return i == 5;
      case SRECT_OP_SAMPLE_DREF_IMPLICIT:
      case SRECT_OP_SAMPLE_DREF_EXPLICIT:
      case SRECT_OP_SAMPLE_PROJ_DREF_IMP:
      case SRECT_OP_SAMPLE_PROJ_DREF_EXP:
      case SRECT_OP_IMAGE_GATHER:
      case SRECT_OP_IMAGE_DREF_GATHER:  return i == 6;
      case SRECT_OP_IMAGE_WRITE:        return i == 4;
      default:
         break;
   }
   return false;
}

static bool srect_is_annotation(uint32_t op)
{
   return op == SRECT_OP_DECORATE || op == SRECT_OP_MEMBER_DECORATE
       || op == 73 || op == 74 || op == 75
       || op == SRECT_OP_DECORATE_ID || op == SRECT_OP_DECORATE_STRING
       || op == SRECT_OP_MEMBER_DECORATE_STR;
}

static bool srect_is_debug(uint32_t op)
{
   return op == SRECT_OP_SOURCE_CONTINUED || op == SRECT_OP_SOURCE
       || op == SRECT_OP_SOURCE_EXTENSION || op == SRECT_OP_NAME
       || op == SRECT_OP_MEMBER_NAME || op == SRECT_OP_STRING
       || op == SRECT_OP_MODULE_PROCESSED;
}

static bool srect_is_preamble(uint32_t op)
{
   return op == SRECT_OP_CAPABILITY || op == SRECT_OP_EXTENSION
       || op == SRECT_OP_EXT_INST_IMPORT || op == SRECT_OP_MEMORY_MODEL
       || op == SRECT_OP_ENTRY_POINT || op == SRECT_OP_EXECUTION_MODE
       || op == SRECT_OP_EXECUTION_MODE_ID;
}

/* ---- types and constants ------------------------------------------- */

static uint32_t srect_find_type(const srect_rect_t *r, uint32_t op,
      uint32_t a, uint32_t b, unsigned nargs)
{
   size_t p = 5;
   while (p < r->len)
   {
      const uint32_t *w = r->in + p;
      unsigned        n = SRECT_INST_LEN(w[0]);
      if (!n)
         break;
      if (SRECT_INST_OP(w[0]) == SRECT_OP_FUNCTION)
         break;
      if (     SRECT_INST_OP(w[0]) == op
            && n == 2 + nargs
            && (nargs < 1 || w[2] == a)
            && (nargs < 2 || w[3] == b))
         return w[1];
      p += n;
   }
   return 0;
}

static uint32_t srect_type_make(srect_rect_t *r, uint32_t op,
      uint32_t a, uint32_t b, unsigned nargs)
{
   uint32_t ops[3];
   uint32_t id = srect_find_type(r, op, a, b, nargs);
   if (id)
      return id;
   id     = srect_new_id(r);
   ops[0] = id;
   ops[1] = a;
   ops[2] = b;
   srect_rb_inst(r, &r->types, op, ops, 1 + nargs);
   return id;
}

static bool srect_types_init(srect_rect_t *r)
{
   r->t_float = srect_find_type(r, SRECT_OP_TYPE_FLOAT, 32, 0, 1);
   r->t_v4f   = r->t_float ? srect_find_type(r, SRECT_OP_TYPE_VECTOR, r->t_float, 4, 2) : 0;
   /* vec4 has to precede the push block it is appended to; every stage
    * this sees writes one. */
   if (!r->t_float || !r->t_v4f)
      return false;
   r->t_v2f        = srect_type_make(r, SRECT_OP_TYPE_VECTOR, r->t_float, 2, 2);
   r->t_int        = srect_find_type(r, SRECT_OP_TYPE_INT, 32, 1, 2);
   if (!r->t_int)
      r->t_int     = srect_type_make(r, SRECT_OP_TYPE_INT, 32, 1, 2);
   r->t_v2i        = srect_type_make(r, SRECT_OP_TYPE_VECTOR, r->t_int, 2, 2);
   r->t_bool       = srect_type_make(r, SRECT_OP_TYPE_BOOL, 0, 0, 0);
   r->t_v2b        = srect_type_make(r, SRECT_OP_TYPE_VECTOR, r->t_bool, 2, 2);
   r->t_v4b        = srect_type_make(r, SRECT_OP_TYPE_VECTOR, r->t_bool, 4, 2);
   r->t_ptr_blk_v4f = srect_type_make(r, SRECT_OP_TYPE_POINTER, r->sc, r->t_v4f, 2);
   return !r->fail;
}

static uint32_t srect_const_int(srect_rect_t *r, int32_t v)
{
   uint32_t ops[3];
   ops[0] = r->t_int;
   ops[1] = srect_new_id(r);
   ops[2] = (uint32_t)v;
   srect_rb_inst(r, &r->types, SRECT_OP_CONSTANT, ops, 3);
   return ops[1];
}

static uint32_t srect_const_float(srect_rect_t *r, float v)
{
   uint32_t ops[3];
   ops[0] = r->t_float;
   ops[1] = srect_new_id(r);
   memcpy(&ops[2], &v, sizeof(v));
   srect_rb_inst(r, &r->types, SRECT_OP_CONSTANT, ops, 3);
   return ops[1];
}

static uint32_t srect_const_vec(srect_rect_t *r, uint32_t type, uint32_t c,
      unsigned n)
{
   uint32_t ops[6];
   unsigned i;
   ops[0] = type;
   ops[1] = srect_new_id(r);
   for (i = 0; i < n; i++)
      ops[2 + i] = c;
   srect_rb_inst(r, &r->types, SRECT_OP_CONSTANT_COMPOSITE, ops, 2 + n);
   return ops[1];
}

static void srect_consts_init(srect_rect_t *r)
{
   r->c_int0     = srect_const_int(r, 0);
   r->c_v2i0     = srect_const_vec(r, r->t_v2i, r->c_int0, 2);
   r->c_v2i1     = srect_const_vec(r, r->t_v2i, srect_const_int(r, 1), 2);
   r->c_v2f_half = srect_const_vec(r, r->t_v2f, srect_const_float(r, 0.5f), 2);
   r->c_v2f_one  = srect_const_vec(r, r->t_v2f, srect_const_float(r, 1.0f), 2);
   r->c_v4f0     = srect_const_vec(r, r->t_v4f, srect_const_float(r, 0.0f), 4);
}

/* ---- the push block ------------------------------------------------ */

static const uint32_t *srect_def_inst(const srect_rect_t *r, uint32_t id)
{
   if (id >= r->in_bound || !r->def[id])
      return NULL;
   return r->in + r->def[id];
}

/* The decoration @dec of @id, or of member @member of it when that is
 * not ~0u; ~0u when there is none. */
static uint32_t srect_decoration(const srect_rect_t *r, uint32_t id, uint32_t member,
      uint32_t dec)
{
   size_t p = 5;
   while (p < r->len)
   {
      const uint32_t *w = r->in + p;
      unsigned        n = SRECT_INST_LEN(w[0]);
      if (!n || SRECT_INST_OP(w[0]) == SRECT_OP_FUNCTION)
         break;
      if (     member == ~0u && SRECT_INST_OP(w[0]) == SRECT_OP_DECORATE
            && n >= 4 && w[1] == id && w[2] == dec)
         return w[3];
      if (     member != ~0u && SRECT_INST_OP(w[0]) == SRECT_OP_MEMBER_DECORATE
            && n >= 5 && w[1] == id && w[2] == member && w[3] == dec)
         return w[4];
      p += n;
   }
   return ~0u;
}

/* The bytes member @m of struct @s takes from its offset; 0 when this
 * cannot tell. */
static uint32_t srect_member_size(const srect_rect_t *r, uint32_t s, uint32_t m,
      uint32_t type)
{
   const uint32_t *t = srect_def_inst(r, type);
   if (!t)
      return 0;
   switch (SRECT_INST_OP(t[0]))
   {
      case SRECT_OP_TYPE_INT:
      case SRECT_OP_TYPE_FLOAT:
         return t[2] == 32 ? 4 : 0;
      case SRECT_OP_TYPE_VECTOR:
         {
            uint32_t c = srect_member_size(r, s, m, t[2]);
            return c ? c * t[3] : 0;
         }
      case SRECT_OP_TYPE_MATRIX:
         {
            uint32_t stride = srect_decoration(r, s, m, SRECT_DEC_MATRIX_STRIDE);
            return stride == ~0u ? 0 : stride * t[3];
         }
      case SRECT_OP_TYPE_ARRAY:
         {
            uint32_t        stride = srect_decoration(r, type, ~0u, SRECT_DEC_ARRAY_STRIDE);
            const uint32_t *len    = srect_def_inst(r, t[3]);
            if (     stride == ~0u || !len
                  || SRECT_INST_OP(len[0]) != SRECT_OP_CONSTANT)
               return 0;
            return stride * len[3];
         }
      default:
         break;
   }
   return 0;
}

static bool srect_has_decoration(const srect_rect_t *r, uint32_t id, uint32_t dec)
{
   size_t p = 5;
   while (p < r->len)
   {
      const uint32_t *w = r->in + p;
      unsigned        n = SRECT_INST_LEN(w[0]);
      if (!n || SRECT_INST_OP(w[0]) == SRECT_OP_FUNCTION)
         break;
      if (SRECT_INST_OP(w[0]) == SRECT_OP_DECORATE && n >= 3 && w[1] == id
            && w[2] == dec)
         return true;
      p += n;
   }
   return false;
}

/* The block the place names: the push block, or the uniform block at its
 * set and binding. false for one whose layout this cannot read. */
static bool srect_block_find(srect_rect_t *r)
{
   size_t p = 5;
   while (p < r->len)
   {
      const uint32_t *w = r->in + p;
      unsigned        n = SRECT_INST_LEN(w[0]);
      if (!n || SRECT_INST_OP(w[0]) == SRECT_OP_FUNCTION)
         break;
      if (SRECT_INST_OP(w[0]) == SRECT_OP_VARIABLE && n >= 4 && w[3] == r->sc)
      {
         const uint32_t *ptr = srect_def_inst(r, w[1]);
         const uint32_t *st;
         uint32_t        end = 0, m;
         if (r->sc == SRECT_SC_UNIFORM)
         {
            uint32_t set     = srect_decoration(r, w[2], ~0u, SRECT_DEC_DESCRIPTOR_SET);
            uint32_t binding = srect_decoration(r, w[2], ~0u, SRECT_DEC_BINDING);
            if (set == ~0u)
               set = 0;
            if (     set     != r->place.set
                  || binding != r->place.binding)
            {
               p += n;
               continue;
            }
         }
         if (r->blk_var || !ptr || SRECT_INST_OP(ptr[0]) != SRECT_OP_TYPE_POINTER)
            return false;
         if (!(st = srect_def_inst(r, ptr[3])) || SRECT_INST_OP(st[0]) != SRECT_OP_TYPE_STRUCT
               || !srect_has_decoration(r, ptr[3], SRECT_DEC_BLOCK))
            return false;
         r->blk_var     = w[2];
         r->blk_struct  = ptr[3];
         r->blk_members = SRECT_INST_LEN(st[0]) - 2;
         for (m = 0; m < r->blk_members; m++)
         {
            uint32_t off  = srect_decoration(r, r->blk_struct, m, SRECT_DEC_OFFSET);
            uint32_t size = srect_member_size(r, r->blk_struct, m, st[2 + m]);
            if (off == ~0u || !size)
               return false;
            if (off + size > end)
               end = off + size;
         }
         r->blk_end = end;
      }
      p += n;
   }
   return true;
}

/* The three members, after what the block has, or a block of their
 * own. */
static bool srect_block_make(srect_rect_t *r)
{
   static const char *names[3] = {
      SLANG_RECT_NAME_RECT, SLANG_RECT_NAME_CLAMP, SLANG_RECT_NAME_TEXELS };
   uint32_t ops[16];
   unsigned i, n;

   r->blk_base = r->blk_end > r->place.offset ? r->blk_end : r->place.offset;
   r->blk_base = (r->blk_base + 15) & ~15u;
   if (     r->sc == SRECT_SC_PUSH_CONSTANT
         && r->blk_base + 48 > SLANG_RECT_PUSH_LIMIT)
      return false;

   if (!r->blk_var)
   {
      uint32_t ptr;
      r->blk_new     = true;
      r->blk_struct  = srect_new_id(r);
      ops[0]         = r->blk_struct;
      ops[1]         = r->t_v4f;
      ops[2]         = r->t_v4f;
      ops[3]         = r->t_v4f;
      srect_rb_inst(r, &r->types, SRECT_OP_TYPE_STRUCT, ops, 4);
      ptr            = srect_type_make(r, SRECT_OP_TYPE_POINTER, r->sc, r->blk_struct, 2);
      r->blk_var     = srect_new_id(r);
      ops[0]         = ptr;
      ops[1]         = r->blk_var;
      ops[2]         = r->sc;
      srect_rb_inst(r, &r->types, SRECT_OP_VARIABLE, ops, 3);
      ops[0]         = r->blk_struct;
      ops[1]         = SRECT_DEC_BLOCK;
      srect_rb_inst(r, &r->annots, SRECT_OP_DECORATE, ops, 2);
      if (r->sc == SRECT_SC_UNIFORM)
      {
         ops[0]      = r->blk_var;
         ops[1]      = SRECT_DEC_DESCRIPTOR_SET;
         ops[2]      = r->place.set;
         srect_rb_inst(r, &r->annots, SRECT_OP_DECORATE, ops, 3);
         ops[1]      = SRECT_DEC_BINDING;
         ops[2]      = r->place.binding;
         srect_rb_inst(r, &r->annots, SRECT_OP_DECORATE, ops, 3);
      }
      ops[0]         = r->blk_struct;
      n              = srect_str_words("RARCH_Rect", ops + 1, 15);
      srect_rb_inst(r, &r->names, SRECT_OP_NAME, ops, 1 + n);
      ops[0]         = r->blk_var;
      n              = srect_str_words("rarch_rect", ops + 1, 15);
      srect_rb_inst(r, &r->names, SRECT_OP_NAME, ops, 1 + n);
      r->blk_members = 0;
   }

   for (i = 0; i < 3; i++)
   {
      ops[0] = r->blk_struct;
      ops[1] = r->blk_members + i;
      ops[2] = SRECT_DEC_OFFSET;
      ops[3] = r->blk_base + 16 * i;
      srect_rb_inst(r, &r->annots, SRECT_OP_MEMBER_DECORATE, ops, 4);
      ops[0] = r->blk_struct;
      ops[1] = r->blk_members + i;
      n      = srect_str_words(names[i], ops + 2, 14);
      srect_rb_inst(r, &r->names, SRECT_OP_MEMBER_NAME, ops, 2 + n);
      r->member_idx[i] = srect_const_int(r, (int32_t)(r->blk_members + i));
   }
   return !r->fail;
}

/* ---- writing a function -------------------------------------------- */

static uint32_t srect_m_id(const srect_rect_t *r, uint32_t id)
{
   if (r->map && id < r->in_bound && r->map[id])
      return r->map[id];
   return id;
}

static bool srect_tainted(const srect_rect_t *r, uint32_t id)
{
   return id < r->taint_cap && r->taint[id];
}


static void srect_taint(srect_rect_t *r, uint32_t id)
{
   if (id < r->taint_cap)
      r->taint[id] = 1;
}

/* An instruction into the function being written; @result 0 for a
 * fresh one. */
static uint32_t srect_emit_r(srect_rect_t *r, uint32_t op, uint32_t type,
      uint32_t result, const uint32_t *args, unsigned n)
{
   uint32_t ops[12];
   unsigned i;
   if (!result)
      result = srect_new_id(r);
   ops[0] = type;
   ops[1] = result;
   for (i = 0; i < n; i++)
      ops[2 + i] = args[i];
   srect_rb_inst(r, &r->funcs, op, ops, 2 + n);
   return result;
}

static uint32_t srect_emit(srect_rect_t *r, uint32_t op, uint32_t type,
      const uint32_t *args, unsigned n)
{
   return srect_emit_r(r, op, type, 0, args, n);
}

static uint32_t srect_emit1(srect_rect_t *r, uint32_t op, uint32_t type, uint32_t a)
{
   return srect_emit(r, op, type, &a, 1);
}

static uint32_t srect_emit2(srect_rect_t *r, uint32_t op, uint32_t type,
      uint32_t a, uint32_t b)
{
   uint32_t args[2];
   args[0] = a;
   args[1] = b;
   return srect_emit(r, op, type, args, 2);
}

static uint32_t srect_ext(srect_rect_t *r, uint32_t type, uint32_t inst,
      const uint32_t *args, unsigned n)
{
   uint32_t ops[8];
   unsigned i;
   ops[0] = r->glsl450;
   ops[1] = inst;
   for (i = 0; i < n; i++)
      ops[2 + i] = args[i];
   return srect_emit(r, SRECT_OP_EXT_INST, type, ops, 2 + n);
}

static uint32_t srect_ext1(srect_rect_t *r, uint32_t type, uint32_t inst, uint32_t a)
{
   return srect_ext(r, type, inst, &a, 1);
}

static uint32_t srect_half_of(srect_rect_t *r, uint32_t v4, unsigned first)
{
   uint32_t args[4];
   args[0] = v4;
   args[1] = v4;
   args[2] = first;
   args[3] = first + 1;
   return srect_emit(r, SRECT_OP_VECTOR_SHUFFLE, r->t_v2f, args, 4);
}

/* One of the three push members. */
static uint32_t srect_block_load(srect_rect_t *r, unsigned which)
{
   uint32_t ptr = srect_emit2(r, SRECT_OP_ACCESS_CHAIN, r->t_ptr_blk_v4f,
         r->blk_var, r->member_idx[which]);
   return srect_emit1(r, SRECT_OP_LOAD, r->t_v4f, ptr);
}

/* The rectangle's size, as floats and as ints, and its origin. */
static void srect_rect_texels(srect_rect_t *r, uint32_t *size_f, uint32_t *size_i,
      uint32_t *org_i)
{
   uint32_t texels = srect_block_load(r, 2);
   *size_f = srect_half_of(r, texels, 0);
   if (size_i)
      *size_i = srect_emit1(r, SRECT_OP_CONVERT_F_TO_S, r->t_v2i, *size_f);
   if (org_i)
      *org_i  = srect_emit1(r, SRECT_OP_CONVERT_F_TO_S, r->t_v2i,
            srect_half_of(r, texels, 2));
}

/* @i modulo @n, never negative (ivec2s). Not OpSMod: GLSL and HLSL
 * have no such remainder for SPIRV-Cross to write it as. The quotient
 * is the float one, which the two corrections make exact. */
static uint32_t srect_mod_floor(srect_rect_t *r, uint32_t i, uint32_t n)
{
   uint32_t args[3];
   uint32_t q = srect_emit1(r, SRECT_OP_CONVERT_F_TO_S, r->t_v2i,
         srect_ext1(r, r->t_v2f, SRECT_GLSL_FLOOR,
            srect_emit2(r, SRECT_OP_FDIV, r->t_v2f,
               srect_emit1(r, SRECT_OP_CONVERT_S_TO_F, r->t_v2f, i),
               srect_emit1(r, SRECT_OP_CONVERT_S_TO_F, r->t_v2f, n))));
   uint32_t m = srect_emit2(r, SRECT_OP_ISUB, r->t_v2i, i,
         srect_emit2(r, SRECT_OP_IMUL, r->t_v2i, q, n));
   args[0] = srect_emit2(r, SRECT_OP_SLESS, r->t_v2b, m, r->c_v2i0);
   args[1] = srect_emit2(r, SRECT_OP_IADD, r->t_v2i, m, n);
   args[2] = m;
   m       = srect_emit(r, SRECT_OP_SELECT, r->t_v2i, args, 3);
   args[0] = srect_emit2(r, SRECT_OP_SGREATER_EQUAL, r->t_v2b, m, n);
   args[1] = srect_emit2(r, SRECT_OP_ISUB, r->t_v2i, m, n);
   args[2] = m;
   return srect_emit(r, SRECT_OP_SELECT, r->t_v2i, args, 3);
}

/* Texel @i (ivec2, of the rectangle) wrapped as the hardware wraps it
 * into the rectangle of size @n (ivec2); *inside, unless NULL, is the
 * bool that it was in it, for border. */
static uint32_t srect_wrap_texel(srect_rect_t *r, uint32_t i, uint32_t n,
      uint32_t *inside)
{
   uint32_t args[3];
   switch (r->wrap)
   {
      case SLANG_RECT_WRAP_REPEAT:
         return srect_mod_floor(r, i, n);
      case SLANG_RECT_WRAP_MIRROR:
         {
            uint32_t n2  = srect_emit2(r, SRECT_OP_IADD, r->t_v2i, n, n);
            uint32_t m   = srect_mod_floor(r, i, n2);
            uint32_t lt  = srect_emit2(r, SRECT_OP_SLESS, r->t_v2b, m, n);
            uint32_t alt = srect_emit2(r, SRECT_OP_ISUB, r->t_v2i,
                  srect_emit2(r, SRECT_OP_ISUB, r->t_v2i, n2, r->c_v2i1), m);
            args[0] = lt;
            args[1] = m;
            args[2] = alt;
            return srect_emit(r, SRECT_OP_SELECT, r->t_v2i, args, 3);
         }
      case SLANG_RECT_WRAP_BORDER:
         if (inside)
         {
            uint32_t ge = srect_emit2(r, SRECT_OP_SGREATER_EQUAL, r->t_v2b, i, r->c_v2i0);
            uint32_t lt = srect_emit2(r, SRECT_OP_SLESS, r->t_v2b, i, n);
            *inside     = srect_emit1(r, SRECT_OP_ALL, r->t_bool,
                  srect_emit2(r, SRECT_OP_LOGICAL_AND, r->t_v2b, ge, lt));
         }
         /* fetched in range, the border chosen after */
         break;
      case SLANG_RECT_WRAP_EDGE:
      default:
         break;
   }
   args[0] = i;
   args[1] = r->c_v2i0;
   args[2] = srect_emit2(r, SRECT_OP_ISUB, r->t_v2i, n, r->c_v2i1);
   return srect_ext(r, r->t_v2i, SRECT_GLSL_SCLAMP, args, 3);
}

/* Texel @i of the rectangle, wrapped, out of image @img; the border
 * colour, transparent black, where border wrap puts it outside. */
static uint32_t srect_fetch_texel(srect_rect_t *r, uint32_t img, uint32_t i,
      uint32_t n, uint32_t org, uint32_t result)
{
   uint32_t args[4];
   uint32_t b4[4];
   uint32_t inside = 0;
   uint32_t t      = srect_wrap_texel(r, i, n,
         r->wrap == SLANG_RECT_WRAP_BORDER ? &inside : NULL);
   uint32_t v;
   args[0] = img;
   args[1] = srect_emit2(r, SRECT_OP_IADD, r->t_v2i, t, org);
   args[2] = SRECT_IMG_LOD;
   args[3] = r->c_int0;
   if (!inside)
      return srect_emit_r(r, SRECT_OP_IMAGE_FETCH, r->t_v4f, result, args, 4);
   v       = srect_emit(r, SRECT_OP_IMAGE_FETCH, r->t_v4f, args, 4);
   b4[0]   = b4[1] = b4[2] = b4[3] = inside;
   args[0] = srect_emit(r, SRECT_OP_COMPOSITE_CONSTRUCT, r->t_v4b, b4, 4);
   args[1] = v;
   args[2] = r->c_v4f0;
   return srect_emit_r(r, SRECT_OP_SELECT, r->t_v4f, result, args, 3);
}

/* The image operands of an image instruction from word @mpos, split. */
typedef struct
{
   uint32_t mask;
   uint32_t bias, lod, dx, dy, offset, min_lod;
} srect_img_ops_t;

static bool srect_img_ops_parse(const uint32_t *w, unsigned len, unsigned mpos,
      srect_img_ops_t *o)
{
   unsigned k = mpos + 1;
   memset(o, 0, sizeof(*o));
   if (mpos >= len)
      return true;
   o->mask = w[mpos];
   if (o->mask & ~(uint32_t)(SRECT_IMG_BIAS | SRECT_IMG_LOD | SRECT_IMG_GRAD
            | SRECT_IMG_CONST_OFFSET | SRECT_IMG_OFFSET | SRECT_IMG_MIN_LOD))
      return false;
   if ((o->mask & SRECT_IMG_BIAS)         && k < len) o->bias    = w[k++];
   if ((o->mask & SRECT_IMG_LOD)          && k < len) o->lod     = w[k++];
   if ((o->mask & SRECT_IMG_GRAD)         && k + 1 < len)
   {
      o->dx = w[k++];
      o->dy = w[k++];
   }
   if ((o->mask & SRECT_IMG_CONST_OFFSET) && k < len) o->offset  = w[k++];
   if ((o->mask & SRECT_IMG_OFFSET)       && k < len) o->offset  = w[k++];
   if ((o->mask & SRECT_IMG_MIN_LOD)      && k < len) o->min_lod = w[k++];
   return k == len;
}

/* The image type a sampled image value of type @si_type holds. */
static uint32_t srect_image_type_of(const srect_rect_t *r, uint32_t si_type)
{
   const uint32_t *t = srect_def_inst(r, si_type);
   if (!t || SRECT_INST_OP(t[0]) != SRECT_OP_TYPE_SAMPLED_IMAGE)
      return 0;
   return t[2];
}

/* The result type of a value in the input. */
static uint32_t srect_type_of(const srect_rect_t *r, uint32_t id)
{
   const uint32_t *w = srect_def_inst(r, id);
   if (!w || SRECT_INST_OP(w[0]) == SRECT_OP_FUNCTION_PARAMETER)
      return w ? w[1] : 0;
   if (srect_result_pos(SRECT_INST_OP(w[0])) != 2 && SRECT_INST_OP(w[0]) != SRECT_OP_CONSTANT
         && SRECT_INST_OP(w[0]) != SRECT_OP_CONSTANT_COMPOSITE)
      return 0;
   return w[1];
}

/* The image under sampled image @si (an input id, mapped here). */
static uint32_t srect_image_of(srect_rect_t *r, uint32_t si)
{
   uint32_t type = srect_image_type_of(r, srect_type_of(r, si));
   if (!type)
   {
      r->fail = true;
      return 0;
   }
   return srect_emit1(r, SRECT_OP_IMAGE, type, srect_m_id(r, si));
}

/* The texel nearest filtering picks, unwrapped: ivec2. */
/* The texel nearest filtering picks, or with @gather the first of the
 * two linear filtering mixes, unwrapped: ivec2. */
static uint32_t srect_nearest_texel(srect_rect_t *r, uint32_t coord, uint32_t size_f,
      const srect_img_ops_t *o, bool gather)
{
   uint32_t uv, i;
   bool     wraps = r->wrap == SLANG_RECT_WRAP_REPEAT
                 || r->wrap == SLANG_RECT_WRAP_MIRROR;
   /* Repeat and mirror take the coordinate into its period, offset and
    * all, and mirror folds it, before it is scaled, as samplers do, so
    * that it rounds as theirs does; the texel's wrap after does the
    * rest. Every step is exact. A gather's texels come back in the
    * order of the period unfolded. */
   if (wraps && o->offset)
      coord = srect_emit2(r, SRECT_OP_FADD, r->t_v2f, coord,
            srect_emit2(r, SRECT_OP_FDIV, r->t_v2f,
               srect_emit1(r, SRECT_OP_CONVERT_S_TO_F, r->t_v2f, srect_m_id(r, o->offset)),
               size_f));
   if (r->wrap == SLANG_RECT_WRAP_REPEAT)
      coord = srect_emit2(r, SRECT_OP_FSUB, r->t_v2f, coord,
            srect_ext1(r, r->t_v2f, SRECT_GLSL_FLOOR, coord));
   else if (wraps)
   {
      /* f = fract(c / 2): c is 2f in the period, which folds to 2f
       * below 1/2 and to 2(1 - f) above */
      uint32_t args[3];
      uint32_t h  = srect_emit2(r, SRECT_OP_FMUL, r->t_v2f, coord, r->c_v2f_half);
      uint32_t f  = srect_emit2(r, SRECT_OP_FSUB, r->t_v2f, h,
            srect_ext1(r, r->t_v2f, SRECT_GLSL_FLOOR, h));
      uint32_t g;
      if (gather)
         coord    = srect_emit2(r, SRECT_OP_FADD, r->t_v2f, f, f);
      else
      {
         g        = srect_emit2(r, SRECT_OP_FSUB, r->t_v2f, r->c_v2f_one, f);
         args[0]  = srect_emit2(r, SRECT_OP_FORD_LESS_THAN, r->t_v2b, f, r->c_v2f_half);
         args[1]  = srect_emit2(r, SRECT_OP_FADD, r->t_v2f, f, f);
         args[2]  = srect_emit2(r, SRECT_OP_FADD, r->t_v2f, g, g);
         coord    = srect_emit(r, SRECT_OP_SELECT, r->t_v2f, args, 3);
      }
   }
   uv = srect_emit2(r, SRECT_OP_FMUL, r->t_v2f, coord, size_f);
   if (gather)
      uv = srect_emit2(r, SRECT_OP_FSUB, r->t_v2f, uv, r->c_v2f_half);
   i = srect_emit1(r, SRECT_OP_CONVERT_F_TO_S, r->t_v2i, srect_ext1(r, r->t_v2f, SRECT_GLSL_FLOOR, uv));
   if (o->offset && !wraps)
      i = srect_emit2(r, SRECT_OP_IADD, r->t_v2i, i, srect_m_id(r, o->offset));
   return i;
}

/* OpImageSample{Implicit,Explicit}Lod of the frame. */
static void srect_write_sample(srect_rect_t *r, const uint32_t *w, unsigned len)
{
   srect_img_ops_t o;
   uint32_t  rtype  = w[1];
   uint32_t  result = srect_m_id(r, w[2]);
   uint32_t  coord  = srect_m_id(r, w[4]);
   uint32_t  size_f, size_i, org_i;

   if (     !srect_img_ops_parse(w, len, 5, &o)
         || srect_type_of(r, w[4]) != r->t_v2f
         || rtype != r->t_v4f)
   {
      r->fail = true;
      return;
   }

   if (r->linear)
   {
      uint32_t ops[12];
      uint32_t args[3];
      uint32_t rect, lim, scale;
      unsigned k = 0;
      /* Linear filtering mixes across the rectangle's edge for every
       * wrap but clamp to edge. */
      if (r->wrap != SLANG_RECT_WRAP_EDGE)
      {
         r->fail = true;
         return;
      }
      rect  = srect_block_load(r, 0);
      lim   = srect_block_load(r, 1);
      scale = srect_half_of(r, rect, 0);
      if (o.offset)
      {
         srect_rect_texels(r, &size_f, NULL, NULL);
         coord = srect_emit2(r, SRECT_OP_FADD, r->t_v2f, coord,
               srect_emit2(r, SRECT_OP_FDIV, r->t_v2f,
                  srect_emit1(r, SRECT_OP_CONVERT_S_TO_F, r->t_v2f, srect_m_id(r, o.offset)),
                  size_f));
      }
      coord   = srect_emit2(r, SRECT_OP_FADD, r->t_v2f,
            srect_emit2(r, SRECT_OP_FMUL, r->t_v2f, coord, scale),
            srect_half_of(r, rect, 2));
      args[0] = coord;
      args[1] = srect_half_of(r, lim, 0);
      args[2] = srect_half_of(r, lim, 2);
      coord   = srect_ext(r, r->t_v2f, SRECT_GLSL_FCLAMP, args, 3);

      /* Gradients are in the rectangle's coordinates: scaled into the
       * texture's, written ahead of the sample. */
      if (o.mask & SRECT_IMG_GRAD)
      {
         o.dx = srect_emit2(r, SRECT_OP_FMUL, r->t_v2f, srect_m_id(r, o.dx), scale);
         o.dy = srect_emit2(r, SRECT_OP_FMUL, r->t_v2f, srect_m_id(r, o.dy), scale);
      }
      else
      {
         o.dx = srect_m_id(r, o.dx);
         o.dy = srect_m_id(r, o.dy);
      }

      ops[k++] = rtype;
      ops[k++] = result;
      ops[k++] = srect_m_id(r, w[3]);
      ops[k++] = coord;
      o.mask  &= ~(uint32_t)(SRECT_IMG_CONST_OFFSET | SRECT_IMG_OFFSET);
      if (o.mask)
      {
         ops[k++] = o.mask;
         if (o.mask & SRECT_IMG_BIAS)    ops[k++] = srect_m_id(r, o.bias);
         if (o.mask & SRECT_IMG_LOD)     ops[k++] = srect_m_id(r, o.lod);
         if (o.mask & SRECT_IMG_GRAD)
         {
            ops[k++] = o.dx;
            ops[k++] = o.dy;
         }
         if (o.mask & SRECT_IMG_MIN_LOD) ops[k++] = srect_m_id(r, o.min_lod);
      }
      srect_rb_inst(r, &r->funcs, SRECT_INST_OP(w[0]), ops, k);
      return;
   }

   /* Nearest: the texel the hardware picks, fetched. The frame has one
    * level, so bias, lod and gradients choose nothing. */
   {
      uint32_t img = srect_image_of(r, w[3]);
      srect_rect_texels(r, &size_f, &size_i, &org_i);
      srect_fetch_texel(r, img, srect_nearest_texel(r, coord, size_f, &o, false),
            size_i, org_i, result);
   }
}

/* OpImageGather of the frame: the four texels linear filtering would
 * mix, in gather order. */
static void srect_write_gather(srect_rect_t *r, const uint32_t *w, unsigned len)
{
   srect_img_ops_t o;
   uint32_t  c[4];
   uint32_t  xy[4];
   uint32_t  img, size_f, size_i, org_i, i0, i1;
   unsigned  k;
   uint32_t  rtype  = w[1];
   uint32_t  result = srect_m_id(r, w[2]);
   uint32_t  coord  = srect_m_id(r, w[4]);
   uint32_t  comp   = srect_m_id(r, w[5]);

   if (     !srect_img_ops_parse(w, len, 6, &o)
         || (o.mask & ~(uint32_t)(SRECT_IMG_CONST_OFFSET | SRECT_IMG_OFFSET))
         || srect_type_of(r, w[4]) != r->t_v2f
         || rtype != r->t_v4f)
   {
      r->fail = true;
      return;
   }

   img = srect_image_of(r, w[3]);
   srect_rect_texels(r, &size_f, &size_i, &org_i);
   i0  = srect_nearest_texel(r, coord, size_f, &o, true);
   i1  = srect_emit2(r, SRECT_OP_IADD, r->t_v2i, i0, r->c_v2i1);
   /* x0, y0, x1, y1 */
   xy[0] = srect_emit2(r, SRECT_OP_COMPOSITE_EXTRACT, r->t_int, i0, 0);
   xy[1] = srect_emit2(r, SRECT_OP_COMPOSITE_EXTRACT, r->t_int, i0, 1);
   xy[2] = srect_emit2(r, SRECT_OP_COMPOSITE_EXTRACT, r->t_int, i1, 0);
   xy[3] = srect_emit2(r, SRECT_OP_COMPOSITE_EXTRACT, r->t_int, i1, 1);
   for (k = 0; k < 4; k++)
   {
      /* (i0, j1), (i1, j1), (i1, j0), (i0, j0) */
      static const unsigned xs[4] = { 0, 2, 2, 0 };
      static const unsigned ys[4] = { 3, 3, 1, 1 };
      uint32_t at = srect_emit2(r, SRECT_OP_COMPOSITE_CONSTRUCT, r->t_v2i,
            xy[xs[k]], xy[ys[k]]);
      uint32_t v  = srect_fetch_texel(r, img, at, size_i, org_i, 0);
      c[k]        = srect_emit2(r, SRECT_OP_VECTOR_EXTRACT_DYN, r->t_float, v, comp);
   }
   srect_emit_r(r, SRECT_OP_COMPOSITE_CONSTRUCT, r->t_v4f, result, c, 4);
}

/* OpImageFetch of the frame's image: moved by the origin. */
static void srect_write_fetch(srect_rect_t *r, const uint32_t *w, unsigned len)
{
   srect_img_ops_t o;
   uint32_t  ops[8];
   uint32_t  size_f, org_i, coord;
   unsigned  k = 0;

   if (     !srect_img_ops_parse(w, len, 5, &o)
         || (o.mask & ~(uint32_t)(SRECT_IMG_LOD | SRECT_IMG_CONST_OFFSET | SRECT_IMG_OFFSET))
         || srect_type_of(r, w[4]) != r->t_v2i)
   {
      r->fail = true;
      return;
   }
   srect_rect_texels(r, &size_f, NULL, &org_i);
   coord = srect_emit2(r, SRECT_OP_IADD, r->t_v2i, srect_m_id(r, w[4]), org_i);
   if (o.offset)
      coord = srect_emit2(r, SRECT_OP_IADD, r->t_v2i, coord, srect_m_id(r, o.offset));
   ops[k++] = w[1];
   ops[k++] = srect_m_id(r, w[2]);
   ops[k++] = srect_m_id(r, w[3]);
   ops[k++] = coord;
   if (o.mask & SRECT_IMG_LOD)
   {
      ops[k++] = SRECT_IMG_LOD;
      ops[k++] = srect_m_id(r, o.lod);
   }
   srect_rb_inst(r, &r->funcs, SRECT_OP_IMAGE_FETCH, ops, k);
}

/* OpImageQuerySize[Lod] of the frame's image: the rectangle's size. */
static void srect_write_query(srect_rect_t *r, const uint32_t *w)
{
   uint32_t size_f;
   if (w[1] != r->t_v2i)
   {
      r->fail = true;
      return;
   }
   srect_rect_texels(r, &size_f, NULL, NULL);
   srect_emit_r(r, SRECT_OP_CONVERT_F_TO_S, r->t_v2i, srect_m_id(r, w[2]), &size_f, 1);
}

/* ---- functions ----------------------------------------------------- */

static void srect_write_copy(srect_rect_t *r, const uint32_t *w, unsigned n)
{
   unsigned i;
   if (!r->map)
   {
      srect_rb_copy(r, &r->funcs, w, n);
      return;
   }
   if (!srect_rb_reserve(&r->funcs, n))
   {
      r->fail = true;
      return;
   }
   r->funcs.w[r->funcs.n] = w[0];
   for (i = 1; i < n; i++)
      r->funcs.w[r->funcs.n + i] = srect_word_is_literal(w, i)
         ? w[i] : srect_m_id(r, w[i]);
   r->funcs.n += n;
}

/* Whether an instruction takes the frame as an operand. */
static bool srect_uses_taint(const srect_rect_t *r, const uint32_t *w, unsigned n)
{
   unsigned i;
   for (i = 1; i < n; i++)
      if (!srect_word_is_literal(w, i) && srect_tainted(r, srect_m_id(r, w[i])))
         return true;
   return false;
}

/* The function written for @fid called with the frame for the
 * parameters in @mask; 0 when there are too many. */
static uint32_t srect_spec_get(srect_rect_t *r, uint32_t fid, uint32_t mask)
{
   const uint32_t *f = srect_def_inst(r, fid);
   unsigned        i;
   srect_rect_spec_t    *s;
   if (!f || SRECT_INST_OP(f[0]) != SRECT_OP_FUNCTION)
      return 0;
   for (i = 0; i < r->num_specs; i++)
      if (r->specs[i].fid == fid && r->specs[i].mask == mask)
         return r->specs[i].new_fid;
   if (r->num_specs >= SRECT_RECT_MAX_SPECS)
      return 0;
   s          = &r->specs[r->num_specs++];
   s->fid     = fid;
   s->mask    = mask;
   s->new_fid = srect_new_id(r);
   s->clone   = true;
   return s->new_fid;
}

/* Fresh ids for everything function @start defines, @fid becoming
 * @new_fid, and its ids' decorations for them. */
static bool srect_clone_map(srect_rect_t *r, size_t start, uint32_t new_fid)
{
   size_t p = start;
   r->map = r->map_buf;
   memset(r->map, 0, r->in_bound * sizeof(*r->map));
   for (;;)
   {
      const uint32_t *w;
      unsigned        n, rp;
      uint32_t        op;
      if (p >= r->len)
         return false;
      w  = r->in + p;
      n  = SRECT_INST_LEN(w[0]);
      op = SRECT_INST_OP(w[0]);
      rp = srect_result_pos(op);
      if (rp == 255)
         return false;
      if (rp)
      {
         if (rp >= n || w[rp] >= r->in_bound)
            return false;
         r->map[w[rp]] = op == SRECT_OP_FUNCTION ? new_fid : srect_new_id(r);
      }
      if (op == SRECT_OP_FUNCTION_END)
         break;
      p += n;
   }

   for (p = 5; p < r->len; )
   {
      const uint32_t *w = r->in + p;
      unsigned        n = SRECT_INST_LEN(w[0]);
      if (SRECT_INST_OP(w[0]) == SRECT_OP_FUNCTION)
         break;
      if (     SRECT_INST_OP(w[0]) == SRECT_OP_DECORATE && n >= 3
            && w[1] < r->in_bound && r->map[w[1]])
      {
         if (!srect_rb_reserve(&r->annots, n))
            return false;
         memcpy(r->annots.w + r->annots.n, w, n * sizeof(*w));
         r->annots.w[r->annots.n + 1] = r->map[w[1]];
         r->annots.n += n;
      }
      p += n;
   }
   return !r->fail;
}

/* Writes the function at @start: as it is, or as spec @s. */
static void srect_write_function(srect_rect_t *r, size_t start, const srect_rect_spec_t *s)
{
   size_t   p     = start;
   unsigned param = 0;

   if (s && s->clone)
   {
      if (!srect_clone_map(r, start, s->new_fid))
      {
         r->fail = true;
         return;
      }
   }
   else
      r->map = NULL;

   while (!r->fail)
   {
      const uint32_t *w;
      unsigned        n;
      uint32_t        op;

      if (p >= r->len)
      {
         r->fail = true;
         break;
      }
      w  = r->in + p;
      n  = SRECT_INST_LEN(w[0]);
      op = SRECT_INST_OP(w[0]);

      switch (op)
      {
         case SRECT_OP_FUNCTION_PARAMETER:
            if (s && param < 32 && (s->mask & (1u << param)))
               srect_taint(r, srect_m_id(r, w[2]));
            param++;
            srect_write_copy(r, w, n);
            break;
         case SRECT_OP_LOAD:
         case SRECT_OP_COPY_OBJECT:
         case SRECT_OP_IMAGE:
            if (n < 4 || n > 6)
               r->fail = true;
            else
            {
               if (srect_tainted(r, srect_m_id(r, w[3])))
                  srect_taint(r, srect_m_id(r, w[2]));
               srect_write_copy(r, w, n);
            }
            break;
         case SRECT_OP_SAMPLE_IMPLICIT:
         case SRECT_OP_SAMPLE_EXPLICIT:
            if (n >= 5 && srect_tainted(r, srect_m_id(r, w[3])))
            {
               r->touched = true;
               srect_write_sample(r, w, n);
            }
            else if (srect_uses_taint(r, w, n))
               r->fail = true;
            else
               srect_write_copy(r, w, n);
            break;
         case SRECT_OP_IMAGE_GATHER:
            if (n >= 6 && srect_tainted(r, srect_m_id(r, w[3])))
            {
               r->touched = true;
               srect_write_gather(r, w, n);
            }
            else if (srect_uses_taint(r, w, n))
               r->fail = true;
            else
               srect_write_copy(r, w, n);
            break;
         case SRECT_OP_IMAGE_FETCH:
            if (n >= 5 && srect_tainted(r, srect_m_id(r, w[3])))
            {
               r->touched = true;
               srect_write_fetch(r, w, n);
            }
            else if (srect_uses_taint(r, w, n))
               r->fail = true;
            else
               srect_write_copy(r, w, n);
            break;
         case SRECT_OP_QUERY_SIZE_LOD:
         case SRECT_OP_QUERY_SIZE:
            if (n >= 4 && srect_tainted(r, srect_m_id(r, w[3])))
            {
               r->touched = true;
               srect_write_query(r, w);
            }
            else
               srect_write_copy(r, w, n);
            break;
         case SRECT_OP_FUNCTION_CALL:
            {
               uint32_t mask = 0;
               uint32_t ops[64];
               unsigned a;
               if (n < 4 || n - 1 > 64)
               {
                  r->fail = true;
                  break;
               }
               for (a = 4; a < n; a++)
                  if (srect_tainted(r, srect_m_id(r, w[a])))
                  {
                     if (a - 4 >= 32)
                        r->fail = true;
                     else
                        mask |= 1u << (a - 4);
                  }
               if (!mask)
               {
                  srect_write_copy(r, w, n);
                  break;
               }
               ops[0] = w[1];
               ops[1] = srect_m_id(r, w[2]);
               if (!(ops[2] = srect_spec_get(r, w[3], mask)))
               {
                  r->fail = true;
                  break;
               }
               for (a = 4; a < n; a++)
                  ops[a - 1] = srect_m_id(r, w[a]);
               srect_rb_inst(r, &r->funcs, SRECT_OP_FUNCTION_CALL, ops, n - 1);
            }
            break;
         default:
            if (srect_uses_taint(r, w, n))
               r->fail = true;
            else
               srect_write_copy(r, w, n);
            break;
      }

      if (op == SRECT_OP_FUNCTION_END)
         break;
      p += n;
   }
   r->map = NULL;
}

/* ---- the module ---------------------------------------------------- */

/* Whether the string literal at @w, of at most @max words, is @s. */
static bool srect_str_is(const uint32_t *w, unsigned max, const char *s)
{
   size_t len = strlen(s) + 1;
   if ((len + 3) / 4 > max)
      return false;
   return !memcmp(w, s, len);
}

/* Words a string literal of at most @max words takes; 0 if it does not
 * end there. */
static unsigned srect_str_len(const uint32_t *w, unsigned max)
{
   unsigned i;
   for (i = 0; i < max; i++)
   {
      const unsigned char *b = (const unsigned char*)&w[i];
      if (!b[0] || !b[1] || !b[2] || !b[3])
         return i + 1;
   }
   return 0;
}

/* The word of an instruction that holds its result id, for def[]. */
static unsigned srect_def_pos(uint32_t op)
{
   unsigned rp;
   if (op == SRECT_OP_STRING || op == SRECT_OP_EXT_INST_IMPORT)
      return 1;
   if (op >= 19 && op <= 38) /* OpTypeVoid .. OpTypePipe */
      return 1;
   if (op >= 41 && op <= 52) /* OpConstantTrue .. OpSpecConstantOp */
      return 2;
   rp = srect_result_pos(op);
   return rp == 255 ? 0 : rp;
}

static bool srect_parse(srect_rect_t *r, const uint32_t *in, size_t len)
{
   size_t p;
   memset(r, 0, sizeof(*r));
   if (len < 5 || in[0] != SRECT_SPV_MAGIC || !in[3] || in[3] > (1u << 22))
      return false;
   r->in       = in;
   r->len      = len;
   r->version  = in[1];
   r->in_bound = in[3];
   r->bound    = in[3];
   if (!(r->def = (size_t*)calloc(r->in_bound, sizeof(*r->def))))
      return false;
   for (p = 5; p < len; )
   {
      const uint32_t *w  = in + p;
      unsigned        n  = SRECT_INST_LEN(w[0]);
      unsigned        dp;
      if (!n || p + n > len)
         return false;
      dp = srect_def_pos(SRECT_INST_OP(w[0]));
      if (dp && dp < n)
      {
         if (w[dp] >= r->in_bound)
            return false;
         r->def[w[dp]] = p;
      }
      p += n;
   }
   return true;
}

static void srect_rect_free(srect_rect_t *r)
{
   free(r->def);
   free(r->taint);
   free(r->map_buf);
   free(r->types.w);
   free(r->names.w);
   free(r->annots.w);
   free(r->funcs.w);
}

/* Marks the frame's variables: 2D, single-sampled, not arrayed, combined
 * sampled images. false for one this cannot read. */
static bool srect_find_frame(srect_rect_t *r, bool *any)
{
   size_t p;
   *any = false;
   for (p = 5; p < r->len; )
   {
      const uint32_t *w = r->in + p;
      unsigned        n = SRECT_INST_LEN(w[0]);
      if (SRECT_INST_OP(w[0]) == SRECT_OP_FUNCTION)
         break;
      if (SRECT_INST_OP(w[0]) == SRECT_OP_NAME && n >= 3 && w[1] < r->in_bound
            && (     srect_str_is(w + 2, n - 2, "Original")
                  || srect_str_is(w + 2, n - 2, "OriginalHistory0")
                  || (r->source && srect_str_is(w + 2, n - 2, "Source"))))
      {
         const uint32_t *v = srect_def_inst(r, w[1]);
         const uint32_t *ptr, *si, *img;
         if (!v || SRECT_INST_OP(v[0]) != SRECT_OP_VARIABLE)
         {
            p += n;
            continue;
         }
         if (v[3] != SRECT_SC_UNIFORM_CONSTANT)
            return false;
         ptr = srect_def_inst(r, v[1]);
         si  = ptr ? srect_def_inst(r, ptr[3]) : NULL;
         if (!si || SRECT_INST_OP(si[0]) != SRECT_OP_TYPE_SAMPLED_IMAGE)
            return false;
         img = srect_def_inst(r, si[2]);
         /* Dim 2D, not depth, not arrayed, not multisampled. */
         if (!img || SRECT_INST_LEN(img[0]) < 9 || img[3] != 1 || img[4] == 1
               || img[5] != 0 || img[6] != 0)
            return false;
         r->taint[w[1]] = 1;
         *any = true;
      }
      p += n;
   }
   return true;
}

static void srect_glsl450_find(srect_rect_t *r)
{
   size_t p;
   for (p = 5; p < r->len; )
   {
      const uint32_t *w = r->in + p;
      unsigned        n = SRECT_INST_LEN(w[0]);
      if (!srect_is_preamble(SRECT_INST_OP(w[0])) && SRECT_INST_OP(w[0]) != SRECT_OP_EXT_INST_IMPORT)
         break;
      if (SRECT_INST_OP(w[0]) == SRECT_OP_EXT_INST_IMPORT && n >= 3
            && srect_str_is(w + 2, n - 2, "GLSL.std.450"))
      {
         r->glsl450 = w[1];
         return;
      }
      p += n;
   }
   r->glsl450    = srect_new_id(r);
   r->new_import = true;
}

static void srect_put(srect_rect_t *r, srect_rbuf_t *o, const uint32_t *w, unsigned n)
{
   srect_rb_copy(r, o, w, n);
}

static void srect_put_buf(srect_rect_t *r, srect_rbuf_t *o, srect_rbuf_t *b, bool *done)
{
   if (*done)
      return;
   *done = true;
   if (b->n)
      srect_rb_copy(r, o, b->w, (unsigned)b->n);
}

static bool srect_assemble(srect_rect_t *r, uint32_t **out, size_t *out_len)
{
   srect_rbuf_t o;
   size_t p;
   bool   names_done  = false;
   bool   annots_done = false;
   bool   vec4_done   = false;

   memset(&o, 0, sizeof(o));
   srect_put(r, &o, r->in, 5);

   for (p = 5; p < r->len && !r->fail; )
   {
      const uint32_t *w  = r->in + p;
      unsigned        n  = SRECT_INST_LEN(w[0]);
      uint32_t        op = SRECT_INST_OP(w[0]);

      if (op == SRECT_OP_FUNCTION)
         break;

      if (srect_is_preamble(op))
      {
         if (op == SRECT_OP_MEMORY_MODEL && r->new_import)
         {
            uint32_t ops[8];
            unsigned k;
            ops[0] = r->glsl450;
            k      = srect_str_words("GLSL.std.450", ops + 1, 7);
            srect_rb_inst(r, &o, SRECT_OP_EXT_INST_IMPORT, ops, 1 + k);
         }
         if (op == SRECT_OP_ENTRY_POINT && r->version >= 0x00010400u && n >= 4)
         {
            /* From SPIR-V 1.4 the interface lists every global used. */
            unsigned s = srect_str_len(w + 3, n - 3);
            unsigned i;
            bool     listed = false;
            for (i = 3 + s; s && i < n; i++)
               if (w[i] == r->blk_var)
                  listed = true;
            srect_put(r, &o, w, n);
            if (s && !listed)
            {
               o.w[o.n - n] += 1u << 16;
               srect_rb_copy(r, &o, &r->blk_var, 1);
            }
         }
         else
            srect_put(r, &o, w, n);
      }
      else if (srect_is_debug(op))
      {
         if (op == SRECT_OP_MODULE_PROCESSED)
            srect_put_buf(r, &o, &r->names, &names_done);
         srect_put(r, &o, w, n);
      }
      else if (srect_is_annotation(op))
      {
         srect_put_buf(r, &o, &r->names, &names_done);
         srect_put(r, &o, w, n);
      }
      else
      {
         srect_put_buf(r, &o, &r->names, &names_done);
         srect_put_buf(r, &o, &r->annots, &annots_done);
         if (!vec4_done)
         {
            /* float and vec4 first: the push block takes vec4s. */
            vec4_done = true;
            srect_put(r, &o, r->in + r->def[r->t_float], 3);
            srect_put(r, &o, r->in + r->def[r->t_v4f], 4);
         }
         if (     (op == SRECT_OP_TYPE_FLOAT  && w[1] == r->t_float)
               || (op == SRECT_OP_TYPE_VECTOR && w[1] == r->t_v4f))
            ;
         else if (op == SRECT_OP_TYPE_STRUCT && w[1] == r->blk_struct && !r->blk_new)
         {
            uint32_t v4[3];
            v4[0] = v4[1] = v4[2] = r->t_v4f;
            srect_put(r, &o, w, n);
            o.w[o.n - n] += 3u << 16;
            srect_rb_copy(r, &o, v4, 3);
         }
         else
            srect_put(r, &o, w, n);
      }
      p += n;
   }

   srect_put_buf(r, &o, &r->names, &names_done);
   srect_put_buf(r, &o, &r->annots, &annots_done);
   srect_rb_copy(r, &o, r->types.w, (unsigned)r->types.n);
   srect_rb_copy(r, &o, r->funcs.w, (unsigned)r->funcs.n);

   if (r->fail || !vec4_done)
   {
      free(o.w);
      return false;
   }
   o.w[3]   = r->bound;
   *out     = o.w;
   *out_len = o.n;
   return true;
}

static void srect_place_set(srect_rect_t *r, const struct slang_rect_place *place)
{
   r->place = *place;
   r->sc    = place->where == SLANG_RECT_UBO ? SRECT_SC_UNIFORM : SRECT_SC_PUSH_CONSTANT;
}

bool slang_rect_uniform_block(const uint32_t *in, size_t in_len,
      unsigned *set, unsigned *binding)
{
   srect_rect_t r;
   size_t p;
   bool   found = false;
   if (srect_parse(&r, in, in_len))
   {
      for (p = 5; p < r.len; p += SRECT_INST_LEN(r.in[p]))
      {
         const uint32_t *w = r.in + p;
         const uint32_t *ptr;
         if (SRECT_INST_OP(w[0]) == SRECT_OP_FUNCTION)
            break;
         if (     SRECT_INST_OP(w[0]) != SRECT_OP_VARIABLE || SRECT_INST_LEN(w[0]) < 4
               || w[3] != SRECT_SC_UNIFORM
               || !(ptr = srect_def_inst(&r, w[1]))
               || SRECT_INST_OP(ptr[0]) != SRECT_OP_TYPE_POINTER
               || !srect_has_decoration(&r, ptr[3], SRECT_DEC_BLOCK))
            continue;
         *set     = srect_decoration(&r, w[2], ~0u, SRECT_DEC_DESCRIPTOR_SET);
         *binding = srect_decoration(&r, w[2], ~0u, SRECT_DEC_BINDING);
         if (*set == ~0u)
            *set = 0;
         found = *binding != ~0u;
         break;
      }
   }
   srect_rect_free(&r);
   return found;
}

uint32_t slang_rect_block_end(const uint32_t *in, size_t in_len,
      const struct slang_rect_place *place)
{
   srect_rect_t   r;
   uint32_t end = ~0u;
   if (srect_parse(&r, in, in_len))
   {
      srect_place_set(&r, place);
      if (srect_block_find(&r))
         end = r.blk_var ? r.blk_end : 0;
   }
   srect_rect_free(&r);
   return end;
}

enum slang_rect_result slang_rect_remap(const uint32_t *in, size_t in_len,
      bool source, bool linear, enum slang_rect_wrap wrap,
      const struct slang_rect_place *place, uint32_t **out, size_t *out_len)
{
   srect_rect_t                 r;
   size_t                 p;
   unsigned               i;
   bool                   any = false;
   enum slang_rect_result res = SLANG_RECT_UNSUPPORTED;

   if (!srect_parse(&r, in, in_len))
      goto end;
   r.source    = source;
   r.linear    = linear;
   r.wrap      = wrap;
   srect_place_set(&r, place);
   r.taint_cap = r.in_bound < 64 ? 64 : r.in_bound;
   if (!(r.taint = (uint8_t*)calloc(r.taint_cap, 1)))
      goto end;
   if (!srect_find_frame(&r, &any))
      goto end;
   if (!any)
   {
      res = SLANG_RECT_UNCHANGED;
      goto end;
   }

   if (     !(r.map_buf = (uint32_t*)calloc(r.in_bound, sizeof(*r.map_buf)))
         || !srect_types_init(&r)
         || !srect_block_find(&r))
      goto end;
   srect_glsl450_find(&r);
   srect_consts_init(&r);
   if (!srect_block_make(&r))
      goto end;

   /* Every function as it is, then the ones the frame is passed to,
    * each way it is; those may pass it on further. */
   for (p = 5; p < r.len && !r.fail; )
   {
      const uint32_t *w = r.in + p;
      if (SRECT_INST_OP(w[0]) == SRECT_OP_FUNCTION)
      {
         srect_write_function(&r, p, NULL);
         while (p < r.len && SRECT_INST_OP(r.in[p]) != SRECT_OP_FUNCTION_END)
            p += SRECT_INST_LEN(r.in[p]);
         if (p >= r.len)
            break;
      }
      p += SRECT_INST_LEN(r.in[p]);
   }
   for (i = 0; i < r.num_specs && !r.fail; i++)
      srect_write_function(&r, r.def[r.specs[i].fid], &r.specs[i]);

   if (r.fail)
      goto end;
   if (!r.touched)
   {
      res = SLANG_RECT_UNCHANGED;
      goto end;
   }
   if (srect_assemble(&r, out, out_len))
      res = SLANG_RECT_REWRITTEN;

end:
   srect_rect_free(&r);
   return res;
}
