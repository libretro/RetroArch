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
 * of it, and members are reached through that instance.
 *
 * The HLSL skin (opts->version is the shader model, 40 or 50) lays
 * the stage out as the D3D filter chains bind it: the uniform block
 * as cbuffer b0 and the push block as cbuffer b1, each member at its
 * block offset, Texture2D + SamplerState pairs at the binding, the
 * inputs and outputs as statics behind a wrapper main whose structs
 * carry TEXCOORDn / SV_Target / SV_Position semantics.  Matrices are
 * spelled transposed, so a row_major cbuffer member overlays its
 * std140 bytes and a product prints as mul() with swapped operands.
 * Aggregate constructors, which HLSL has no expression for, become
 * initialiser lists in declarations and temporaries elsewhere.
 *
 * The MSL skin lays the stage out as the Metal filter chain binds it:
 * the uniform block at buffer 0 and the push block at buffer 1 as
 * structs at their block offsets (vec3 packed where its tail is
 * taken, padding where Metal would pack tighter), textures and
 * samplers at the binding, attributes, varyings and colour outputs in
 * the stage_in / return structs.  Metal has no globals: every
 * function takes what it reaches - interface variables and globals
 * by reference, blocks as constant references, samplers as texture +
 * sampler pairs - and main0 holds them.  Arrays are metal::array. */

#include <stdio.h>   /* sprintf: a float's nine digits and a version number */
#include <string.h>

#include "rslc_share.h"

struct rslc_pr
{
   struct rslc_ctx         *c;
   struct rslc_pbuf         out;
   const struct rslc_names *names;
   uint8_t                 *reach;
   uint8_t                 *used;          /* variables reachable code names */
   uint8_t                 *lifted;        /* HLSL: local constant aggregates moved to file scope */
   /* MSL has no globals: fenv[f * envn + v] says function f reaches
    * interface variable, global or sampler v, or (v = nvars + k) block
    * k, which it then takes as a parameter */
   uint8_t                 *fenv;
   uint32_t                 envn;
   /* MSL: C++ scoping.  A variable named like a function is suffixed
    * (symfn, by symbol index); an environment parameter shadowed by a
    * local of the function being printed is suffixed too (renamed) */
   uint8_t                 *symfn;
   uint8_t                 *renamed;
   /* MSL: printing a program-scope constant's initialiser, where a
    * constant it names prints as its value (Metal builds no global
    * constructors to copy one constant into another) */
   bool                     msl_ginit;
   uint8_t                 *active[2];
   unsigned                 version;
   unsigned                 indent;
   unsigned                 sampler_prec_done;
   /* HLSL: aggregate constructors have no expression form, so each
    * one in a statement is hoisted into a temporary declared before
    * it; the table maps those nodes to their temporaries */
   uint32_t                *hoist;
   uint32_t                 nhoist;
   uint32_t                 hoist_cap;
   uint32_t                 ntemps;
   /* MSL: vector components passed to out parameters go through a
    * temporary written back after the statement (node, temp pairs) */
   uint32_t                 wb[16];
   uint32_t                 nwb;
   uint32_t                 hl_uses;        /* RSLC_HL_* helpers the code needs */
   bool                     es;
   bool                     cafe;        /* GLSL for CafeGLSL, see rslc.h */
   bool                     hlsl;
   bool                     msl;
   bool                     ok;
   bool                     uses_packing;   /* a pack or unpack builtin in reachable code */
   bool                     uses_gather;    /* textureGather in reachable code */
   bool                     uses_bitcast;   /* floatBitsToInt and kin in reachable code */
   bool                     uses_gather_comp; /* ... with a component other than 0 */
};

/* MSL functions but main0: under fast math a call rounds apart from inlined code */
#define RSLC_MSL_FN "static inline __attribute__((always_inline)) "

/* HLSL helper functions, printed once when used. */
#define RSLC_HL_MOD           (1u << 0)
#define RSLC_HL_SIZE_F        (1u << 1)
#define RSLC_HL_SIZE_I        (1u << 2)
#define RSLC_HL_SIZE_U        (1u << 3)
#define RSLC_HL_INV2          (1u << 4)
#define RSLC_HL_INV3          (1u << 5)
#define RSLC_HL_INV4          (1u << 6)
#define RSLC_HL_PACK_UNORM2   (1u << 7)
#define RSLC_HL_UNPACK_UNORM2 (1u << 8)
#define RSLC_HL_PACK_SNORM2   (1u << 9)
#define RSLC_HL_UNPACK_SNORM2 (1u << 10)
#define RSLC_HL_PACK_UNORM4   (1u << 11)
#define RSLC_HL_UNPACK_UNORM4 (1u << 12)
#define RSLC_HL_PACK_SNORM4   (1u << 13)
#define RSLC_HL_UNPACK_SNORM4 (1u << 14)
#define RSLC_HL_PACK_HALF     (1u << 15)
#define RSLC_HL_UNPACK_HALF   (1u << 16)
#define RSLC_HL_BFE_I         (1u << 17)
#define RSLC_HL_BFE_U         (1u << 18)
#define RSLC_HL_BFI           (1u << 19)
#define RSLC_HL_LEVELS        (1u << 20)
#define RSLC_HL_CMUL2         (1u << 21)
#define RSLC_HL_CMUL3         (1u << 22)
#define RSLC_HL_CMUL4         (1u << 23)
#define RSLC_HL_FINDLSB       (1u << 24)
#define RSLC_HL_FINDMSB_I     (1u << 25)
#define RSLC_HL_FINDMSB_U     (1u << 26)

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
      pr_s(p, p->hlsl ? "asfloat(0x7fc00000u)" : p->msl ? "as_type<float>(0x7fc00000u)" : "(0.0 / 0.0)");
      return;
   }
   if (f > 3.4e38f || f < -3.4e38f)
   {
      if (p->hlsl)
         pr_s(p, f < 0 ? "asfloat(0xff800000u)" : "asfloat(0x7f800000u)");
      else if (p->msl)
         pr_s(p, f < 0 ? "as_type<float>(0xff800000u)" : "as_type<float>(0x7f800000u)");
      else
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
   }
   if (p->hlsl || p->msl)
      buf[n++] = 'f';
   buf[n] = '\0';
   pr_s(p, buf);
}

static void pr_int(struct rslc_pr *p, int32_t v)
{
   char buf[24];
   if (v == (-2147483647 - 1))
      strcpy(buf, p->hlsl ? "int(0x80000000u)" : "(-2147483647 - 1)");
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

static void pr_scalar_name(struct rslc_pr *p, unsigned base)
{
   switch (base)
   {
      case RSLC_BASE_BOOL: pr_s(p, "bool"); break;
      case RSLC_BASE_INT:  pr_s(p, "int"); break;
      case RSLC_BASE_UINT: pr_s(p, "uint"); break;
      default:             pr_s(p, "float"); break;
   }
}

/* HLSL: a GLSL matrix of C columns is printed as floatCxR, whose rows
 * are the GLSL columns, so M[i] and every constructor keep their
 * meaning and the memory layout of a row_major cbuffer member is the
 * std140 one; products swap their operands in mul() (see pr_expr). */
static void pr_type(struct rslc_pr *p, const struct rslc_type *t)
{
   switch (t->kind)
   {
      case RSLC_TY_VOID:
         pr_s(p, "void");
         break;
      case RSLC_TY_SCALAR:
         pr_scalar_name(p, t->base);
         break;
      case RSLC_TY_VEC:
         if (p->hlsl || p->msl)
         {
            pr_scalar_name(p, t->base);
            pr_u(p, t->rows);
            break;
         }
         pr_scalar_prefix(p, t->base);
         pr_s(p, "vec");
         pr_u(p, t->rows);
         break;
      case RSLC_TY_MAT:
         if (p->hlsl || p->msl)
         {
            pr_s(p, "float");
            pr_u(p, t->cols);
            pr_c(p, 'x');
            pr_u(p, t->rows);
            break;
         }
         pr_s(p, "mat");
         pr_u(p, t->cols);
         if (t->rows != t->cols)
         {
            pr_c(p, 'x');
            pr_u(p, t->rows);
         }
         break;
      case RSLC_TY_SAMPLER2D:
         if (p->hlsl)
         {
            pr_s(p, "Texture2D<");
            pr_scalar_name(p, t->base);
            pr_s(p, "4>");
            break;
         }
         if (p->msl)
         {
            pr_s(p, "texture2d<");
            pr_scalar_name(p, t->base);
            pr_c(p, '>');
            break;
         }
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
         if (p->msl)
         {
            /* metal::array copies and returns, which a C array cannot */
            pr_s(p, "array<");
            pr_type(p, t->elem);
            pr_s(p, ", ");
            pr_u(p, t->array_len);
            pr_c(p, '>');
            break;
         }
         pr_type(p, t->elem);
         break;
   }
}

/* The array suffix of a declarator: name[N][M]. */
static void pr_array_suffix(struct rslc_pr *p, const struct rslc_type *t)
{
   if (p->msl)
      return;   /* the type carries it */
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
      "dmat4", "packed", "row_major", "attribute", "varying", NULL
   };
   /* HLSL keywords, type names and intrinsics that are not GLSL's */
   static const char *const hlsl_words[] = {
      "asm", "auto", "bool1", "bool2", "bool3", "bool4", "cbuffer", "centroid",
      "char", "class", "column_major", "compile", "const_cast", "delete",
      "double", "dword", "dynamic_cast", "enum", "explicit", "export", "extern",
      "false", "float1", "float2", "float3", "float4", "friend", "fxgroup",
      "goto", "groupshared", "half", "inline", "int1", "int2", "int3", "int4",
      "interface", "line", "lineadj", "linear", "long", "matrix", "min16float",
      "min10float", "min16int", "min12int", "min16uint", "mutable", "namespace",
      "new", "nointerpolation", "noperspective", "operator", "packoffset",
      "pass", "pixelfragment", "point", "precise", "private", "protected",
      "public", "register", "reinterpret_cast", "row_major", "sample",
      "sampler", "shared", "short", "signed", "sizeof", "snorm", "stateblock",
      "static", "static_cast", "string", "tbuffer", "technique", "template",
      "texture", "this", "throw", "triangle", "triangleadj", "try", "typedef",
      "typename", "uint1", "uint2", "uint3", "uint4", "uniform", "union",
      "unorm", "unsigned", "using", "vector", "vertexfragment", "virtual",
      "volatile", "abort", "asdouble", "asfloat", "asint", "asuint", "atan2",
      "clip", "countbits", "ddx", "ddx_coarse", "ddx_fine", "ddy", "ddy_coarse",
      "ddy_fine", "dst", "errorf", "f16tof32", "f32tof16", "firstbithigh",
      "firstbitlow", "fmod", "frac", "isfinite", "lerp", "lit", "log10", "mad",
      "msad4", "mul", "noise", "printf", "rcp", "reversebits", "rsqrt",
      "saturate", "sincos", "tex1D", "tex2D", "tex3D", "texCUBE", "Buffer",
      "Texture1D", "Texture2D", "Texture3D", "TextureCube", "SamplerState",
      "SamplerComparisonState", NULL
   };
   /* C++ and the Metal standard library */
   static const char *const msl_words[] = {
      "alignas", "alignof", "and", "and_eq", "asm", "auto", "bitand", "bitor",
      "catch", "char", "char16_t", "char32_t", "class", "compl", "concept",
      "const_cast", "constexpr", "decltype", "delete", "dynamic_cast", "enum",
      "explicit", "export", "extern", "friend", "goto", "inline", "long",
      "mutable", "namespace", "new", "noexcept", "not", "not_eq", "nullptr",
      "operator", "or", "or_eq", "private", "protected", "public", "register",
      "reinterpret_cast", "requires", "short", "signed", "sizeof", "static",
      "static_assert", "static_cast", "template", "this", "thread_local",
      "throw", "try", "typedef", "typeid", "typename", "union", "unsigned",
      "using", "virtual", "volatile", "wchar_t", "xor", "xor_eq", "constant",
      "device", "thread", "threadgroup", "threadgroup_imageblock", "kernel",
      "vertex", "fragment", "sampler", "texture", "half", "half2", "half3",
      "half4", "array", "select", "saturate", "rsqrt", "fmod", "rint",
      "popcount", "ctz", "clz", "as_type", "size_t", "ptrdiff_t", "packed_float3",
      "metal", "simd", "main0", "level", "bias", "gradient2d", "component",
      "fract", "dfdx", "dfdy", "extract_bits", "insert_bits", "reverse_bits",
      "mix", "atan2", "min", "max", "abs", "exp", "log", "pow", "sqrt", "sin",
      "cos", "tan", "floor", "ceil", "round", "trunc", "sign", "step",
      "smoothstep", "clamp", "length", "distance", "dot", "cross", "normalize",
      "reflect", "refract", "faceforward", "any", "all", "isnan", "isinf",
      "transpose", "determinant", "fma", "modf", "frexp", "ldexp", "fwidth",
      "pack_float_to_unorm4x8", "unpack_unorm4x8_to_float", "decay", "quad",
      "simd_broadcast", "vec", "ushort", "uchar", "powr", NULL
   };
   const char *const *list = p->hlsl ? hlsl_words : p->msl ? msl_words : words;
   unsigned i;
   if (name->bfn)
      return true;   /* a builtin function's name */
   for (i = 0; list[i]; i++)
      if (!strcmp(list[i], name->str))
         return true;
   return false;
}

static void pr_ident(struct rslc_pr *p, const struct rslc_sym *name)
{
   pr_s(p, name->str);
   if (pr_name_reserved(p, name))
      pr_c(p, '_');
}

/* HLSL: a cbuffer has no instance, so a block's members are globals
 * named instance_member (bare for an unnamed block). */
static void pr_hlsl_member(struct rslc_pr *p, unsigned block, uint32_t member)
{
   const struct rslc_blockdecl *b = &p->c->prog.blocks[block];
   if (b->instance)
   {
      /* the prefix keeps it clear of every reserved word */
      pr_s(p, b->instance->str);
      pr_c(p, '_');
      pr_s(p, b->type->fields[member].name->str);
      return;
   }
   pr_ident(p, b->type->fields[member].name);
}

/* MSL: the block instance a function or the entry point receives. */
static const char *pr_msl_instance(struct rslc_pr *p, unsigned block)
{
   const struct rslc_blockdecl *b = &p->c->prog.blocks[block];
   return b->instance ? b->instance->str : (b->push ? "_push" : "_ubo");
}

/* Flattened blocks (rslc_names.flatten): the vec4 array's name. */
static const char *pr_flat_name(struct rslc_pr *p, unsigned block)
{
   const struct rslc_blockdecl *b = &p->c->prog.blocks[block];
   const char *forced = b->push ? p->names->push_block : p->names->ubo_block;
   if (forced)
      return forced;
   return b->type->name ? b->type->name->str : (b->push ? "Push" : "UBO");
}

/* The vec4 slots a flattened block takes; 0 when a member cannot be
 * read out of them. */
static uint32_t pr_flat_slots(struct rslc_pr *p, unsigned block)
{
   const struct rslc_blockdecl *b = &p->c->prog.blocks[block];
   bool     std140 = !b->push || b->std140;
   uint32_t i, end = 0;
   for (i = 0; i < b->type->nfields; i++)
   {
      const struct rslc_type *t = b->type->fields[i].type;
      uint32_t a, sz, st, off = rslc_layout_field_offset(b->type, i, std140);
      rslc_layout_of_type(t, std140, &a, &sz, &st);
      (void)a;
      if (     (t->kind != RSLC_TY_SCALAR && t->kind != RSLC_TY_VEC
               && t->kind != RSLC_TY_MAT)
            || t->base == RSLC_BASE_BOOL
            || (t->kind == RSLC_TY_MAT && (st != 16 || (off & 15)))
            || (t->kind != RSLC_TY_MAT && (off & 15) + sz > 16))
         return 0;
      if (off + sz > end)
         end = off + sz;
   }
   return (end + 15) / 16;
}

/* Member @m of flattened block @block, read out of its vec4 array. */
static void pr_flat_member(struct rslc_pr *p, unsigned block, uint32_t m)
{
   const struct rslc_blockdecl *b = &p->c->prog.blocks[block];
   const struct rslc_type      *t = b->type->fields[m].type;
   const char *name = pr_flat_name(p, block);
   uint32_t off  = rslc_layout_field_offset(b->type, m, !b->push || b->std140);
   uint32_t slot = off / 16, comp = (off & 15) / 4, i, c;
   if (t->kind == RSLC_TY_MAT)
   {
      pr_type(p, t);
      pr_c(p, '(');
      for (c = 0; c < t->cols; c++)
      {
         if (c)
            pr_s(p, ", ");
         pr_s(p, name);
         pr_c(p, '[');
         pr_u(p, slot + c);
         pr_c(p, ']');
         if (t->rows < 4)
         {
            pr_c(p, '.');
            for (i = 0; i < t->rows; i++)
               pr_c(p, "xyzw"[i]);
         }
      }
      pr_c(p, ')');
      return;
   }
   if (t->base == RSLC_BASE_INT)
      pr_s(p, "floatBitsToInt(");
   else if (t->base == RSLC_BASE_UINT)
      pr_s(p, "floatBitsToUint(");
   pr_s(p, name);
   pr_c(p, '[');
   pr_u(p, slot);
   pr_c(p, ']');
   if (comp || t->rows < 4)
   {
      pr_c(p, '.');
      for (i = 0; i < t->rows; i++)
         pr_c(p, "xyzw"[comp + i]);
   }
   if (t->base == RSLC_BASE_INT || t->base == RSLC_BASE_UINT)
      pr_c(p, ')');
}

/* Whether an expression reads another variable named like v.  In
 * GLSL a local's name comes into scope after its initializer, so
 * vec2 x = x; reads the outer x; in HLSL it would read itself. */
static bool pr_reads_namesake(struct rslc_pr *p, uint32_t node, const struct rslc_var *v)
{
   const struct rslc_node *n;
   uint32_t i, cnt;
   if (!node)
      return false;
   n = rslc_node_at(p->c, node);
   switch (n->op)
   {
      case RSLC_OP_CONST:
         return false;
      case RSLC_OP_VAR:
         return p->c->prog.vars[n->a] != v && p->c->prog.vars[n->a]->name == v->name;
      case RSLC_OP_MEMBER:
      case RSLC_OP_SWIZZLE:
      case RSLC_OP_CONVERT:
      case RSLC_OP_UNARY:
         return pr_reads_namesake(p, n->a, v);
      case RSLC_OP_CALL:
      case RSLC_OP_BUILTIN:
      case RSLC_OP_CONSTRUCT:
         cnt = n->b ? rslc_list_len(p->c, n->b) : 0;
         for (i = 0; i < cnt; i++)
            if (pr_reads_namesake(p, rslc_list_item(p->c, n->b, i), v))
               return true;
         return false;
      default:
         return pr_reads_namesake(p, n->a, v) || pr_reads_namesake(p, n->b, v)
            || pr_reads_namesake(p, n->c, v);
   }
}

static void pr_var_name(struct rslc_pr *p, const struct rslc_var *v)
{
   const struct rslc_names *nm = p->names;
   if (p->msl)
   {
      switch (v->storage)
      {
         case RSLC_ST_MEMBER:
            pr_s(p, pr_msl_instance(p, v->block));
            pr_c(p, '.');
            pr_ident(p, p->c->prog.blocks[v->block].type->fields[v->member].name);
            return;
         case RSLC_ST_BLOCK:
            pr_s(p, pr_msl_instance(p, v->block));
            return;
         case RSLC_ST_BUILTIN:
            pr_s(p, v->name->str);
            if (p->renamed[v->id])
               pr_s(p, "_g");
            return;
         default:
            if (v->name)
            {
               pr_ident(p, v->name);
               if (p->symfn[v->name->index])
                  pr_s(p, "_v");
            }
            else
               pr_c(p, '_');
            if (p->renamed[v->id])
               pr_s(p, "_g");
            return;
      }
   }
   if (p->hlsl)
   {
      switch (v->storage)
      {
         case RSLC_ST_MEMBER:
            pr_hlsl_member(p, v->block, v->member);
            return;
         case RSLC_ST_BLOCK:
            /* the instance as a whole has no HLSL form */
            rslc_err(p->c, "hlsl: a block used as a value", NULL, v->line);
            p->ok = false;
            return;
         case RSLC_ST_BUILTIN:
            pr_s(p, v->name->str);
            return;
         default:
            if (v->name)
               pr_ident(p, v->name);
            else
               pr_c(p, '_');
            if (!v->global && (p->lifted[v->id]
                  || (v->init && v->name && pr_reads_namesake(p, v->init, v))))
            {
               /* a local moved to file scope, or one whose initializer
                * reads its namesake: unique by id */
               pr_c(p, '_');
               pr_u(p, v->id);
            }
            return;
      }
   }
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
         if (nm && nm->flatten)
         {
            rslc_err(p->c, "glsl: a flattened block used as a value", NULL, v->line);
            p->ok = false;
            return;
         }
         pr_s(p, inst ? inst : v->name->str);
         return;
      }
      case RSLC_ST_MEMBER:
      {
         const char *inst = pr_block_instance(p, v->block);
         if (nm && nm->flatten)
         {
            pr_flat_member(p, v->block, v->member);
            return;
         }
         if (inst)
         {
            pr_s(p, inst);
            pr_c(p, '.');
         }
         pr_ident(p, p->c->prog.blocks[v->block].type->fields[v->member].name);
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
   /* Metal's matrix of scalars is no constant expression, its matrix
    * of columns is: a program-scope constant built from the former
    * needs a global constructor, which Metal refuses */
   bool cols = p->msl && t->kind == RSLC_TY_MAT;
   if (t->kind != RSLC_TY_SCALAR)
   {
      pr_type(p, t);
      pr_c(p, '(');
   }
   for (i = 0; i < n; i++)
   {
      if (cols && i % t->rows == 0)
      {
         if (i)
            pr_s(p, "), ");
         pr_type(p, rslc_type_basic(p->c, RSLC_TY_VEC, (enum rslc_base)t->base, t->rows, 1));
         pr_c(p, '(');
      }
      else if (i)
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
   if (cols && n)
      pr_c(p, ')');
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

/* HLSL: a sampler2D is a Texture2D and a SamplerState; the state is
 * named after the texture.  Only a variable (a uniform or a parameter)
 * can stand where a sampler is used. */
static void pr_hl_tex(struct rslc_pr *p, uint32_t node, bool state)
{
   const struct rslc_node *n = rslc_node_at(p->c, node);
   if (n->op != RSLC_OP_VAR)
   {
      rslc_err(p->c, "hlsl: sampler expression", NULL, n->line);
      p->ok = false;
      return;
   }
   if (state && !p->msl)
      pr_c(p, '_');
   pr_var_name(p, p->c->prog.vars[n->a]);
   if (state)
      pr_s(p, p->msl ? "Smplr" : "_sampler");
}

static void pr_args(struct rslc_pr *p, uint32_t list)
{
   uint32_t i, n = list ? rslc_list_len(p->c, list) : 0;
   pr_c(p, '(');
   for (i = 0; i < n; i++)
   {
      uint32_t a = rslc_list_item(p->c, list, i);
      if (i)
         pr_s(p, ", ");
      if ((p->hlsl || p->msl) && rslc_node_at(p->c, a)->type
            && rslc_node_at(p->c, a)->type->kind == RSLC_TY_SAMPLER2D)
      {
         pr_hl_tex(p, a, false);
         pr_s(p, ", ");
         pr_hl_tex(p, a, true);
         continue;
      }
      pr_expr(p, a, PR_PREC_ASSIGN);
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

/* The remaining arguments of a texture builtin, from index @first. */
static void pr_hl_rest(struct rslc_pr *p, uint32_t list, uint32_t first, uint32_t cnt)
{
   uint32_t i;
   for (i = first; i < cnt; i++)
   {
      pr_s(p, ", ");
      pr_expr(p, rslc_list_item(p->c, list, i), PR_PREC_ASSIGN);
   }
}

/* T.Method(_T_sampler, coord, rest...) */
static void pr_hl_sample(struct rslc_pr *p, uint32_t list, uint32_t cnt,
      const char *method, const char *after_coord)
{
   pr_hl_tex(p, rslc_list_item(p->c, list, 0), false);
   pr_c(p, '.');
   pr_s(p, method);
   pr_c(p, '(');
   pr_hl_tex(p, rslc_list_item(p->c, list, 0), true);
   pr_s(p, ", ");
   pr_expr(p, rslc_list_item(p->c, list, 1), PR_PREC_ASSIGN);
   if (after_coord)
      pr_s(p, after_coord);
   pr_hl_rest(p, list, 2, cnt);
   pr_c(p, ')');
}

static const struct rslc_type *pr_arg_type(struct rslc_pr *p, uint32_t list, uint32_t i)
{
   return rslc_node_at(p->c, rslc_list_item(p->c, list, i))->type;
}

static void pr_hlsl_builtin(struct rslc_pr *p, const struct rslc_node *n)
{
   unsigned    bfn  = n->a;
   uint32_t    list = n->b;
   uint32_t    cnt  = list ? rslc_list_len(p->c, list) : 0;
   const char *name = NULL;
   bool        cast = false;   /* wrap in the result type */
   bool        vertex = p->c->stage == RSLC_STAGE_VERTEX;
   struct rslc_cval cv;

   switch (bfn)
   {
      case RSLC_BFN_ATAN:          name = cnt == 2 ? "atan2" : "atan"; break;
      case RSLC_BFN_INVERSESQRT:   name = "rsqrt"; break;
      case RSLC_BFN_ROUNDEVEN:     name = "round"; break;
      case RSLC_BFN_FRACT:         name = "frac"; break;
      case RSLC_BFN_FLOATBITSTOINT:  name = "asint"; break;
      case RSLC_BFN_FLOATBITSTOUINT: name = "asuint"; break;
      case RSLC_BFN_INTBITSTOFLOAT:
      case RSLC_BFN_UINTBITSTOFLOAT: name = "asfloat"; break;
      case RSLC_BFN_FMA:           name = "mad"; break;
      case RSLC_BFN_BITFIELDREVERSE: name = "reversebits"; break;
      case RSLC_BFN_BITCOUNT:      name = "countbits"; cast = true; break;
      case RSLC_BFN_FINDLSB:       name = "firstbitlow"; cast = true; break;
      case RSLC_BFN_FINDMSB:       name = "firstbithigh"; cast = true; break;
      case RSLC_BFN_SIGN:          name = "sign"; cast = n->type->base == RSLC_BASE_FLOAT; break;
      case RSLC_BFN_DFDX:          name = "ddx"; break;
      case RSLC_BFN_DFDY:          name = "ddy"; break;
      case RSLC_BFN_DFDXFINE:      name = p->version >= 50 ? "ddx_fine" : "ddx"; break;
      case RSLC_BFN_DFDYFINE:      name = p->version >= 50 ? "ddy_fine" : "ddy"; break;
      case RSLC_BFN_DFDXCOARSE:    name = p->version >= 50 ? "ddx_coarse" : "ddx"; break;
      case RSLC_BFN_DFDYCOARSE:    name = p->version >= 50 ? "ddy_coarse" : "ddy"; break;
      case RSLC_BFN_FWIDTH:
      case RSLC_BFN_FWIDTHFINE:
      case RSLC_BFN_FWIDTHCOARSE:  name = "fwidth"; break;
      case RSLC_BFN_PACKUNORM2X16:   name = "rslc_packUnorm2x16"; break;
      case RSLC_BFN_UNPACKUNORM2X16: name = "rslc_unpackUnorm2x16"; break;
      case RSLC_BFN_PACKSNORM2X16:   name = "rslc_packSnorm2x16"; break;
      case RSLC_BFN_UNPACKSNORM2X16: name = "rslc_unpackSnorm2x16"; break;
      case RSLC_BFN_PACKUNORM4X8:    name = "rslc_packUnorm4x8"; break;
      case RSLC_BFN_UNPACKUNORM4X8:  name = "rslc_unpackUnorm4x8"; break;
      case RSLC_BFN_PACKSNORM4X8:    name = "rslc_packSnorm4x8"; break;
      case RSLC_BFN_UNPACKSNORM4X8:  name = "rslc_unpackSnorm4x8"; break;
      case RSLC_BFN_PACKHALF2X16:    name = "rslc_packHalf2x16"; break;
      case RSLC_BFN_UNPACKHALF2X16:  name = "rslc_unpackHalf2x16"; break;
      case RSLC_BFN_TEXTUREQUERYLEVELS: name = "rslc_levels"; break;
      case RSLC_BFN_BITFIELDEXTRACT:
         name = n->type->base == RSLC_BASE_INT ? "rslc_bfe_i" : "rslc_bfe_u";
         break;
      case RSLC_BFN_BITFIELDINSERT:
         name = "rslc_bfi";
         cast = true;
         break;
      case RSLC_BFN_INVERSE:
      {
         const struct rslc_type *t = pr_arg_type(p, list, 0);
         name = t->cols == 2 ? "rslc_inverse2" : t->cols == 3 ? "rslc_inverse3" : "rslc_inverse4";
         break;
      }
      case RSLC_BFN_MIX:
         if (cnt == 3 && pr_arg_type(p, list, 2)->base == RSLC_BASE_BOOL)
         {
            /* a select: the ternary is componentwise on a bool vector */
            pr_c(p, '(');
            pr_expr(p, rslc_list_item(p->c, list, 2), PR_PREC_COND - 1);
            pr_s(p, " ? ");
            pr_expr(p, rslc_list_item(p->c, list, 1), PR_PREC_ASSIGN);
            pr_s(p, " : ");
            pr_expr(p, rslc_list_item(p->c, list, 0), PR_PREC_ASSIGN);
            pr_c(p, ')');
            return;
         }
         name = "lerp";
         break;
      case RSLC_BFN_MOD:
      {
         /* GLSL mod is x - y * floor(x / y), not fmod */
         const struct rslc_type *tx = pr_arg_type(p, list, 0);
         const struct rslc_type *ty = pr_arg_type(p, list, 1);
         pr_s(p, "mod(");
         pr_expr(p, rslc_list_item(p->c, list, 0), PR_PREC_ASSIGN);
         pr_s(p, ", ");
         if (tx->kind == RSLC_TY_VEC && ty->kind == RSLC_TY_SCALAR)
         {
            pr_c(p, '(');
            pr_expr(p, rslc_list_item(p->c, list, 1), PR_PREC_SEQ);
            pr_s(p, ").");
            pr_s(p, &"xxxx"[4 - tx->rows]);
         }
         else
            pr_expr(p, rslc_list_item(p->c, list, 1), PR_PREC_ASSIGN);
         pr_c(p, ')');
         return;
      }
      case RSLC_BFN_MATRIXCOMPMULT:
         pr_c(p, '(');
         pr_expr(p, rslc_list_item(p->c, list, 0), PR_PREC_MUL);
         pr_s(p, " * ");
         pr_expr(p, rslc_list_item(p->c, list, 1), PR_PREC_MUL - 1);
         pr_c(p, ')');
         return;
      case RSLC_BFN_LESSTHAN:
      case RSLC_BFN_LESSTHANEQUAL:
      case RSLC_BFN_GREATERTHAN:
      case RSLC_BFN_GREATERTHANEQUAL:
      case RSLC_BFN_EQUAL:
      case RSLC_BFN_NOTEQUAL:
      {
         /* componentwise on vectors, as HLSL's operators are */
         const char *op = bfn == RSLC_BFN_LESSTHAN ? "<" : bfn == RSLC_BFN_LESSTHANEQUAL ? "<="
            : bfn == RSLC_BFN_GREATERTHAN ? ">" : bfn == RSLC_BFN_GREATERTHANEQUAL ? ">="
            : bfn == RSLC_BFN_EQUAL ? "==" : "!=";
         int prec = bfn == RSLC_BFN_EQUAL || bfn == RSLC_BFN_NOTEQUAL ? PR_PREC_EQ : PR_PREC_REL;
         pr_c(p, '(');
         pr_expr(p, rslc_list_item(p->c, list, 0), prec);
         pr_c(p, ' ');
         pr_s(p, op);
         pr_c(p, ' ');
         pr_expr(p, rslc_list_item(p->c, list, 1), prec - 1);
         pr_c(p, ')');
         return;
      }
      case RSLC_BFN_NOT:
         pr_s(p, "(!");
         pr_expr(p, rslc_list_item(p->c, list, 0), PR_PREC_UNARY);
         pr_c(p, ')');
         return;
      case RSLC_BFN_TEXTURESIZE:
      {
         pr_s(p, "int2(rslc_size(");
         pr_hl_tex(p, rslc_list_item(p->c, list, 0), false);
         pr_s(p, ", uint(");
         pr_expr(p, rslc_list_item(p->c, list, 1), PR_PREC_SEQ);
         pr_s(p, ")))");
         return;
      }
      case RSLC_BFN_TEXTURE:
         if (vertex)
            pr_hl_sample(p, list, cnt, "SampleLevel", ", 0.0f");
         else
            pr_hl_sample(p, list, cnt, cnt == 3 ? "SampleBias" : "Sample", NULL);
         return;
      case RSLC_BFN_TEXTUREOFFSET:
         /* (s, uv, offset [, bias]): HLSL takes the offset last */
         if (vertex || cnt == 4)
         {
            pr_hl_tex(p, rslc_list_item(p->c, list, 0), false);
            pr_s(p, vertex ? ".SampleLevel(" : ".SampleBias(");
            pr_hl_tex(p, rslc_list_item(p->c, list, 0), true);
            pr_s(p, ", ");
            pr_expr(p, rslc_list_item(p->c, list, 1), PR_PREC_ASSIGN);
            pr_s(p, ", ");
            if (vertex)
               pr_s(p, "0.0f");
            else
               pr_expr(p, rslc_list_item(p->c, list, 3), PR_PREC_ASSIGN);
            pr_s(p, ", ");
            pr_expr(p, rslc_list_item(p->c, list, 2), PR_PREC_ASSIGN);
            pr_c(p, ')');
         }
         else
            pr_hl_sample(p, list, cnt, "Sample", NULL);
         return;
      case RSLC_BFN_TEXTUREPROJ:
      {
         const struct rslc_type *ct = pr_arg_type(p, list, 1);
         pr_hl_tex(p, rslc_list_item(p->c, list, 0), false);
         pr_s(p, vertex ? ".SampleLevel(" : cnt == 3 ? ".SampleBias(" : ".Sample(");
         pr_hl_tex(p, rslc_list_item(p->c, list, 0), true);
         pr_s(p, ", (");
         pr_expr(p, rslc_list_item(p->c, list, 1), PR_PREC_SEQ);
         pr_s(p, ").xy / (");
         pr_expr(p, rslc_list_item(p->c, list, 1), PR_PREC_SEQ);
         pr_s(p, ct->rows == 4 ? ").w" : ").z");
         if (vertex)
            pr_s(p, ", 0.0f");
         else
            pr_hl_rest(p, list, 2, cnt);
         pr_c(p, ')');
         return;
      }
      case RSLC_BFN_TEXTURELOD:
      case RSLC_BFN_TEXTURELODOFFSET:
         pr_hl_sample(p, list, cnt, "SampleLevel", NULL);
         return;
      case RSLC_BFN_TEXTUREGRAD:
      case RSLC_BFN_TEXTUREGRADOFFSET:
         pr_hl_sample(p, list, cnt, "SampleGrad", NULL);
         return;
      case RSLC_BFN_TEXELFETCH:
      case RSLC_BFN_TEXELFETCHOFFSET:
         pr_hl_tex(p, rslc_list_item(p->c, list, 0), false);
         pr_s(p, ".Load(int3(");
         pr_expr(p, rslc_list_item(p->c, list, 1), PR_PREC_ASSIGN);
         pr_s(p, ", ");
         pr_expr(p, rslc_list_item(p->c, list, 2), PR_PREC_ASSIGN);
         pr_c(p, ')');
         pr_hl_rest(p, list, 3, cnt);
         pr_c(p, ')');
         return;
      case RSLC_BFN_TEXTUREGATHER:
      case RSLC_BFN_TEXTUREGATHEROFFSET:
      {
         /* (s, uv [, offset] [, comp]): the component picks the method;
          * shader model 4 gathers the red component only */
         uint32_t base_args = bfn == RSLC_BFN_TEXTUREGATHER ? 2 : 3;
         uint32_t comp = 0;
         if (cnt > base_args)
         {
            if (!rslc_const_eval(p->c, rslc_list_item(p->c, list, cnt - 1), &cv))
            {
               rslc_err(p->c, "hlsl: textureGather component must be constant", NULL, n->line);
               p->ok = false;
               return;
            }
            comp = cv.v[0] & 3u;
         }
         if (p->version < 41 && comp)
         {
            rslc_err(p->c, "hlsl: shader model 4 gathers the red component only", NULL, n->line);
            p->ok = false;
            return;
         }
         pr_hl_sample(p, list, base_args, p->version < 41 ? "Gather"
               : comp == 0 ? "GatherRed" : comp == 1 ? "GatherGreen"
               : comp == 2 ? "GatherBlue" : "GatherAlpha", NULL);
         return;
      }
      case RSLC_BFN_ASINH:
      case RSLC_BFN_ACOSH:
      case RSLC_BFN_ATANH:
      case RSLC_BFN_FREXP:
      case RSLC_BFN_OUTERPRODUCT:
      case RSLC_BFN_UADDCARRY:
      case RSLC_BFN_USUBBORROW:
      case RSLC_BFN_UMULEXTENDED:
      case RSLC_BFN_IMULEXTENDED:
      case RSLC_BFN_TEXTUREQUERYLOD:
      case RSLC_BFN_TEXTUREPROJOFFSET:
      case RSLC_BFN_TEXTUREPROJLOD:
      case RSLC_BFN_TEXTUREPROJLODOFFSET:
      case RSLC_BFN_TEXTUREPROJGRAD:
      case RSLC_BFN_TEXTUREPROJGRADOFFSET:
         rslc_err(p->c, "hlsl: no form of this builtin", rslc_bfn_name(bfn), n->line);
         p->ok = false;
         return;
      default:
         name = rslc_bfn_name(bfn);
         break;
   }
   if (cast)
   {
      pr_type(p, n->type);
      pr_c(p, '(');
   }
   pr_s(p, name);
   pr_args(p, list);
   if (cast)
      pr_c(p, ')');
}

/* MSL: T.sample(TSmplr, coord, <after>, rest...) where @after names
 * the level / bias / gradient of argument @arg (0: none). */
static void pr_ms_sample(struct rslc_pr *p, uint32_t list, uint32_t cnt,
      const char *method, const char *after, uint32_t arg, uint32_t arg2,
      uint32_t offset_arg)
{
   pr_hl_tex(p, rslc_list_item(p->c, list, 0), false);
   pr_c(p, '.');
   pr_s(p, method);
   pr_c(p, '(');
   pr_hl_tex(p, rslc_list_item(p->c, list, 0), true);
   pr_s(p, ", ");
   pr_expr(p, rslc_list_item(p->c, list, 1), PR_PREC_ASSIGN);
   if (after)
   {
      pr_s(p, ", ");
      pr_s(p, after);
      pr_c(p, '(');
      if (arg)
         pr_expr(p, rslc_list_item(p->c, list, arg), PR_PREC_ASSIGN);
      else
         pr_s(p, "0.0");
      if (arg2)
      {
         pr_s(p, ", ");
         pr_expr(p, rslc_list_item(p->c, list, arg2), PR_PREC_ASSIGN);
      }
      pr_c(p, ')');
   }
   if (offset_arg && offset_arg < cnt)
   {
      pr_s(p, ", ");
      pr_expr(p, rslc_list_item(p->c, list, offset_arg), PR_PREC_ASSIGN);
   }
   pr_c(p, ')');
}

static void pr_msl_builtin(struct rslc_pr *p, const struct rslc_node *n)
{
   unsigned    bfn  = n->a;
   uint32_t    list = n->b;
   uint32_t    cnt  = list ? rslc_list_len(p->c, list) : 0;
   const char *name = NULL;
   bool        vertex = p->c->stage == RSLC_STAGE_VERTEX;
   struct rslc_cval cv;

   switch (bfn)
   {
      case RSLC_BFN_ATAN:          name = cnt == 2 ? "atan2" : "atan"; break;
      case RSLC_BFN_INVERSESQRT:   name = "rsqrt"; break;
      case RSLC_BFN_ROUNDEVEN:     name = "rint"; break;
      case RSLC_BFN_POW:
      {
         /* fast-math pow() takes |x|; powr() leaves x < 0 undefined
          * as GLSL does, but a constant base >= 0 keeps the more exact pow() */
         unsigned i, ncomp = 0;
         name = "powr";
         if (cnt == 2 && rslc_const_eval(p->c, rslc_list_item(p->c, list, 0), &cv))
         {
            ncomp = rslc_ty_components(cv.type);
            for (i = 0; i < ncomp; i++)
               if (!(cv.d[i] >= 0.0))
                  break;
            if (i == ncomp)
               name = "pow";
         }
         break;
      }
      case RSLC_BFN_DFDX:
      case RSLC_BFN_DFDXFINE:
      case RSLC_BFN_DFDXCOARSE:    name = "dfdx"; break;
      case RSLC_BFN_DFDY:
      case RSLC_BFN_DFDYFINE:
      case RSLC_BFN_DFDYCOARSE:    name = "dfdy"; break;
      case RSLC_BFN_FWIDTH:
      case RSLC_BFN_FWIDTHFINE:
      case RSLC_BFN_FWIDTHCOARSE:  name = "fwidth"; break;
      case RSLC_BFN_BITCOUNT:      name = "popcount"; break;
      case RSLC_BFN_BITFIELDEXTRACT: name = "extract_bits"; break;
      case RSLC_BFN_BITFIELDINSERT:  name = "insert_bits"; break;
      case RSLC_BFN_BITFIELDREVERSE: name = "reverse_bits"; break;
      case RSLC_BFN_FINDLSB:       name = "rslc_findlsb"; break;
      case RSLC_BFN_FINDMSB:       name = "rslc_findmsb"; break;
      case RSLC_BFN_PACKUNORM2X16:   name = "pack_float_to_unorm2x16"; break;
      case RSLC_BFN_UNPACKUNORM2X16: name = "unpack_unorm2x16_to_float"; break;
      case RSLC_BFN_PACKSNORM2X16:   name = "pack_float_to_snorm2x16"; break;
      case RSLC_BFN_UNPACKSNORM2X16: name = "unpack_snorm2x16_to_float"; break;
      case RSLC_BFN_PACKUNORM4X8:    name = "pack_float_to_unorm4x8"; break;
      case RSLC_BFN_UNPACKUNORM4X8:  name = "unpack_unorm4x8_to_float"; break;
      case RSLC_BFN_PACKSNORM4X8:    name = "pack_float_to_snorm4x8"; break;
      case RSLC_BFN_UNPACKSNORM4X8:  name = "unpack_snorm4x8_to_float"; break;
      case RSLC_BFN_PACKHALF2X16:
         pr_s(p, "as_type<uint>(half2(");
         pr_expr(p, rslc_list_item(p->c, list, 0), PR_PREC_SEQ);
         pr_s(p, "))");
         return;
      case RSLC_BFN_UNPACKHALF2X16:
         pr_s(p, "float2(as_type<half2>(");
         pr_expr(p, rslc_list_item(p->c, list, 0), PR_PREC_SEQ);
         pr_s(p, "))");
         return;
      case RSLC_BFN_FLOATBITSTOINT:
      case RSLC_BFN_FLOATBITSTOUINT:
      case RSLC_BFN_INTBITSTOFLOAT:
      case RSLC_BFN_UINTBITSTOFLOAT:
         pr_s(p, "as_type<");
         pr_type(p, n->type);
         pr_s(p, ">(");
         pr_expr(p, rslc_list_item(p->c, list, 0), PR_PREC_SEQ);
         pr_c(p, ')');
         return;
      case RSLC_BFN_RADIANS:
      case RSLC_BFN_DEGREES:
         pr_c(p, '(');
         pr_expr(p, rslc_list_item(p->c, list, 0), PR_PREC_MUL);
         pr_s(p, bfn == RSLC_BFN_RADIANS ? " * 0.0174532924" : " * 57.2957802");
         pr_c(p, ')');
         return;
      case RSLC_BFN_LENGTH:
      case RSLC_BFN_DISTANCE:
      case RSLC_BFN_DOT:
      case RSLC_BFN_NORMALIZE:
         /* Metal has these for vectors only; of scalars they are |x|,
          * |a - b|, a * b and sign(x) */
         if (pr_arg_type(p, list, 0)->kind == RSLC_TY_SCALAR)
         {
            pr_s(p, bfn == RSLC_BFN_DOT ? "(" : bfn == RSLC_BFN_NORMALIZE ? "sign(" : "abs(");
            pr_expr(p, rslc_list_item(p->c, list, 0),
                  cnt == 2 ? PR_PREC_MUL : PR_PREC_SEQ);
            if (cnt == 2)
            {
               pr_s(p, bfn == RSLC_BFN_DOT ? " * " : " - ");
               pr_expr(p, rslc_list_item(p->c, list, 1), PR_PREC_MUL);
            }
            pr_c(p, ')');
            return;
         }
         name = rslc_bfn_name(bfn);
         break;
      case RSLC_BFN_MOD:
         name = "mod";   /* the helper: x - y * floor(x / y) */
         break;
      case RSLC_BFN_MATRIXCOMPMULT:
         name = "rslc_cmul";
         break;
      case RSLC_BFN_INVERSE:
      {
         const struct rslc_type *t = pr_arg_type(p, list, 0);
         name = t->cols == 2 ? "rslc_inverse2" : t->cols == 3 ? "rslc_inverse3" : "rslc_inverse4";
         break;
      }
      case RSLC_BFN_MIX:
         if (cnt == 3 && pr_arg_type(p, list, 2)->base == RSLC_BASE_BOOL)
         {
            name = "select";   /* (a, b, c): b where c */
            break;
         }
         name = "mix";
         break;
      case RSLC_BFN_LESSTHAN:
      case RSLC_BFN_LESSTHANEQUAL:
      case RSLC_BFN_GREATERTHAN:
      case RSLC_BFN_GREATERTHANEQUAL:
      case RSLC_BFN_EQUAL:
      case RSLC_BFN_NOTEQUAL:
      {
         const char *op = bfn == RSLC_BFN_LESSTHAN ? "<" : bfn == RSLC_BFN_LESSTHANEQUAL ? "<="
            : bfn == RSLC_BFN_GREATERTHAN ? ">" : bfn == RSLC_BFN_GREATERTHANEQUAL ? ">="
            : bfn == RSLC_BFN_EQUAL ? "==" : "!=";
         int prec = bfn == RSLC_BFN_EQUAL || bfn == RSLC_BFN_NOTEQUAL ? PR_PREC_EQ : PR_PREC_REL;
         pr_c(p, '(');
         pr_expr(p, rslc_list_item(p->c, list, 0), prec);
         pr_c(p, ' ');
         pr_s(p, op);
         pr_c(p, ' ');
         pr_expr(p, rslc_list_item(p->c, list, 1), prec - 1);
         pr_c(p, ')');
         return;
      }
      case RSLC_BFN_NOT:
         pr_s(p, "(!");
         pr_expr(p, rslc_list_item(p->c, list, 0), PR_PREC_UNARY);
         pr_c(p, ')');
         return;
      case RSLC_BFN_TEXTURESIZE:
         pr_s(p, "rslc_size(");
         pr_hl_tex(p, rslc_list_item(p->c, list, 0), false);
         pr_s(p, ", uint(");
         pr_expr(p, rslc_list_item(p->c, list, 1), PR_PREC_SEQ);
         pr_s(p, "))");
         return;
      case RSLC_BFN_TEXTUREQUERYLEVELS:
         pr_s(p, "int(");
         pr_hl_tex(p, rslc_list_item(p->c, list, 0), false);
         pr_s(p, ".get_num_mip_levels())");
         return;
      case RSLC_BFN_TEXTURE:
         if (vertex)
            pr_ms_sample(p, list, cnt, "sample", "level", 0, 0, 0);
         else if (cnt == 3)
            pr_ms_sample(p, list, cnt, "sample", "bias", 2, 0, 0);
         else
            pr_ms_sample(p, list, cnt, "sample", NULL, 0, 0, 0);
         return;
      case RSLC_BFN_TEXTUREOFFSET:
         /* (s, uv, offset [, bias]) */
         if (vertex)
            pr_ms_sample(p, list, cnt, "sample", "level", 0, 0, 2);
         else if (cnt == 4)
            pr_ms_sample(p, list, cnt, "sample", "bias", 3, 0, 2);
         else
            pr_ms_sample(p, list, cnt, "sample", NULL, 0, 0, 2);
         return;
      case RSLC_BFN_TEXTURELOD:
         pr_ms_sample(p, list, cnt, "sample", "level", 2, 0, 0);
         return;
      case RSLC_BFN_TEXTURELODOFFSET:
         pr_ms_sample(p, list, cnt, "sample", "level", 2, 0, 3);
         return;
      case RSLC_BFN_TEXTUREGRAD:
         pr_ms_sample(p, list, cnt, "sample", "gradient2d", 2, 3, 0);
         return;
      case RSLC_BFN_TEXTUREGRADOFFSET:
         pr_ms_sample(p, list, cnt, "sample", "gradient2d", 2, 3, 4);
         return;
      case RSLC_BFN_TEXTUREPROJ:
      {
         const struct rslc_type *ct = pr_arg_type(p, list, 1);
         pr_hl_tex(p, rslc_list_item(p->c, list, 0), false);
         pr_s(p, ".sample(");
         pr_hl_tex(p, rslc_list_item(p->c, list, 0), true);
         pr_s(p, ", (");
         pr_expr(p, rslc_list_item(p->c, list, 1), PR_PREC_SEQ);
         pr_s(p, ").xy / (");
         pr_expr(p, rslc_list_item(p->c, list, 1), PR_PREC_SEQ);
         pr_s(p, ct->rows == 4 ? ").w" : ").z");
         if (vertex)
            pr_s(p, ", level(0.0)");
         else if (cnt == 3)
         {
            pr_s(p, ", bias(");
            pr_expr(p, rslc_list_item(p->c, list, 2), PR_PREC_ASSIGN);
            pr_c(p, ')');
         }
         pr_c(p, ')');
         return;
      }
      case RSLC_BFN_TEXELFETCH:
      case RSLC_BFN_TEXELFETCHOFFSET:
         pr_hl_tex(p, rslc_list_item(p->c, list, 0), false);
         pr_s(p, ".read(uint2(");
         pr_expr(p, rslc_list_item(p->c, list, 1), PR_PREC_ADD);
         if (cnt == 4)
         {
            pr_s(p, " + ");
            pr_expr(p, rslc_list_item(p->c, list, 3), PR_PREC_ADD - 1);
         }
         pr_s(p, "), uint(");
         pr_expr(p, rslc_list_item(p->c, list, 2), PR_PREC_SEQ);
         pr_s(p, "))");
         return;
      case RSLC_BFN_TEXTUREGATHER:
      case RSLC_BFN_TEXTUREGATHEROFFSET:
      {
         /* (s, uv [, offset] [, comp]) */
         uint32_t base_args = bfn == RSLC_BFN_TEXTUREGATHER ? 2 : 3;
         uint32_t comp = 0;
         if (cnt > base_args)
         {
            if (!rslc_const_eval(p->c, rslc_list_item(p->c, list, cnt - 1), &cv))
            {
               rslc_err(p->c, "msl: textureGather component must be constant", NULL, n->line);
               p->ok = false;
               return;
            }
            comp = cv.v[0] & 3u;
         }
         pr_hl_tex(p, rslc_list_item(p->c, list, 0), false);
         pr_s(p, ".gather(");
         pr_hl_tex(p, rslc_list_item(p->c, list, 0), true);
         pr_s(p, ", ");
         pr_expr(p, rslc_list_item(p->c, list, 1), PR_PREC_ASSIGN);
         pr_s(p, ", ");
         if (base_args == 3)
            pr_expr(p, rslc_list_item(p->c, list, 2), PR_PREC_ASSIGN);
         else
            pr_s(p, "int2(0)");
         pr_s(p, comp == 0 ? ", component::x)" : comp == 1 ? ", component::y)"
               : comp == 2 ? ", component::z)" : ", component::w)");
         return;
      }
      case RSLC_BFN_OUTERPRODUCT:
      case RSLC_BFN_UADDCARRY:
      case RSLC_BFN_USUBBORROW:
      case RSLC_BFN_UMULEXTENDED:
      case RSLC_BFN_IMULEXTENDED:
      case RSLC_BFN_TEXTUREQUERYLOD:
      case RSLC_BFN_TEXTUREPROJOFFSET:
      case RSLC_BFN_TEXTUREPROJLOD:
      case RSLC_BFN_TEXTUREPROJLODOFFSET:
      case RSLC_BFN_TEXTUREPROJGRAD:
      case RSLC_BFN_TEXTUREPROJGRADOFFSET:
         rslc_err(p->c, "msl: no form of this builtin", rslc_bfn_name(bfn), n->line);
         p->ok = false;
         return;
      default:
         name = rslc_bfn_name(bfn);
         break;
   }
   pr_s(p, name);
   pr_args(p, list);
}

static void pr_builtin(struct rslc_pr *p, const struct rslc_node *n)
{
   unsigned bfn = n->a;
   if (p->hlsl)
   {
      pr_hlsl_builtin(p, n);
      return;
   }
   if (p->msl)
   {
      pr_msl_builtin(p, n);
      return;
   }
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

static bool pr_is_aggregate(const struct rslc_type *t)
{
   return t && (t->kind == RSLC_TY_ARRAY || t->kind == RSLC_TY_STRUCT);
}

/* HLSL: the temporary an aggregate constructor was hoisted into, or
 * RSLC_UNSET. */
static uint32_t pr_hoisted(struct rslc_pr *p, uint32_t node)
{
   uint32_t i;
   for (i = 0; i < p->nhoist; i += 2)
      if (p->hoist[i] == node)
         return p->hoist[i + 1];
   return RSLC_UNSET;
}

static void pr_temp_name(struct rslc_pr *p, uint32_t temp)
{
   pr_s(p, "_rslc_t");
   pr_u(p, temp);
}

/* HLSL: an aggregate constructor as an initialiser list, nested
 * aggregates as nested lists. */
static void pr_initlist(struct rslc_pr *p, uint32_t node)
{
   const struct rslc_node *n = rslc_node_at(p->c, node);
   uint32_t i, cnt = n->b ? rslc_list_len(p->c, n->b) : 0;
   /* metal::array is a struct around the elements: one brace for it,
    * one for them */
   bool twice = p->msl && n->type->kind == RSLC_TY_ARRAY;
   pr_s(p, twice ? "{{ " : "{ ");
   for (i = 0; i < cnt; i++)
   {
      uint32_t an = rslc_list_item(p->c, n->b, i);
      const struct rslc_node *a = rslc_node_at(p->c, an);
      if (i)
         pr_s(p, ", ");
      if (a->op == RSLC_OP_CONSTRUCT && pr_is_aggregate(a->type) && pr_hoisted(p, an) == RSLC_UNSET)
         pr_initlist(p, an);
      else
         pr_expr(p, an, PR_PREC_ASSIGN);
   }
   pr_s(p, twice ? " }}" : " }");
}

/* HLSL: a value of another base as a bool, spelled as the comparison
 * HLSL compilers agree on. */
static bool pr_hlsl_to_bool(struct rslc_pr *p, const struct rslc_type *t, uint32_t an)
{
   const struct rslc_type *at = rslc_node_at(p->c, an)->type;
   if (t->base != RSLC_BASE_BOOL || at->base == RSLC_BASE_BOOL
         || (t->kind != RSLC_TY_SCALAR && t->kind != RSLC_TY_VEC)
         || (at->kind != RSLC_TY_SCALAR && at->kind != RSLC_TY_VEC)
         || rslc_ty_components(at) != rslc_ty_components(t))
      return false;
   pr_c(p, '(');
   pr_expr(p, an, PR_PREC_EQ);
   pr_s(p, at->base == RSLC_BASE_FLOAT ? " != 0.0f" : at->base == RSLC_BASE_UINT ? " != 0u" : " != 0");
   if (at->kind == RSLC_TY_VEC)
   {
      pr_s(p, ".");
      pr_s(p, &"xxxx"[4 - at->rows]);
   }
   pr_c(p, ')');
   return true;
}

/* HLSL: a vector or matrix constructor.  HLSL's constructors take
 * exactly the components they need, with no scalar splat and no
 * truncation, and a matrix from one scalar or one matrix is spelled
 * out. */
static void pr_hlsl_construct(struct rslc_pr *p, const struct rslc_node *n)
{
   const struct rslc_type *t = n->type;
   uint32_t cnt = n->b ? rslc_list_len(p->c, n->b) : 0;
   unsigned need = rslc_ty_components(t);
   unsigned have = 0, i;
   static const char comps[] = "xyzw";

   if (cnt == 1)
   {
      uint32_t an = rslc_list_item(p->c, n->b, 0);
      const struct rslc_type *at = rslc_node_at(p->c, an)->type;
      if (pr_hlsl_to_bool(p, t, an))
         return;
      if (t->kind == RSLC_TY_VEC && at->kind == RSLC_TY_SCALAR)
      {
         /* a splat */
         if (at->base != t->base)
         {
            pr_type(p, t);
            pr_c(p, '(');
         }
         pr_c(p, '(');
         pr_expr(p, an, PR_PREC_SEQ);
         pr_s(p, ").");
         pr_s(p, &"xxxx"[4 - t->rows]);
         if (at->base != t->base)
            pr_c(p, ')');
         return;
      }
      if ((t->kind == RSLC_TY_VEC || t->kind == RSLC_TY_SCALAR) && at->kind == RSLC_TY_VEC
            && at->rows > (t->kind == RSLC_TY_VEC ? t->rows : 1))
      {
         /* the leading components */
         unsigned keep = t->kind == RSLC_TY_VEC ? t->rows : 1;
         if (at->base != t->base)
         {
            pr_type(p, t);
            pr_c(p, '(');
         }
         pr_c(p, '(');
         pr_expr(p, an, PR_PREC_SEQ);
         pr_s(p, ").");
         for (i = 0; i < keep; i++)
            pr_c(p, comps[i]);
         if (at->base != t->base)
            pr_c(p, ')');
         return;
      }
      if (t->kind == RSLC_TY_MAT && at->kind == RSLC_TY_SCALAR)
      {
         /* the scalar on the diagonal */
         unsigned r, cc;
         pr_type(p, t);
         pr_c(p, '(');
         for (cc = 0; cc < t->cols; cc++)
            for (r = 0; r < t->rows; r++)
            {
               if (cc || r)
                  pr_s(p, ", ");
               if (cc == r)
               {
                  pr_c(p, '(');
                  pr_expr(p, an, PR_PREC_SEQ);
                  pr_c(p, ')');
               }
               else
                  pr_s(p, "0.0f");
            }
         pr_c(p, ')');
         return;
      }
      if (t->kind == RSLC_TY_MAT && at->kind == RSLC_TY_MAT)
      {
         /* columns kept, the rest from the identity */
         unsigned r, cc;
         pr_type(p, t);
         pr_c(p, '(');
         for (cc = 0; cc < t->cols; cc++)
            for (r = 0; r < t->rows; r++)
            {
               if (cc || r)
                  pr_s(p, ", ");
               if (cc < at->cols && r < at->rows)
               {
                  pr_c(p, '(');
                  pr_expr(p, an, PR_PREC_SEQ);
                  pr_s(p, ")[");
                  pr_u(p, cc);
                  pr_s(p, "][");
                  pr_u(p, r);
                  pr_c(p, ']');
               }
               else
                  pr_s(p, cc == r ? "1.0f" : "0.0f");
            }
         pr_c(p, ')');
         return;
      }
      if (t->kind == RSLC_TY_VEC && at->kind == RSLC_TY_MAT)
      {
         rslc_err(p->c, "hlsl: vector from a matrix", NULL, n->line);
         p->ok = false;
         return;
      }
   }
   pr_type(p, t);
   pr_c(p, '(');
   for (i = 0; i < cnt; i++)
   {
      uint32_t an = rslc_list_item(p->c, n->b, i);
      const struct rslc_node *a = rslc_node_at(p->c, an);
      unsigned ac = rslc_ty_components(a->type);
      struct rslc_cval cv;
      if (have >= need)
         break;   /* GLSL ignores arguments past the last component */
      if (i)
         pr_s(p, ", ");
      if (have + ac > need && a->type->kind == RSLC_TY_VEC)
      {
         /* a partly used vector: its leading components */
         unsigned k, keep = need - have;
         pr_c(p, '(');
         pr_expr(p, an, PR_PREC_SEQ);
         pr_s(p, ").");
         for (k = 0; k < keep; k++)
            pr_c(p, comps[k]);
         have = need;
         continue;
      }
      have += ac;
      if (a->op == RSLC_OP_CONST && a->type->kind == RSLC_TY_SCALAR
            && a->type->base != t->base && rslc_const_eval(p->c, an, &cv))
      {
         /* a literal of another base converts here */
         struct rslc_cval cc;
         const struct rslc_type *st = rslc_type_basic(p->c, RSLC_TY_SCALAR,
               (enum rslc_base)t->base, 1, 1);
         double d = a->type->base == RSLC_BASE_FLOAT ? cv.d[0]
            : a->type->base == RSLC_BASE_INT ? (double)(int32_t)cv.v[0] : (double)cv.v[0];
         memset(&cc, 0, sizeof(cc));
         cc.type = st;
         if (t->base == RSLC_BASE_FLOAT)
         {
            float f = (float)d;
            memcpy(&cc.v[0], &f, sizeof(f));
         }
         else if (t->base == RSLC_BASE_BOOL)
            cc.v[0] = d != 0.0;
         else if (a->type->base == RSLC_BASE_FLOAT)
            cc.v[0] = t->base == RSLC_BASE_INT ? (uint32_t)(int32_t)d : (uint32_t)d;
         else
            cc.v[0] = cv.v[0];
         pr_cval(p, st, &cc);
      }
      else
         pr_expr(p, an, PR_PREC_ASSIGN);
   }
   pr_c(p, ')');
}

/* MSL: a constructor.  Vectors and matrices construct as in GLSL but
 * for a vector from a longer one (a swizzle) and a matrix from another
 * (spelled by columns); an aggregate is a braced temporary. */
static void pr_msl_construct(struct rslc_pr *p, uint32_t node)
{
   const struct rslc_node *n = rslc_node_at(p->c, node);
   const struct rslc_type *t = n->type;
   uint32_t cnt = n->b ? rslc_list_len(p->c, n->b) : 0;
   uint32_t i;
   static const char comps[] = "xyzw";

   if (pr_is_aggregate(t))
   {
      pr_type(p, t);
      pr_initlist(p, node);
      return;
   }
   if (cnt == 1)
   {
      uint32_t an = rslc_list_item(p->c, n->b, 0);
      const struct rslc_type *at = rslc_node_at(p->c, an)->type;
      if ((t->kind == RSLC_TY_VEC || t->kind == RSLC_TY_SCALAR) && at->kind == RSLC_TY_VEC
            && at->rows > (t->kind == RSLC_TY_VEC ? t->rows : 1))
      {
         unsigned keep = t->kind == RSLC_TY_VEC ? t->rows : 1;
         if (at->base != t->base)
         {
            pr_type(p, t);
            pr_c(p, '(');
         }
         pr_c(p, '(');
         pr_expr(p, an, PR_PREC_SEQ);
         pr_s(p, ").");
         for (i = 0; i < keep; i++)
            pr_c(p, comps[i]);
         if (at->base != t->base)
            pr_c(p, ')');
         return;
      }
      if (t->kind == RSLC_TY_MAT && at->kind == RSLC_TY_MAT)
      {
         unsigned r, cc;
         pr_type(p, t);
         pr_c(p, '(');
         for (cc = 0; cc < t->cols; cc++)
         {
            if (cc)
               pr_s(p, ", ");
            if (cc < at->cols)
            {
               if (at->rows == t->rows)
               {
                  pr_c(p, '(');
                  pr_expr(p, an, PR_PREC_SEQ);
                  pr_s(p, ")[");
                  pr_u(p, cc);
                  pr_c(p, ']');
                  continue;
               }
               pr_type(p, rslc_type_basic(p->c, RSLC_TY_VEC, RSLC_BASE_FLOAT, t->rows, 1));
               pr_s(p, "((");
               pr_expr(p, an, PR_PREC_SEQ);
               pr_s(p, ")[");
               pr_u(p, cc);
               pr_c(p, ']');
               if (at->rows > t->rows)
               {
                  pr_c(p, '.');
                  for (r = 0; r < t->rows; r++)
                     pr_c(p, comps[r]);
               }
               else
                  for (r = at->rows; r < t->rows; r++)
                     pr_s(p, r == cc ? ", 1.0" : ", 0.0");
               pr_c(p, ')');
            }
            else
            {
               pr_type(p, rslc_type_basic(p->c, RSLC_TY_VEC, RSLC_BASE_FLOAT, t->rows, 1));
               pr_c(p, '(');
               for (r = 0; r < t->rows; r++)
               {
                  if (r)
                     pr_s(p, ", ");
                  pr_s(p, r == cc ? "1.0" : "0.0");
               }
               pr_c(p, ')');
            }
         }
         pr_c(p, ')');
         return;
      }
      if (t->kind == RSLC_TY_VEC && at->kind == RSLC_TY_MAT)
      {
         rslc_err(p->c, "msl: vector from a matrix", NULL, n->line);
         p->ok = false;
         return;
      }
   }
   pr_type(p, t);
   pr_c(p, '(');
   if (t->kind == RSLC_TY_VEC || t->kind == RSLC_TY_MAT)
   {
      /* GLSL lets the last argument run past the components and a
       * matrix take scalars and vectors mixed; Metal takes neither, so
       * the components are spelled out where the arguments are not
       * whole columns, and what runs past is left off */
      unsigned want = t->kind == RSLC_TY_VEC ? t->rows : (unsigned)t->rows * t->cols;
      unsigned have = 0, k;
      bool     split = false;
      for (i = 0; i < cnt; i++)
      {
         const struct rslc_type *at = rslc_node_at(p->c, rslc_list_item(p->c, n->b, i))->type;
         unsigned c = at->kind == RSLC_TY_VEC ? at->rows : 1;
         if (t->kind == RSLC_TY_MAT && !(cnt == t->cols && at->kind == RSLC_TY_VEC
                  && at->rows == t->rows))
            split = split || at->kind == RSLC_TY_VEC;
         have += c;
      }
      if (have > want || split)
      {
         unsigned out = 0;
         for (i = 0; i < cnt && out < want; i++)
         {
            uint32_t an = rslc_list_item(p->c, n->b, i);
            const struct rslc_type *at = rslc_node_at(p->c, an)->type;
            unsigned c = at->kind == RSLC_TY_VEC ? at->rows : 1;
            if (out)
               pr_s(p, ", ");
            if (c == 1)
            {
               pr_expr(p, an, PR_PREC_ASSIGN);
               out++;
               continue;
            }
            if (!split)
            {
               /* the vector whole, or the part of it that fits */
               unsigned use = c < want - out ? c : want - out;
               if (use == c)
                  pr_expr(p, an, PR_PREC_ASSIGN);
               else
               {
                  pr_c(p, '(');
                  pr_expr(p, an, PR_PREC_SEQ);
                  pr_s(p, ").");
                  for (k = 0; k < use; k++)
                     pr_c(p, comps[k]);
               }
               out += use;
               continue;
            }
            for (k = 0; k < c && out < want; k++, out++)
            {
               if (k)
                  pr_s(p, ", ");
               pr_c(p, '(');
               pr_expr(p, an, PR_PREC_SEQ);
               pr_s(p, ").");
               pr_c(p, comps[k]);
            }
         }
         pr_c(p, ')');
         return;
      }
   }
   for (i = 0; i < cnt; i++)
   {
      if (i)
         pr_s(p, ", ");
      pr_expr(p, rslc_list_item(p->c, n->b, i), PR_PREC_ASSIGN);
   }
   pr_c(p, ')');
}

/* MSL: the environment a function takes as parameters, declared
 * (@decl) or passed. */
static void pr_msl_env(struct rslc_pr *p, const struct rslc_func *f, bool decl, bool *first)
{
   const struct rslc_prog *pr = &p->c->prog;
   const uint8_t *env = p->fenv + f->id * p->envn;
   uint32_t i;
   for (i = 1; i < pr->nvars; i++)
   {
      const struct rslc_var *v = pr->vars[i];
      if (!env[i])
         continue;
      if (!*first)
         pr_s(p, ", ");
      *first = false;
      if (v->storage == RSLC_ST_SAMPLER)
      {
         if (decl)
         {
            pr_type(p, v->type);
            pr_c(p, ' ');
         }
         pr_var_name(p, v);
         pr_s(p, ", ");
         if (decl)
            pr_s(p, "sampler ");
         pr_var_name(p, v);
         pr_s(p, "Smplr");
         continue;
      }
      if (decl)
      {
         pr_s(p, "thread ");
         pr_type(p, v->type);
         pr_s(p, "& ");
      }
      pr_var_name(p, v);
   }
   for (i = 0; i < 2; i++)
   {
      const struct rslc_blockdecl *b = &pr->blocks[i];
      if (!env[pr->nvars + i])
         continue;
      if (!*first)
         pr_s(p, ", ");
      *first = false;
      if (decl)
      {
         pr_s(p, "constant ");
         pr_s(p, b->type->name ? b->type->name->str : (b->push ? "Push" : "UBO"));
         pr_s(p, "& ");
      }
      pr_s(p, pr_msl_instance(p, i));
   }
}

/* MSL: environment parameters a local or parameter of the function
 * would shadow are renamed for the function's duration. */
static void pr_msl_shadow(struct rslc_pr *p, const struct rslc_func *f, const struct rslc_sym *name)
{
   const struct rslc_prog *pr = &p->c->prog;
   const uint8_t *env = p->fenv + f->id * p->envn;
   uint32_t i;
   if (!name)
      return;
   for (i = 1; i < pr->nvars; i++)
      if (env[i] && pr->vars[i]->name == name)
         p->renamed[i] = 1;
}

static void pr_msl_rename_walk(struct rslc_pr *p, const struct rslc_func *f, uint32_t node)
{
   const struct rslc_node *n;
   uint32_t i, cnt;
   if (!node)
      return;
   n = rslc_node_at(p->c, node);
   switch (n->op)
   {
      case RSLC_OP_DECL:
         pr_msl_shadow(p, f, p->c->prog.vars[n->a]->name);
         return;
      case RSLC_OP_BLOCK:
      case RSLC_OP_SWITCH:
         cnt = n->b ? rslc_list_len(p->c, n->b) : 0;
         for (i = 0; i < cnt; i++)
            pr_msl_rename_walk(p, f, rslc_list_item(p->c, n->b, i));
         return;
      case RSLC_OP_FOR:
         pr_msl_rename_walk(p, f, rslc_list_item(p->c, n->b, 0));
         pr_msl_rename_walk(p, f, rslc_list_item(p->c, n->b, 3));
         return;
      case RSLC_OP_IF:
         pr_msl_rename_walk(p, f, n->b);
         pr_msl_rename_walk(p, f, n->c);
         return;
      case RSLC_OP_WHILE:
         pr_msl_rename_walk(p, f, n->b);
         return;
      case RSLC_OP_DO:
         pr_msl_rename_walk(p, f, n->a);
         return;
      default:
         return;
   }
}

static void pr_msl_rename(struct rslc_pr *p, const struct rslc_func *f)
{
   uint32_t i;
   memset(p->renamed, 0, p->c->prog.nvars + 1);
   if (!f)
      return;
   for (i = 0; i < f->nparams; i++)
      pr_msl_shadow(p, f, f->params[i]->name);
   pr_msl_rename_walk(p, f, f->body);
}

/* The environment of each reachable function, to a fixed point over
 * the calls. */
static void pr_msl_env_mark(struct rslc_pr *p, uint8_t *env, uint32_t node, bool *changed)
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
         uint32_t slot;
         switch (v->storage)
         {
            case RSLC_ST_IN: case RSLC_ST_OUT: case RSLC_ST_BUILTIN:
            case RSLC_ST_GLOBAL: case RSLC_ST_SAMPLER:
               slot = n->a;
               break;
            case RSLC_ST_BLOCK: case RSLC_ST_MEMBER:
               slot = p->c->prog.nvars + v->block;
               break;
            default:
               return;
         }
         if (!env[slot])
         {
            env[slot] = 1;
            *changed  = true;
         }
         return;
      }
      case RSLC_OP_DECL:
         pr_msl_env_mark(p, env, p->c->prog.vars[n->a]->init, changed);
         return;
      case RSLC_OP_CALL:
      {
         /* the callee's environment is the caller's too */
         const uint8_t *callee = p->fenv + p->c->prog.funcs[n->a]->id * p->envn;
         for (i = 1; i < p->envn; i++)
            if (callee[i] && !env[i])
            {
               env[i]   = 1;
               *changed = true;
            }
         cnt = n->b ? rslc_list_len(p->c, n->b) : 0;
         for (i = 0; i < cnt; i++)
            pr_msl_env_mark(p, env, rslc_list_item(p->c, n->b, i), changed);
         return;
      }
      case RSLC_OP_MEMBER: case RSLC_OP_SWIZZLE: case RSLC_OP_CONVERT:
      case RSLC_OP_EXPR: case RSLC_OP_RETURN: case RSLC_OP_CASE:
         pr_msl_env_mark(p, env, n->a, changed);
         return;
      case RSLC_OP_BUILTIN: case RSLC_OP_CONSTRUCT: case RSLC_OP_BLOCK:
      case RSLC_OP_FOR: case RSLC_OP_SWITCH:
         if (n->op == RSLC_OP_SWITCH)
            pr_msl_env_mark(p, env, n->a, changed);
         cnt = n->b ? rslc_list_len(p->c, n->b) : 0;
         for (i = 0; i < cnt; i++)
            pr_msl_env_mark(p, env, rslc_list_item(p->c, n->b, i), changed);
         return;
      default:
         pr_msl_env_mark(p, env, n->a, changed);
         pr_msl_env_mark(p, env, n->b, changed);
         pr_msl_env_mark(p, env, n->c, changed);
         return;
   }
}

static bool pr_msl_env_build(struct rslc_pr *p)
{
   const struct rslc_prog *pr = &p->c->prog;
   bool changed = true;
   uint32_t i;
   p->envn = pr->nvars + 2;
   p->fenv = (uint8_t*)rslc_arena_zalloc(p->c, (size_t)p->envn * (pr->nfuncs + 1));
   if (!p->fenv)
      return false;
   while (changed)
   {
      changed = false;
      for (i = 1; i < pr->nfuncs; i++)
         if (p->reach[i] && pr->funcs[i]->body)
            pr_msl_env_mark(p, p->fenv + pr->funcs[i]->id * p->envn, pr->funcs[i]->body, &changed);
   }
   /* main0 initialises the globals rslc_main reaches, so whatever
    * their initialisers name is in its scope too */
   changed = true;
   while (changed)
   {
      uint8_t *menv = p->fenv + pr->main_fn->id * p->envn;
      changed = false;
      for (i = 1; i < pr->nvars; i++)
         if (menv[i] && pr->vars[i]->storage == RSLC_ST_GLOBAL && pr->vars[i]->init)
            pr_msl_env_mark(p, menv, pr->vars[i]->init, &changed);
   }
   return true;
}

/* M / s printed as M * (1.0 / s): what glslang's module computes. */
static void pr_mat_div_scalar(struct rslc_pr *p, uint32_t m, uint32_t s, int ctx)
{
   int prec = PR_PREC_MUL;
   if (prec > ctx)
      pr_c(p, '(');
   pr_expr(p, m, prec);
   pr_s(p, p->hlsl || p->msl ? " * (1.0f / " : " * (1.0 / ");
   pr_expr(p, s, PR_PREC_MUL);
   pr_c(p, ')');
   if (prec > ctx)
      pr_c(p, ')');
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

   if (p->msl_ginit && n->type && (n->type->kind == RSLC_TY_SCALAR
            || n->type->kind == RSLC_TY_VEC || n->type->kind == RSLC_TY_MAT)
         && rslc_const_eval(p->c, node, &cv))
   {
      /* a program-scope constant's value, whatever names it */
      pr_cval(p, n->type, &cv);
      return;
   }

   if (p->msl && p->nhoist && (n->op == RSLC_OP_SWIZZLE || n->op == RSLC_OP_INDEX))
   {
      uint32_t temp = pr_hoisted(p, node);
      if (temp != RSLC_UNSET)
      {
         pr_temp_name(p, temp);
         return;
      }
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
         if (p->hlsl && base->op == RSLC_OP_VAR
               && p->c->prog.vars[base->a]->storage == RSLC_ST_BLOCK)
         {
            pr_hlsl_member(p, p->c->prog.vars[base->a]->block, n->b);
            break;
         }
         if (p->names && p->names->flatten && base->op == RSLC_OP_VAR
               && p->c->prog.vars[base->a]->storage == RSLC_ST_BLOCK)
         {
            pr_flat_member(p, p->c->prog.vars[base->a]->block, n->b);
            break;
         }
         pr_expr(p, n->a, PR_PREC_POSTFIX);
         pr_c(p, '.');
         pr_ident(p, base->type->fields[n->b].name);
         break;
      }
      case RSLC_OP_INDEX:
      {
         const struct rslc_node *base = rslc_node_at(p->c, n->a);
         struct rslc_cval cv;
         /* a constant component of a vector prints as the swizzle, as
          * SPIRV-Cross prints it: Mesa compiles v[0] and v.x apart and
          * rounds the two differently */
         if (base->type->kind == RSLC_TY_VEC && rslc_const_eval(p->c, n->b, &cv) && cv.v[0] < 4)
         {
            pr_expr(p, n->a, PR_PREC_POSTFIX);
            pr_c(p, '.');
            pr_c(p, "xyzw"[cv.v[0]]);
            break;
         }
         pr_expr(p, n->a, PR_PREC_POSTFIX);
         pr_c(p, '[');
         pr_expr(p, n->b, PR_PREC_SEQ);
         pr_c(p, ']');
         break;
      }
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
            else if (p->hlsl)
            {
               pr_c(p, '(');
               pr_expr(p, n->a, PR_PREC_SEQ);
               pr_s(p, ").");
               pr_s(p, &"xxxx"[4 - cnt]);
            }
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
         if (p->msl)
         {
            /* the arguments, then the environment the callee needs */
            const struct rslc_func *f = p->c->prog.funcs[n->a];
            uint32_t i, cnt = n->b ? rslc_list_len(p->c, n->b) : 0;
            bool first = cnt == 0;
            pr_c(p, '(');
            for (i = 0; i < cnt; i++)
            {
               uint32_t a = rslc_list_item(p->c, n->b, i);
               if (i)
                  pr_s(p, ", ");
               if (rslc_node_at(p->c, a)->type->kind == RSLC_TY_SAMPLER2D)
               {
                  pr_hl_tex(p, a, false);
                  pr_s(p, ", ");
                  pr_hl_tex(p, a, true);
               }
               else
                  pr_expr(p, a, PR_PREC_ASSIGN);
            }
            pr_msl_env(p, f, false, &first);
            pr_c(p, ')');
            break;
         }
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
         if (p->msl)
         {
            pr_msl_construct(p, node);
            break;
         }
         if (p->hlsl)
         {
            if (pr_is_aggregate(n->type))
            {
               /* hoisted into a temporary by the statement */
               uint32_t temp = pr_hoisted(p, node);
               if (temp == RSLC_UNSET)
               {
                  rslc_err(p->c, "hlsl: aggregate constructor here", NULL, n->line);
                  p->ok = false;
                  break;
               }
               pr_temp_name(p, temp);
               break;
            }
            pr_hlsl_construct(p, n);
            break;
         }
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
         if (p->hlsl && pr_hlsl_to_bool(p, n->type, n->a))
            break;
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
         if (n->aux == RSLC_BIN_DIV && rslc_node_at(p->c, n->a)->type->kind == RSLC_TY_MAT
               && rslc_node_at(p->c, n->b)->type->kind == RSLC_TY_SCALAR)
         {
            /* a matrix over a scalar is the matrix times the reciprocal,
             * as the SPIR-V emitter and glslang lower it */
            pr_mat_div_scalar(p, n->a, n->b, ctx);
            break;
         }
         if (p->hlsl || p->msl)
         {
            const struct rslc_type *ta = rslc_node_at(p->c, n->a)->type;
            const struct rslc_type *tb = rslc_node_at(p->c, n->b)->type;
            if (p->hlsl && n->aux == RSLC_BIN_MUL && (ta->kind == RSLC_TY_MAT || tb->kind == RSLC_TY_MAT)
                  && ta->kind != RSLC_TY_SCALAR && tb->kind != RSLC_TY_SCALAR)
            {
               /* a linear-algebra product: with matrices transposed
                * in this spelling, A * B is mul(B, A) */
               pr_s(p, "mul(");
               pr_expr(p, n->b, PR_PREC_ASSIGN);
               pr_s(p, ", ");
               pr_expr(p, n->a, PR_PREC_ASSIGN);
               pr_c(p, ')');
               break;
            }
            if ((n->aux == RSLC_BIN_EQ || n->aux == RSLC_BIN_NE) && ta->kind != RSLC_TY_SCALAR)
            {
               /* HLSL compares componentwise; GLSL asks for the whole */
               if (pr_is_aggregate(ta))
               {
                  rslc_err(p->c, "aggregate comparison", NULL, n->line);
                  p->ok = false;
                  break;
               }
               if (p->msl && ta->kind == RSLC_TY_MAT)
               {
                  /* column by column: Metal matrices have no comparison */
                  unsigned cc;
                  pr_c(p, '(');
                  for (cc = 0; cc < ta->cols; cc++)
                  {
                     if (cc)
                        pr_s(p, n->aux == RSLC_BIN_EQ ? " && " : " || ");
                     pr_s(p, n->aux == RSLC_BIN_EQ ? "all(" : "any(");
                     pr_expr(p, n->a, PR_PREC_POSTFIX);
                     pr_c(p, '[');
                     pr_u(p, cc);
                     pr_s(p, "] ");
                     pr_s(p, op);
                     pr_c(p, ' ');
                     pr_expr(p, n->b, PR_PREC_POSTFIX);
                     pr_c(p, '[');
                     pr_u(p, cc);
                     pr_s(p, "])");
                  }
                  pr_c(p, ')');
                  break;
               }
               pr_s(p, n->aux == RSLC_BIN_EQ ? "all(" : "any(");
               pr_expr(p, n->a, prec);
               pr_c(p, ' ');
               pr_s(p, op);
               pr_c(p, ' ');
               pr_expr(p, n->b, prec - 1);
               pr_c(p, ')');
               break;
            }
            if (n->aux == RSLC_BIN_XOR)
            {
               op   = "!=";
               prec = PR_PREC_EQ;
            }
         }
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
         if (p->msl && n->aux == RSLC_BIN_MUL
               && (rslc_node_at(p->c, n->b)->type->kind == RSLC_TY_MAT
                  || n->type->kind == RSLC_TY_MAT))
         {
            /* Metal has no *= with a matrix operand: x = x * M */
            pr_s(p, "= ");
            pr_expr(p, n->a, PR_PREC_MUL);
            pr_s(p, " * ");
            pr_expr(p, n->b, PR_PREC_MUL);
            if (PR_PREC_ASSIGN > ctx)
               pr_c(p, ')');
            break;
         }
         if (p->hlsl && n->aux == RSLC_BIN_MUL
               && rslc_node_at(p->c, n->b)->type->kind == RSLC_TY_MAT)
         {
            /* x *= M is x = M-product, swapped as above */
            pr_s(p, "= mul(");
            pr_expr(p, n->b, PR_PREC_ASSIGN);
            pr_s(p, ", ");
            pr_expr(p, n->a, PR_PREC_ASSIGN);
            pr_c(p, ')');
            if (PR_PREC_ASSIGN > ctx)
               pr_c(p, ')');
            break;
         }
         if (n->aux == RSLC_BIN_DIV && n->type->kind == RSLC_TY_MAT
               && rslc_node_at(p->c, n->b)->type->kind == RSLC_TY_SCALAR)
         {
            pr_s(p, "= ");
            pr_mat_div_scalar(p, n->a, n->b, PR_PREC_ASSIGN);
            if (PR_PREC_ASSIGN > ctx)
               pr_c(p, ')');
            break;
         }
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
   /* HLSL globals are uniforms unless static; MSL has constants only */
   if (p->hlsl && v->global)
      pr_s(p, "static ");
   if (p->msl && v->global)
      pr_s(p, "constant ");
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
      const struct rslc_node *in = rslc_node_at(p->c, v->init);
      pr_s(p, " = ");
      if (p->hlsl && in->op == RSLC_OP_CONSTRUCT && pr_is_aggregate(in->type)
            && pr_hoisted(p, v->init) == RSLC_UNSET)
         pr_initlist(p, v->init);
      else
         pr_expr(p, v->init, PR_PREC_ASSIGN);
   }
   else if (p->hlsl && with_init && v->storage == RSLC_ST_LOCAL && !v->global
         && !p->lifted[v->id] && (v->type->kind == RSLC_TY_SCALAR
            || v->type->kind == RSLC_TY_VEC || v->type->kind == RSLC_TY_MAT))
   {
      /* GLSL reads an unset local as undefined; D3DCompile refuses a
       * read it cannot prove follows a write, so the local starts at 0 */
      pr_s(p, " = (");
      pr_type(p, v->type);
      pr_s(p, ")0");
   }
}

static bool pr_hoist_add(struct rslc_pr *p, uint32_t node, uint32_t temp)
{
   if (p->nhoist + 2 > p->hoist_cap)
   {
      uint32_t  ncap = p->hoist_cap ? p->hoist_cap * 2 : 16;
      uint32_t *nh   = (uint32_t*)rslc_arena_zalloc(p->c, ncap * sizeof(*nh));
      if (!nh)
      {
         p->ok = false;
         return false;
      }
      if (p->nhoist)
         memcpy(nh, p->hoist, p->nhoist * sizeof(*nh));
      p->hoist     = nh;
      p->hoist_cap = ncap;
   }
   p->hoist[p->nhoist++] = node;
   p->hoist[p->nhoist++] = temp;
   return true;
}

/* HLSL: hoists the aggregate constructors of an expression into
 * temporaries declared before the statement, innermost first.  A
 * constructor that is an initialiser list (@initlist) stays where it
 * is and only its arguments are visited. */
static void pr_hoist(struct rslc_pr *p, uint32_t node, bool initlist)
{
   const struct rslc_node *n;
   uint32_t i, cnt;
   if (!node || !p->ok)
      return;
   n = rslc_node_at(p->c, node);
   switch (n->op)
   {
      case RSLC_OP_CONST:
      case RSLC_OP_VAR:
         return;
      case RSLC_OP_MEMBER:
      case RSLC_OP_SWIZZLE:
      case RSLC_OP_CONVERT:
      case RSLC_OP_UNARY:
         pr_hoist(p, n->a, false);
         return;
      case RSLC_OP_CALL:
      case RSLC_OP_BUILTIN:
      case RSLC_OP_CONSTRUCT:
      {
         bool agg = n->op == RSLC_OP_CONSTRUCT && pr_is_aggregate(n->type);
         cnt = n->b ? rslc_list_len(p->c, n->b) : 0;
         for (i = 0; i < cnt; i++)
         {
            uint32_t an = rslc_list_item(p->c, n->b, i);
            const struct rslc_node *a = rslc_node_at(p->c, an);
            pr_hoist(p, an, agg && a->op == RSLC_OP_CONSTRUCT && pr_is_aggregate(a->type));
         }
         if (agg && !initlist)
         {
            uint32_t temp = p->ntemps++;
            pr_type(p, n->type);
            pr_c(p, ' ');
            pr_temp_name(p, temp);
            pr_array_suffix(p, n->type);
            pr_s(p, " = ");
            pr_initlist(p, node);
            pr_c(p, ';');
            pr_nl(p);
            pr_hoist_add(p, node, temp);
         }
         return;
      }
      default:
         pr_hoist(p, n->a, false);
         pr_hoist(p, n->b, false);
         pr_hoist(p, n->c, false);
         return;
   }
}

/* Whether an expression holds an aggregate constructor, which a loop
 * condition or iteration expression cannot carry in HLSL. */
static bool pr_has_aggregate(struct rslc_pr *p, uint32_t node)
{
   const struct rslc_node *n;
   uint32_t i, cnt;
   if (!node)
      return false;
   n = rslc_node_at(p->c, node);
   switch (n->op)
   {
      case RSLC_OP_CONST:
      case RSLC_OP_VAR:
         return false;
      case RSLC_OP_MEMBER:
      case RSLC_OP_SWIZZLE:
      case RSLC_OP_CONVERT:
      case RSLC_OP_UNARY:
         return pr_has_aggregate(p, n->a);
      case RSLC_OP_CALL:
      case RSLC_OP_BUILTIN:
      case RSLC_OP_CONSTRUCT:
         if (n->op == RSLC_OP_CONSTRUCT && pr_is_aggregate(n->type))
            return true;
         cnt = n->b ? rslc_list_len(p->c, n->b) : 0;
         for (i = 0; i < cnt; i++)
            if (pr_has_aggregate(p, rslc_list_item(p->c, n->b, i)))
               return true;
         return false;
      default:
         return pr_has_aggregate(p, n->a) || pr_has_aggregate(p, n->b)
            || pr_has_aggregate(p, n->c);
   }
}


/* MSL: a reference cannot bind to a vector component, so one passed
 * to an out or inout parameter goes through a temporary, copied in
 * here and back after the statement. */
static void pr_msl_hoist(struct rslc_pr *p, uint32_t node)
{
   const struct rslc_node *n;
   uint32_t i, cnt;
   if (!node || !p->ok)
      return;
   n = rslc_node_at(p->c, node);
   switch (n->op)
   {
      case RSLC_OP_CONST:
      case RSLC_OP_VAR:
         return;
      case RSLC_OP_CALL:
      {
         const struct rslc_func *f = p->c->prog.funcs[n->a];
         cnt = n->b ? rslc_list_len(p->c, n->b) : 0;
         for (i = 0; i < cnt; i++)
         {
            uint32_t an = rslc_list_item(p->c, n->b, i);
            const struct rslc_node *a = rslc_node_at(p->c, an);
            bool component = false;
            pr_msl_hoist(p, an);
            if (i >= f->nparams || f->params[i]->dir == RSLC_DIR_IN)
               continue;
            if (a->op == RSLC_OP_SWIZZLE)
               component = true;
            else if (a->op == RSLC_OP_INDEX)
               component = rslc_node_at(p->c, a->a)->type->kind == RSLC_TY_VEC;
            if (!component)
               continue;
            if (p->nwb >= 16)
            {
               rslc_err(p->c, "msl: too many component out-arguments in one statement", NULL, n->line);
               p->ok = false;
               return;
            }
            pr_type(p, a->type);
            pr_c(p, ' ');
            pr_temp_name(p, p->ntemps);
            pr_s(p, " = ");
            pr_expr(p, an, PR_PREC_ASSIGN);
            pr_c(p, ';');
            pr_nl(p);
            p->wb[p->nwb++] = an;
            if (!pr_hoist_add(p, an, p->ntemps))
               return;
            p->ntemps++;
         }
         return;
      }
      case RSLC_OP_MEMBER: case RSLC_OP_SWIZZLE: case RSLC_OP_CONVERT:
      case RSLC_OP_UNARY:
         pr_msl_hoist(p, n->a);
         return;
      case RSLC_OP_BUILTIN: case RSLC_OP_CONSTRUCT:
         cnt = n->b ? rslc_list_len(p->c, n->b) : 0;
         for (i = 0; i < cnt; i++)
            pr_msl_hoist(p, rslc_list_item(p->c, n->b, i));
         return;
      default:
         pr_msl_hoist(p, n->a);
         pr_msl_hoist(p, n->b);
         pr_msl_hoist(p, n->c);
         return;
   }
}

static void pr_msl_hoist_stmt(struct rslc_pr *p, uint32_t node)
{
   const struct rslc_node *n = rslc_node_at(p->c, node);
   p->nhoist = 0;
   p->nwb    = 0;
   switch (n->op)
   {
      case RSLC_OP_DECL:
         pr_msl_hoist(p, p->c->prog.vars[n->a]->init);
         break;
      case RSLC_OP_EXPR:
      case RSLC_OP_IF:
      case RSLC_OP_SWITCH:
      case RSLC_OP_RETURN:
         pr_msl_hoist(p, n->a);
         break;
      default:
         break;
   }
}

/* ... and the copies back, once the statement has printed. */
static void pr_msl_writeback(struct rslc_pr *p)
{
   uint32_t i, nwb = p->nwb;
   if (!nwb)
      return;
   p->nwb    = 0;
   p->nhoist = 0;   /* the arguments print as themselves again */
   for (i = 0; i < nwb; i++)
   {
      uint32_t temp = p->ntemps - nwb + i;
      pr_nl(p);
      pr_expr(p, p->wb[i], PR_PREC_UNARY);
      pr_s(p, " = ");
      pr_temp_name(p, temp);
      pr_c(p, ';');
   }
}

/* The hoisting a statement needs before it prints. */
static void pr_hoist_stmt(struct rslc_pr *p, uint32_t node)
{
   const struct rslc_node *n = rslc_node_at(p->c, node);
   p->nhoist = 0;   /* temporaries live in the statement's own scope */
   switch (n->op)
   {
      case RSLC_OP_DECL:
      {
         const struct rslc_var *v = p->c->prog.vars[n->a];
         if (v->init)
         {
            const struct rslc_node *in = rslc_node_at(p->c, v->init);
            pr_hoist(p, v->init, in->op == RSLC_OP_CONSTRUCT && pr_is_aggregate(in->type));
         }
         break;
      }
      case RSLC_OP_EXPR:
      case RSLC_OP_RETURN:
      case RSLC_OP_IF:
      case RSLC_OP_SWITCH:
         pr_hoist(p, n->a, false);
         break;
      case RSLC_OP_FOR:
      {
         uint32_t init = rslc_list_item(p->c, n->b, 0);
         if (init)
         {
            const struct rslc_node *in = rslc_node_at(p->c, init);
            if (in->op == RSLC_OP_BLOCK && in->b)
            {
               uint32_t k, cnt = rslc_list_len(p->c, in->b);
               for (k = 0; k < cnt; k++)
               {
                  const struct rslc_node *d = rslc_node_at(p->c, rslc_list_item(p->c, in->b, k));
                  pr_hoist(p, p->c->prog.vars[d->a]->init, false);
               }
            }
            else
               pr_hoist_stmt(p, init);
         }
         if (pr_has_aggregate(p, rslc_list_item(p->c, n->b, 1))
               || pr_has_aggregate(p, rslc_list_item(p->c, n->b, 2)))
         {
            rslc_err(p->c, "hlsl: aggregate constructor in a loop header", NULL, n->line);
            p->ok = false;
         }
         break;
      }
      case RSLC_OP_WHILE:
         if (pr_has_aggregate(p, n->a))
         {
            rslc_err(p->c, "hlsl: aggregate constructor in a loop header", NULL, n->line);
            p->ok = false;
         }
         break;
      case RSLC_OP_DO:
         if (pr_has_aggregate(p, n->b))
         {
            rslc_err(p->c, "hlsl: aggregate constructor in a loop header", NULL, n->line);
            p->ok = false;
         }
         break;
      default:
         break;
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
   if (p->lifted[v->id])
      return true;
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

/* An expression statement.  HLSL takes no assignment inside ?: and
 * evaluates both of its operands, so a c ? a = x : b = y statement is
 * printed as the if / else it means. */
static void pr_expr_stmt(struct rslc_pr *p, uint32_t expr)
{
   const struct rslc_node *e = rslc_node_at(p->c, expr);
   if (p->hlsl && e->op == RSLC_OP_TERNARY)
   {
      pr_s(p, "if (");
      pr_expr(p, e->a, PR_PREC_SEQ);
      pr_s(p, ") {");
      p->indent++;
      pr_nl(p);
      pr_expr_stmt(p, e->b);
      p->indent--;
      pr_nl(p);
      pr_s(p, "} else {");
      p->indent++;
      pr_nl(p);
      pr_expr_stmt(p, e->c);
      p->indent--;
      pr_nl(p);
      pr_c(p, '}');
      return;
   }
   pr_expr(p, expr, PR_PREC_SEQ);
   pr_c(p, ';');
}

static void pr_stmt(struct rslc_pr *p, uint32_t node)
{
   const struct rslc_node *n = rslc_node_at(p->c, node);
   if (!p->ok)
      return;
   if (p->hlsl && n->op != RSLC_OP_BLOCK)
      pr_hoist_stmt(p, node);
   if (p->msl && n->op != RSLC_OP_BLOCK)
      pr_msl_hoist_stmt(p, node);
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
         if (!p->lifted[p->c->prog.vars[n->a]->id])
            pr_decl(p, p->c->prog.vars[n->a], true);
         pr_c(p, ';');
         break;
      case RSLC_OP_EXPR:
         pr_expr_stmt(p, n->a);
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
         pr_s(p, p->msl ? "discard_fragment();" : "discard;");
         break;
      default:
         rslc_err(p->c, "unsupported statement", NULL, n->line);
         p->ok = false;
         break;
   }
   if (p->msl)
      pr_msl_writeback(p);
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
      pr_ident(p, f->name);
      pr_array_suffix(p, f->type);
      pr_c(p, ';');
   }
   p->indent--;
   pr_nl(p);
   pr_s(p, "};");
   pr_nl(p);
}

/* Whether control can reach the end of a statement: a return or
 * discard ends it, an if ends when both arms do, and a block when its
 * last statement does. */
static bool pr_falls_off(struct rslc_pr *p, uint32_t node)
{
   const struct rslc_node *n;
   uint32_t cnt;
   if (!node)
      return true;
   n = rslc_node_at(p->c, node);
   switch (n->op)
   {
      case RSLC_OP_RETURN:
      case RSLC_OP_DISCARD:
         return false;
      case RSLC_OP_IF:
         return !n->c || pr_falls_off(p, n->b) || pr_falls_off(p, n->c);
      case RSLC_OP_BLOCK:
         cnt = n->b ? rslc_list_len(p->c, n->b) : 0;
         return !cnt || pr_falls_off(p, rslc_list_item(p->c, n->b, cnt - 1));
      default:
         return true;
   }
}

static void pr_func_head(struct rslc_pr *p, const struct rslc_func *f)
{
   uint32_t i;
   if (p->hlsl && f->ret->kind == RSLC_TY_ARRAY)
   {
      rslc_err(p->c, "hlsl: a function returning an array", NULL, f->line);
      p->ok = false;
      return;
   }
   if (p->msl)
      pr_s(p, RSLC_MSL_FN);
   pr_type_full(p, f->ret);
   pr_c(p, ' ');
   if ((p->hlsl || p->msl) && f == p->c->prog.main_fn)
      pr_s(p, "rslc_main");   /* main is the wrapper around it */
   else
      pr_ident(p, f->name);
   pr_c(p, '(');
   for (i = 0; i < f->nparams; i++)
   {
      const struct rslc_var *v = f->params[i];
      if (i)
         pr_s(p, ", ");
      if (p->msl)
      {
         /* out and inout by reference; a sampler as its pair */
         if (v->type->kind == RSLC_TY_SAMPLER2D)
         {
            pr_type(p, v->type);
            pr_c(p, ' ');
            pr_var_name(p, v);
            pr_s(p, ", sampler ");
            pr_var_name(p, v);
            pr_s(p, "Smplr");
            continue;
         }
         if (v->dir != RSLC_DIR_IN)
            pr_s(p, "thread ");
         pr_type(p, v->type);
         pr_s(p, v->dir != RSLC_DIR_IN ? "& " : " ");
         pr_var_name(p, v);
         continue;
      }
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
      if (p->hlsl && v->type->kind == RSLC_TY_SAMPLER2D)
      {
         /* the texture's sampler state travels with it */
         pr_s(p, ", SamplerState _");
         if (v->name)
            pr_ident(p, v->name);
         else
            pr_c(p, '_');
         pr_s(p, "_sampler");
      }
   }
   if (p->msl)
   {
      bool first = f->nparams == 0;
      pr_msl_env(p, f, true, &first);
   }
   pr_c(p, ')');
}

/* HLSL: the helper functions a builtin call needs, noted before
 * anything prints so they can go first. */
static void pr_hl_note(struct rslc_pr *p, const struct rslc_node *n)
{
   const struct rslc_type *t;
   switch (n->a)
   {
      case RSLC_BFN_MOD:             p->hl_uses |= RSLC_HL_MOD; break;
      case RSLC_BFN_PACKUNORM2X16:   p->hl_uses |= RSLC_HL_PACK_UNORM2; break;
      case RSLC_BFN_UNPACKUNORM2X16: p->hl_uses |= RSLC_HL_UNPACK_UNORM2; break;
      case RSLC_BFN_PACKSNORM2X16:   p->hl_uses |= RSLC_HL_PACK_SNORM2; break;
      case RSLC_BFN_UNPACKSNORM2X16: p->hl_uses |= RSLC_HL_UNPACK_SNORM2; break;
      case RSLC_BFN_PACKUNORM4X8:    p->hl_uses |= RSLC_HL_PACK_UNORM4; break;
      case RSLC_BFN_UNPACKUNORM4X8:  p->hl_uses |= RSLC_HL_UNPACK_UNORM4; break;
      case RSLC_BFN_PACKSNORM4X8:    p->hl_uses |= RSLC_HL_PACK_SNORM4; break;
      case RSLC_BFN_UNPACKSNORM4X8:  p->hl_uses |= RSLC_HL_UNPACK_SNORM4; break;
      case RSLC_BFN_PACKHALF2X16:    p->hl_uses |= RSLC_HL_PACK_HALF; break;
      case RSLC_BFN_UNPACKHALF2X16:  p->hl_uses |= RSLC_HL_UNPACK_HALF; break;
      case RSLC_BFN_TEXTUREQUERYLEVELS: p->hl_uses |= RSLC_HL_LEVELS; break;
      case RSLC_BFN_BITFIELDINSERT:  p->hl_uses |= RSLC_HL_BFI; break;
      case RSLC_BFN_MATRIXCOMPMULT:
         t = rslc_node_at(p->c, rslc_list_item(p->c, n->b, 0))->type;
         p->hl_uses |= t->cols == 2 ? RSLC_HL_CMUL2 : t->cols == 3 ? RSLC_HL_CMUL3 : RSLC_HL_CMUL4;
         break;
      case RSLC_BFN_FINDLSB:         p->hl_uses |= RSLC_HL_FINDLSB; break;
      case RSLC_BFN_FINDMSB:
         p->hl_uses |= n->type->base == RSLC_BASE_INT ? RSLC_HL_FINDMSB_I : RSLC_HL_FINDMSB_U;
         break;
      case RSLC_BFN_BITFIELDEXTRACT:
         p->hl_uses |= n->type->base == RSLC_BASE_INT ? RSLC_HL_BFE_I : RSLC_HL_BFE_U;
         break;
      case RSLC_BFN_INVERSE:
         t = rslc_node_at(p->c, rslc_list_item(p->c, n->b, 0))->type;
         p->hl_uses |= t->cols == 2 ? RSLC_HL_INV2 : t->cols == 3 ? RSLC_HL_INV3 : RSLC_HL_INV4;
         break;
      case RSLC_BFN_TEXTURESIZE:
         t = rslc_node_at(p->c, rslc_list_item(p->c, n->b, 0))->type;
         p->hl_uses |= t->base == RSLC_BASE_INT ? RSLC_HL_SIZE_I
            : t->base == RSLC_BASE_UINT ? RSLC_HL_SIZE_U : RSLC_HL_SIZE_F;
         break;
      default:
         break;
   }
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
         p->used[n->a] = 1;
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
         if (n->op == RSLC_OP_BUILTIN && n->a >= RSLC_BFN_FLOATBITSTOINT && n->a <= RSLC_BFN_UINTBITSTOFLOAT)
            p->uses_bitcast = true;
         if (n->op == RSLC_OP_BUILTIN && (p->hlsl || p->msl))
            pr_hl_note(p, n);
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
   if (p->names && p->names->flatten)
   {
      uint32_t slots = pr_flat_slots(p, idx);
      if (!slots)
      {
         rslc_err(p->c, "glsl: the block does not flatten into vec4s", NULL, b->line);
         p->ok = false;
         return;
      }
      pr_s(p, p->es ? "uniform highp vec4 " : "uniform vec4 ");
      pr_s(p, type_name);
      pr_c(p, '[');
      pr_u(p, slots);
      pr_s(p, "];");
      pr_nl(p);
      return;
   }
   if (b->push && !p->cafe)
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
   /* CafeGLSL takes the push block as a second uniform block, laid out
    * as the source declared it; the runtime reads offsets back from
    * the .gsh */
   if (!b->push || b->std140)
      pr_s(p, "layout(std140) ");
   pr_s(p, "uniform ");
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
      pr_ident(p, f->name);
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
    * bound by name by the driver.  CafeGLSL takes a location on every
    * in and out, as the Wii U lane has always handed it. */
   if ((p->cafe || (layout_ok && fragment && !is_in)) && v->location != RSLC_UNSET)
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

/* Globals, then prototypes and bodies for the functions main reaches:
 * a global's constant initialiser prints in place, any other runs at
 * the top of main. */
static void pr_globals_and_funcs(struct rslc_pr *p)
{
   struct rslc_ctx        *c  = p->c;
   const struct rslc_prog *pr = &c->prog;
   uint32_t i;

   p->nhoist = 0;
   for (i = 1; i < pr->nvars; i++)
   {
      const struct rslc_var *v = pr->vars[i];
      if (!v->global || (v->storage != RSLC_ST_GLOBAL && v->storage != RSLC_ST_CONST))
         continue;
      if (v->storage == RSLC_ST_CONST && !p->used[i])
         continue;   /* a constant nothing reachable names */
      if (p->lifted[i])
         continue;   /* printed ahead of everything else */
      if (p->msl && !(v->storage == RSLC_ST_CONST && pr_global_init_in_place(p, v)))
         continue;   /* a mutable global lives in main0 and travels by reference */
      p->msl_ginit = p->msl;
      pr_decl(p, v, pr_global_init_in_place(p, v));
      p->msl_ginit = false;
      pr_c(p, ';');
      pr_nl(p);
   }

   for (i = 1; i < pr->nfuncs; i++)
   {
      const struct rslc_func *f = pr->funcs[i];
      if (!p->reach[i] || !f->body || f == pr->main_fn)
         continue;
      if (p->msl)
         pr_msl_rename(p, f);
      pr_func_head(p, f);
      pr_c(p, ';');
      pr_nl(p);
   }
   for (i = 1; i < pr->nfuncs; i++)
   {
      const struct rslc_func *f = pr->funcs[i];
      if (!p->reach[i] || !f->body)
         continue;
      if (p->msl)
         pr_msl_rename(p, f);
      pr_func_head(p, f);
      pr_c(p, ' ');
      if (f == pr->main_fn)
      {
         const struct rslc_node *body = rslc_node_at(c, f->body);
         uint32_t k;
         pr_c(p, '{');
         p->indent++;
         for (k = 1; k < pr->nvars; k++)
         {
            const struct rslc_var *v = pr->vars[k];
            if (!v->global || !v->init || p->msl
                  || (v->storage != RSLC_ST_GLOBAL && v->storage != RSLC_ST_CONST)
                  || pr_global_init_in_place(p, v))
               continue;
            pr_nl(p);
            if (p->hlsl)
            {
               p->nhoist = 0;
               pr_hoist(p, v->init, false);
            }
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
      else if (p->hlsl && f->ret->kind != RSLC_TY_VOID && pr_falls_off(p, f->body))
      {
         /* GLSL leaves the value of a function that runs off its end
          * undefined; D3DCompile refuses the function, so it returns 0 */
         const struct rslc_node *body = rslc_node_at(c, f->body);
         pr_c(p, '{');
         p->indent++;
         pr_stmt_list(p, body->b);
         pr_nl(p);
         pr_s(p, "return (");
         pr_type(p, f->ret);
         pr_s(p, ")0;");
         p->indent--;
         pr_nl(p);
         pr_c(p, '}');
      }
      else
         pr_stmt(p, f->body);
      pr_nl(p);
   }
}

/* Ends the text payload and the blob around it. */
static bool pr_finish(struct rslc_pr *p, struct rslc_result *result)
{
   struct rslc_ctx *c = p->c;
   size_t text_len;
   if (!p->ok)
      return false;
   /* the text payload carries its NUL */
   if (!rslc_pbuf_putc(c, &p->out, '\0'))
      return false;
   text_len = p->out.len - ((struct rslc_blob_hdr*)p->out.data)->payload_off - 1;
   rslc_blob_end(c, &p->out, c->opts->dst, result);
   result->text     = (const char*)(p->out.data + ((struct rslc_blob_hdr*)p->out.data)->payload_off);
   result->text_len = text_len;
   return true;
}

static bool pr_glsl(struct rslc_pr *p, struct rslc_result *result)
{
   struct rslc_ctx        *c  = p->c;
   const struct rslc_prog *pr = &c->prog;
   uint32_t i;
   char     buf[32];

   /* a flattened block reads its integer members through bitcasts */
   if (p->names && p->names->flatten)
      for (i = 0; i < 2; i++)
      {
         uint32_t k;
         if (!pr->has_block[i])
            continue;
         for (k = 0; k < pr->blocks[i].type->nfields; k++)
            if (     pr->blocks[i].type->fields[k].type->base == RSLC_BASE_INT
                  || pr->blocks[i].type->fields[k].type->base == RSLC_BASE_UINT)
               p->uses_bitcast = true;
      }

   /* version line */
   sprintf(buf, "#version %u%s", p->version % 10000, p->es ? " es" : "");
   pr_s(p, buf);
   pr_nl(p);
   if (!p->es && p->version < 420 && p->uses_packing)
   {
      pr_s(p, "#extension GL_ARB_shading_language_packing : require");
      pr_nl(p);
   }
   if (!p->es && p->version < 330 && p->uses_bitcast)
   {
      pr_s(p, "#extension GL_ARB_shader_bit_encoding : require");
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

   pr_globals_and_funcs(p);
   return pr_finish(p, result);
}

/* ---- HLSL ---- */

/* inverse(): the classical adjugate over the determinant, the same
 * text in HLSL and MSL (transposition commutes with inversion, so the
 * spelling of M is immaterial); @pre is the declaration prefix. */
static void pr_inverse_helpers(struct rslc_pr *p, const char *pre)
{
   if (p->hl_uses & RSLC_HL_INV2)
   {
      pr_s(p, pre);
      pr_s(p, "float2x2 rslc_inverse2(float2x2 m) { float d = m[0][0] * m[1][1] - m[0][1] * m[1][0];"
            " return float2x2(m[1][1], -m[0][1], -m[1][0], m[0][0]) / d; }");
      pr_nl(p);
   }
   if (p->hl_uses & RSLC_HL_INV3)
   {
      /* the classical adjugate over the determinant; transposition
       * commutes with inversion, so the spelling of M is immaterial */
      pr_s(p, pre);
      pr_s(p, "float3x3 rslc_inverse3(float3x3 m) {");
      pr_nl(p);
      pr_s(p, "   float3 c0 = float3(m[1][1] * m[2][2] - m[2][1] * m[1][2], m[2][1] * m[0][2] - m[0][1] * m[2][2], m[0][1] * m[1][2] - m[1][1] * m[0][2]);");
      pr_nl(p);
      pr_s(p, "   float3 c1 = float3(m[2][0] * m[1][2] - m[1][0] * m[2][2], m[0][0] * m[2][2] - m[2][0] * m[0][2], m[1][0] * m[0][2] - m[0][0] * m[1][2]);");
      pr_nl(p);
      pr_s(p, "   float3 c2 = float3(m[1][0] * m[2][1] - m[2][0] * m[1][1], m[2][0] * m[0][1] - m[0][0] * m[2][1], m[0][0] * m[1][1] - m[1][0] * m[0][1]);");
      pr_nl(p);
      pr_s(p, "   float d = m[0][0] * c0.x + m[1][0] * c0.y + m[2][0] * c0.z;");
      pr_nl(p);
      pr_s(p, "   return float3x3(c0, c1, c2) / d; }");
      pr_nl(p);
   }
   if (p->hl_uses & RSLC_HL_INV4)
   {
      pr_s(p, pre);
      pr_s(p, "float4x4 rslc_inverse4(float4x4 m) {");
      pr_nl(p);
      pr_s(p, "   float s0 = m[0][0] * m[1][1] - m[1][0] * m[0][1], s1 = m[0][0] * m[1][2] - m[1][0] * m[0][2], s2 = m[0][0] * m[1][3] - m[1][0] * m[0][3];");
      pr_nl(p);
      pr_s(p, "   float s3 = m[0][1] * m[1][2] - m[1][1] * m[0][2], s4 = m[0][1] * m[1][3] - m[1][1] * m[0][3], s5 = m[0][2] * m[1][3] - m[1][2] * m[0][3];");
      pr_nl(p);
      pr_s(p, "   float c5 = m[2][2] * m[3][3] - m[3][2] * m[2][3], c4 = m[2][1] * m[3][3] - m[3][1] * m[2][3], c3 = m[2][1] * m[3][2] - m[3][1] * m[2][2];");
      pr_nl(p);
      pr_s(p, "   float c2 = m[2][0] * m[3][3] - m[3][0] * m[2][3], c1 = m[2][0] * m[3][2] - m[3][0] * m[2][2], c0 = m[2][0] * m[3][1] - m[3][0] * m[2][1];");
      pr_nl(p);
      pr_s(p, "   float d = s0 * c5 - s1 * c4 + s2 * c3 + s3 * c2 - s4 * c1 + s5 * c0;");
      pr_nl(p);
      pr_s(p, "   float4x4 r;");
      pr_nl(p);
      pr_s(p, "   r[0][0] = m[1][1] * c5 - m[1][2] * c4 + m[1][3] * c3; r[0][1] = -m[0][1] * c5 + m[0][2] * c4 - m[0][3] * c3;");
      pr_nl(p);
      pr_s(p, "   r[0][2] = m[3][1] * s5 - m[3][2] * s4 + m[3][3] * s3; r[0][3] = -m[2][1] * s5 + m[2][2] * s4 - m[2][3] * s3;");
      pr_nl(p);
      pr_s(p, "   r[1][0] = -m[1][0] * c5 + m[1][2] * c2 - m[1][3] * c1; r[1][1] = m[0][0] * c5 - m[0][2] * c2 + m[0][3] * c1;");
      pr_nl(p);
      pr_s(p, "   r[1][2] = -m[3][0] * s5 + m[3][2] * s2 - m[3][3] * s1; r[1][3] = m[2][0] * s5 - m[2][2] * s2 + m[2][3] * s1;");
      pr_nl(p);
      pr_s(p, "   r[2][0] = m[1][0] * c4 - m[1][1] * c2 + m[1][3] * c0; r[2][1] = -m[0][0] * c4 + m[0][1] * c2 - m[0][3] * c0;");
      pr_nl(p);
      pr_s(p, "   r[2][2] = m[3][0] * s4 - m[3][1] * s2 + m[3][3] * s0; r[2][3] = -m[2][0] * s4 + m[2][1] * s2 - m[2][3] * s0;");
      pr_nl(p);
      pr_s(p, "   r[3][0] = -m[1][0] * c3 + m[1][1] * c1 - m[1][2] * c0; r[3][1] = m[0][0] * c3 - m[0][1] * c1 + m[0][2] * c0;");
      pr_nl(p);
      pr_s(p, "   r[3][2] = -m[3][0] * s3 + m[3][1] * s1 - m[3][2] * s0; r[3][3] = m[2][0] * s3 - m[2][1] * s1 + m[2][2] * s0;");
      pr_nl(p);
      pr_s(p, "   return r / d; }");
      pr_nl(p);
   }
}

/* The helper functions the printed code calls. */
static void pr_hlsl_helpers(struct rslc_pr *p)
{
   static const char *const tex[] = { "float4", "int4", "uint4" };
   unsigned k;
   if (p->hl_uses & RSLC_HL_MOD)
   {
      static const char *const vt[] = { "float", "float2", "float3", "float4" };
      for (k = 0; k < 4; k++)
      {
         pr_s(p, vt[k]);
         pr_s(p, " mod(");
         pr_s(p, vt[k]);
         pr_s(p, " x, ");
         pr_s(p, vt[k]);
         pr_s(p, " y) { return x - y * floor(x / y); }");
         pr_nl(p);
      }
   }
   for (k = 0; k < 3; k++)
   {
      if (!(p->hl_uses & (RSLC_HL_SIZE_F << k)))
         continue;
      pr_s(p, "uint2 rslc_size(Texture2D<");
      pr_s(p, tex[k]);
      pr_s(p, "> t, uint lod) { uint2 d; uint l; t.GetDimensions(lod, d.x, d.y, l); return d; }");
      pr_nl(p);
   }
   if (p->hl_uses & RSLC_HL_LEVELS)
   {
      pr_s(p, "int rslc_levels(Texture2D<float4> t) { uint2 d; uint l; t.GetDimensions(0u, d.x, d.y, l); return int(l); }");
      pr_nl(p);
   }
   pr_inverse_helpers(p, "");
   if (p->hl_uses & RSLC_HL_PACK_UNORM2)
   {
      pr_s(p, "uint rslc_packUnorm2x16(float2 v) { uint2 i = uint2(round(saturate(v) * 65535.0f)); return i.x | (i.y << 16); }");
      pr_nl(p);
   }
   if (p->hl_uses & RSLC_HL_UNPACK_UNORM2)
   {
      pr_s(p, "float2 rslc_unpackUnorm2x16(uint v) { return float2(v & 0xffffu, v >> 16) / 65535.0f; }");
      pr_nl(p);
   }
   if (p->hl_uses & RSLC_HL_PACK_SNORM2)
   {
      pr_s(p, "uint rslc_packSnorm2x16(float2 v) { int2 i = int2(round(clamp(v, -1.0f, 1.0f) * 32767.0f)); return (uint(i.x) & 0xffffu) | (uint(i.y) << 16); }");
      pr_nl(p);
   }
   if (p->hl_uses & RSLC_HL_UNPACK_SNORM2)
   {
      pr_s(p, "float2 rslc_unpackSnorm2x16(uint v) { int2 i = int2(int(v << 16) >> 16, int(v) >> 16); return clamp(float2(i) / 32767.0f, -1.0f, 1.0f); }");
      pr_nl(p);
   }
   if (p->hl_uses & RSLC_HL_PACK_UNORM4)
   {
      pr_s(p, "uint rslc_packUnorm4x8(float4 v) { uint4 i = uint4(round(saturate(v) * 255.0f)); return i.x | (i.y << 8) | (i.z << 16) | (i.w << 24); }");
      pr_nl(p);
   }
   if (p->hl_uses & RSLC_HL_UNPACK_UNORM4)
   {
      pr_s(p, "float4 rslc_unpackUnorm4x8(uint v) { return float4(v & 0xffu, (v >> 8) & 0xffu, (v >> 16) & 0xffu, v >> 24) / 255.0f; }");
      pr_nl(p);
   }
   if (p->hl_uses & RSLC_HL_PACK_SNORM4)
   {
      pr_s(p, "uint rslc_packSnorm4x8(float4 v) { int4 i = int4(round(clamp(v, -1.0f, 1.0f) * 127.0f)); return (uint(i.x) & 0xffu) | ((uint(i.y) & 0xffu) << 8) | ((uint(i.z) & 0xffu) << 16) | (uint(i.w) << 24); }");
      pr_nl(p);
   }
   if (p->hl_uses & RSLC_HL_UNPACK_SNORM4)
   {
      pr_s(p, "float4 rslc_unpackSnorm4x8(uint v) { int4 i = int4(int(v << 24) >> 24, int(v << 16) >> 24, int(v << 8) >> 24, int(v) >> 24); return clamp(float4(i) / 127.0f, -1.0f, 1.0f); }");
      pr_nl(p);
   }
   if (p->hl_uses & RSLC_HL_PACK_HALF)
   {
      pr_s(p, "uint rslc_packHalf2x16(float2 v) { uint2 h = f32tof16(v); return h.x | (h.y << 16); }");
      pr_nl(p);
   }
   if (p->hl_uses & RSLC_HL_UNPACK_HALF)
   {
      pr_s(p, "float2 rslc_unpackHalf2x16(uint v) { return f16tof32(uint2(v & 0xffffu, v >> 16)); }");
      pr_nl(p);
   }
   if (p->hl_uses & RSLC_HL_BFE_U)
   {
      pr_s(p, "uint rslc_bfe_u(uint v, int off, int bits) { return bits == 0 ? 0u : bits == 32 ? v : (v >> uint(off)) & ((1u << uint(bits)) - 1u); }");
      pr_nl(p);
   }
   if (p->hl_uses & RSLC_HL_BFE_I)
   {
      pr_s(p, "int rslc_bfe_i(int v, int off, int bits) { return bits == 0 ? 0 : bits == 32 ? v : (v << uint(32 - off - bits)) >> uint(32 - bits); }");
      pr_nl(p);
   }
   if (p->hl_uses & RSLC_HL_BFI)
   {
      pr_s(p, "uint rslc_bfi(uint base, uint ins, int off, int bits) { uint m = bits == 32 ? 0xffffffffu : ((1u << uint(bits)) - 1u) << uint(off); return (base & ~m) | ((ins << uint(off)) & m); }");
      pr_nl(p);
   }
}

/* The HLSL semantic of an interface variable. */
static void pr_hlsl_semantic(struct rslc_pr *p, const struct rslc_var *v, bool is_in)
{
   bool fragment = p->c->stage == RSLC_STAGE_FRAGMENT;
   if (v->storage == RSLC_ST_BUILTIN)
   {
      switch (v->builtin)
      {
         case RSLC_BI_POSITION:
         case RSLC_BI_FRAGCOORD:     pr_s(p, "SV_Position"); return;
         case RSLC_BI_FRAGDEPTH:     pr_s(p, "SV_Depth"); return;
         case RSLC_BI_VERTEXINDEX:   pr_s(p, "SV_VertexID"); return;
         case RSLC_BI_INSTANCEINDEX: pr_s(p, "SV_InstanceID"); return;
         case RSLC_BI_FRONTFACING:   pr_s(p, "SV_IsFrontFace"); return;
         default: return;
      }
   }
   if (fragment && !is_in)
      pr_s(p, "SV_Target");
   else
      pr_s(p, "TEXCOORD");
   pr_u(p, v->location == RSLC_UNSET ? 0 : v->location);
}

/* One field of the stage's input or output struct. */
static void pr_hlsl_io_field(struct rslc_pr *p, const struct rslc_var *v, bool is_in)
{
   bool fragment = p->c->stage == RSLC_STAGE_FRAGMENT;
   bool builtin  = v->storage == RSLC_ST_BUILTIN;
   pr_nl(p);
   if (!builtin && !(fragment && !is_in))
   {
      if (v->interp == RSLC_INTERP_FLAT || pr_type_is_integer(v->type))
         pr_s(p, "nointerpolation ");
      else if (v->interp == RSLC_INTERP_NOPERSPECTIVE)
         pr_s(p, "noperspective ");
      if (v->quals & RSLC_VQ_CENTROID)
         pr_s(p, "centroid ");
      if (v->quals & RSLC_VQ_SAMPLE)
         pr_s(p, "sample ");
   }
   if (builtin && (v->builtin == RSLC_BI_VERTEXINDEX || v->builtin == RSLC_BI_INSTANCEINDEX))
      pr_s(p, "uint");
   else
      pr_type(p, v->type);
   pr_c(p, ' ');
   pr_var_name(p, v);
   pr_array_suffix(p, v->type);
   pr_s(p, " : ");
   pr_hlsl_semantic(p, v, is_in);
   pr_c(p, ';');
}

/* Whether a builtin variable is carried by the stage's input (true)
 * or output struct; gl_PointSize is neither. */
static bool pr_hlsl_builtin_in(const struct rslc_var *v, bool *in_struct)
{
   *in_struct = true;
   switch (v->builtin)
   {
      case RSLC_BI_FRAGCOORD:
      case RSLC_BI_VERTEXINDEX:
      case RSLC_BI_INSTANCEINDEX:
      case RSLC_BI_FRONTFACING:
         return true;
      case RSLC_BI_POSITION:
      case RSLC_BI_FRAGDEPTH:
         return false;
      default:
         *in_struct = false;
         return false;
   }
}

static void pr_hlsl_cbuffer(struct rslc_pr *p, unsigned idx)
{
   const struct rslc_blockdecl *b = &p->c->prog.blocks[idx];
   bool std140 = b->push ? b->std140 : true;
   uint32_t i;
   pr_s(p, "cbuffer ");
   pr_s(p, b->type->name ? b->type->name->str : (b->push ? "Push" : "UBO"));
   /* the uniform block is slot 0 and the push block slot 1, as the
    * D3D filter chains bind them */
   pr_s(p, b->push ? " : register(b1)" : " : register(b0)");
   pr_nl(p);
   pr_c(p, '{');
   p->indent++;
   for (i = 0; i < b->type->nfields; i++)
   {
      const struct rslc_field *f = &b->type->fields[i];
      const struct rslc_type  *t = f->type;
      uint32_t off = rslc_layout_field_offset(b->type, i, std140);
      /* HLSL packs array elements and matrix columns at 16 bytes; a
       * std430 push member with a tighter stride has no cbuffer form */
      if ((t->kind == RSLC_TY_ARRAY || t->kind == RSLC_TY_MAT) && !std140)
      {
         const struct rslc_type *bt = t;
         while (bt->kind == RSLC_TY_ARRAY)
            bt = bt->elem;
         if (bt->kind == RSLC_TY_SCALAR || bt->rows < 3)
         {
            rslc_err(p->c, "hlsl: std430 member stride", f->name->str, f->line);
            p->ok = false;
            return;
         }
      }
      if (t->kind == RSLC_TY_STRUCT || (t->kind == RSLC_TY_ARRAY && t->elem->kind == RSLC_TY_STRUCT))
      {
         rslc_err(p->c, "hlsl: struct block member", f->name->str, f->line);
         p->ok = false;
         return;
      }
      pr_nl(p);
      if (t->kind == RSLC_TY_MAT || (t->kind == RSLC_TY_ARRAY && t->elem->kind == RSLC_TY_MAT))
         pr_s(p, "row_major ");
      pr_type(p, t);
      pr_c(p, ' ');
      pr_hlsl_member(p, idx, i);
      pr_array_suffix(p, t);
      pr_s(p, " : packoffset(c");
      pr_u(p, off / 16);
      if (off % 16)
      {
         pr_c(p, '.');
         pr_c(p, "xyzw"[(off % 16) / 4]);
      }
      pr_s(p, ");");
   }
   p->indent--;
   pr_nl(p);
   pr_s(p, "};");
   pr_nl(p);
}

/* HLSL: a local constant array or struct goes to file scope as a
 * static const, named after its variable: an initialiser list inside a
 * function is what some HLSL compilers get wrong, and a constant is
 * the same anywhere. */
static void pr_hlsl_lift(struct rslc_pr *p, uint32_t node)
{
   const struct rslc_node *n;
   uint32_t i, cnt;
   if (!node || !p->ok)
      return;
   n = rslc_node_at(p->c, node);
   switch (n->op)
   {
      case RSLC_OP_DECL:
      {
         const struct rslc_var *v = p->c->prog.vars[n->a];
         if (!v->global && v->storage == RSLC_ST_CONST && v->init
               && pr_is_aggregate(v->type) && pr_node_is_const(p, v->init))
         {
            p->lifted[v->id] = 1;
            pr_s(p, "static ");
            pr_decl(p, v, true);
            pr_c(p, ';');
            pr_nl(p);
         }
         return;
      }
      case RSLC_OP_BLOCK:
      case RSLC_OP_SWITCH:
         cnt = n->b ? rslc_list_len(p->c, n->b) : 0;
         for (i = 0; i < cnt; i++)
            pr_hlsl_lift(p, rslc_list_item(p->c, n->b, i));
         return;
      case RSLC_OP_FOR:
         pr_hlsl_lift(p, rslc_list_item(p->c, n->b, 3));
         return;
      case RSLC_OP_IF:
         pr_hlsl_lift(p, n->b);
         pr_hlsl_lift(p, n->c);
         return;
      case RSLC_OP_WHILE:
         pr_hlsl_lift(p, n->b);
         return;
      case RSLC_OP_DO:
         pr_hlsl_lift(p, n->a);
         return;
      default:
         return;
   }
}

static bool pr_hlsl(struct rslc_pr *p, struct rslc_result *result)
{
   struct rslc_ctx        *c  = p->c;
   const struct rslc_prog *pr = &c->prog;
   unsigned nin = 0, nout = 0;
   uint32_t i;

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

   /* constants first, in declaration order so each may name the ones
    * before it: the globals, then the local arrays and structs lifted
    * out of their functions, which is the one place every HLSL
    * compiler takes an initialiser list */
   for (i = 1; i < pr->nvars; i++)
   {
      const struct rslc_var *v = pr->vars[i];
      if (v->global && v->storage == RSLC_ST_CONST && p->used[i] && v->init
            && pr_node_is_const(p, v->init))
      {
         p->lifted[i] = 1;
         pr_decl(p, v, true);
         pr_c(p, ';');
         pr_nl(p);
      }
   }
   for (i = 1; i < pr->nfuncs; i++)
      if (p->reach[i] && pr->funcs[i]->body)
         pr_hlsl_lift(p, pr->funcs[i]->body);

   pr_hlsl_helpers(p);
   for (i = 0; i < 2; i++)
      if (pr->has_block[i])
         pr_hlsl_cbuffer(p, i);

   for (i = 0; i < pr->nsamplers; i++)
   {
      const struct rslc_var *v = pr->samplers[i];
      pr_type(p, v->type);
      pr_c(p, ' ');
      pr_var_name(p, v);
      pr_s(p, " : register(t");
      pr_u(p, v->binding == RSLC_UNSET ? 0 : v->binding);
      pr_s(p, ");");
      pr_nl(p);
      pr_s(p, "SamplerState _");
      pr_var_name(p, v);
      pr_s(p, "_sampler : register(s");
      pr_u(p, v->binding == RSLC_UNSET ? 0 : v->binding);
      pr_s(p, ");");
      pr_nl(p);
   }

   /* the interface as statics the body reads and writes; the wrapper
    * main copies them from and to the stage structs */
   for (i = 1; i < pr->nvars; i++)
   {
      const struct rslc_var *v = pr->vars[i];
      bool in_struct;
      if (!v->global)
         continue;
      if (v->storage != RSLC_ST_IN && v->storage != RSLC_ST_OUT && v->storage != RSLC_ST_BUILTIN)
         continue;
      if (v->storage == RSLC_ST_BUILTIN && !p->used[i])
         continue;
      if (v->storage == RSLC_ST_IN)
         nin++;
      else if (v->storage == RSLC_ST_OUT)
         nout++;
      else if (pr_hlsl_builtin_in(v, &in_struct))
         nin++;
      else if (in_struct)
         nout++;
      pr_s(p, "static ");
      pr_type(p, v->type);
      pr_c(p, ' ');
      pr_var_name(p, v);
      pr_array_suffix(p, v->type);
      pr_c(p, ';');
      pr_nl(p);
   }
   if (nin)
   {
      pr_s(p, "struct rslc_in");
      pr_nl(p);
      pr_c(p, '{');
      p->indent++;
      for (i = 0; i < pr->ninputs; i++)
         pr_hlsl_io_field(p, pr->inputs[i], true);
      for (i = 1; i < pr->nvars; i++)
      {
         const struct rslc_var *v = pr->vars[i];
         bool in_struct;
         if (v->global && v->storage == RSLC_ST_BUILTIN && p->used[i] && pr_hlsl_builtin_in(v, &in_struct))
            pr_hlsl_io_field(p, v, true);
      }
      p->indent--;
      pr_nl(p);
      pr_s(p, "};");
      pr_nl(p);
   }
   if (nout)
   {
      pr_s(p, "struct rslc_out");
      pr_nl(p);
      pr_c(p, '{');
      p->indent++;
      for (i = 0; i < pr->noutputs; i++)
         pr_hlsl_io_field(p, pr->outputs[i], false);
      for (i = 1; i < pr->nvars; i++)
      {
         const struct rslc_var *v = pr->vars[i];
         bool in_struct;
         if (v->global && v->storage == RSLC_ST_BUILTIN && p->used[i]
               && !pr_hlsl_builtin_in(v, &in_struct) && in_struct)
            pr_hlsl_io_field(p, v, false);
      }
      p->indent--;
      pr_nl(p);
      pr_s(p, "};");
      pr_nl(p);
   }

   pr_globals_and_funcs(p);

   /* the entry point around rslc_main */
   pr_s(p, nout ? "rslc_out main(" : "void main(");
   if (nin)
      pr_s(p, "rslc_in stage_input");
   pr_c(p, ')');
   pr_nl(p);
   pr_c(p, '{');
   p->indent++;
   if (nout)
   {
      pr_nl(p);
      pr_s(p, "rslc_out stage_output;");
   }
   for (i = 1; i < pr->nvars; i++)
   {
      const struct rslc_var *v = pr->vars[i];
      bool in_struct;
      if (!v->global || (v->storage == RSLC_ST_BUILTIN && !p->used[i]))
         continue;
      if (v->storage == RSLC_ST_IN
            || (v->storage == RSLC_ST_BUILTIN && pr_hlsl_builtin_in(v, &in_struct)))
      {
         pr_nl(p);
         pr_var_name(p, v);
         pr_s(p, " = ");
         if (v->storage == RSLC_ST_BUILTIN
               && (v->builtin == RSLC_BI_VERTEXINDEX || v->builtin == RSLC_BI_INSTANCEINDEX))
            pr_s(p, "int(stage_input.");
         else
            pr_s(p, "stage_input.");
         pr_var_name(p, v);
         if (v->storage == RSLC_ST_BUILTIN
               && (v->builtin == RSLC_BI_VERTEXINDEX || v->builtin == RSLC_BI_INSTANCEINDEX))
            pr_c(p, ')');
         pr_c(p, ';');
         if (v->storage == RSLC_ST_BUILTIN && v->builtin == RSLC_BI_FRAGCOORD)
         {
            /* SV_Position.w is w; gl_FragCoord.w is 1 / w */
            pr_nl(p);
            pr_s(p, "gl_FragCoord.w = 1.0f / gl_FragCoord.w;");
         }
      }
   }
   pr_nl(p);
   pr_s(p, "rslc_main();");
   for (i = 1; i < pr->nvars; i++)
   {
      const struct rslc_var *v = pr->vars[i];
      bool in_struct;
      if (!v->global || (v->storage == RSLC_ST_BUILTIN && !p->used[i]))
         continue;
      if (v->storage == RSLC_ST_OUT
            || (v->storage == RSLC_ST_BUILTIN && !pr_hlsl_builtin_in(v, &in_struct) && in_struct))
      {
         pr_nl(p);
         pr_s(p, "stage_output.");
         pr_var_name(p, v);
         pr_s(p, " = ");
         pr_var_name(p, v);
         pr_c(p, ';');
      }
   }
   if (nout)
   {
      pr_nl(p);
      pr_s(p, "return stage_output;");
   }
   p->indent--;
   pr_nl(p);
   pr_c(p, '}');
   pr_nl(p);
   return pr_finish(p, result);
}

/* ---- MSL ---- */

static void pr_msl_helpers(struct rslc_pr *p)
{
   static const char *const tex[] = { "float", "int", "uint" };
   unsigned k;
   if (p->hl_uses & RSLC_HL_MOD)
   {
      pr_s(p, "template<typename Tx, typename Ty> " RSLC_MSL_FN "Tx mod(Tx x, Ty y) { return x - y * floor(x / y); }");
      pr_nl(p);
   }
   for (k = 0; k < 3; k++)
   {
      if (!(p->hl_uses & (RSLC_HL_SIZE_F << k)))
         continue;
      pr_s(p, RSLC_MSL_FN "int2 rslc_size(texture2d<");
      pr_s(p, tex[k]);
      pr_s(p, "> t, uint lod) { return int2(t.get_width(lod), t.get_height(lod)); }");
      pr_nl(p);
   }
   for (k = 2; k <= 4; k++)
   {
      char n = (char)('0' + k);
      unsigned c;
      if (!(p->hl_uses & (RSLC_HL_CMUL2 << (k - 2))))
         continue;
      pr_s(p, RSLC_MSL_FN "float");
      pr_c(p, n);
      pr_c(p, 'x');
      pr_c(p, n);
      pr_s(p, " rslc_cmul(float");
      pr_c(p, n);
      pr_c(p, 'x');
      pr_c(p, n);
      pr_s(p, " a, float");
      pr_c(p, n);
      pr_c(p, 'x');
      pr_c(p, n);
      pr_s(p, " b) { return float");
      pr_c(p, n);
      pr_c(p, 'x');
      pr_c(p, n);
      pr_c(p, '(');
      for (c = 0; c < k; c++)
      {
         if (c)
            pr_s(p, ", ");
         pr_s(p, "a[");
         pr_u(p, c);
         pr_s(p, "] * b[");
         pr_u(p, c);
         pr_c(p, ']');
      }
      pr_s(p, "); }");
      pr_nl(p);
   }
   pr_inverse_helpers(p, RSLC_MSL_FN);
   if (p->hl_uses & RSLC_HL_FINDLSB)
   {
      pr_s(p, "template<typename T> " RSLC_MSL_FN "T rslc_findlsb(T v) { return select(T(ctz(v)), T(-1), v == T(0)); }");
      pr_nl(p);
   }
   if (p->hl_uses & RSLC_HL_FINDMSB_U)
   {
      pr_s(p, "template<typename T> " RSLC_MSL_FN "T rslc_findmsb(T v) { return select(T(31) - T(clz(v)), T(-1), v == T(0)); }");
      pr_nl(p);
   }
   if (p->hl_uses & RSLC_HL_FINDMSB_I)
   {
      /* a negative number's highest zero bit */
      pr_s(p, "template<typename T> " RSLC_MSL_FN "T rslc_findmsb(T v) { T w = select(v, ~v, v < T(0)); return select(T(31) - T(clz(w)), T(-1), w == T(0)); }");
      pr_nl(p);
   }
}

/* The natural alignment and size of a block member's MSL type, with
 * vec3 packed when the next member sits in its tail. */
static bool pr_msl_member(struct rslc_pr *p, const struct rslc_type *t, bool packed,
      bool std140, uint32_t *align, uint32_t *size)
{
   switch (t->kind)
   {
      case RSLC_TY_SCALAR:
         if (t->base == RSLC_BASE_BOOL)
            return false;   /* 1 byte here, 4 in the block */
         *align = *size = 4;
         return true;
      case RSLC_TY_VEC:
         if (t->rows == 3 && packed)
         {
            *align = 4;
            *size  = 12;
         }
         else
         {
            *align = t->rows == 2 ? 8 : 16;
            *size  = t->rows == 2 ? 8 : 16;
         }
         return true;
      case RSLC_TY_MAT:
         *align = t->rows == 2 ? 8 : 16;
         *size  = *align * t->cols;
         return !(std140 && t->rows == 2);   /* std140 pads mat2 columns to 16 */
      case RSLC_TY_ARRAY:
      {
         uint32_t ea, es;
         if (!pr_msl_member(p, t->elem, false, std140, &ea, &es))
            return false;
         if (std140 && ea < 16)
            return false;   /* std140 pads elements to 16, MSL does not */
         *align = ea;
         *size  = es * t->array_len;
         return true;
      }
      default:
         return false;
   }
}

/* A block as a struct whose members sit at the block's offsets. */
static void pr_msl_block(struct rslc_pr *p, unsigned idx)
{
   const struct rslc_blockdecl *b = &p->c->prog.blocks[idx];
   bool std140 = b->push ? b->std140 : true;
   uint32_t i, cur = 0, npad = 0;
   pr_s(p, "struct ");
   pr_s(p, b->type->name ? b->type->name->str : (b->push ? "Push" : "UBO"));
   pr_nl(p);
   pr_c(p, '{');
   p->indent++;
   for (i = 0; i < b->type->nfields; i++)
   {
      const struct rslc_field *f = &b->type->fields[i];
      uint32_t off  = rslc_layout_field_offset(b->type, i, std140);
      uint32_t next = i + 1 < b->type->nfields
         ? rslc_layout_field_offset(b->type, i + 1, std140) : off + 16;
      bool packed   = f->type->kind == RSLC_TY_VEC && f->type->rows == 3 && next < off + 16;
      uint32_t align, size, at;
      if (!pr_msl_member(p, f->type, packed, std140, &align, &size))
      {
         rslc_err(p->c, "msl: block member layout", f->name->str, f->line);
         p->ok = false;
         return;
      }
      at = (cur + align - 1) / align * align;
      if (at > off)
      {
         rslc_err(p->c, "msl: block member offset", f->name->str, f->line);
         p->ok = false;
         return;
      }
      if (at < off)
      {
         pr_nl(p);
         pr_s(p, "char _pad");
         pr_u(p, npad++);
         pr_c(p, '[');
         pr_u(p, off - at);
         pr_s(p, "];");
      }
      pr_nl(p);
      if (packed)
         pr_s(p, "packed_float3");
      else
         pr_type(p, f->type);
      pr_c(p, ' ');
      pr_ident(p, f->name);
      pr_c(p, ';');
      cur = off + size;
   }
   p->indent--;
   pr_nl(p);
   pr_s(p, "};");
   pr_nl(p);
}

/* Metal takes only scalars and vectors in the stage structs: an array,
 * matrix or struct goes in one member per element, column or field
 * (v_0, v_1 ... v_field), at consecutive locations, as the GLSL
 * locations of their parts count. */
#define PR_MSL_IO_DECL 0
#define PR_MSL_IO_IN   1
#define PR_MSL_IO_OUT  2

static bool pr_msl_io_flat(const struct rslc_type *t)
{
   return t->kind == RSLC_TY_SCALAR || t->kind == RSLC_TY_VEC;
}

static void pr_msl_io_leaf(struct rslc_pr *p, const struct rslc_var *v,
      const struct rslc_type *t, bool is_in, uint32_t loc);

static void pr_msl_io_walk(struct rslc_pr *p, const struct rslc_var *v,
      const struct rslc_type *t, bool is_in, int mode, uint32_t *loc,
      char *suf, size_t suf_len, char *acc, size_t acc_len)
{
   uint32_t i, n;
   if (pr_msl_io_flat(t))
   {
      pr_nl(p);
      if (mode == PR_MSL_IO_DECL)
      {
         pr_type(p, t);
         pr_c(p, ' ');
         pr_var_name(p, v);
         pr_s(p, suf);
         pr_msl_io_leaf(p, v, t, is_in, *loc);
      }
      else if (mode == PR_MSL_IO_IN)
      {
         pr_var_name(p, v);
         pr_s(p, acc);
         pr_s(p, " = in.");
         pr_var_name(p, v);
         pr_s(p, suf);
         pr_c(p, ';');
      }
      else
      {
         pr_s(p, "out.");
         pr_var_name(p, v);
         pr_s(p, suf);
         pr_s(p, " = ");
         pr_var_name(p, v);
         pr_s(p, acc);
         pr_c(p, ';');
      }
      (*loc)++;
      return;
   }
   n = t->kind == RSLC_TY_MAT ? t->cols
      : t->kind == RSLC_TY_ARRAY ? t->array_len
      : t->kind == RSLC_TY_STRUCT ? t->nfields : 0;
   for (i = 0; i < n; i++)
   {
      size_t sl = strlen(suf), al = strlen(acc);
      const struct rslc_type *et;
      if (t->kind == RSLC_TY_STRUCT)
      {
         const char *fn = t->fields[i].name ? t->fields[i].name->str : "_";
         const char *rn = t->fields[i].name && pr_name_reserved(p, t->fields[i].name) ? "_" : "";
         et = t->fields[i].type;
         if (sl + strlen(fn) + 3 >= suf_len || al + strlen(fn) + 3 >= acc_len)
            break;
         sprintf(suf + sl, "_%s%s", fn, rn);
         sprintf(acc + al, ".%s%s", fn, rn);
      }
      else
      {
         et = t->kind == RSLC_TY_MAT
            ? rslc_type_basic(p->c, RSLC_TY_VEC, (enum rslc_base)t->base, t->rows, 1)
            : t->elem;
         if (sl + 12 >= suf_len || al + 13 >= acc_len)
            break;
         sprintf(suf + sl, "_%u", (unsigned)i);
         sprintf(acc + al, "[%u]", (unsigned)i);
      }
      if (et)
         pr_msl_io_walk(p, v, et, is_in, mode, loc, suf, suf_len, acc, acc_len);
      suf[sl] = '\0';
      acc[al] = '\0';
   }
}

static void pr_msl_io(struct rslc_pr *p, const struct rslc_var *v, bool is_in, int mode)
{
   char     suf[256], acc[256];
   uint32_t loc = v->location == RSLC_UNSET ? 0 : v->location;
   suf[0] = acc[0] = '\0';
   pr_msl_io_walk(p, v, v->type, is_in, mode, &loc, suf, sizeof(suf), acc, sizeof(acc));
}

/* [[attribute(n)]], [[user(locnN)]], [[color(n)]] and the builtins. */
static void pr_msl_io_field(struct rslc_pr *p, const struct rslc_var *v, bool is_in)
{
   if (v->storage != RSLC_ST_BUILTIN && !pr_msl_io_flat(v->type))
   {
      pr_msl_io(p, v, is_in, PR_MSL_IO_DECL);
      return;
   }
   pr_nl(p);
   pr_type(p, v->type);
   pr_c(p, ' ');
   pr_var_name(p, v);
   pr_msl_io_leaf(p, v, v->type, is_in,
         v->location == RSLC_UNSET ? 0 : v->location);
}

static void pr_msl_io_leaf(struct rslc_pr *p, const struct rslc_var *v,
      const struct rslc_type *t, bool is_in, uint32_t loc)
{
   bool fragment = p->c->stage == RSLC_STAGE_FRAGMENT;
   pr_s(p, " [[");
   if (v->storage == RSLC_ST_BUILTIN)
   {
      switch (v->builtin)
      {
         case RSLC_BI_POSITION:  pr_s(p, "position"); break;
         case RSLC_BI_POINTSIZE: pr_s(p, "point_size"); break;
         case RSLC_BI_FRAGDEPTH: pr_s(p, "depth(any)"); break;
         default: break;
      }
   }
   else if (!fragment && is_in)
   {
      pr_s(p, "attribute(");
      pr_u(p, loc);
      pr_c(p, ')');
   }
   else if (fragment && !is_in)
   {
      pr_s(p, "color(");
      pr_u(p, loc);
      pr_c(p, ')');
   }
   else
   {
      pr_s(p, "user(locn");
      pr_u(p, loc);
      pr_c(p, ')');
      if (fragment)
      {
         if (v->interp == RSLC_INTERP_FLAT || pr_type_is_integer(t))
            pr_s(p, ", flat");
         else if (v->interp == RSLC_INTERP_NOPERSPECTIVE)
            pr_s(p, ", center_no_perspective");
         else if (v->quals & RSLC_VQ_SAMPLE)
            pr_s(p, ", sample_perspective");
         else if (v->quals & RSLC_VQ_CENTROID)
            pr_s(p, ", centroid_perspective");
      }
   }
   pr_s(p, "]];");
}

static bool pr_msl(struct rslc_pr *p, struct rslc_result *result)
{
   struct rslc_ctx        *c  = p->c;
   const struct rslc_prog *pr = &c->prog;
   const uint8_t *menv;
   bool fragment = c->stage == RSLC_STAGE_FRAGMENT;
   bool first;
   unsigned nin = 0, nout = 0;
   uint32_t i;

   if (!pr_msl_env_build(p))
      return false;
   menv = p->fenv + pr->main_fn->id * p->envn;

   pr_s(p, "#include <metal_stdlib>");
   pr_nl(p);
   pr_s(p, "#include <simd/simd.h>");
   pr_nl(p);
   pr_s(p, "using namespace metal;");
   pr_nl(p);
   pr_msl_helpers(p);

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
         pr_msl_block(p, i);

   for (i = 0; i < pr->ninputs; i++)
      nin++;
   for (i = 0; i < pr->noutputs; i++)
      nout++;
   for (i = 1; i < pr->nvars; i++)
   {
      const struct rslc_var *v = pr->vars[i];
      if (!v->global || v->storage != RSLC_ST_BUILTIN || !p->used[i])
         continue;
      if (v->builtin == RSLC_BI_POSITION || v->builtin == RSLC_BI_POINTSIZE
            || v->builtin == RSLC_BI_FRAGDEPTH)
         nout++;
   }
   if (nin)
   {
      pr_s(p, "struct main0_in");
      pr_nl(p);
      pr_c(p, '{');
      p->indent++;
      for (i = 0; i < pr->ninputs; i++)
         pr_msl_io_field(p, pr->inputs[i], true);
      p->indent--;
      pr_nl(p);
      pr_s(p, "};");
      pr_nl(p);
   }
   if (nout)
   {
      pr_s(p, "struct main0_out");
      pr_nl(p);
      pr_c(p, '{');
      p->indent++;
      for (i = 0; i < pr->noutputs; i++)
         pr_msl_io_field(p, pr->outputs[i], false);
      for (i = 1; i < pr->nvars; i++)
      {
         const struct rslc_var *v = pr->vars[i];
         if (v->global && v->storage == RSLC_ST_BUILTIN && p->used[i]
               && (v->builtin == RSLC_BI_POSITION || v->builtin == RSLC_BI_POINTSIZE
                  || v->builtin == RSLC_BI_FRAGDEPTH))
            pr_msl_io_field(p, v, false);
      }
      p->indent--;
      pr_nl(p);
      pr_s(p, "};");
      pr_nl(p);
   }

   pr_globals_and_funcs(p);
   pr_msl_rename(p, NULL);

   /* the entry point: the stage structs, every block and sampler at
    * its binding, the builtins used */
   pr_s(p, fragment ? "fragment " : "vertex ");
   pr_s(p, nout ? "main0_out main0(" : "void main0(");
   first = true;
   if (nin)
   {
      pr_s(p, "main0_in in [[stage_in]]");
      first = false;
   }
   for (i = 0; i < 2; i++)
   {
      const struct rslc_blockdecl *b;
      if (!pr->has_block[i])
         continue;
      b = &pr->blocks[i];
      if (!first)
         pr_s(p, ", ");
      first = false;
      pr_s(p, "constant ");
      pr_s(p, b->type->name ? b->type->name->str : (b->push ? "Push" : "UBO"));
      pr_s(p, "& ");
      pr_s(p, pr_msl_instance(p, i));
      /* the uniform block is buffer 0 and the push block buffer 1, as
       * the Metal filter chain binds them */
      pr_s(p, b->push ? " [[buffer(1)]]" : " [[buffer(0)]]");
   }
   for (i = 0; i < pr->nsamplers; i++)
   {
      const struct rslc_var *v = pr->samplers[i];
      uint32_t binding = v->binding == RSLC_UNSET ? 0 : v->binding;
      if (!first)
         pr_s(p, ", ");
      first = false;
      pr_type(p, v->type);
      pr_c(p, ' ');
      pr_var_name(p, v);
      pr_s(p, " [[texture(");
      pr_u(p, binding);
      pr_s(p, ")]], sampler ");
      pr_var_name(p, v);
      pr_s(p, "Smplr [[sampler(");
      pr_u(p, binding);
      pr_s(p, ")]]");
   }
   for (i = 1; i < pr->nvars; i++)
   {
      const struct rslc_var *v = pr->vars[i];
      const char *attr;
      if (!v->global || v->storage != RSLC_ST_BUILTIN || !p->used[i])
         continue;
      switch (v->builtin)
      {
         case RSLC_BI_FRAGCOORD:     attr = "float4 gl_FragCoord [[position]]"; break;
         case RSLC_BI_FRONTFACING:   attr = "bool gl_FrontFacing [[front_facing]]"; break;
         case RSLC_BI_VERTEXINDEX:   attr = "uint gl_VertexIndex_in [[vertex_id]]"; break;
         case RSLC_BI_INSTANCEINDEX: attr = "uint gl_InstanceIndex_in [[instance_id]]"; break;
         default: continue;
      }
      if (!first)
         pr_s(p, ", ");
      first = false;
      pr_s(p, attr);
   }
   pr_c(p, ')');
   pr_nl(p);
   pr_c(p, '{');
   p->indent++;
   if (nout)
   {
      pr_nl(p);
      pr_s(p, "main0_out out = {};");
   }
   /* what rslc_main and the functions it calls take by reference */
   for (i = 1; i < pr->nvars; i++)
   {
      const struct rslc_var *v = pr->vars[i];
      if (!menv[i])
         continue;
      switch (v->storage)
      {
         case RSLC_ST_IN:
            pr_nl(p);
            pr_type(p, v->type);
            pr_c(p, ' ');
            pr_var_name(p, v);
            if (!pr_msl_io_flat(v->type))
            {
               pr_s(p, " = {};");
               pr_msl_io(p, v, true, PR_MSL_IO_IN);
               break;
            }
            pr_s(p, " = in.");
            pr_var_name(p, v);
            pr_c(p, ';');
            break;
         case RSLC_ST_OUT:
            pr_nl(p);
            pr_type(p, v->type);
            pr_c(p, ' ');
            pr_var_name(p, v);
            pr_s(p, " = {};");
            break;
         case RSLC_ST_BUILTIN:
            if (v->builtin == RSLC_BI_VERTEXINDEX || v->builtin == RSLC_BI_INSTANCEINDEX)
            {
               pr_nl(p);
               pr_s(p, "int ");
               pr_var_name(p, v);
               pr_s(p, " = int(");
               pr_var_name(p, v);
               pr_s(p, "_in);");
            }
            else if (v->builtin == RSLC_BI_POSITION || v->builtin == RSLC_BI_POINTSIZE
                  || v->builtin == RSLC_BI_FRAGDEPTH)
            {
               pr_nl(p);
               pr_type(p, v->type);
               pr_c(p, ' ');
               pr_var_name(p, v);
               pr_s(p, " = {};");
            }
            break;
         case RSLC_ST_GLOBAL:
            pr_nl(p);
            pr_type(p, v->type);
            pr_c(p, ' ');
            pr_var_name(p, v);
            if (v->init)
            {
               pr_s(p, " = ");
               pr_expr(p, v->init, PR_PREC_ASSIGN);
            }
            else
               pr_s(p, " = {}");
            pr_c(p, ';');
            break;
         default:
            break;
      }
   }
   pr_nl(p);
   pr_s(p, "rslc_main(");
   first = true;
   pr_msl_env(p, pr->main_fn, false, &first);
   pr_s(p, ");");
   for (i = 0; i < pr->noutputs; i++)
   {
      const struct rslc_var *v = pr->outputs[i];
      if (!menv[v->id])
         continue;
      if (!pr_msl_io_flat(v->type))
      {
         pr_msl_io(p, v, false, PR_MSL_IO_OUT);
         continue;
      }
      pr_nl(p);
      pr_s(p, "out.");
      pr_var_name(p, v);
      pr_s(p, " = ");
      pr_var_name(p, v);
      pr_c(p, ';');
   }
   for (i = 1; i < pr->nvars; i++)
   {
      const struct rslc_var *v = pr->vars[i];
      if (!menv[i] || v->storage != RSLC_ST_BUILTIN)
         continue;
      if (v->builtin != RSLC_BI_POSITION && v->builtin != RSLC_BI_POINTSIZE
            && v->builtin != RSLC_BI_FRAGDEPTH)
         continue;
      pr_nl(p);
      pr_s(p, "out.");
      pr_var_name(p, v);
      pr_s(p, " = ");
      pr_var_name(p, v);
      pr_c(p, ';');
   }
   if (nout)
   {
      pr_nl(p);
      pr_s(p, "return out;");
   }
   p->indent--;
   pr_nl(p);
   pr_c(p, '}');
   pr_nl(p);
   return pr_finish(p, result);
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
   p->used    = (uint8_t*)rslc_arena_zalloc(c, pr->nvars + 1);
   p->lifted  = (uint8_t*)rslc_arena_zalloc(c, pr->nvars + 1);
   p->renamed = (uint8_t*)rslc_arena_zalloc(c, pr->nvars + 1);
   p->symfn   = (uint8_t*)rslc_arena_zalloc(c, c->nsyms + 1);
   if (!p->reach || !p->used || !p->lifted || !p->renamed || !p->symfn)
      return false;
   for (k = 1; k < pr->nfuncs; k++)
      if (pr->funcs[k]->name)
         p->symfn[pr->funcs[k]->name->index] = 1;
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
      case RSLC_DST_GLSL_CAFE:
         if (c->opts->dst == RSLC_DST_GLSL_ES)
            p->es = true;
         if (c->opts->dst == RSLC_DST_GLSL_CAFE)
         {
            p->cafe    = true;
            p->version = 150;
         }
         if (!p->version)
            p->version = p->es ? 300 : 330;
         break;
      case RSLC_DST_HLSL:
         p->hlsl = true;
         if (!p->version)
            p->version = 50;
         break;
      case RSLC_DST_MSL:
         p->msl = true;
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

   if (!rslc_blob_begin(c, &p->out, 4096, p->active, p->used))
      return false;
   if (!(p->hlsl ? pr_hlsl(p, result) : p->msl ? pr_msl(p, result) : pr_glsl(p, result)))
   {
      if (!c->failed)
         rslc_err(c, "print failed", NULL, 0);
      rslc_pbuf_free(c, &p->out);
      return false;
   }
   return true;
}

/* ---- frame-rectangle remap (opts.remap) ---- */

/* The stage's reads of the named samplers become calls of helpers
 * parsed from GLSL this writes for the pass's sampling, which read the
 * rectangle as gfx/drivers_shader/slang_rect.c's rewrite of a SPIR-V
 * module does, operation for operation; a function the frame is passed
 * to is written again for each way it is.  The rectangle's three vec4s
 * go after what the named block holds (or into a block of their own),
 * at the first multiple of 16 at or past opts.remap->offset. */

#define RSLC_RM_MAX_SPECS 256

struct rslc_rm_spec
{
   uint32_t fid;     /* the function as parsed */
   uint32_t mask;    /* its parameters that are the frame */
   uint32_t clone;   /* the function written for it; fid for mask 0 */
};

struct rslc_rm;
static uint32_t rslc_rm_helper(struct rslc_rm *r, unsigned bfn, unsigned nargs,
      const struct rslc_type *sampler);

struct rslc_rm
{
   struct rslc_ctx          *c;
   const struct rslc_remap  *rm;
   uint8_t                  *frame;     /* by var id: the frame */
   uint32_t                 *vmap;      /* by var id, while a clone is copied */
   struct rslc_rm_spec      *specs;
   uint32_t                  nspecs;
   uint32_t                  nvars0;    /* vars before cloning */
   bool                      fail;
   bool                      touched;
};

/* Whether a read of the frame, of builtin n->a with @nargs arguments,
 * has a helper: the builtin's name prefixed and its parameters.  Only
 * fetches and the size read an integer frame. */
static bool rslc_rm_supported(const struct rslc_rm *r, const struct rslc_node *n,
      unsigned nargs, const struct rslc_type *sampler)
{
   const struct rslc_remap *rm   = r->rm;
   const struct rslc_type  *vec4 = rslc_type_basic(r->c, RSLC_TY_VEC,
         sampler->base, 4, 1);
   bool lin_ok = !rm->linear
      || rm->wrap == RSLC_REMAP_WRAP_EDGE || rm->wrap == RSLC_REMAP_WRAP_BORDER;
   if (     sampler->base != RSLC_BASE_FLOAT
         && n->a != RSLC_BFN_TEXELFETCH && n->a != RSLC_BFN_TEXELFETCHOFFSET
         && n->a != RSLC_BFN_TEXTURESIZE)
      return false;
   switch (n->a)
   {
      case RSLC_BFN_TEXTURE:
         return (nargs == 2 || nargs == 3) && n->type == vec4 && lin_ok;
      case RSLC_BFN_TEXTURELOD:
         return nargs == 3 && n->type == vec4 && lin_ok;
      case RSLC_BFN_TEXTUREOFFSET:
         return (nargs == 3 || nargs == 4) && n->type == vec4 && lin_ok;
      case RSLC_BFN_TEXTURELODOFFSET:
      case RSLC_BFN_TEXTUREGRAD:
         return nargs == 4 && n->type == vec4 && lin_ok;
      case RSLC_BFN_TEXTUREGRADOFFSET:
         return nargs == 5 && n->type == vec4 && lin_ok;
      case RSLC_BFN_TEXTUREGATHER:
         return (nargs == 2 || nargs == 3) && n->type == vec4;
      case RSLC_BFN_TEXTUREGATHEROFFSET:
         return (nargs == 3 || nargs == 4) && n->type == vec4;
      case RSLC_BFN_TEXELFETCH:
         return nargs == 3 && n->type == vec4;
      case RSLC_BFN_TEXELFETCHOFFSET:
         return nargs == 4 && n->type == vec4;
      case RSLC_BFN_TEXTURESIZE:
         return nargs == 2;
      default:
         break;
   }
   return false;
}

static bool rslc_rm_is_frame(const struct rslc_rm *r, uint32_t node)
{
   const struct rslc_node *n = rslc_node_at(r->c, node);
   return n->op == RSLC_OP_VAR && r->frame[n->a];
}

/* The spec for @fid called with the frame for @mask, added when new. */
static struct rslc_rm_spec *rslc_rm_spec(struct rslc_rm *r, uint32_t fid,
      uint32_t mask)
{
   uint32_t i;
   for (i = 0; i < r->nspecs; i++)
      if (r->specs[i].fid == fid && r->specs[i].mask == mask)
         return &r->specs[i];
   if (r->nspecs >= RSLC_RM_MAX_SPECS)
   {
      r->fail = true;
      return NULL;
   }
   r->specs[r->nspecs].fid   = fid;
   r->specs[r->nspecs].mask  = mask;
   r->specs[r->nspecs].clone = fid;
   return &r->specs[r->nspecs++];
}

/* Which arguments of a call are the frame. */
static uint32_t rslc_rm_mask(struct rslc_rm *r, const struct rslc_node *n)
{
   struct rslc_ctx *c   = r->c;
   uint32_t         cnt = n->b ? rslc_list_len(c, n->b) : 0;
   uint32_t         i, mask = 0;
   for (i = 0; i < cnt; i++)
      if (rslc_rm_is_frame(r, rslc_list_item(c, n->b, i)))
      {
         if (i >= 32)
            r->fail = true;
         else
            mask |= 1u << i;
      }
   return mask;
}

/* Walks @node: with @apply, rewrites the frame's reads and calls;
 * without, only finds whether it can and what it calls how. */
static void rslc_rm_walk(struct rslc_rm *r, uint32_t node, bool apply)
{
   struct rslc_ctx  *c = r->c;
   struct rslc_node *n;
   uint32_t          i, cnt;
   if (!node || r->fail)
      return;
   n = rslc_node_at(c, node);
   switch (n->op)
   {
      case RSLC_OP_CONST:
         return;
      case RSLC_OP_VAR:
         /* the frame anywhere but a read or a call */
         if (r->frame[n->a])
            r->fail = true;
         return;
      case RSLC_OP_DECL:
         rslc_rm_walk(r, c->prog.vars[n->a]->init, apply);
         return;
      case RSLC_OP_MEMBER: case RSLC_OP_SWIZZLE: case RSLC_OP_CONVERT:
      case RSLC_OP_EXPR: case RSLC_OP_RETURN: case RSLC_OP_CASE:
         rslc_rm_walk(r, n->a, apply);
         return;
      case RSLC_OP_BUILTIN:
         cnt = n->b ? rslc_list_len(c, n->b) : 0;
         if (cnt && rslc_rm_is_frame(r, rslc_list_item(c, n->b, 0)))
         {
            const struct rslc_type *st   = rslc_node_at(c,
                  rslc_list_item(c, n->b, 0))->type;
            if (!rslc_rm_supported(r, n, cnt, st))
            {
               r->fail = true;
               return;
            }
            r->touched = true;
            for (i = 1; i < cnt; i++)
               rslc_rm_walk(r, rslc_list_item(c, n->b, i), apply);
            if (apply)
            {
               uint32_t f = rslc_rm_helper(r, n->a, cnt, st);
               if (!f)
               {
                  r->fail = true;
                  return;
               }
               n->op = RSLC_OP_CALL;
               n->a  = f;
            }
            return;
         }
         for (i = 0; i < cnt; i++)
            rslc_rm_walk(r, rslc_list_item(c, n->b, i), apply);
         return;
      case RSLC_OP_CALL:
      {
         uint32_t             mask = rslc_rm_mask(r, n);
         uint32_t             nspecs = r->nspecs;
         struct rslc_rm_spec *s    = rslc_rm_spec(r, n->a, mask);
         if (!s)
            return;
         /* rewriting finds every spec the scan did */
         if (apply && r->nspecs != nspecs)
         {
            r->fail = true;
            return;
         }
         cnt = n->b ? rslc_list_len(c, n->b) : 0;
         for (i = 0; i < cnt; i++)
            if (!(mask & (1u << i)))
               rslc_rm_walk(r, rslc_list_item(c, n->b, i), apply);
         if (apply)
            n->a = s->clone;
         return;
      }
      case RSLC_OP_CONSTRUCT: case RSLC_OP_BLOCK: case RSLC_OP_FOR:
      case RSLC_OP_SWITCH:
         if (n->op == RSLC_OP_SWITCH)
            rslc_rm_walk(r, n->a, apply);
         cnt = n->b ? rslc_list_len(c, n->b) : 0;
         for (i = 0; i < cnt; i++)
            rslc_rm_walk(r, rslc_list_item(c, n->b, i), apply);
         return;
      default:
         rslc_rm_walk(r, n->a, apply);
         rslc_rm_walk(r, n->b, apply);
         rslc_rm_walk(r, n->c, apply);
         return;
   }
}

/* Every function main reaches, each way the frame is passed to it. */
static void rslc_rm_scan(struct rslc_rm *r, bool apply)
{
   struct rslc_ctx  *c  = r->c;
   struct rslc_prog *pr = &c->prog;
   uint32_t          i, k;
   if (!rslc_rm_spec(r, pr->main_fn->id, 0))
      return;
   for (k = 0; k < r->nspecs && !r->fail; k++)
   {
      const struct rslc_func *f = pr->funcs[r->specs[k].clone];
      uint32_t mask = r->specs[k].mask;
      for (i = 0; i < f->nparams && i < 32; i++)
         if (mask & (1u << i))
            r->frame[f->params[i]->id] = 1;
      rslc_rm_walk(r, f->body, apply);
      for (i = 0; i < f->nparams && i < 32; i++)
         if (mask & (1u << i))
            r->frame[f->params[i]->id] = 0;
   }
   for (i = 1; i < pr->nvars && !r->fail; i++)
      if (pr->vars[i]->global && pr->vars[i]->init)
         rslc_rm_walk(r, pr->vars[i]->init, apply);
}

/* ---- cloning ---- */

static struct rslc_var *rslc_rm_var_copy(struct rslc_rm *r, const struct rslc_var *src)
{
   struct rslc_var *v = rslc_front_var_copy(r->c, src);
   if (v && src->id < r->nvars0)
      r->vmap[src->id] = v->id;
   return v;
}

static uint32_t rslc_rm_copy(struct rslc_rm *r, uint32_t node);

static uint32_t rslc_rm_copy_list(struct rslc_rm *r, uint32_t list)
{
   struct rslc_ctx *c = r->c;
   uint32_t         cnt, i, *items, out;
   struct rslc_mark mark;
   if (!list)
      return 0;
   cnt = rslc_list_len(c, list);
   rslc_scratch_mark(c, &mark);
   if (!(items = (uint32_t*)rslc_scratch_alloc(c, (cnt ? cnt : 1) * sizeof(*items))))
      return 0;
   for (i = 0; i < cnt; i++)
   {
      uint32_t it = rslc_list_item(c, list, i);
      items[i]    = it ? rslc_rm_copy(r, it) : 0;
      if (it && !items[i])
         return 0;
   }
   out = rslc_list_new(c, items, cnt);
   rslc_scratch_release(c, &mark);
   return out;
}

/* A copy of the tree at @node with the clone's own variables. */
static uint32_t rslc_rm_copy(struct rslc_rm *r, uint32_t node)
{
   struct rslc_ctx        *c = r->c;
   const struct rslc_node *n;
   struct rslc_node        t;
   uint32_t                id;
   if (!node || r->fail)
      return 0;
   n = rslc_node_at(c, node);
   t = *n;
   switch (n->op)
   {
      case RSLC_OP_CONST: case RSLC_OP_BREAK: case RSLC_OP_CONTINUE:
      case RSLC_OP_DISCARD:
         break;
      case RSLC_OP_VAR:
         if (n->a < r->nvars0 && r->vmap[n->a])
            t.a = r->vmap[n->a];
         break;
      case RSLC_OP_DECL:
      {
         struct rslc_var *v = rslc_rm_var_copy(r, c->prog.vars[n->a]);
         if (!v)
            return 0;
         if (v->init && !(v->init = rslc_rm_copy(r, v->init)))
            return 0;
         t.a = v->id;
         break;
      }
      case RSLC_OP_MEMBER:
         t.a = rslc_rm_copy(r, n->a);
         break;
      case RSLC_OP_CALL: case RSLC_OP_BUILTIN:
         t.b = rslc_rm_copy_list(r, n->b);
         break;
      case RSLC_OP_CONSTRUCT: case RSLC_OP_BLOCK: case RSLC_OP_FOR:
         t.b = rslc_rm_copy_list(r, n->b);
         break;
      case RSLC_OP_SWITCH:
         t.a = rslc_rm_copy(r, n->a);
         t.b = rslc_rm_copy_list(r, n->b);
         break;
      default:
         t.a = n->a ? rslc_rm_copy(r, n->a) : 0;
         t.b = n->b ? rslc_rm_copy(r, n->b) : 0;
         t.c = n->c ? rslc_rm_copy(r, n->c) : 0;
         break;
   }
   if (c->failed)
      return 0;
   if (!(id = rslc_node_new(c, (enum rslc_op)t.op, t.type, t.a, t.b, t.c, t.aux, t.line)))
      return 0;
   rslc_node_at(c, id)->prec = t.prec;
   return id;
}

/* Function @s->fid written again for spec @s, named apart. */
static bool rslc_rm_clone(struct rslc_rm *r, struct rslc_rm_spec *s)
{
   struct rslc_ctx        *c  = r->c;
   struct rslc_prog       *pr = &c->prog;
   const struct rslc_func *src = pr->funcs[s->fid];
   struct rslc_func       *f;
   char                    name[96];
   size_t                  len;
   uint32_t                i;

   if (!(f = (struct rslc_func*)rslc_arena_zalloc(c, sizeof(*f))))
      return false;
   memset(r->vmap, 0, r->nvars0 * sizeof(*r->vmap));
   len = src->name->len < 64 ? src->name->len : 64;
   memcpy(name, src->name->str, len);
   memcpy(name + len, "_rarch_rect", 11);
   len += 11;
   {
      unsigned v = (unsigned)(s - r->specs), d = 100;
      for (; d; d /= 10)
         name[len++] = (char)('0' + (v / d) % 10);
   }
   if (!(f->name = rslc_sym(c, name, len)))
      return false;
   f->ret     = src->ret;
   f->line    = src->line;
   f->nparams = src->nparams;
   if (src->nparams)
   {
      if (!(f->params = (struct rslc_var**)rslc_arena_alloc(c,
                  src->nparams * sizeof(*f->params))))
         return false;
      for (i = 0; i < src->nparams; i++)
         if (!(f->params[i] = rslc_rm_var_copy(r, src->params[i])))
            return false;
   }
   if (!(f->body = rslc_rm_copy(r, src->body)))
      return false;
   if (pr->nfuncs == pr->funcs_cap)
   {
      uint32_t           ncap = pr->funcs_cap * 2;
      struct rslc_func **nv   = (struct rslc_func**)rslc_arena_alloc(c, ncap * sizeof(*nv));
      if (!nv)
         return false;
      memcpy(nv, pr->funcs, pr->nfuncs * sizeof(*nv));
      pr->funcs     = nv;
      pr->funcs_cap = ncap;
   }
   f->id                   = pr->nfuncs;
   pr->funcs[pr->nfuncs++] = f;
   s->clone                = f->id;
   return true;
}

/* ---- the helpers ---- */

/* The helper parsed for builtin @bfn with @nargs arguments, the first
 * a @sampler. */
static uint32_t rslc_rm_helper(struct rslc_rm *r, unsigned bfn, unsigned nargs,
      const struct rslc_type *sampler)
{
   struct rslc_ctx  *c = r->c;
   const char       *bn = rslc_bfn_name(bfn);
   char              name[48];
   size_t            len;
   struct rslc_sym  *s;
   uint32_t          i;
   if (!bn)
      return 0;
   len = strlen(bn);
   if (len > sizeof(name) - 12)
      return 0;
   memcpy(name, "rarch_rect_", 11);
   memcpy(name + 11, bn, len);
   len += 11;
   if (!(s = rslc_sym(c, name, len)))
      return 0;
   for (i = 1; i < c->prog.nfuncs; i++)
   {
      const struct rslc_func *f = c->prog.funcs[i];
      if (     f->name == s && f->nparams == nargs && f->body
            && f->params[0]->type == sampler)
         return f->id;
   }
   return 0;
}

/* @s into @b, "$R", "$C" and "$T" spelled as the rect, clamp and
 * texels members are named. */
static bool rslc_rm_text(struct rslc_rm *r, struct rslc_pbuf *b, const char *s)
{
   struct rslc_ctx *c = r->c;
   for (; *s; s++)
   {
      if (*s == '$' && (s[1] == 'R' || s[1] == 'C' || s[1] == 'T'))
      {
         const char *nm = s[1] == 'R' ? r->rm->rect_name
            : s[1] == 'C' ? r->rm->clamp_name : r->rm->texels_name;
         if (!rslc_pbuf_puts(c, b, nm))
            return false;
         s++;
         continue;
      }
      if (!rslc_pbuf_putc(c, b, *s))
         return false;
   }
   return true;
}

/* The nearest texel's helper: with an offset (@off) and for a gather
 * (@gather), unwrapped. */
static bool rslc_rm_text_near(struct rslc_rm *r, struct rslc_pbuf *b,
      bool off, bool gather)
{
   enum rslc_remap_wrap w = r->rm->wrap;
   bool wraps = w == RSLC_REMAP_WRAP_REPEAT || w == RSLC_REMAP_WRAP_MIRROR;
   bool ok    = rslc_rm_text(r, b, "ivec2 rarch_rect_near")
      && rslc_rm_text(r, b, off ? "_o" : "")
      && rslc_rm_text(r, b, gather ? "_g" : "")
      && rslc_rm_text(r, b, "(vec2 rarch_rect_c, vec2 rarch_rect_z")
      && rslc_rm_text(r, b, off ? ", ivec2 rarch_rect_o)\n{\n" : ")\n{\n");
   if (ok && wraps && off)
      ok = rslc_rm_text(r, b, "rarch_rect_c = rarch_rect_c + vec2(rarch_rect_o) / rarch_rect_z;\n");
   if (ok && w == RSLC_REMAP_WRAP_REPEAT)
      ok = rslc_rm_text(r, b, "rarch_rect_c = rarch_rect_c - floor(rarch_rect_c);\n");
   else if (ok && wraps)
   {
      ok = rslc_rm_text(r, b,
            "vec2 rarch_rect_h = rarch_rect_c * 0.5;\n"
            "vec2 rarch_rect_f = rarch_rect_h - floor(rarch_rect_h);\n");
      if (ok && gather)
         ok = rslc_rm_text(r, b, "rarch_rect_c = rarch_rect_f + rarch_rect_f;\n");
      else if (ok)
         ok = rslc_rm_text(r, b,
               "vec2 rarch_rect_g = 1.0 - rarch_rect_f;\n"
               "rarch_rect_c = vec2("
               "rarch_rect_f.x < 0.5 ? rarch_rect_f.x + rarch_rect_f.x : rarch_rect_g.x + rarch_rect_g.x, "
               "rarch_rect_f.y < 0.5 ? rarch_rect_f.y + rarch_rect_f.y : rarch_rect_g.y + rarch_rect_g.y);\n");
   }
   if (ok)
      ok = rslc_rm_text(r, b, "vec2 rarch_rect_uv = rarch_rect_c * rarch_rect_z;\n");
   if (ok && gather)
      ok = rslc_rm_text(r, b, "rarch_rect_uv = rarch_rect_uv - 0.5;\n");
   if (ok)
      ok = rslc_rm_text(r, b, "ivec2 rarch_rect_i = ivec2(floor(rarch_rect_uv));\n");
   if (ok && off && !wraps)
      ok = rslc_rm_text(r, b, "rarch_rect_i = rarch_rect_i + rarch_rect_o;\n");
   return ok && rslc_rm_text(r, b, "return rarch_rect_i;\n}\n");
}

/* Builtin @bn's helper for linear filtering: its parameters after
 * the coordinate are @params, with an offset (@off) folded into the
 * coordinate, and the sample is @call at the moved coordinate.  Edge
 * and border wrap only: linear filtering mixes across the rectangle's
 * edge with repeat and mirror, which the clamp cannot keep. */
static bool rslc_rm_text_linear(struct rslc_rm *r, struct rslc_pbuf *b,
      const char *bn, const char *params, bool off, const char *call)
{
   bool border = r->rm->wrap == RSLC_REMAP_WRAP_BORDER;
   bool ok = rslc_rm_text(r, b, "vec4 rarch_rect_")
      && rslc_rm_text(r, b, bn)
      && rslc_rm_text(r, b, "(sampler2D rarch_rect_s, vec2 rarch_rect_c")
      && rslc_rm_text(r, b, params)
      && rslc_rm_text(r, b, ")\n{\nvec4 rarch_rect_r = $R;\nvec4 rarch_rect_l = $C;\n");
   if (ok && off)
      ok = rslc_rm_text(r, b, "rarch_rect_c = rarch_rect_c + vec2(rarch_rect_o) / $T.xy;\n");
   if (ok && border)
      ok = rslc_rm_text(r, b,
            "vec2 rarch_rect_t = rarch_rect_c * $T.xy - 0.5;\n"
            "vec2 rarch_rect_lo = clamp(rarch_rect_t + 1.0, 0.0, 1.0);\n"
            "vec2 rarch_rect_hi = clamp($T.xy - rarch_rect_t, 0.0, 1.0);\n"
            "vec2 rarch_rect_in = rarch_rect_lo * rarch_rect_hi;\n"
            "float rarch_rect_inner = rarch_rect_in.x * rarch_rect_in.y;\n");
   if (ok)
      ok = rslc_rm_text(r, b,
            "rarch_rect_c = clamp(rarch_rect_c * rarch_rect_r.xy + rarch_rect_r.zw, "
            "rarch_rect_l.xy, rarch_rect_l.zw);\nreturn ")
         && rslc_rm_text(r, b, call)
         && rslc_rm_text(r, b, border ? " * rarch_rect_inner;\n}\n" : ";\n}\n");
   return ok;
}

/* Builtin @bn's helper for nearest filtering: the texel the sampler
 * picks, fetched; the frame has one level, so a bias, a level or
 * gradients choose nothing. */
static bool rslc_rm_text_nearest(struct rslc_rm *r, struct rslc_pbuf *b,
      const char *bn, const char *params, bool off)
{
   return rslc_rm_text(r, b, "vec4 rarch_rect_")
      && rslc_rm_text(r, b, bn)
      && rslc_rm_text(r, b, "(sampler2D rarch_rect_s, vec2 rarch_rect_c")
      && rslc_rm_text(r, b, params)
      && rslc_rm_text(r, b, ")\n{\nreturn rarch_rect_fetch(rarch_rect_s, rarch_rect_near")
      && rslc_rm_text(r, b, off ? "_o(rarch_rect_c, $T.xy, rarch_rect_o)"
            : "(rarch_rect_c, $T.xy)")
      && rslc_rm_text(r, b, ", ivec2($T.xy), ivec2($T.zw));\n}\n");
}

static bool rslc_rm_text_gather(struct rslc_rm *r, struct rslc_pbuf *b,
      bool off, bool comp)
{
   static const char *const ends[4] = {
      "ivec2(rarch_rect_i0.x, rarch_rect_i1.y)", "rarch_rect_i1",
      "ivec2(rarch_rect_i1.x, rarch_rect_i0.y)", "rarch_rect_i0" };
   unsigned k;
   bool ok = rslc_rm_text(r, b, off ? "vec4 rarch_rect_textureGatherOffset"
            : "vec4 rarch_rect_textureGather")
      && rslc_rm_text(r, b, "(sampler2D rarch_rect_s, vec2 rarch_rect_c")
      && rslc_rm_text(r, b, off ? ", ivec2 rarch_rect_o" : "")
      && rslc_rm_text(r, b, comp ? ", int rarch_rect_k" : "")
      && rslc_rm_text(r, b, ")\n{\n"
            "ivec2 rarch_rect_n = ivec2($T.xy);\n"
            "ivec2 rarch_rect_org = ivec2($T.zw);\n"
            "ivec2 rarch_rect_i0 = rarch_rect_near")
      && rslc_rm_text(r, b, off ? "_o_g(rarch_rect_c, $T.xy, rarch_rect_o);\n"
            : "_g(rarch_rect_c, $T.xy);\n")
      && rslc_rm_text(r, b, "ivec2 rarch_rect_i1 = rarch_rect_i0 + ivec2(1);\n");
   for (k = 0; ok && k < 4; k++)
      ok = rslc_rm_text(r, b, "vec4 rarch_rect_v")
         && rslc_pbuf_putu(r->c, b, k)
         && rslc_rm_text(r, b, " = rarch_rect_fetch(rarch_rect_s, ")
         && rslc_rm_text(r, b, ends[k])
         && rslc_rm_text(r, b, ", rarch_rect_n, rarch_rect_org);\n");
   return ok && rslc_rm_text(r, b, comp
         ? "return vec4(rarch_rect_v0[rarch_rect_k], rarch_rect_v1[rarch_rect_k], "
           "rarch_rect_v2[rarch_rect_k], rarch_rect_v3[rarch_rect_k]);\n}\n"
         : "return vec4(rarch_rect_v0[0], rarch_rect_v1[0], "
           "rarch_rect_v2[0], rarch_rect_v3[0]);\n}\n");
}

/* The helpers for the pass's sampling, as GLSL. */
static bool rslc_rm_snippet(struct rslc_rm *r, struct rslc_pbuf *b)
{
   enum rslc_remap_wrap w = r->rm->wrap;
   unsigned i;
   bool ok = rslc_rm_text(r, b, "#version 450\n");

   /* the texel wrapped into the rectangle: the remainder the float
    * quotient gives, made exact by two corrections */
   if (ok && (w == RSLC_REMAP_WRAP_REPEAT || w == RSLC_REMAP_WRAP_MIRROR))
      ok = rslc_rm_text(r, b,
            "ivec2 rarch_rect_mod(ivec2 rarch_rect_i, ivec2 rarch_rect_n)\n{\n"
            "ivec2 rarch_rect_q = ivec2(floor(vec2(rarch_rect_i) / vec2(rarch_rect_n)));\n"
            "ivec2 rarch_rect_m = rarch_rect_i - rarch_rect_q * rarch_rect_n;\n"
            "rarch_rect_m = ivec2("
            "rarch_rect_m.x < 0 ? rarch_rect_m.x + rarch_rect_n.x : rarch_rect_m.x, "
            "rarch_rect_m.y < 0 ? rarch_rect_m.y + rarch_rect_n.y : rarch_rect_m.y);\n"
            "return ivec2("
            "rarch_rect_m.x >= rarch_rect_n.x ? rarch_rect_m.x - rarch_rect_n.x : rarch_rect_m.x, "
            "rarch_rect_m.y >= rarch_rect_n.y ? rarch_rect_m.y - rarch_rect_n.y : rarch_rect_m.y);\n}\n");
   if (ok)
      ok = rslc_rm_text(r, b, "ivec2 rarch_rect_wrap(ivec2 rarch_rect_i, ivec2 rarch_rect_n)\n{\n");
   if (ok && w == RSLC_REMAP_WRAP_REPEAT)
      ok = rslc_rm_text(r, b, "return rarch_rect_mod(rarch_rect_i, rarch_rect_n);\n}\n");
   else if (ok && w == RSLC_REMAP_WRAP_MIRROR)
      ok = rslc_rm_text(r, b,
            "ivec2 rarch_rect_n2 = rarch_rect_n + rarch_rect_n;\n"
            "ivec2 rarch_rect_m = rarch_rect_mod(rarch_rect_i, rarch_rect_n2);\n"
            "ivec2 rarch_rect_a = rarch_rect_n2 - ivec2(1) - rarch_rect_m;\n"
            "return ivec2("
            "rarch_rect_m.x < rarch_rect_n.x ? rarch_rect_m.x : rarch_rect_a.x, "
            "rarch_rect_m.y < rarch_rect_n.y ? rarch_rect_m.y : rarch_rect_a.y);\n}\n");
   else if (ok)
      ok = rslc_rm_text(r, b,
            "return clamp(rarch_rect_i, ivec2(0), rarch_rect_n - ivec2(1));\n}\n");

   /* a texel of the rectangle, wrapped; border wrap's transparent black
    * outside it */
   if (ok)
      ok = rslc_rm_text(r, b,
            "vec4 rarch_rect_fetch(sampler2D rarch_rect_s, ivec2 rarch_rect_i, "
            "ivec2 rarch_rect_n, ivec2 rarch_rect_org)\n{\n");
   if (ok && w == RSLC_REMAP_WRAP_BORDER)
      ok = rslc_rm_text(r, b,
            "bool rarch_rect_in = all(greaterThanEqual(rarch_rect_i, ivec2(0))) "
            "&& all(lessThan(rarch_rect_i, rarch_rect_n));\n");
   if (ok)
      ok = rslc_rm_text(r, b,
            "vec4 rarch_rect_v = texelFetch(rarch_rect_s, "
            "rarch_rect_wrap(rarch_rect_i, rarch_rect_n) + rarch_rect_org, 0);\n");
   if (ok)
      ok = rslc_rm_text(r, b, w == RSLC_REMAP_WRAP_BORDER
            ? "return rarch_rect_in ? rarch_rect_v : vec4(0.0);\n}\n"
            : "return rarch_rect_v;\n}\n");

   ok = ok && rslc_rm_text_near(r, b, false, false)
      && rslc_rm_text_near(r, b, true, false)
      && rslc_rm_text_near(r, b, false, true)
      && rslc_rm_text_near(r, b, true, true);

   /* the samples: linear filtering with repeat or mirror was refused */
   if (ok && r->rm->linear)
      ok = rslc_rm_text_linear(r, b, "texture", "", false,
               "texture(rarch_rect_s, rarch_rect_c)")
         && rslc_rm_text_linear(r, b, "texture", ", float rarch_rect_b", false,
               "texture(rarch_rect_s, rarch_rect_c, rarch_rect_b)")
         && rslc_rm_text_linear(r, b, "textureLod", ", float rarch_rect_b", false,
               "textureLod(rarch_rect_s, rarch_rect_c, rarch_rect_b)")
         && rslc_rm_text_linear(r, b, "textureOffset", ", ivec2 rarch_rect_o", true,
               "texture(rarch_rect_s, rarch_rect_c)")
         && rslc_rm_text_linear(r, b, "textureOffset",
               ", ivec2 rarch_rect_o, float rarch_rect_b", true,
               "texture(rarch_rect_s, rarch_rect_c, rarch_rect_b)")
         && rslc_rm_text_linear(r, b, "textureLodOffset",
               ", float rarch_rect_b, ivec2 rarch_rect_o", true,
               "textureLod(rarch_rect_s, rarch_rect_c, rarch_rect_b)")
         && rslc_rm_text_linear(r, b, "textureGrad",
               ", vec2 rarch_rect_dx, vec2 rarch_rect_dy", false,
               "textureGrad(rarch_rect_s, rarch_rect_c, "
               "rarch_rect_dx * rarch_rect_r.xy, rarch_rect_dy * rarch_rect_r.xy)")
         && rslc_rm_text_linear(r, b, "textureGradOffset",
               ", vec2 rarch_rect_dx, vec2 rarch_rect_dy, ivec2 rarch_rect_o", true,
               "textureGrad(rarch_rect_s, rarch_rect_c, "
               "rarch_rect_dx * rarch_rect_r.xy, rarch_rect_dy * rarch_rect_r.xy)");
   else if (ok)
      ok = rslc_rm_text_nearest(r, b, "texture", "", false)
         && rslc_rm_text_nearest(r, b, "texture", ", float rarch_rect_b", false)
         && rslc_rm_text_nearest(r, b, "textureLod", ", float rarch_rect_b", false)
         && rslc_rm_text_nearest(r, b, "textureOffset", ", ivec2 rarch_rect_o", true)
         && rslc_rm_text_nearest(r, b, "textureOffset",
               ", ivec2 rarch_rect_o, float rarch_rect_b", true)
         && rslc_rm_text_nearest(r, b, "textureLodOffset",
               ", float rarch_rect_b, ivec2 rarch_rect_o", true)
         && rslc_rm_text_nearest(r, b, "textureGrad",
               ", vec2 rarch_rect_dx, vec2 rarch_rect_dy", false)
         && rslc_rm_text_nearest(r, b, "textureGradOffset",
               ", vec2 rarch_rect_dx, vec2 rarch_rect_dy, ivec2 rarch_rect_o", true);

   /* gathers, fetches and the size, whatever the filtering; fetches
    * and the size of an integer frame too */
   ok = ok
      && rslc_rm_text_gather(r, b, false, false)
      && rslc_rm_text_gather(r, b, false, true)
      && rslc_rm_text_gather(r, b, true, false)
      && rslc_rm_text_gather(r, b, true, true);
   for (i = 0; ok && i < 3; i++)
   {
      static const char *const pfx[3] = { "", "i", "u" };
      ok = rslc_rm_text(r, b, pfx[i])
         && rslc_rm_text(r, b, "vec4 rarch_rect_texelFetch(")
         && rslc_rm_text(r, b, pfx[i])
         && rslc_rm_text(r, b, "sampler2D rarch_rect_s, ivec2 rarch_rect_p, int rarch_rect_b)\n{\n"
               "return texelFetch(rarch_rect_s, rarch_rect_p + ivec2($T.zw), rarch_rect_b);\n}\n")
         && rslc_rm_text(r, b, pfx[i])
         && rslc_rm_text(r, b, "vec4 rarch_rect_texelFetchOffset(")
         && rslc_rm_text(r, b, pfx[i])
         && rslc_rm_text(r, b, "sampler2D rarch_rect_s, ivec2 rarch_rect_p, "
               "int rarch_rect_b, ivec2 rarch_rect_o)\n{\n"
               "return texelFetch(rarch_rect_s, rarch_rect_p + ivec2($T.zw) + rarch_rect_o, rarch_rect_b);\n}\n"
               "ivec2 rarch_rect_textureSize(")
         && rslc_rm_text(r, b, pfx[i])
         && rslc_rm_text(r, b, "sampler2D rarch_rect_s, int rarch_rect_b)\n{\n"
               "return ivec2($T.xy);\n}\n");
   }
   return ok;
}

/* ---- the block ---- */

/* Where the block the remap names ends; false for one holding what the
 * SPIR-V rewrite cannot measure (a struct member) or a uniform block at
 * another set and binding. */
static bool rslc_rm_block_end(struct rslc_rm *r, unsigned idx, uint32_t *end)
{
   const struct rslc_prog      *pr = &r->c->prog;
   const struct rslc_blockdecl *b  = &pr->blocks[idx];
   uint32_t i;
   *end = 0;
   if (!pr->has_block[idx])
      return true;
   if (     idx == 0
         && (   (b->set == RSLC_UNSET ? 0 : b->set) != r->rm->set
             || b->binding != r->rm->binding))
      return false;
   for (i = 0; i < b->type->nfields; i++)
   {
      const struct rslc_type *t = b->type->fields[i].type;
      uint32_t a, s, st, off;
      if (t->kind == RSLC_TY_STRUCT)
         return false;
      rslc_layout_of_type(t, b->std140, &a, &s, &st);
      (void)a;
      (void)st;
      off = rslc_layout_field_offset(b->type, i, b->std140);
      if (off + s > *end)
         *end = off + s;
   }
   return true;
}

/* The three members, after the block's own at @base, or in a block of
 * their own; bound by name for the helpers. */
static bool rslc_rm_block_add(struct rslc_rm *r, unsigned idx, uint32_t end,
      uint32_t base)
{
   struct rslc_ctx       *c   = r->c;
   struct rslc_prog      *pr  = &c->prog;
   struct rslc_blockdecl *b   = &pr->blocks[idx];
   const struct rslc_type *v4 = rslc_type_basic(c, RSLC_TY_VEC, RSLC_BASE_FLOAT, 4, 1);
   const char *names[3];
   uint32_t old = pr->has_block[idx] ? b->type->nfields : 0;
   uint32_t at  = (end + 15) & ~15u;   /* where a vec4 would go */
   uint32_t pad = (base - at) / 16;
   uint32_t n   = old + (pad ? 1 : 0) + 3;
   struct rslc_field *f;
   struct rslc_type  *st;
   uint32_t i;

   names[0] = r->rm->rect_name;
   names[1] = r->rm->clamp_name;
   names[2] = r->rm->texels_name;
   if (!v4 || !(f = (struct rslc_field*)rslc_arena_zalloc(c, n * sizeof(*f))))
      return false;
   if (old)
      memcpy(f, b->type->fields, old * sizeof(*f));
   i = old;
   if (pad)
   {
      f[i].type = pad == 1 ? v4 : rslc_type_array(c, v4, pad);
      f[i].name = rslc_sym(c, "rarch_rect_pad", 14);
      if (!f[i].type || !f[i].name)
         return false;
      i++;
   }
   for (; i < n; i++)
   {
      const char *nm = names[i - (n - 3)];
      f[i].type      = v4;
      f[i].precision = RSLC_PREC_HIGH;
      if (!(f[i].name = rslc_sym(c, nm, strlen(nm))))
         return false;
   }
   if (!pr->has_block[idx])
   {
      b->instance = rslc_sym(c, "rarch_rect", 10);
      b->binding  = idx == 0 ? r->rm->binding : RSLC_UNSET;
      b->set      = idx == 0 ? r->rm->set : RSLC_UNSET;
      b->line     = 0;
      b->push     = idx == 1;
      b->std140   = idx == 0;
      if (!b->instance)
         return false;
   }
   if (!(st = rslc_type_struct(c, pr->has_block[idx] ? b->type->name
               : rslc_sym(c, "RARCH_Rect", 10), f, n)))
      return false;
   b->type              = st;
   pr->has_block[idx]   = true;
   for (i = n - 3; i < n; i++)
   {
      struct rslc_var *v = rslc_front_global(c, f[i].name, v4);
      if (!v)
         return false;
      v->storage   = RSLC_ST_MEMBER;
      v->block     = idx;
      v->member    = i;
      v->precision = RSLC_PREC_HIGH;
   }
   return true;
}

bool rslc_remap_run(struct rslc_ctx *c)
{
   struct rslc_prog        *pr = &c->prog;
   const struct rslc_remap *rm = c->opts->remap;
   struct rslc_rm           r;
   struct rslc_pbuf         text;
   unsigned                 idx = rm->where == RSLC_REMAP_UBO ? 0 : 1;
   uint32_t                 i, k, end, base;
   bool                     any = false, ok;

   c->remap = RSLC_REMAP_UNSUPPORTED;
   memset(&r, 0, sizeof(r));
   r.c  = c;
   r.rm = rm;
   if (!(r.frame = (uint8_t*)rslc_arena_zalloc(c, pr->nvars + 1)))
      return false;
   for (i = 1; i < pr->nvars; i++)
   {
      const struct rslc_var *v = pr->vars[i];
      if (v->storage != RSLC_ST_SAMPLER || !v->name)
         continue;
      for (k = 0; k < rm->sampler_count; k++)
         if (     strlen(rm->samplers[k]) == v->name->len
               && !memcmp(rm->samplers[k], v->name->str, v->name->len))
         {
            r.frame[i] = 1;
            any        = true;
         }
   }
   if (!any)
   {
      c->remap = RSLC_REMAP_UNCHANGED;
      return true;
   }
   if (!rslc_rm_block_end(&r, idx, &end))
      return true;
   base = end > rm->offset ? end : rm->offset;
   base = (base + 15) & ~15u;
   if (idx == 1 && base + 48 > RSLC_REMAP_PUSH_LIMIT)
      return true;

   if (!(r.specs = (struct rslc_rm_spec*)rslc_arena_alloc(c,
               RSLC_RM_MAX_SPECS * sizeof(*r.specs))))
      return false;
   rslc_rm_scan(&r, false);
   if (c->failed)
      return false;
   if (r.fail)
      return true;
   if (!r.touched)
   {
      c->remap = RSLC_REMAP_UNCHANGED;
      return true;
   }

   /* the members, then the helpers parsed after the stage */
   if (!rslc_rm_block_add(&r, idx, end, base))
      return false;
   if (!rslc_pbuf_init(c, &text, 0, 6144))
      return false;
   ok = rslc_rm_snippet(&r, &text)
      && rslc_front_parse(c, (const char*)text.data, text.len);
   rslc_pbuf_free(c, &text);
   if (!ok)
      return false;

   /* the functions the frame is passed to, written again, then every
    * read rewritten */
   r.nvars0 = pr->nvars;
   if (!(r.vmap = (uint32_t*)rslc_arena_alloc(c, (pr->nvars + 1) * sizeof(*r.vmap))))
      return false;
   for (k = 0; k < r.nspecs; k++)
      if (r.specs[k].mask && !rslc_rm_clone(&r, &r.specs[k]))
         return false;
   if (!(r.frame = (uint8_t*)rslc_arena_zalloc(c, pr->nvars + 1)))
      return false;
   for (i = 1; i < r.nvars0; i++)
   {
      const struct rslc_var *v = pr->vars[i];
      if (v->storage != RSLC_ST_SAMPLER || !v->name)
         continue;
      for (k = 0; k < rm->sampler_count; k++)
         if (     strlen(rm->samplers[k]) == v->name->len
               && !memcmp(rm->samplers[k], v->name->str, v->name->len))
            r.frame[i] = 1;
   }
   rslc_rm_scan(&r, true);
   if (!c->failed && r.fail)
      rslc_err(c, "remap: a read the scan took would not rewrite", NULL, 0);
   if (c->failed || r.fail)
      return false;
   c->remap = RSLC_REMAP_REWRITTEN;
   return true;
}
