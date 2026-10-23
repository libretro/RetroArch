/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rslc_emit.c).
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

/* rslc TU 3: GLSL / GLSL ES / HLSL / MSL printers.
 *
 * One structured walker over the IR with a skin per language.  A
 * Vulkan-only build leaves this unit out and defines RSLC_NO_TEXT, and
 * rslc_ir.c refuses the textual destinations itself.
 *
 * The GLSL skin prints what the SPIR-V emitter encodes: constant
 * expressions appear already folded (the GL compiler must not fold
 * them again in its own precision), only functions main reaches are
 * printed, and the interface is named as the caller asks through
 * opts->names so a driver can bind by name.  Push constants have no
 * GLSL form; the push block becomes a struct type and one uniform
 * of it, and members are reached through that instance. */

#include <stdio.h>   /* sprintf: a float's nine digits and a version number */
#include <string.h>

#include "rslc_share.h"

struct rslc_pr
{
   struct rslc_ctx         *c;
   struct rslc_pbuf         out;
   const struct rslc_names *names;
   uint8_t                 *reach;
   uint8_t                 *active[2];
   unsigned                 version;
   unsigned                 indent;
   unsigned                 sampler_prec_done;
   bool                     es;
   bool                     ok;
   bool                     uses_packing;   /* a pack or unpack builtin in reachable code */
   bool                     uses_gather;    /* textureGather in reachable code */
   bool                     uses_gather_comp; /* ... with a component other than 0 */
};

/* ---- output ---- */

static void pr_s(struct rslc_pr *p, const char *s)
{
   if (p->ok && !rslc_pbuf_puts(p->c, &p->out, s))
      p->ok = false;
}

static void pr_c(struct rslc_pr *p, char ch)
{
   if (p->ok && !rslc_pbuf_putc(p->c, &p->out, ch))
      p->ok = false;
}

static void pr_u(struct rslc_pr *p, uint32_t v)
{
   if (p->ok && !rslc_pbuf_putu(p->c, &p->out, v))
      p->ok = false;
}

static void pr_nl(struct rslc_pr *p)
{
   unsigned i;
   pr_c(p, '\n');
   for (i = 0; i < p->indent; i++)
      pr_s(p, "   ");
}

/* A float as a literal that reads back to the same float: nine
 * significant digits, always with a point or an exponent. */
static void pr_float(struct rslc_pr *p, float f)
{
   char buf[48];
   size_t i, n;
   bool has_point = false;
   if (f != f)
   {
      pr_s(p, "(0.0 / 0.0)");
      return;
   }
   if (f > 3.4e38f || f < -3.4e38f)
   {
      pr_s(p, f < 0 ? "(-1.0 / 0.0)" : "(1.0 / 0.0)");
      return;
   }
   n = (size_t)sprintf(buf, "%.9g", (double)f);   /* at most 16 chars */
   for (i = 0; i < n; i++)
      if (buf[i] == '.' || buf[i] == 'e')
         has_point = true;
   if (!has_point)
   {
      buf[n++] = '.';
      buf[n++] = '0';
      buf[n]   = '\0';
   }
   pr_s(p, buf);
}

static void pr_int(struct rslc_pr *p, int32_t v)
{
   char buf[24];
   if (v == (-2147483647 - 1))
      strcpy(buf, "(-2147483647 - 1)");
   else
      sprintf(buf, "%d", (int)v);
   pr_s(p, buf);
}

/* ---- types ---- */

static void pr_type(struct rslc_pr *p, const struct rslc_type *t);
static void pr_ident(struct rslc_pr *p, const struct rslc_sym *name);

static void pr_scalar_prefix(struct rslc_pr *p, unsigned base)
{
   switch (base)
   {
      case RSLC_BASE_BOOL: pr_c(p, 'b'); break;
      case RSLC_BASE_INT:  pr_c(p, 'i'); break;
      case RSLC_BASE_UINT: pr_c(p, 'u'); break;
      default: break;
   }
}

static void pr_type(struct rslc_pr *p, const struct rslc_type *t)
{
   switch (t->kind)
   {
      case RSLC_TY_VOID:
         pr_s(p, "void");
         break;
      case RSLC_TY_SCALAR:
         switch (t->base)
         {
            case RSLC_BASE_BOOL: pr_s(p, "bool"); break;
            case RSLC_BASE_INT:  pr_s(p, "int"); break;
            case RSLC_BASE_UINT: pr_s(p, "uint"); break;
            default:             pr_s(p, "float"); break;
         }
         break;
      case RSLC_TY_VEC:
         pr_scalar_prefix(p, t->base);
         pr_s(p, "vec");
         pr_u(p, t->rows);
         break;
      case RSLC_TY_MAT:
         pr_s(p, "mat");
         pr_u(p, t->cols);
         if (t->rows != t->cols)
         {
            pr_c(p, 'x');
            pr_u(p, t->rows);
         }
         break;
      case RSLC_TY_SAMPLER2D:
         pr_scalar_prefix(p, t->base);
         pr_s(p, "sampler2D");
         break;
      case RSLC_TY_STRUCT:
         if (t->name)
            pr_ident(p, t->name);
         else
            pr_s(p, "anon");
         break;
      case RSLC_TY_ARRAY:
         pr_type(p, t->elem);
         break;
   }
}

/* The array suffix of a declarator: name[N][M]. */
static void pr_array_suffix(struct rslc_pr *p, const struct rslc_type *t)
{
   while (t->kind == RSLC_TY_ARRAY)
   {
      pr_c(p, '[');
      pr_u(p, t->array_len);
      pr_c(p, ']');
      t = t->elem;
   }
}

/* type[N] for constructor syntax */
static void pr_type_full(struct rslc_pr *p, const struct rslc_type *t)
{
   pr_type(p, t);
   pr_array_suffix(p, t);
}

static void pr_precision(struct rslc_pr *p, unsigned prec, const struct rslc_type *t)
{
   const struct rslc_type *bt = t;
   if (!p->es)
      return;
   while (bt->kind == RSLC_TY_ARRAY)
      bt = bt->elem;
   if (bt->kind == RSLC_TY_STRUCT || bt->kind == RSLC_TY_VOID || bt->base == RSLC_BASE_BOOL)
      return;
   switch (prec)
   {
      case RSLC_PREC_LOW:    pr_s(p, "lowp "); break;
      case RSLC_PREC_MEDIUM: pr_s(p, "mediump "); break;
      case RSLC_PREC_HIGH:   pr_s(p, "highp "); break;
      default: break;
   }
}

/* ---- names ---- */

static const char *pr_block_instance(struct rslc_pr *p, unsigned block)
{
   const struct rslc_blockdecl *b = &p->c->prog.blocks[block];
   const char *forced = NULL;
   if (p->names)
      forced = b->push ? p->names->push_instance : p->names->ubo_instance;
   if (forced)
      return forced;
   return b->instance ? b->instance->str : NULL;
}

/* Identifiers the shader may use that the printed language reserves
 * or that collide with a builtin function: GLSL ES forbids redefining
 * builtins, and a few words are keywords only outside Vulkan GLSL. */
static bool pr_name_reserved(struct rslc_pr *p, const struct rslc_sym *name)
{
   static const char *const words[] = {
      "input", "output", "filter", "sizeof", "cast", "namespace", "using",
      "sample", "subroutine", "patch", "common", "partition", "active",
      "asm", "class", "union", "enum", "typedef", "template", "this",
      "resource", "goto", "inline", "noinline", "public", "static", "extern",
      "external", "interface", "long", "short", "half", "fixed", "unsigned",
      "superp", "hvec2", "hvec3", "hvec4", "fvec2", "fvec3", "fvec4",
      "sampler3DRect", "texture", "precise", "coherent", "restrict",
      "readonly", "writeonly", "buffer", "shared", "image1D", "image2D",
      "atomic_uint", "double", "dvec2", "dvec3", "dvec4", "dmat2", "dmat3",
      "dmat4", NULL
   };
   unsigned i;
   if (name->bfn)
      return true;   /* a builtin function's name */
   for (i = 0; words[i]; i++)
      if (!strcmp(words[i], name->str))
         return true;
   (void)p;
   return false;
}

static void pr_ident(struct rslc_pr *p, const struct rslc_sym *name)
{
   pr_s(p, name->str);
   if (pr_name_reserved(p, name))
      pr_c(p, '_');
}

static void pr_var_name(struct rslc_pr *p, const struct rslc_var *v)
{
   const struct rslc_names *nm = p->names;
   switch (v->storage)
   {
      case RSLC_ST_IN:
         if (nm && p->c->stage == RSLC_STAGE_VERTEX && nm->attr_prefix && v->location != RSLC_UNSET)
         {
            pr_s(p, nm->attr_prefix);
            pr_u(p, v->location);
            return;
         }
         if (nm && p->c->stage == RSLC_STAGE_FRAGMENT && nm->varying_prefix && v->location != RSLC_UNSET)
         {
            pr_s(p, nm->varying_prefix);
            pr_u(p, v->location);
            return;
         }
         break;
      case RSLC_ST_OUT:
         if (nm && p->c->stage == RSLC_STAGE_VERTEX && nm->varying_prefix && v->location != RSLC_UNSET)
         {
            pr_s(p, nm->varying_prefix);
            pr_u(p, v->location);
            return;
         }
         break;
      case RSLC_ST_SAMPLER:
         if (nm && nm->sampler_prefix && v->binding != RSLC_UNSET)
         {
            pr_s(p, nm->sampler_prefix);
            pr_u(p, v->binding);
            return;
         }
         break;
      case RSLC_ST_BLOCK:
      {
         const char *inst = pr_block_instance(p, v->block);
         pr_s(p, inst ? inst : v->name->str);
         return;
      }
      case RSLC_ST_MEMBER:
      {
         const char *inst = pr_block_instance(p, v->block);
         if (inst)
         {
            pr_s(p, inst);
            pr_c(p, '.');
         }
         pr_s(p, p->c->prog.blocks[v->block].type->fields[v->member].name->str);
         return;
      }
      case RSLC_ST_BUILTIN:
         switch (v->builtin)
         {
            case RSLC_BI_VERTEXINDEX:   pr_s(p, "gl_VertexID"); return;
            case RSLC_BI_INSTANCEINDEX: pr_s(p, "gl_InstanceID"); return;
            default: break;
         }
         pr_s(p, v->name->str);
         return;
      default:
         break;
   }
   if (v->name)
      pr_ident(p, v->name);
   else
      pr_c(p, '_');
}

/* ---- constants ---- */

static void pr_cval(struct rslc_pr *p, const struct rslc_type *t, const struct rslc_cval *cv)
{
   unsigned i, n = rslc_ty_components(t);
   if (t->kind != RSLC_TY_SCALAR)
   {
      pr_type(p, t);
      pr_c(p, '(');
   }
   for (i = 0; i < n; i++)
   {
      if (i)
         pr_s(p, ", ");
      switch (t->base)
      {
         case RSLC_BASE_BOOL: pr_s(p, cv->v[i] ? "true" : "false"); break;
         case RSLC_BASE_INT:  pr_int(p, (int32_t)cv->v[i]); break;
         case RSLC_BASE_UINT: pr_u(p, cv->v[i]); pr_c(p, 'u'); break;
         default:
         {
            float f;
            memcpy(&f, &cv->v[i], sizeof(f));
            pr_float(p, f);
            break;
         }
      }
   }
   if (t->kind != RSLC_TY_SCALAR)
      pr_c(p, ')');
}

/* ---- expressions ---- */

/* Precedence levels, lower binds tighter. */
#define PR_PREC_POSTFIX 1
#define PR_PREC_UNARY   2
#define PR_PREC_MUL     3
#define PR_PREC_ADD     4
#define PR_PREC_SHIFT   5
#define PR_PREC_REL     6
#define PR_PREC_EQ      7
#define PR_PREC_BAND    8
#define PR_PREC_BXOR    9
#define PR_PREC_BOR     10
#define PR_PREC_AND     11
#define PR_PREC_XOR     12
#define PR_PREC_OR      13
#define PR_PREC_COND    14
#define PR_PREC_ASSIGN  15
#define PR_PREC_SEQ     16

static void pr_expr(struct rslc_pr *p, uint32_t node, int ctx);

static const char *pr_binop_str(uint16_t op, int *prec)
{
   switch (op)
   {
      case RSLC_BIN_ADD:    *prec = PR_PREC_ADD;   return "+";
      case RSLC_BIN_SUB:    *prec = PR_PREC_ADD;   return "-";
      case RSLC_BIN_MUL:    *prec = PR_PREC_MUL;   return "*";
      case RSLC_BIN_DIV:    *prec = PR_PREC_MUL;   return "/";
      case RSLC_BIN_MOD:    *prec = PR_PREC_MUL;   return "%";
      case RSLC_BIN_LSHIFT: *prec = PR_PREC_SHIFT; return "<<";
      case RSLC_BIN_RSHIFT: *prec = PR_PREC_SHIFT; return ">>";
      case RSLC_BIN_LT:     *prec = PR_PREC_REL;   return "<";
      case RSLC_BIN_GT:     *prec = PR_PREC_REL;   return ">";
      case RSLC_BIN_LE:     *prec = PR_PREC_REL;   return "<=";
      case RSLC_BIN_GE:     *prec = PR_PREC_REL;   return ">=";
      case RSLC_BIN_EQ:     *prec = PR_PREC_EQ;    return "==";
      case RSLC_BIN_NE:     *prec = PR_PREC_EQ;    return "!=";
      case RSLC_BIN_AND:    *prec = PR_PREC_AND;   return "&&";
      case RSLC_BIN_OR:     *prec = PR_PREC_OR;    return "||";
      case RSLC_BIN_XOR:    *prec = PR_PREC_XOR;   return "^^";
      case RSLC_BIN_BAND:   *prec = PR_PREC_BAND;  return "&";
      case RSLC_BIN_BOR:    *prec = PR_PREC_BOR;   return "|";
      case RSLC_BIN_BXOR:   *prec = PR_PREC_BXOR;  return "^";
      default:              *prec = PR_PREC_SEQ;   return "?";
   }
}

static void pr_args(struct rslc_pr *p, uint32_t list)
{
   uint32_t i, n = list ? rslc_list_len(p->c, list) : 0;
   pr_c(p, '(');
   for (i = 0; i < n; i++)
   {
      if (i)
         pr_s(p, ", ");
      pr_expr(p, rslc_list_item(p->c, list, i), PR_PREC_ASSIGN);
   }
   pr_c(p, ')');
}

/* Whether a folded value stands in for the node: the same rule the
 * SPIR-V emitter applies, so both outputs carry the same bits. */
static bool pr_fold(struct rslc_pr *p, const struct rslc_node *n, uint32_t node, struct rslc_cval *cv)
{
   if (!n->type)
      return false;
   if (n->op == RSLC_OP_CONST || n->op == RSLC_OP_VAR)
   {
      if (n->op == RSLC_OP_VAR)
      {
         const struct rslc_var *v = p->c->prog.vars[n->a];
         if (v->storage != RSLC_ST_CONST || !v->init || v->global)
            return false;   /* a global const keeps its name */
      }
   }
   else if (n->op != RSLC_OP_CONSTRUCT && n->op != RSLC_OP_UNARY && n->op != RSLC_OP_BINARY
         && n->op != RSLC_OP_SWIZZLE && n->op != RSLC_OP_CONVERT && n->op != RSLC_OP_BUILTIN
         && n->op != RSLC_OP_MEMBER && n->op != RSLC_OP_INDEX)
      return false;
   if (n->type->kind != RSLC_TY_SCALAR && n->type->kind != RSLC_TY_VEC && n->type->kind != RSLC_TY_MAT)
      return false;
   return rslc_const_eval(p->c, node, cv);
}

static void pr_builtin(struct rslc_pr *p, const struct rslc_node *n)
{
   unsigned bfn = n->a;
   /* fma() needs GLSL 4.00 / ES 3.20; below that it is written out */
   if (bfn == RSLC_BFN_FMA && ((!p->es && p->version < 400) || (p->es && p->version < 320)))
   {
      pr_c(p, '(');
      pr_expr(p, rslc_list_item(p->c, n->b, 0), PR_PREC_MUL);
      pr_s(p, " * ");
      pr_expr(p, rslc_list_item(p->c, n->b, 1), PR_PREC_MUL - 1);
      pr_s(p, " + ");
      pr_expr(p, rslc_list_item(p->c, n->b, 2), PR_PREC_ADD - 1);
      pr_c(p, ')');
      return;
   }
   if ((bfn == RSLC_BFN_TEXTUREGATHER || bfn == RSLC_BFN_TEXTUREGATHEROFFSET)
         && (!p->es && p->version < 400))
   {
      /* the component argument needs ARB_gpu_shader5; a constant 0 is
       * the default and is left out, as GL_ARB_texture_gather allows */
      uint32_t cnt  = rslc_list_len(p->c, n->b);
      uint32_t last = cnt ? rslc_list_item(p->c, n->b, cnt - 1) : 0;
      struct rslc_cval cv;
      unsigned base_args = bfn == RSLC_BFN_TEXTUREGATHER ? 2 : 3;
      if (cnt == base_args + 1 && rslc_const_eval(p->c, last, &cv) && cv.v[0] == 0)
      {
         uint32_t i;
         pr_s(p, rslc_bfn_name(bfn));
         pr_c(p, '(');
         for (i = 0; i < base_args; i++)
         {
            if (i)
               pr_s(p, ", ");
            pr_expr(p, rslc_list_item(p->c, n->b, i), PR_PREC_ASSIGN);
         }
         pr_c(p, ')');
         return;
      }
   }
   pr_s(p, rslc_bfn_name(bfn));
   pr_args(p, n->b);
}

static void pr_expr(struct rslc_pr *p, uint32_t node, int ctx)
{
   const struct rslc_node *n = rslc_node_at(p->c, node);
   struct rslc_cval cv;
   int prec;

   if (!p->ok)
      return;
   if (pr_fold(p, n, node, &cv))
   {
      bool neg = n->type->kind == RSLC_TY_SCALAR
         && ((n->type->base == RSLC_BASE_FLOAT && (cv.v[0] & 0x80000000u))
            || (n->type->base == RSLC_BASE_INT && (int32_t)cv.v[0] < 0));
      if (neg && ctx < PR_PREC_UNARY)
         pr_c(p, '(');
      pr_cval(p, n->type, &cv);
      if (neg && ctx < PR_PREC_UNARY)
         pr_c(p, ')');
      return;
   }

   switch (n->op)
   {
      case RSLC_OP_CONST:
         cv.type = n->type;
         cv.v[0] = n->a;
         pr_cval(p, n->type, &cv);
         break;
      case RSLC_OP_VAR:
         pr_var_name(p, p->c->prog.vars[n->a]);
         break;
      case RSLC_OP_MEMBER:
      {
         const struct rslc_node *base = rslc_node_at(p->c, n->a);
         pr_expr(p, n->a, PR_PREC_POSTFIX);
         pr_c(p, '.');
         pr_s(p, base->type->fields[n->b].name->str);
         break;
      }
      case RSLC_OP_INDEX:
         pr_expr(p, n->a, PR_PREC_POSTFIX);
         pr_c(p, '[');
         pr_expr(p, n->b, PR_PREC_SEQ);
         pr_c(p, ']');
         break;
      case RSLC_OP_SWIZZLE:
      {
         static const char comps[] = "xyzw";
         unsigned cnt = n->aux & 3u, i;
         const struct rslc_node *base = rslc_node_at(p->c, n->a);
         if (!cnt)
            cnt = 4;
         if (base->type->kind == RSLC_TY_SCALAR)
         {
            /* a scalar swizzle (x.xxx) needs GLSL 4.20: a splat does */
            if (cnt == 1)
               pr_expr(p, n->a, ctx);
            else
            {
               pr_type(p, n->type);
               pr_c(p, '(');
               pr_expr(p, n->a, PR_PREC_SEQ);
               pr_c(p, ')');
            }
            break;
         }
         pr_expr(p, n->a, PR_PREC_POSTFIX);
         pr_c(p, '.');
         for (i = 0; i < cnt; i++)
            pr_c(p, comps[(n->aux >> (2 + 2 * i)) & 3u]);
         break;
      }
      case RSLC_OP_CALL:
         pr_ident(p, p->c->prog.funcs[n->a]->name);
         pr_args(p, n->b);
         break;
      case RSLC_OP_BUILTIN:
         pr_builtin(p, n);
         break;
      case RSLC_OP_CONSTRUCT:
      {
         /* a literal of another base converts here, as it did in the
          * SPIR-V: vec4(x, 0, 1) carries 0.0 and 1.0 */
         uint32_t i, cnt = n->b ? rslc_list_len(p->c, n->b) : 0;
         const struct rslc_type *bt = n->type;
         while (bt->kind == RSLC_TY_ARRAY)
            bt = bt->elem;
         pr_type_full(p, n->type);
         pr_c(p, '(');
         for (i = 0; i < cnt; i++)
         {
            uint32_t an = rslc_list_item(p->c, n->b, i);
            const struct rslc_node *a = rslc_node_at(p->c, an);
            if (i)
               pr_s(p, ", ");
            if (a->op == RSLC_OP_CONST && a->type->kind == RSLC_TY_SCALAR
                  && bt->kind != RSLC_TY_STRUCT && a->type->base != bt->base
                  && rslc_const_eval(p->c, an, &cv))
            {
               struct rslc_cval cc;
               const struct rslc_type *st = rslc_type_basic(p->c, RSLC_TY_SCALAR,
                     (enum rslc_base)bt->base, 1, 1);
               double d = a->type->base == RSLC_BASE_FLOAT ? cv.d[0]
                  : a->type->base == RSLC_BASE_INT ? (double)(int32_t)cv.v[0] : (double)cv.v[0];
               memset(&cc, 0, sizeof(cc));
               cc.type = st;
               if (bt->base == RSLC_BASE_FLOAT)
               {
                  float f = (float)d;
                  memcpy(&cc.v[0], &f, sizeof(f));
               }
               else if (bt->base == RSLC_BASE_BOOL)
                  cc.v[0] = d != 0.0;
               else if (a->type->base == RSLC_BASE_FLOAT)
                  cc.v[0] = bt->base == RSLC_BASE_INT ? (uint32_t)(int32_t)d : (uint32_t)d;
               else
                  cc.v[0] = cv.v[0];
               pr_cval(p, st, &cc);
            }
            else
               pr_expr(p, an, PR_PREC_ASSIGN);
         }
         pr_c(p, ')');
         break;
      }
      case RSLC_OP_CONVERT:
         pr_type_full(p, n->type);
         pr_c(p, '(');
         pr_expr(p, n->a, PR_PREC_SEQ);
         pr_c(p, ')');
         break;
      case RSLC_OP_UNARY:
      {
         bool post = n->aux == RSLC_UN_POSTINC || n->aux == RSLC_UN_POSTDEC;
         int  need = post ? PR_PREC_POSTFIX : PR_PREC_UNARY;
         if (need > ctx)
            pr_c(p, '(');
         if (!post)
            pr_s(p, n->aux == RSLC_UN_NEG ? "-" : n->aux == RSLC_UN_NOT ? "!"
                  : n->aux == RSLC_UN_BITNOT ? "~" : n->aux == RSLC_UN_PREINC ? "++" : "--");
         pr_expr(p, n->a, need);
         if (post)
            pr_s(p, n->aux == RSLC_UN_POSTINC ? "++" : "--");
         if (need > ctx)
            pr_c(p, ')');
         break;
      }
      case RSLC_OP_BINARY:
      {
         const char *op = pr_binop_str(n->aux, &prec);
         if (prec > ctx)
            pr_c(p, '(');
         pr_expr(p, n->a, prec);
         pr_c(p, ' ');
         pr_s(p, op);
         pr_c(p, ' ');
         pr_expr(p, n->b, prec - 1);
         if (prec > ctx)
            pr_c(p, ')');
         break;
      }
      case RSLC_OP_ASSIGN:
         if (PR_PREC_ASSIGN > ctx)
            pr_c(p, '(');
         pr_expr(p, n->a, PR_PREC_UNARY);
         pr_c(p, ' ');
         if (n->aux != RSLC_BIN_NONE)
            pr_s(p, pr_binop_str(n->aux, &prec));
         pr_s(p, "= ");
         pr_expr(p, n->b, PR_PREC_ASSIGN);
         if (PR_PREC_ASSIGN > ctx)
            pr_c(p, ')');
         break;
      case RSLC_OP_TERNARY:
         if (PR_PREC_COND > ctx)
            pr_c(p, '(');
         pr_expr(p, n->a, PR_PREC_COND - 1);
         pr_s(p, " ? ");
         pr_expr(p, n->b, PR_PREC_ASSIGN);
         pr_s(p, " : ");
         pr_expr(p, n->c, PR_PREC_ASSIGN);
         if (PR_PREC_COND > ctx)
            pr_c(p, ')');
         break;
      case RSLC_OP_SEQ:
         if (PR_PREC_SEQ > ctx)
            pr_c(p, '(');
         pr_expr(p, n->a, PR_PREC_SEQ);
         pr_s(p, ", ");
         pr_expr(p, n->b, PR_PREC_SEQ - 1);
         if (PR_PREC_SEQ > ctx)
            pr_c(p, ')');
         break;
      default:
         rslc_err(p->c, "glsl: unsupported expression", NULL, n->line);
         p->ok = false;
         break;
   }
}

/* ---- statements ---- */

static void pr_stmt(struct rslc_pr *p, uint32_t node);

/* A constant expression, or a constructor of them, for a const
 * declaration's initialiser. */
static bool pr_node_is_const(struct rslc_pr *p, uint32_t node)
{
   const struct rslc_node *n = rslc_node_at(p->c, node);
   struct rslc_cval cv;
   if (!n->type)
      return false;
   if (n->type->kind == RSLC_TY_SCALAR || n->type->kind == RSLC_TY_VEC || n->type->kind == RSLC_TY_MAT)
      return rslc_const_eval(p->c, node, &cv);
   if (n->op == RSLC_OP_CONSTRUCT)
   {
      uint32_t i, cnt = n->b ? rslc_list_len(p->c, n->b) : 0;
      for (i = 0; i < cnt; i++)
         if (!pr_node_is_const(p, rslc_list_item(p->c, n->b, i)))
            return false;
      return true;
   }
   return false;
}

static void pr_decl(struct rslc_pr *p, const struct rslc_var *v, bool with_init)
{
   /* const needs a constant expression here; glslang under Vulkan
    * rules took a non-constant one, so that keyword is dropped */
   if (v->storage == RSLC_ST_CONST && with_init && v->init && pr_node_is_const(p, v->init))
      pr_s(p, "const ");
   pr_precision(p, v->precision, v->type);
   pr_type(p, v->type);
   pr_c(p, ' ');
   pr_var_name(p, v);
   pr_array_suffix(p, v->type);
   if (with_init && v->init)
   {
      pr_s(p, " = ");
      pr_expr(p, v->init, PR_PREC_ASSIGN);
   }
}

/* A statement as the body of if / for / while: a block prints its
 * braces, anything else gets them. */
static void pr_body(struct rslc_pr *p, uint32_t node)
{
   const struct rslc_node *n = node ? rslc_node_at(p->c, node) : NULL;
   if (n && n->op == RSLC_OP_BLOCK && n->aux != RSLC_BLOCK_GROUP)
   {
      pr_stmt(p, node);
      return;
   }
   pr_c(p, '{');
   p->indent++;
   if (node)
   {
      pr_nl(p);
      pr_stmt(p, node);
   }
   p->indent--;
   pr_nl(p);
   pr_c(p, '}');
}

/* A local const whose value folds is never named in the output (its
 * uses print the value), so its declaration is left out. */
static bool pr_stmt_is_silent(struct rslc_pr *p, uint32_t node)
{
   const struct rslc_node *n = rslc_node_at(p->c, node);
   const struct rslc_var  *v;
   struct rslc_cval cv;
   if (n->op != RSLC_OP_DECL)
      return false;
   v = p->c->prog.vars[n->a];
   return v->storage == RSLC_ST_CONST && v->init && !v->global
      && (v->type->kind == RSLC_TY_SCALAR || v->type->kind == RSLC_TY_VEC || v->type->kind == RSLC_TY_MAT)
      && rslc_const_eval(p->c, v->init, &cv);
}

static void pr_stmt_list(struct rslc_pr *p, uint32_t list)
{
   uint32_t i, n = list ? rslc_list_len(p->c, list) : 0;
   for (i = 0; i < n; i++)
   {
      uint32_t st = rslc_list_item(p->c, list, i);
      if (pr_stmt_is_silent(p, st))
         continue;
      pr_nl(p);
      pr_stmt(p, st);
   }
}

static void pr_stmt(struct rslc_pr *p, uint32_t node)
{
   const struct rslc_node *n = rslc_node_at(p->c, node);
   if (!p->ok)
      return;
   switch (n->op)
   {
      case RSLC_OP_BLOCK:
         if (n->aux == RSLC_BLOCK_GROUP)
         {
            /* declarators of one declaration, or an empty statement */
            uint32_t i, cnt = n->b ? rslc_list_len(p->c, n->b) : 0;
            unsigned printed = 0;
            for (i = 0; i < cnt; i++)
            {
               uint32_t st = rslc_list_item(p->c, n->b, i);
               if (pr_stmt_is_silent(p, st))
                  continue;
               if (printed++)
                  pr_nl(p);
               pr_stmt(p, st);
            }
            if (!printed)
               pr_c(p, ';');
            break;
         }
         pr_c(p, '{');
         p->indent++;
         pr_stmt_list(p, n->b);
         p->indent--;
         pr_nl(p);
         pr_c(p, '}');
         break;
      case RSLC_OP_DECL:
         pr_decl(p, p->c->prog.vars[n->a], true);
         pr_c(p, ';');
         break;
      case RSLC_OP_EXPR:
         pr_expr(p, n->a, PR_PREC_SEQ);
         pr_c(p, ';');
         break;
      case RSLC_OP_IF:
         pr_s(p, "if (");
         pr_expr(p, n->a, PR_PREC_SEQ);
         pr_s(p, ") ");
         pr_body(p, n->b);
         if (n->c)
         {
            const struct rslc_node *el = rslc_node_at(p->c, n->c);
            pr_s(p, " else ");
            if (el->op == RSLC_OP_IF)
               pr_stmt(p, n->c);
            else
               pr_body(p, n->c);
         }
         break;
      case RSLC_OP_FOR:
      {
         uint32_t init = rslc_list_item(p->c, n->b, 0);
         uint32_t cond = rslc_list_item(p->c, n->b, 1);
         uint32_t iter = rslc_list_item(p->c, n->b, 2);
         uint32_t body = rslc_list_item(p->c, n->b, 3);
         pr_s(p, "for (");
         if (init)
         {
            const struct rslc_node *in = rslc_node_at(p->c, init);
            if (in->op == RSLC_OP_DECL)
               pr_decl(p, p->c->prog.vars[in->a], true);
            else if (in->op == RSLC_OP_EXPR)
               pr_expr(p, in->a, PR_PREC_SEQ);
            else if (in->op == RSLC_OP_BLOCK && in->b)
            {
               /* several declarators share the type */
               uint32_t k, cnt = rslc_list_len(p->c, in->b);
               for (k = 0; k < cnt; k++)
               {
                  const struct rslc_node *d = rslc_node_at(p->c, rslc_list_item(p->c, in->b, k));
                  const struct rslc_var  *v = p->c->prog.vars[d->a];
                  if (!k)
                     pr_decl(p, v, true);
                  else
                  {
                     pr_s(p, ", ");
                     pr_var_name(p, v);
                     pr_array_suffix(p, v->type);
                     if (v->init)
                     {
                        pr_s(p, " = ");
                        pr_expr(p, v->init, PR_PREC_ASSIGN);
                     }
                  }
               }
            }
         }
         pr_s(p, "; ");
         if (cond)
            pr_expr(p, cond, PR_PREC_SEQ);
         pr_s(p, "; ");
         if (iter)
            pr_expr(p, iter, PR_PREC_SEQ);
         pr_s(p, ") ");
         pr_body(p, body);
         break;
      }
      case RSLC_OP_WHILE:
         pr_s(p, "while (");
         pr_expr(p, n->a, PR_PREC_SEQ);
         pr_s(p, ") ");
         pr_body(p, n->b);
         break;
      case RSLC_OP_DO:
         pr_s(p, "do ");
         pr_body(p, n->a);
         pr_s(p, " while (");
         pr_expr(p, n->b, PR_PREC_SEQ);
         pr_s(p, ");");
         break;
      case RSLC_OP_SWITCH:
      {
         uint32_t i, cnt = n->b ? rslc_list_len(p->c, n->b) : 0;
         pr_s(p, "switch (");
         pr_expr(p, n->a, PR_PREC_SEQ);
         pr_s(p, ") {");
         p->indent++;
         for (i = 0; i < cnt; i++)
         {
            uint32_t sn = rslc_list_item(p->c, n->b, i);
            const struct rslc_node *s = rslc_node_at(p->c, sn);
            if (s->op == RSLC_OP_CASE)
            {
               p->indent--;
               pr_nl(p);
               if (s->a)
               {
                  pr_s(p, "case ");
                  pr_expr(p, s->a, PR_PREC_SEQ);
                  pr_c(p, ':');
               }
               else
                  pr_s(p, "default:");
               p->indent++;
               continue;
            }
            pr_nl(p);
            pr_stmt(p, sn);
         }
         p->indent--;
         pr_nl(p);
         pr_c(p, '}');
         break;
      }
      case RSLC_OP_CASE:
         break;
      case RSLC_OP_BREAK:
         pr_s(p, "break;");
         break;
      case RSLC_OP_CONTINUE:
         pr_s(p, "continue;");
         break;
      case RSLC_OP_RETURN:
         pr_s(p, "return");
         if (n->a)
         {
            pr_c(p, ' ');
            pr_expr(p, n->a, PR_PREC_SEQ);
         }
         pr_c(p, ';');
         break;
      case RSLC_OP_DISCARD:
         pr_s(p, "discard;");
         break;
      default:
         rslc_err(p->c, "glsl: unsupported statement", NULL, n->line);
         p->ok = false;
         break;
   }
}

/* ---- declarations ---- */

/* name NULL: the struct's own name */
static void pr_struct(struct rslc_pr *p, const struct rslc_type *t, const char *name)
{
   uint32_t i;
   pr_s(p, "struct ");
   if (name)
      pr_s(p, name);
   else
      pr_ident(p, t->name);
   pr_s(p, " {");
   p->indent++;
   for (i = 0; i < t->nfields; i++)
   {
      const struct rslc_field *f = &t->fields[i];
      pr_nl(p);
      pr_precision(p, f->precision, f->type);
      pr_type(p, f->type);
      pr_c(p, ' ');
      pr_s(p, f->name->str);
      pr_array_suffix(p, f->type);
      pr_c(p, ';');
   }
   p->indent--;
   pr_nl(p);
   pr_s(p, "};");
   pr_nl(p);
}

static void pr_func_head(struct rslc_pr *p, const struct rslc_func *f)
{
   uint32_t i;
   pr_type_full(p, f->ret);
   pr_c(p, ' ');
   pr_ident(p, f->name);
   pr_c(p, '(');
   for (i = 0; i < f->nparams; i++)
   {
      const struct rslc_var *v = f->params[i];
      if (i)
         pr_s(p, ", ");
      if (v->dir == RSLC_DIR_OUT)
         pr_s(p, "out ");
      else if (v->dir == RSLC_DIR_INOUT)
         pr_s(p, "inout ");
      pr_precision(p, v->precision, v->type);
      pr_type(p, v->type);
      pr_c(p, ' ');
      if (v->name)
         pr_ident(p, v->name);
      else
         pr_c(p, '_');
      pr_array_suffix(p, v->type);
   }
   pr_c(p, ')');
}

/* Marks the block members a reachable function reads, for the
 * reflection table. */
static void pr_mark_members(struct rslc_pr *p, uint32_t node)
{
   const struct rslc_node *n;
   uint32_t i, cnt;
   if (!node)
      return;
   n = rslc_node_at(p->c, node);
   switch (n->op)
   {
      case RSLC_OP_CONST:
         return;
      case RSLC_OP_VAR:
      {
         const struct rslc_var *v = p->c->prog.vars[n->a];
         if (v->storage == RSLC_ST_MEMBER && p->active[v->block])
            p->active[v->block][v->member] = 1;
         else if (v->storage == RSLC_ST_BLOCK && p->active[v->block])
            memset(p->active[v->block], 1, p->c->prog.blocks[v->block].type->nfields);
         return;
      }
      case RSLC_OP_MEMBER:
      {
         const struct rslc_node *bn = rslc_node_at(p->c, n->a);
         if (bn->op == RSLC_OP_VAR && p->c->prog.vars[bn->a]->storage == RSLC_ST_BLOCK)
         {
            const struct rslc_var *bv = p->c->prog.vars[bn->a];
            if (p->active[bv->block])
               p->active[bv->block][n->b] = 1;
            return;
         }
         pr_mark_members(p, n->a);
         return;
      }
      case RSLC_OP_DECL:
         pr_mark_members(p, p->c->prog.vars[n->a]->init);
         return;
      case RSLC_OP_SWIZZLE: case RSLC_OP_CONVERT: case RSLC_OP_EXPR:
      case RSLC_OP_RETURN: case RSLC_OP_CASE:
         pr_mark_members(p, n->a);
         return;
      case RSLC_OP_CALL: case RSLC_OP_BUILTIN: case RSLC_OP_CONSTRUCT:
      case RSLC_OP_BLOCK: case RSLC_OP_FOR: case RSLC_OP_SWITCH:
         if (n->op == RSLC_OP_SWITCH)
            pr_mark_members(p, n->a);
         if (n->op == RSLC_OP_BUILTIN && n->a >= RSLC_BFN_PACKUNORM2X16 && n->a <= RSLC_BFN_UNPACKHALF2X16)
            p->uses_packing = true;
         if (n->op == RSLC_OP_BUILTIN && (n->a == RSLC_BFN_TEXTUREGATHER || n->a == RSLC_BFN_TEXTUREGATHEROFFSET))
         {
            unsigned base_args = n->a == RSLC_BFN_TEXTUREGATHER ? 2 : 3;
            struct rslc_cval cv;
            p->uses_gather = true;
            cnt = rslc_list_len(p->c, n->b);
            if (cnt == base_args + 1
                  && !(rslc_const_eval(p->c, rslc_list_item(p->c, n->b, cnt - 1), &cv) && cv.v[0] == 0))
               p->uses_gather_comp = true;
         }
         cnt = n->b ? rslc_list_len(p->c, n->b) : 0;
         for (i = 0; i < cnt; i++)
            pr_mark_members(p, rslc_list_item(p->c, n->b, i));
         return;
      default:
         pr_mark_members(p, n->a);
         pr_mark_members(p, n->b);
         pr_mark_members(p, n->c);
         return;
   }
}

static void pr_block_decl(struct rslc_pr *p, unsigned idx)
{
   const struct rslc_blockdecl *b = &p->c->prog.blocks[idx];
   const char *type_name = b->type->name ? b->type->name->str : (b->push ? "Push" : "UBO");
   const char *inst      = pr_block_instance(p, idx);
   uint32_t i;
   if (p->names)
   {
      const char *forced = b->push ? p->names->push_block : p->names->ubo_block;
      if (forced)
         type_name = forced;
   }
   if (b->push)
   {
      /* no push constants in GLSL: a struct and one uniform of it */
      pr_struct(p, b->type, type_name);
      pr_s(p, "uniform ");
      pr_s(p, type_name);
      pr_c(p, ' ');
      pr_s(p, inst ? inst : "_push");
      pr_c(p, ';');
      pr_nl(p);
      return;
   }
   pr_s(p, "layout(std140) uniform ");
   pr_s(p, type_name);
   pr_s(p, " {");
   p->indent++;
   for (i = 0; i < b->type->nfields; i++)
   {
      const struct rslc_field *f = &b->type->fields[i];
      pr_nl(p);
      pr_precision(p, f->precision, f->type);
      pr_type(p, f->type);
      pr_c(p, ' ');
      pr_s(p, f->name->str);
      pr_array_suffix(p, f->type);
      pr_c(p, ';');
   }
   p->indent--;
   pr_nl(p);
   pr_c(p, '}');
   if (inst)
   {
      pr_c(p, ' ');
      pr_s(p, inst);
   }
   pr_c(p, ';');
   pr_nl(p);
}

static bool pr_type_is_integer(const struct rslc_type *t)
{
   while (t->kind == RSLC_TY_ARRAY)
      t = t->elem;
   return (t->kind == RSLC_TY_SCALAR || t->kind == RSLC_TY_VEC)
      && (t->base == RSLC_BASE_INT || t->base == RSLC_BASE_UINT);
}

static void pr_io_decl(struct rslc_pr *p, const struct rslc_var *v, bool is_in)
{
   bool layout_ok = p->es ? p->version >= 300 : p->version >= 330;
   bool fragment  = p->c->stage == RSLC_STAGE_FRAGMENT;
   /* fragment outputs keep their location; attributes and varyings are
    * bound by name by the driver */
   if (layout_ok && fragment && !is_in && v->location != RSLC_UNSET)
   {
      pr_s(p, "layout(location = ");
      pr_u(p, v->location);
      pr_s(p, ") ");
   }
   if (!(fragment && !is_in))
   {
      if (v->interp == RSLC_INTERP_FLAT || pr_type_is_integer(v->type))
         pr_s(p, "flat ");   /* integer varyings must be flat in both stages */
      else if (v->interp == RSLC_INTERP_NOPERSPECTIVE && !p->es)
         pr_s(p, "noperspective ");
   }
   if (v->quals & RSLC_VQ_CENTROID)
      pr_s(p, "centroid ");
   if (v->quals & RSLC_VQ_SAMPLE)
      pr_s(p, "sample ");
   pr_s(p, is_in ? "in " : "out ");
   pr_precision(p, v->precision, v->type);
   pr_type(p, v->type);
   pr_c(p, ' ');
   pr_var_name(p, v);
   pr_array_suffix(p, v->type);
   pr_c(p, ';');
   pr_nl(p);
}

/* Whether a global's initialiser is a constant expression (or a
 * constructor of constants) that prints with the declaration. */
static bool pr_global_init_in_place(struct rslc_pr *p, const struct rslc_var *v)
{
   return v->init && pr_node_is_const(p, v->init);
}

static bool pr_glsl(struct rslc_pr *p, struct rslc_result *result)
{
   struct rslc_ctx        *c  = p->c;
   const struct rslc_prog *pr = &c->prog;
   uint32_t i;
   char     buf[32];

   /* version line */
   sprintf(buf, "#version %u%s", p->version % 10000, p->es ? " es" : "");
   pr_s(p, buf);
   pr_nl(p);
   if (!p->es && p->version < 420 && p->uses_packing)
   {
      pr_s(p, "#extension GL_ARB_shading_language_packing : require");
      pr_nl(p);
   }
   if (!p->es && p->version < 400 && p->uses_gather)
   {
      pr_s(p, "#extension GL_ARB_texture_gather : require");
      pr_nl(p);
      if (p->uses_gather_comp)
      {
         pr_s(p, "#extension GL_ARB_gpu_shader5 : require");
         pr_nl(p);
      }
   }
   if (p->es)
   {
      pr_s(p, "precision highp float;");
      pr_nl(p);
      pr_s(p, "precision highp int;");
      pr_nl(p);
      for (i = 0; i < pr->nsamplers; i++)
      {
         const struct rslc_type *t = pr->samplers[i]->type;
         if (t->base == RSLC_BASE_INT && !(p->sampler_prec_done & 1))
         {
            pr_s(p, "precision highp isampler2D;");
            pr_nl(p);
            p->sampler_prec_done |= 1;
         }
         if (t->base == RSLC_BASE_UINT && !(p->sampler_prec_done & 2))
         {
            pr_s(p, "precision highp usampler2D;");
            pr_nl(p);
            p->sampler_prec_done |= 2;
         }
      }
   }

   /* struct types, in declaration order; the block types print with
    * their blocks */
   for (i = 0; i < pr->nstructs; i++)
   {
      const struct rslc_type *t = pr->structs[i];
      bool is_block = false;
      unsigned k;
      for (k = 0; k < 2; k++)
         if (pr->has_block[k] && pr->blocks[k].type == t)
            is_block = true;
      if (!is_block && t->name)
         pr_struct(p, t, NULL);
   }

   for (i = 0; i < 2; i++)
      if (pr->has_block[i])
         pr_block_decl(p, i);

   for (i = 0; i < pr->nsamplers; i++)
   {
      pr_s(p, "uniform ");
      pr_type(p, pr->samplers[i]->type);
      pr_c(p, ' ');
      pr_var_name(p, pr->samplers[i]);
      pr_c(p, ';');
      pr_nl(p);
   }
   for (i = 0; i < pr->ninputs; i++)
      pr_io_decl(p, pr->inputs[i], true);
   for (i = 0; i < pr->noutputs; i++)
      pr_io_decl(p, pr->outputs[i], false);

   /* globals: a constant initialiser prints in place, any other runs
    * at the top of main */
   for (i = 1; i < pr->nvars; i++)
   {
      const struct rslc_var *v = pr->vars[i];
      if (!v->global || (v->storage != RSLC_ST_GLOBAL && v->storage != RSLC_ST_CONST))
         continue;
      pr_decl(p, v, pr_global_init_in_place(p, v));
      pr_c(p, ';');
      pr_nl(p);
   }

   /* prototypes, then bodies, for the functions main reaches */
   for (i = 1; i < pr->nfuncs; i++)
   {
      const struct rslc_func *f = pr->funcs[i];
      if (!p->reach[i] || !f->body || f == pr->main_fn)
         continue;
      pr_func_head(p, f);
      pr_c(p, ';');
      pr_nl(p);
   }
   for (i = 1; i < pr->nfuncs; i++)
   {
      const struct rslc_func *f = pr->funcs[i];
      if (!p->reach[i] || !f->body)
         continue;
      pr_func_head(p, f);
      pr_c(p, ' ');
      if (f == pr->main_fn)
      {
         /* global initialisers that are not constants run first */
         const struct rslc_node *body = rslc_node_at(c, f->body);
         uint32_t k;
         pr_c(p, '{');
         p->indent++;
         for (k = 1; k < pr->nvars; k++)
         {
            const struct rslc_var *v = pr->vars[k];
            if (!v->global || !v->init
                  || (v->storage != RSLC_ST_GLOBAL && v->storage != RSLC_ST_CONST)
                  || pr_global_init_in_place(p, v))
               continue;
            pr_nl(p);
            pr_var_name(p, v);
            pr_s(p, " = ");
            pr_expr(p, v->init, PR_PREC_ASSIGN);
            pr_c(p, ';');
         }
         pr_stmt_list(p, body->b);
         p->indent--;
         pr_nl(p);
         pr_c(p, '}');
      }
      else
         pr_stmt(p, f->body);
      pr_nl(p);
   }

   if (!p->ok)
      return false;
   /* the text payload carries its NUL */
   if (!rslc_pbuf_putc(c, &p->out, '\0'))
      return false;
   {
      size_t text_len = p->out.len - ((struct rslc_blob_hdr*)p->out.data)->payload_off - 1;
      rslc_blob_end(c, &p->out, c->opts->dst, result);
      result->text     = (const char*)(p->out.data + ((struct rslc_blob_hdr*)p->out.data)->payload_off);
      result->text_len = text_len;
   }
   return true;
}

bool rslc_emit_text(struct rslc_ctx *c, struct rslc_result *result)
{
   struct rslc_pr *p = (struct rslc_pr*)rslc_arena_zalloc(c, sizeof(*p));
   const struct rslc_prog *pr = &c->prog;
   unsigned k;

   if (!p)
      return false;
   if (!pr->main_fn)
   {
      rslc_err(c, "no main() in stage", NULL, 0);
      return false;
   }
   p->c       = c;
   p->names   = c->opts->names;
   p->version = c->opts->version;
   p->es      = c->opts->es;
   p->ok      = true;
   p->reach   = (uint8_t*)rslc_arena_zalloc(c, pr->nfuncs + 1);
   if (!p->reach)
      return false;
   rslc_reach_funcs(c, p->reach);
   for (k = 0; k < 2; k++)
      if (pr->has_block[k])
      {
         p->active[k] = (uint8_t*)rslc_arena_zalloc(c, pr->blocks[k].type->nfields
               ? pr->blocks[k].type->nfields : 1);
         if (!p->active[k])
            return false;
      }

   switch (c->opts->dst)
   {
      case RSLC_DST_GLSL:
      case RSLC_DST_GLSL_ES:
         if (c->opts->dst == RSLC_DST_GLSL_ES)
            p->es = true;
         if (!p->version)
            p->version = p->es ? 300 : 330;
         break;
      default:
         rslc_err(c, "this printer is not implemented", NULL, 0);
         return false;
   }

   /* what the reachable code reads and uses, before anything prints */
   for (k = 1; k < pr->nfuncs; k++)
      if (p->reach[k] && pr->funcs[k]->body)
         pr_mark_members(p, pr->funcs[k]->body);
   for (k = 1; k < pr->nvars; k++)
      if (pr->vars[k]->global && pr->vars[k]->init)
         pr_mark_members(p, pr->vars[k]->init);

   if (!rslc_blob_begin(c, &p->out, 4096, p->active))
      return false;
   if (!pr_glsl(p, result))
   {
      if (!c->failed)
         rslc_err(c, "glsl: print failed", NULL, 0);
      rslc_pbuf_free(c, &p->out);
      return false;
   }
   return true;
}
