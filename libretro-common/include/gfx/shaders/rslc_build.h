/* Copyright  (C) 2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rslc_build.h).
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

#ifndef __LIBRETRO_SDK_RSLC_BUILD_H__
#define __LIBRETRO_SDK_RSLC_BUILD_H__

/* rslc builder: a stage made by calls instead of parsed from text.
 *
 * A program that generates its shaders builds the IR directly and asks
 * for any rslc destination; nothing is printed or parsed on the way.
 * The builder applies the parser's typing: implicit conversions,
 * builtin overload resolution and constant folding are the same, so a
 * stage built here and the same stage written as GLSL compile to the
 * same words.  Only in a build with RSLC_BUILD defined.
 *
 * Expressions and statements are node ids; 0 means the call failed,
 * the message is in opts->err and every later call returns 0 too, so
 * a caller checks once, at rslc_b_end. */

#include <stdint.h>
#include <stddef.h>
#include <boolean.h>
#include <retro_common_api.h>

#include <gfx/shaders/rslc.h>

RETRO_BEGIN_DECLS

struct rslc_builder;
struct rslc_type;
struct rslc_var;
struct rslc_func;

/* Operators, by the parser's numbering. */
enum rslc_b_unop
{
   RSLC_B_NEG = 1, RSLC_B_NOT, RSLC_B_BITNOT,
   RSLC_B_PREINC, RSLC_B_PREDEC, RSLC_B_POSTINC, RSLC_B_POSTDEC
};

enum rslc_b_binop
{
   RSLC_B_SET = 0,   /* plain assignment, for rslc_b_assign */
   RSLC_B_ADD, RSLC_B_SUB, RSLC_B_MUL, RSLC_B_DIV, RSLC_B_MOD,
   RSLC_B_SHL, RSLC_B_SHR,
   RSLC_B_LT, RSLC_B_GT, RSLC_B_LE, RSLC_B_GE, RSLC_B_EQ, RSLC_B_NE,
   RSLC_B_AND, RSLC_B_OR, RSLC_B_XOR,
   RSLC_B_BAND, RSLC_B_BOR, RSLC_B_BXOR
};

enum rslc_b_interp
{
   RSLC_B_INTERP_DEFAULT = 0,
   RSLC_B_INTERP_FLAT,
   RSLC_B_INTERP_NOPERSPECTIVE,
   RSLC_B_INTERP_SMOOTH
};

#define RSLC_B_CENTROID 1u
#define RSLC_B_SAMPLE   2u
#define RSLC_B_INVARIANT 4u

/* Parameter directions. */
#define RSLC_B_IN    0
#define RSLC_B_OUT   1
#define RSLC_B_INOUT 2

/* ---- lifetime ---- */

struct rslc_builder *rslc_b_begin(const struct rslc_opts *opts, enum rslc_stage stage);
/* Emits opts->dst into result and frees the builder; false with
 * opts->err filled when anything failed. */
bool rslc_b_end(struct rslc_builder *b, struct rslc_result *result);
/* Frees the builder without emitting. */
void rslc_b_abort(struct rslc_builder *b);

/* ---- types ---- */

const struct rslc_type *rslc_b_void(struct rslc_builder *b);
/* rows 1: scalar; 2-4: vector */
const struct rslc_type *rslc_b_vec(struct rslc_builder *b, enum rslc_base base, unsigned rows);
const struct rslc_type *rslc_b_mat(struct rslc_builder *b, unsigned cols, unsigned rows);
const struct rslc_type *rslc_b_array(struct rslc_builder *b, const struct rslc_type *elem,
      uint32_t len);
const struct rslc_type *rslc_b_struct(struct rslc_builder *b, const char *name,
      const struct rslc_type *const *types, const char *const *names, uint32_t n);
const struct rslc_type *rslc_b_sampler2d(struct rslc_builder *b, enum rslc_base base);

/* ---- declarations ---- */

struct rslc_var *rslc_b_input(struct rslc_builder *b, const char *name,
      const struct rslc_type *type, uint32_t location, enum rslc_b_interp interp,
      unsigned quals);
struct rslc_var *rslc_b_output(struct rslc_builder *b, const char *name,
      const struct rslc_type *type, uint32_t location, enum rslc_b_interp interp,
      unsigned quals);
struct rslc_var *rslc_b_sampler(struct rslc_builder *b, const char *name,
      const struct rslc_type *type, uint32_t set, uint32_t binding);
/* The uniform block (push false) or the push-constant block.  With an
 * instance name the block is reached through the returned variable;
 * without one, members[] receives a variable per member. */
struct rslc_var *rslc_b_block(struct rslc_builder *b, bool push, bool std140,
      const char *type_name, const char *instance, uint32_t set, uint32_t binding,
      const struct rslc_type *const *types, const char *const *names, uint32_t n,
      struct rslc_var **members);
/* gl_Position, gl_PointSize, gl_VertexIndex, gl_InstanceIndex,
 * gl_FragCoord, gl_FragDepth, gl_FrontFacing; with RSLC_EXTENDED every
 * builtin the parser gives the stage, and a geometry stage's gl_in once
 * its input primitive is laid out. */
struct rslc_var *rslc_b_builtin_var(struct rslc_builder *b, const char *name);
/* A file-scope variable; constant when is_const and init folds. */
struct rslc_var *rslc_b_global(struct rslc_builder *b, const char *name,
      const struct rslc_type *type, uint32_t init, bool is_const);
/* A local: declared, and the declaration appended to the open block. */
struct rslc_var *rslc_b_local(struct rslc_builder *b, const char *name,
      const struct rslc_type *type, uint32_t init, bool is_const);

/* ---- functions ---- */

struct rslc_func *rslc_b_func(struct rslc_builder *b, const char *name,
      const struct rslc_type *ret, uint32_t nparams,
      const struct rslc_type *const *types, const unsigned *dirs, const char *const *names);
struct rslc_var *rslc_b_param(const struct rslc_func *f, uint32_t i);
/* Opens f's body as the current block; rslc_b_func_end closes it. */
bool rslc_b_func_begin(struct rslc_builder *b, struct rslc_func *f);
bool rslc_b_func_end(struct rslc_builder *b);

/* ---- expressions ---- */

uint32_t rslc_b_int(struct rslc_builder *b, int32_t v);
uint32_t rslc_b_uint(struct rslc_builder *b, uint32_t v);
uint32_t rslc_b_float(struct rslc_builder *b, double v);
uint32_t rslc_b_bool(struct rslc_builder *b, bool v);
uint32_t rslc_b_ref(struct rslc_builder *b, const struct rslc_var *v);
uint32_t rslc_b_member(struct rslc_builder *b, uint32_t e, uint32_t field);
uint32_t rslc_b_index(struct rslc_builder *b, uint32_t e, uint32_t idx);
/* "xyz", "rgba", "stpq" */
uint32_t rslc_b_swizzle(struct rslc_builder *b, uint32_t e, const char *s);
uint32_t rslc_b_unary(struct rslc_builder *b, enum rslc_b_unop op, uint32_t e);
uint32_t rslc_b_binary(struct rslc_builder *b, enum rslc_b_binop op, uint32_t l, uint32_t r);
uint32_t rslc_b_select(struct rslc_builder *b, uint32_t cond, uint32_t a, uint32_t c);
uint32_t rslc_b_assign(struct rslc_builder *b, enum rslc_b_binop op, uint32_t l, uint32_t r);
uint32_t rslc_b_construct(struct rslc_builder *b, const struct rslc_type *type,
      const uint32_t *args, uint32_t n);
uint32_t rslc_b_call(struct rslc_builder *b, const struct rslc_func *f,
      const uint32_t *args, uint32_t n);
/* A GLSL builtin function by name: "dot", "texture", "mix" ... */
uint32_t rslc_b_builtin(struct rslc_builder *b, const char *name,
      const uint32_t *args, uint32_t n);

/* ---- statements ---- */

/* Every maker returns a statement for an if, loop or switch to take,
 * or for rslc_b_stmt to append to the innermost open block. */
bool     rslc_b_begin_block(struct rslc_builder *b);
uint32_t rslc_b_end_block(struct rslc_builder *b);
bool     rslc_b_stmt(struct rslc_builder *b, uint32_t stmt);
uint32_t rslc_b_expr(struct rslc_builder *b, uint32_t e);
/* else_stmt is 0 for none */
uint32_t rslc_b_if(struct rslc_builder *b, uint32_t cond, uint32_t then_stmt,
      uint32_t else_stmt);
/* init: a statement or 0; cond, iter: expressions or 0 */
uint32_t rslc_b_for(struct rslc_builder *b, uint32_t init, uint32_t cond, uint32_t iter,
      uint32_t body);
uint32_t rslc_b_while(struct rslc_builder *b, uint32_t cond, uint32_t body);
uint32_t rslc_b_do(struct rslc_builder *b, uint32_t body, uint32_t cond);
/* body: a block holding rslc_b_case labels among its statements */
uint32_t rslc_b_switch(struct rslc_builder *b, uint32_t e, uint32_t body);
uint32_t rslc_b_case(struct rslc_builder *b, int32_t value);
uint32_t rslc_b_default(struct rslc_builder *b);
uint32_t rslc_b_break(struct rslc_builder *b);
uint32_t rslc_b_continue(struct rslc_builder *b);
uint32_t rslc_b_discard(struct rslc_builder *b);
/* e is 0 for a void return */
uint32_t rslc_b_return(struct rslc_builder *b, uint32_t e);
/* A local's declaration, not appended: a for loop's init. */
uint32_t rslc_b_decl_stmt(struct rslc_builder *b, const char *name,
      const struct rslc_type *type, uint32_t init, struct rslc_var **out);

/* ---- RSLC_EXTENDED: the stages, types and storage an emulator's
 * shaders use.  Present only in a build with RSLC_EXTENDED defined
 * too; each makes what the same GLSL declaration makes. ---- */

enum rslc_b_sampler_kind
{
   RSLC_B_SAMPLER_2D = 0,
   RSLC_B_SAMPLER_2D_ARRAY,
   RSLC_B_SAMPLER_2D_MS_ARRAY,
   RSLC_B_SAMPLER_BUFFER,
   RSLC_B_IMAGE_2D,        /* its format and access are the declaration's */
   RSLC_B_IMAGE_2D_ARRAY,
   RSLC_B_SUBPASS_INPUT
};

/* Where a declaration lives. */
enum rslc_b_storage
{
   RSLC_B_ST_GLOBAL = 0,
   RSLC_B_ST_CONST,
   RSLC_B_ST_IN,
   RSLC_B_ST_OUT,
   RSLC_B_ST_UNIFORM,      /* a sampler, image, subpass input or uniform block */
   RSLC_B_ST_BUFFER,       /* a storage block */
   RSLC_B_ST_SHARED        /* a compute stage's shared memory */
};

/* A geometry stage's input and output primitives. */
enum rslc_b_prim
{
   RSLC_B_PRIM_NONE = 0,
   RSLC_B_PRIM_POINTS,
   RSLC_B_PRIM_LINES,
   RSLC_B_PRIM_LINES_ADJACENCY,
   RSLC_B_PRIM_TRIANGLES,
   RSLC_B_PRIM_TRIANGLES_ADJACENCY,
   RSLC_B_PRIM_LINE_STRIP,
   RSLC_B_PRIM_TRIANGLE_STRIP
};

/* Access of an image or a storage block. */
#define RSLC_B_READONLY  1u
#define RSLC_B_WRITEONLY 2u
#define RSLC_B_COHERENT  4u

/* What a declaration's qualifiers and layout say.  rslc_b_quals_init
 * leaves every number unstated and every flag clear. */
struct rslc_b_quals
{
   const char *format;            /* a storage image's, by its GLSL name ("rgba8") */
   uint32_t    location;          /* RSLC_NO_BINDING where not stated */
   uint32_t    binding;
   uint32_t    set;
   uint32_t    index;             /* a fragment output's: 0 or 1 */
   uint32_t    input_attachment;
   uint32_t    local_size[3];     /* 0 where not stated */
   uint32_t    invocations;
   uint32_t    max_vertices;
   unsigned    storage;           /* enum rslc_b_storage */
   unsigned    interp;            /* enum rslc_b_interp */
   unsigned    quals;             /* RSLC_B_CENTROID, RSLC_B_SAMPLE, RSLC_B_INVARIANT */
   unsigned    access;            /* RSLC_B_READONLY, RSLC_B_WRITEONLY, RSLC_B_COHERENT */
   unsigned    prim;              /* enum rslc_b_prim */
   bool        push_constant;
   bool        std140;
   bool        early_fragment_tests;
};

void rslc_b_quals_init(struct rslc_b_quals *q);

/* A sampler, image or subpass input type of base float, int or uint.
 * rslc_b_array with len 0 is an array of unstated length: a geometry
 * input's, or a storage block's last member. */
const struct rslc_type *rslc_b_sampler_type(struct rslc_builder *b,
      enum rslc_b_sampler_kind kind, enum rslc_base base);

/* A file-scope declaration of any storage but a block's. */
struct rslc_var *rslc_b_declare(struct rslc_builder *b, const struct rslc_b_quals *q,
      const char *name, const struct rslc_type *type, uint32_t init);
/* A uniform (q->storage RSLC_B_ST_UNIFORM, or push_constant) or storage
 * (RSLC_B_ST_BUFFER) block; as rslc_b_block otherwise. */
struct rslc_var *rslc_b_block_q(struct rslc_builder *b, const struct rslc_b_quals *q,
      const char *type_name, const char *instance,
      const struct rslc_type *const *types, const char *const *names, uint32_t n,
      struct rslc_var **members);
/* An in or out block, its members with their own interpolation and
 * qualifiers (interps and mquals may be NULL).  The instance is
 * returned whether named or not; a member is rslc_b_member of a
 * reference to it.  array_len -1 for a single instance, 0 for a
 * geometry input's, as long as the input primitive. */
struct rslc_var *rslc_b_io_block(struct rslc_builder *b, const struct rslc_b_quals *q,
      const char *type_name, const char *instance,
      const struct rslc_type *const *types, const char *const *names,
      const unsigned *interps, const unsigned *mquals, uint32_t n, int32_t array_len);
/* A layout on the stage's in or out alone: early_fragment_tests, a
 * local size, a geometry primitive, invocations or max_vertices. */
bool rslc_b_layout(struct rslc_builder *b, const struct rslc_b_quals *q);

RETRO_END_DECLS

#endif
