/* Copyright  (C) 2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rslc_dxbc.c).
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

/* rslc TU 4 (RSLC_EXTENDED): Direct3D shader bytecode.
 *
 * The parsed stage goes straight to a DXBC container of shader model
 * 4.0, 4.1 or 5.0 (opts->version 40 / 41 / 50): an input and an output
 * signature and the SHEX / SHDR program.  The interface is the one the
 * HLSL printer gives the same stage, element for element and register
 * for register, so a stage from here links with one compiled from that
 * HLSL: varyings and attributes are TEXCOORD<location>, the colour
 * outputs SV_Target<location + index>, uniform blocks cb<binding>,
 * samplers t<binding> and s<binding>, storage buffers t<binding> when
 * readonly and u<binding> otherwise, images u<binding>.
 *
 * Values live in four-component registers.  A scalar or vector is one
 * register, a matrix one register per column, an array or struct the
 * registers of its elements in order.  A variable indexed with a
 * value that is not constant lives in an indexable temporary; every
 * other one in temporaries.  The outputs are written to temporaries and
 * copied to the output registers where the stage ends, as the output
 * registers cannot be read.  A call is expanded where it is made, as
 * fxc does: its parameters, locals and result take temporaries above
 * the caller's. */

#include <string.h>

#include "rslc_share.h"

#if defined(RSLC_EXTENDED) && !defined(RSLC_NO_DXBC)

/* ------------------------------------------------------------------ */
/* Tokens                                                              */
/* ------------------------------------------------------------------ */

/* Opcodes of the tokenized program format. */
enum dx_op
{
   DX_ADD = 0, DX_AND = 1, DX_BREAK = 2, DX_BREAKC = 3, DX_CASE = 6,
   DX_CONTINUE = 7, DX_CUT = 9, DX_DEFAULT = 10, DX_DERIV_RTX = 11, DX_DERIV_RTY = 12,
   DX_DISCARD = 13, DX_DIV = 14, DX_DP2 = 15, DX_DP3 = 16, DX_DP4 = 17, DX_ELSE = 18,
   DX_EMIT = 19, DX_ENDIF = 21, DX_ENDLOOP = 22, DX_ENDSWITCH = 23, DX_EQ = 24,
   DX_EXP = 25, DX_FRC = 26, DX_FTOI = 27, DX_FTOU = 28, DX_GE = 29, DX_IADD = 30,
   DX_IF = 31, DX_IEQ = 32, DX_IGE = 33, DX_ILT = 34, DX_IMAD = 35, DX_IMAX = 36,
   DX_IMIN = 37, DX_IMUL = 38, DX_INE = 39, DX_INEG = 40, DX_ISHL = 41, DX_ISHR = 42,
   DX_ITOF = 43, DX_LD = 45, DX_LD_MS = 46, DX_LOG = 47, DX_LOOP = 48,
   DX_LT = 49, DX_MAD = 50, DX_MIN = 51, DX_MAX = 52, DX_MOV = 54, DX_MOVC = 55,
   DX_MUL = 56, DX_NE = 57, DX_NOT = 59, DX_OR = 60, DX_RESINFO = 61, DX_RET = 62,
   DX_ROUND_NE = 64, DX_ROUND_NI = 65, DX_ROUND_PI = 66, DX_ROUND_Z = 67, DX_RSQ = 68,
   DX_SAMPLE = 69, DX_SAMPLE_L = 72, DX_SAMPLE_D = 73, DX_SAMPLE_B = 74, DX_SQRT = 75,
   DX_SWITCH = 76, DX_SINCOS = 77, DX_UDIV = 78, DX_ULT = 79, DX_UGE = 80, DX_UMUL = 81,
   DX_UMAX = 83, DX_UMIN = 84, DX_USHR = 85, DX_UTOF = 86, DX_XOR = 87,
   DX_DCL_RESOURCE = 88, DX_DCL_CONSTANT_BUFFER = 89, DX_DCL_SAMPLER = 90,
   DX_DCL_GS_OUTPUT_TOPOLOGY = 92, DX_DCL_GS_INPUT_PRIMITIVE = 93,
   DX_DCL_MAX_OUTPUT_VERTEX_COUNT = 94, DX_DCL_INPUT = 95, DX_DCL_INPUT_SGV = 96,
   DX_DCL_INPUT_SIV = 97, DX_DCL_INPUT_PS = 98, DX_DCL_INPUT_PS_SGV = 99,
   DX_DCL_INPUT_PS_SIV = 100, DX_DCL_OUTPUT = 101, DX_DCL_OUTPUT_SIV = 103,
   DX_DCL_TEMPS = 104, DX_DCL_INDEXABLE_TEMP = 105, DX_DCL_GLOBAL_FLAGS = 106,
   DX_GATHER4 = 109, DX_DERIV_RTX_COARSE = 122, DX_DERIV_RTX_FINE = 123,
   DX_DERIV_RTY_COARSE = 124, DX_DERIV_RTY_FINE = 125, DX_F32TOF16 = 130,
   DX_F16TOF32 = 131, DX_UADDC = 132, DX_USUBB = 133, DX_COUNTBITS = 134,
   DX_FIRSTBIT_HI = 135, DX_FIRSTBIT_LO = 136, DX_FIRSTBIT_SHI = 137, DX_UBFE = 138,
   DX_IBFE = 139, DX_BFI = 140, DX_BFREV = 141,
   DX_DCL_THREAD_GROUP = 155, DX_DCL_UAV_TYPED = 156, DX_DCL_UAV_RAW = 157,
   DX_DCL_TGSM_RAW = 159, DX_DCL_RESOURCE_RAW = 161, DX_LD_UAV_TYPED = 163,
   DX_STORE_UAV_TYPED = 164, DX_LD_RAW = 165, DX_STORE_RAW = 166,
   DX_ATOMIC_AND = 169, DX_ATOMIC_OR = 170, DX_ATOMIC_XOR = 171, DX_ATOMIC_CMP_STORE = 172,
   DX_ATOMIC_IADD = 173, DX_ATOMIC_IMAX = 174, DX_ATOMIC_IMIN = 175, DX_ATOMIC_UMAX = 176,
   DX_ATOMIC_UMIN = 177, DX_IMM_ATOMIC_IADD = 180, DX_IMM_ATOMIC_AND = 181,
   DX_IMM_ATOMIC_OR = 182, DX_IMM_ATOMIC_XOR = 183, DX_IMM_ATOMIC_EXCH = 184,
   DX_IMM_ATOMIC_CMP_EXCH = 185, DX_IMM_ATOMIC_IMAX = 186, DX_IMM_ATOMIC_IMIN = 187,
   DX_IMM_ATOMIC_UMAX = 188, DX_IMM_ATOMIC_UMIN = 189, DX_SYNC = 190,
   DX_LOD = 108, DX_BUFINFO = 121, DX_SAMPLE_C = 70,
   DX_EMIT_STREAM = 117, DX_CUT_STREAM = 118, DX_DCL_STREAM = 143, DX_DCL_GS_INSTANCES = 206
};

#define DX_RESINFO_UINT (2u << 11)

/* Operand types. */
#define DXO_TEMP       0
#define DXO_INPUT      1
#define DXO_OUTPUT     2
#define DXO_XTEMP      3
#define DXO_IMM32      4
#define DXO_SAMPLER    6
#define DXO_RESOURCE   7
#define DXO_CB         8
#define DXO_ODEPTH     12
#define DXO_NULL       13
#define DXO_UAV        30
#define DXO_TGSM       31
#define DXO_THREADID   32
#define DXO_GROUPID    33
#define DXO_THREADIDINGROUP 34
#define DXO_THREADIDINGROUPFLAT 36
#define DXO_STREAM     16
#define DXO_GSINSTANCEID 37

/* System values of signatures and declarations. */
#define DX_SV_NONE        0
#define DX_SV_POSITION    1
#define DX_SV_CLIP        2
#define DX_SV_CULL        3
#define DX_SV_VERTEXID    6
#define DX_SV_INSTANCEID  8
#define DX_SV_FRONTFACE   9
#define DX_SV_SAMPLEINDEX 10
#define DX_SV_RTINDEX     4

/* Signature component types. */
#define DX_CT_UINT  1
#define DX_CT_SINT  2
#define DX_CT_FLOAT 3

/* Interpolation of a pixel stage input. */
#define DX_INTERP_CONSTANT              1
#define DX_INTERP_LINEAR                2
#define DX_INTERP_LINEAR_CENTROID       3
#define DX_INTERP_NOPERSPECTIVE         4
#define DX_INTERP_NOPERSPECTIVE_CENTROID 5
#define DX_INTERP_LINEAR_SAMPLE         6
#define DX_INTERP_NOPERSPECTIVE_SAMPLE  7

/* Resource dimensions. */
#define DX_DIM_BUFFER     1
#define DX_DIM_TEX2D      3
#define DX_DIM_TEX2DMS    4
#define DX_DIM_TEX2DARRAY 8
#define DX_DIM_TEX2DMSARRAY 9
#define DX_DIM_RAW        11

/* The instruction flag a conditional takes when it tests non-zero. */
#define DX_TEST_NZ (1u << 18)
#define DX_SATURATE (1u << 13)

/* ------------------------------------------------------------------ */
/* Values                                                              */
/* ------------------------------------------------------------------ */

/* An operand: a register file, its index and the component each value
 * component reads (or, as a destination, writes). */
struct dx_opnd
{
   uint32_t idx[2];   /* register; a cb or an xtemp: [slot or array, register] */
   uint32_t imm[4];   /* DXO_IMM32 */
   uint32_t rel;      /* a temporary that adds to idx[last]; RSLC_UNSET when none */
   uint8_t  file;     /* DXO_* */
   uint8_t  n;        /* components, 1 to 4 */
   uint8_t  swz[4];   /* the register component of each value component */
   uint8_t  mod;      /* 1 negate, 2 absolute value, 3 both */
   uint8_t  rel_comp;
   uint8_t  twod;     /* a geometry stage's input: [vertex][register] */
   uint8_t  pad;
};

/* Where an aggregate (or any variable) lives: its first register and
 * the file; an xtemp's array and a dynamic register offset. */
struct dx_place
{
   uint32_t reg;      /* first register, or first element of the xtemp array */
   uint32_t arr;      /* the xtemp array; the cb slot of a block */
   uint32_t rel;      /* a temporary adding to reg; RSLC_UNSET when none */
   uint8_t  file;     /* DXO_TEMP, DXO_XTEMP, DXO_CB, DXO_INPUT */
   uint8_t  rel_comp;
   uint8_t  comp;     /* a cb member's first component in its register */
   uint8_t  pad;
};

/* What an expression evaluated to: a vector operand, or registers
 * holding an aggregate. */
struct dx_val
{
   struct dx_opnd  v;
   struct dx_place p;
   bool            agg;
};

/* An interface element: what the signature says and where it lives. */
struct dx_sig
{
   const char *name;
   uint32_t    index;   /* semantic index */
   uint32_t    reg;     /* RSLC_UNSET: oDepth */
   uint8_t     sv;      /* DX_SV_* */
   uint8_t     ct;      /* DX_CT_* */
   uint8_t     mask;
   uint8_t     rw;      /* inputs: read; outputs: never written */
   uint8_t     interp;  /* pixel inputs: DX_INTERP_* */
   uint8_t     pad[3];
};

#define DX_MAX_SIG   64
#define DX_MAX_XTEMP 64
#define DX_MAX_LOOPS 64
#define DX_MAX_TGSM  16

/* A loop or switch being emitted: what a continue in it must run. */
struct dx_ctl
{
   uint32_t iter;      /* for: the iteration expression */
   uint32_t cond;      /* do: the condition, tested on continue */
   bool     is_loop;
   bool     is_do;
};

/* The per-variable state. */
struct dx_var
{
   struct dx_place home;
   bool            has_home;
   bool            dynamic;   /* indexed with a non-constant somewhere */
   bool            folded;    /* a constant: no storage */
};

/* A call being expanded in place, as fxc does: DXBC subroutines are
 * not taken everywhere (Wine's translator refuses call). A
 * return that is not the body's last statement leaves through a loop
 * wrapped around the body; one from inside a loop or switch of the
 * body sets the flag and breaks out, and each enclosing loop and switch
 * breaks again on the flag. */
#define DX_MAX_TEXPARAMS 8
#define DX_MAX_INLINE    32
struct dx_inline
{
   const struct rslc_func *f;
   uint32_t                ret;    /* the result's first register */
   uint32_t                flag;   /* RSLC_UNSET when no return leaves from deeper */
   uint32_t                nctl;   /* loops and switches open in its wrapper */
   bool                    wrapped;
};

struct dx_emit
{
   struct rslc_ctx  *c;
   struct rslc_pbuf  code;     /* the program's instructions */
   struct rslc_pbuf *out;      /* the one being written */
   struct dx_var    *vars;
   /* the calls being expanded, innermost last */
   struct dx_inline  inl[DX_MAX_INLINE];
   uint32_t          ninl;
   uint32_t         *tex_bind;  /* a sampler parameter's texture, by variable */
   uint32_t          xtemp_len[DX_MAX_XTEMP];
   struct dx_sig     isig[DX_MAX_SIG];
   struct dx_sig     osig[DX_MAX_SIG];
   struct dx_ctl     ctl[DX_MAX_LOOPS];
   /* a pixel stage's output, per signature element: its temporary */
   uint32_t          out_home[DX_MAX_SIG];
   uint32_t          nisig, nosig;
   uint32_t          ntemp;      /* the next free temporary */
   uint32_t          max_temp;   /* one past the highest used */
   uint32_t          nxtemp;
   uint32_t          nctl;
   uint32_t          sm;         /* 40, 41, 50 */
   /* outputs: per signature element, the register its value is kept in
    * until the stage ends; a clip or cull element gathers one register
    * per distance */
   struct dx_place   oc_home[DX_MAX_SIG];
   uint8_t           oc_n[DX_MAX_SIG];      /* components */
   uint8_t           oc_dist[DX_MAX_SIG];   /* distances gathered, 0 if none */
   uint8_t           oc_comp[DX_MAX_SIG];   /* the first component in its register */
   /* inputs copied to temporaries where main begins: an in block's
    * members, which its path reaches by register */
   struct dx_place   ic_home[DX_MAX_SIG * 6];
   uint32_t          ic_off[DX_MAX_SIG * 6];    /* registers past the home */
   uint32_t          ic_vert[DX_MAX_SIG * 6];   /* a geometry stage's vertex, or RSLC_UNSET */
   uint32_t          ic_reg[DX_MAX_SIG * 6];
   uint8_t           ic_comp[DX_MAX_SIG * 6];
   uint8_t           ic_n[DX_MAX_SIG * 6];
   uint32_t          nic;
   uint32_t          gs_verts;   /* vertices a geometry stage's primitive hands it */
   uint8_t          *reach;     /* functions main reaches */
   uint8_t          *used;      /* variables reachable code names */
   bool              cb_dynamic[RSLC_MAX_BLOCKS];
   bool              cb_read[RSLC_MAX_BLOCKS];   /* named by the code emitted */
   uint32_t          tgsm_size[DX_MAX_TGSM];   /* bytes */
   uint32_t          ntgsm;
   uint32_t          cb_size[RSLC_MAX_BLOCKS];   /* registers */
   bool              early_z;
};

/* ------------------------------------------------------------------ */
/* Writing                                                             */
/* ------------------------------------------------------------------ */

static bool dx_w(struct dx_emit *e, uint32_t w)
{
   return rslc_pbuf_put(e->c, e->out, &w, 4);
}

static bool dx_fail(struct dx_emit *e, const char *what, uint32_t line)
{
   if (!e->c->failed)
      rslc_err(e->c, "dxbc: ", what, line);
   return false;
}

/* A register operand. */
static struct dx_opnd dx_reg(uint8_t file, uint32_t idx, unsigned n)
{
   struct dx_opnd o;
   unsigned i;
   memset(&o, 0, sizeof(o));
   o.file   = file;
   o.idx[0] = idx;
   o.n      = (uint8_t)n;
   o.rel    = RSLC_UNSET;
   for (i = 0; i < 4; i++)
      o.swz[i] = (uint8_t)(i < n || !n ? i : n - 1);
   return o;
}

/* An immediate of n components. */
static struct dx_opnd dx_imm(const uint32_t *v, unsigned n)
{
   struct dx_opnd o = dx_reg(DXO_IMM32, 0, n);
   unsigned i;
   for (i = 0; i < 4; i++)
      o.imm[i] = v[i < n ? i : n - 1];
   return o;
}

static struct dx_opnd dx_immu(uint32_t v)
{
   return dx_imm(&v, 1);
}

static struct dx_opnd dx_immf(float f)
{
   uint32_t u;
   memcpy(&u, &f, 4);
   return dx_imm(&u, 1);
}

/* Component k of a value, as a scalar. */
static struct dx_opnd dx_comp(struct dx_opnd o, unsigned k)
{
   if (o.file == DXO_IMM32)
   {
      o.imm[0] = o.imm[k];
      o.n = 1;
      return o;
   }
   o.swz[0] = o.swz[k];
   o.n = 1;
   return o;
}

/* A scalar widened to n components. */
static struct dx_opnd dx_splat(struct dx_opnd o, unsigned n)
{
   unsigned i;
   for (i = 1; i < 4; i++)
   {
      o.swz[i] = o.swz[0];
      o.imm[i] = o.imm[0];
   }
   o.n = (uint8_t)n;
   return o;
}

/* The operand's first token and the index tokens after it.
 * A destination writes a mask of its components; a source reads a
 * swizzle, or one component; an immediate is its values. */
static bool dx_put_opnd(struct dx_emit *e, const struct dx_opnd *o, bool dst, unsigned dst_n)
{
   uint32_t tok = (uint32_t)o->file << 12;
   unsigned dims = 0;
   unsigned i;

   if (o->file == DXO_IMM32)
   {
      tok |= o->n == 1 ? 1u : 2u;
      if (o->mod)
      {
         /* immediates take no modifier: fold the sign or magnitude in
          * as the value's own bits, floats only (ints negate below) */
         return dx_fail(e, "internal: modified immediate", 0);
      }
      if (!dx_w(e, tok))
         return false;
      if (o->n == 1)
         return dx_w(e, o->imm[0]);
      for (i = 0; i < 4; i++)
         if (!dx_w(e, o->imm[i < o->n ? i : o->n - 1u]))
            return false;
      return true;
   }

   switch (o->file)
   {
      case DXO_NULL:
      case DXO_ODEPTH:
         dims = 0;
         break;
      case DXO_CB:
      case DXO_XTEMP:
         dims = 2;
         break;
      case DXO_THREADID:
      case DXO_GROUPID:
      case DXO_THREADIDINGROUP:
      case DXO_THREADIDINGROUPFLAT:
      case DXO_GSINSTANCEID:
         dims = 0;
         break;
      default:
         dims = o->twod ? 2 : 1;
         break;
   }

   if (o->file == DXO_NULL || o->n == 0 || o->file == DXO_STREAM)
      tok |= 0;   /* no components: a null destination, a declared register */
   else if (o->file == DXO_ODEPTH || o->file == DXO_THREADIDINGROUPFLAT)
      tok |= 1;   /* one component */
   else if (o->file == DXO_SAMPLER && o->n != 4)
      tok |= 0;
   else if (o->file == DXO_SAMPLER)
      tok |= 2u | (2u << 2) | ((uint32_t)o->swz[0] << 4);   /* gather's component */
   else
   {
      tok |= 2;   /* four components */
      if (dst)
      {
         uint32_t mask = 0;
         unsigned n = dst_n ? dst_n : o->n;
         for (i = 0; i < n; i++)
            mask |= 1u << o->swz[i];
         tok |= mask << 4;   /* mask mode 0 */
      }
      else if (o->n == 1)
         tok |= (2u << 2) | ((uint32_t)o->swz[0] << 4);   /* select one */
      else
      {
         uint32_t s = 0;
         for (i = 0; i < 4; i++)
            s |= (uint32_t)o->swz[i < o->n ? i : o->n - 1u] << (2 * i);
         tok |= (1u << 2) | (s << 4);   /* swizzle */
      }
   }
   tok |= (uint32_t)dims << 20;
   if (dims)
   {
      unsigned last = dims - 1;
      for (i = 0; i < dims; i++)
         if (i == last && o->rel != RSLC_UNSET)
            tok |= 3u << (22 + 3 * i);   /* immediate plus relative */
   }
   if (o->mod)
      tok |= 1u << 31;
   if (!dx_w(e, tok))
      return false;
   if (o->mod && !dx_w(e, 1u | ((uint32_t)o->mod << 6)))
      return false;
   for (i = 0; i < dims; i++)
   {
      if (!dx_w(e, o->idx[i]))
         return false;
      if (i == dims - 1 && o->rel != RSLC_UNSET)
      {
         /* the relative part: a temporary's one component */
         uint32_t rt = ((uint32_t)DXO_TEMP << 12) | 2u | (2u << 2)
            | ((uint32_t)o->rel_comp << 4) | (1u << 20);
         if (!dx_w(e, rt) || !dx_w(e, o->rel))
            return false;
      }
   }
   return true;
}

/* The length in tokens of an operand as dx_put_opnd writes it. */
static unsigned dx_opnd_len(const struct dx_opnd *o)
{
   unsigned n = 1;
   if (o->file == DXO_IMM32)
      return 1 + (o->n == 1 ? 1 : 4);
   if (o->mod)
      n++;
   switch (o->file)
   {
      case DXO_NULL: case DXO_ODEPTH: case DXO_THREADID: case DXO_GROUPID:
      case DXO_THREADIDINGROUP: case DXO_THREADIDINGROUPFLAT: case DXO_GSINSTANCEID:
         return n;
      case DXO_CB: case DXO_XTEMP:
         n += 2;
         break;
      case DXO_INPUT:
         n += o->twod ? 2 : 1;
         break;
      default:
         n += 1;
         break;
   }
   if (o->rel != RSLC_UNSET)
      n += 2;
   return n;
}

/* One instruction: opcode, destinations, then sources.  flags are
 * ORed into the opcode token. */
static bool dx_inst(struct dx_emit *e, enum dx_op op, uint32_t flags,
      const struct dx_opnd *dst, unsigned ndst, const struct dx_opnd *src, unsigned nsrc)
{
   unsigned len = 1, i;
   for (i = 0; i < ndst; i++)
      len += dx_opnd_len(&dst[i]);
   for (i = 0; i < nsrc; i++)
      len += dx_opnd_len(&src[i]);
   if (!dx_w(e, (uint32_t)op | flags | ((uint32_t)len << 24)))
      return false;
   for (i = 0; i < ndst; i++)
      if (!dx_put_opnd(e, &dst[i], true, 0))
         return false;
   for (i = 0; i < nsrc; i++)
      if (!dx_put_opnd(e, &src[i], false, 0))
         return false;
   return true;
}

/* An instruction with one extended opcode token (a sample's immediate
 * offset). */
static bool dx_inst_ext(struct dx_emit *e, enum dx_op op, uint32_t flags, uint32_t ext,
      const struct dx_opnd *dst, unsigned ndst, const struct dx_opnd *src, unsigned nsrc)
{
   unsigned len = 2, i;
   if (!ext)
      return dx_inst(e, op, flags, dst, ndst, src, nsrc);
   for (i = 0; i < ndst; i++)
      len += dx_opnd_len(&dst[i]);
   for (i = 0; i < nsrc; i++)
      len += dx_opnd_len(&src[i]);
   if (!dx_w(e, (uint32_t)op | flags | ((uint32_t)len << 24) | (1u << 31)) || !dx_w(e, ext))
      return false;
   for (i = 0; i < ndst; i++)
      if (!dx_put_opnd(e, &dst[i], true, 0))
         return false;
   for (i = 0; i < nsrc; i++)
      if (!dx_put_opnd(e, &src[i], false, 0))
         return false;
   return true;
}

static bool dx_op1(struct dx_emit *e, enum dx_op op, struct dx_opnd d, struct dx_opnd a)
{
   return dx_inst(e, op, 0, &d, 1, &a, 1);
}

static bool dx_op2(struct dx_emit *e, enum dx_op op, struct dx_opnd d, struct dx_opnd a,
      struct dx_opnd b)
{
   struct dx_opnd s[2];
   s[0] = a; s[1] = b;
   return dx_inst(e, op, 0, &d, 1, s, 2);
}

static bool dx_op3(struct dx_emit *e, enum dx_op op, struct dx_opnd d, struct dx_opnd a,
      struct dx_opnd b, struct dx_opnd c)
{
   struct dx_opnd s[3];
   s[0] = a; s[1] = b; s[2] = c;
   return dx_inst(e, op, 0, &d, 1, s, 3);
}

static bool dx_op2f(struct dx_emit *e, enum dx_op op, uint32_t flags, struct dx_opnd d,
      struct dx_opnd a, struct dx_opnd b)
{
   struct dx_opnd s[2];
   s[0] = a; s[1] = b;
   return dx_inst(e, op, flags, &d, 1, s, 2);
}

static bool dx_op4(struct dx_emit *e, enum dx_op op, struct dx_opnd d, struct dx_opnd a,
      struct dx_opnd b, struct dx_opnd c, struct dx_opnd x)
{
   struct dx_opnd s[4];
   s[0] = a; s[1] = b; s[2] = c; s[3] = x;
   return dx_inst(e, op, 0, &d, 1, s, 4);
}

/* An instruction with no operands, or only a token of flags. */
static bool dx_op0(struct dx_emit *e, enum dx_op op, uint32_t flags)
{
   return dx_w(e, (uint32_t)op | flags | (1u << 24));
}

/* A conditional on one component. */
static bool dx_cond(struct dx_emit *e, enum dx_op op, bool nz, struct dx_opnd c)
{
   return dx_inst(e, op, nz ? DX_TEST_NZ : 0, NULL, 0, &c, 1);
}

/* ------------------------------------------------------------------ */
/* Registers                                                           */
/* ------------------------------------------------------------------ */

static uint32_t dx_temp(struct dx_emit *e, uint32_t n)
{
   uint32_t r = e->ntemp;
   e->ntemp += n;
   if (e->ntemp > e->max_temp)
      e->max_temp = e->ntemp;
   return r;
}

/* A fresh temporary of n components, as a destination and a source. */
static struct dx_opnd dx_tmp(struct dx_emit *e, unsigned n)
{
   return dx_reg(DXO_TEMP, dx_temp(e, 1), n);
}

/* Registers a type takes. */
static uint32_t dx_slots(const struct rslc_type *t)
{
   uint32_t i, n = 0;
   switch (t->kind)
   {
      case RSLC_TY_MAT:
         return t->cols;
      case RSLC_TY_ARRAY:
         return t->array_len * dx_slots(t->elem);
      case RSLC_TY_STRUCT:
         for (i = 0; i < t->nfields; i++)
            n += dx_slots(t->fields[i].type);
         return n;
      default:
         return 1;
   }
}

/* The register a field starts at within its struct. */
static uint32_t dx_field_slot(const struct rslc_type *t, uint32_t field)
{
   uint32_t i, n = 0;
   for (i = 0; i < field; i++)
      n += dx_slots(t->fields[i].type);
   return n;
}

/* Registers of n components from a place, at register offset k. */
static struct dx_opnd dx_at(const struct dx_place *p, uint32_t k, unsigned n)
{
   struct dx_opnd o;
   if (p->file == DXO_XTEMP || p->file == DXO_CB)
   {
      o = dx_reg(p->file, p->arr, n);
      o.idx[1] = p->reg + k;
   }
   else
      o = dx_reg(p->file, p->reg + k, n);
   o.rel      = p->rel;
   o.rel_comp = p->rel_comp;
   if (p->comp)
   {
      unsigned i;
      for (i = 0; i < 4; i++)
         o.swz[i] = (uint8_t)(p->comp + (i < n ? i : n - 1));
   }
   return o;
}

static struct dx_place dx_place_of(uint8_t file, uint32_t reg)
{
   struct dx_place p;
   memset(&p, 0, sizeof(p));
   p.file = file;
   p.reg  = reg;
   p.rel  = RSLC_UNSET;
   return p;
}

/* Copies a vector into a destination of the same width. */
static bool dx_mov(struct dx_emit *e, struct dx_opnd d, struct dx_opnd s)
{
   return dx_op1(e, DX_MOV, d, s);
}

/* ------------------------------------------------------------------ */
/* Container                                                           */
/* ------------------------------------------------------------------ */

/* The DXBC checksum: MD5's rounds over the container past its hash,
 * with the length in bits put first in the final block. */
static uint32_t dx_rol(uint32_t x, unsigned c)
{
   return (x << c) | (x >> (32 - c));
}

static void dx_md5_block(uint32_t st[4], const uint8_t *blk)
{
   static const uint8_t s[64] = {
      7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
      5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20,
      4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
      6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21 };
   static const uint32_t k[64] = {
      0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a,
      0xa8304613, 0xfd469501, 0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
      0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821, 0xf61e2562, 0xc040b340,
      0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
      0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8,
      0x676f02d9, 0x8d2a4c8a, 0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
      0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70, 0x289b7ec6, 0xeaa127fa,
      0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
      0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92,
      0xffeff47d, 0x85845dd1, 0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
      0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391 };
   uint32_t m[16];
   uint32_t a = st[0], b = st[1], c = st[2], d = st[3];
   unsigned i;
   for (i = 0; i < 16; i++)
      m[i] = (uint32_t)blk[4 * i] | ((uint32_t)blk[4 * i + 1] << 8)
         | ((uint32_t)blk[4 * i + 2] << 16) | ((uint32_t)blk[4 * i + 3] << 24);
   for (i = 0; i < 64; i++)
   {
      uint32_t f, g;
      if (i < 16)
      {
         f = (b & c) | (~b & d);
         g = i;
      }
      else if (i < 32)
      {
         f = (d & b) | (~d & c);
         g = (5 * i + 1) & 15;
      }
      else if (i < 48)
      {
         f = b ^ c ^ d;
         g = (3 * i + 5) & 15;
      }
      else
      {
         f = c ^ (b | ~d);
         g = (7 * i) & 15;
      }
      f = f + a + k[i] + m[g];
      a = d;
      d = c;
      c = b;
      b = b + dx_rol(f, s[i]);
   }
   st[0] += a; st[1] += b; st[2] += c; st[3] += d;
}

static void dx_put32(uint8_t *p, uint32_t v)
{
   p[0] = (uint8_t)v;
   p[1] = (uint8_t)(v >> 8);
   p[2] = (uint8_t)(v >> 16);
   p[3] = (uint8_t)(v >> 24);
}

static void dx_checksum(uint8_t *dxbc, uint32_t size)
{
   const uint8_t *d = dxbc + 20;
   uint32_t n    = size - 20;
   uint32_t bits = n * 8;
   uint32_t full = n & ~63u;
   uint32_t left = n - full;
   uint32_t st[4];
   uint8_t  blk[64];
   uint32_t i;
   st[0] = 0x67452301; st[1] = 0xefcdab89; st[2] = 0x98badcfe; st[3] = 0x10325476;
   for (i = 0; i < full; i += 64)
      dx_md5_block(st, d + i);
   memset(blk, 0, sizeof(blk));
   if (left >= 56)
   {
      memcpy(blk, d + full, left);
      blk[left] = 0x80;
      dx_md5_block(st, blk);
      memset(blk, 0, sizeof(blk));
      dx_put32(blk, bits);
      dx_put32(blk + 60, (bits >> 2) | 1);
      dx_md5_block(st, blk);
   }
   else
   {
      dx_put32(blk, bits);
      memcpy(blk + 4, d + full, left);
      blk[4 + left] = 0x80;
      dx_put32(blk + 60, (bits >> 2) | 1);
      dx_md5_block(st, blk);
   }
   for (i = 0; i < 4; i++)
      dx_put32(dxbc + 4 + 4 * i, st[i]);
}

/* A signature chunk's body: the elements, then their names. */
static bool dx_sig_chunk(struct dx_emit *e, struct rslc_pbuf *b, const struct dx_sig *s,
      uint32_t n, bool stream)
{
   uint32_t es = stream ? 28u : 24u;
   uint32_t i, name_off = 8 + es * n;
   uint32_t hdr[2];
   hdr[0] = n;
   hdr[1] = 8;
   if (!rslc_pbuf_put(e->c, b, hdr, 8))
      return false;
   for (i = 0; i < n; i++)
   {
      uint32_t el[6];
      uint32_t k;
      /* an element naming the same semantic as an earlier one shares
       * its string */
      uint32_t off = name_off;
      for (k = 0; k < i; k++)
      {
         if (!strcmp(s[k].name, s[i].name))
            break;
      }
      if (k < i)
      {
         uint32_t j;
         off = 8 + es * n;
         for (j = 0; j < k; j++)
         {
            uint32_t m;
            for (m = 0; m < j; m++)
               if (!strcmp(s[m].name, s[j].name))
                  break;
            if (m == j)
               off += (uint32_t)((strlen(s[j].name) + 4) & ~3u);
         }
      }
      else
         name_off += (uint32_t)((strlen(s[i].name) + 4) & ~3u);
      el[0] = off;
      el[1] = s[i].index;
      el[2] = s[i].sv;
      el[3] = s[i].ct;
      el[4] = s[i].reg;
      el[5] = (uint32_t)s[i].mask | ((uint32_t)s[i].rw << 8);
      if (stream)
      {
         static const uint32_t zero = 0;
         if (!rslc_pbuf_put(e->c, b, &zero, 4))
            return false;
      }
      if (!rslc_pbuf_put(e->c, b, el, 24))
         return false;
   }
   for (i = 0; i < n; i++)
   {
      uint32_t k;
      size_t   len;
      static const uint8_t zero[4] = { 0, 0, 0, 0 };
      for (k = 0; k < i; k++)
         if (!strcmp(s[k].name, s[i].name))
            break;
      if (k < i)
         continue;
      len = strlen(s[i].name);
      if (!rslc_pbuf_put(e->c, b, s[i].name, len)
            || !rslc_pbuf_put(e->c, b, zero, ((len + 4) & ~(size_t)3) - len))
         return false;
   }
   return true;
}

/* ------------------------------------------------------------------ */
/* Analysis                                                            */
/* ------------------------------------------------------------------ */

/* The variable an lvalue or aggregate expression is rooted at, or 0. */
static uint32_t dx_root_var(struct dx_emit *e, uint32_t node)
{
   for (;;)
   {
      const struct rslc_node *n = rslc_node_at(e->c, node);
      switch (n->op)
      {
         case RSLC_OP_VAR:
            return n->a;
         case RSLC_OP_MEMBER: case RSLC_OP_INDEX: case RSLC_OP_SWIZZLE:
            node = n->a;
            break;
         default:
            return 0;
      }
   }
}

static bool dx_is_const(struct dx_emit *e, uint32_t node)
{
   struct rslc_cval cv;
   const struct rslc_node *n = rslc_node_at(e->c, node);
   if (!n->type || (n->type->kind != RSLC_TY_SCALAR && n->type->kind != RSLC_TY_VEC
            && n->type->kind != RSLC_TY_MAT))
      return false;
   return rslc_const_eval(e->c, node, &cv);
}

/* Marks the variables a non-constant index reaches into: they live in
 * indexable temporaries. */
static void dx_scan(struct dx_emit *e, uint32_t node)
{
   const struct rslc_node *n;
   uint32_t i, cnt;
   if (!node)
      return;
   n = rslc_node_at(e->c, node);
   switch (n->op)
   {
      case RSLC_OP_CONST: case RSLC_OP_VAR: case RSLC_OP_BREAK: case RSLC_OP_CONTINUE:
      case RSLC_OP_DISCARD: case RSLC_OP_NONE:
         return;
      case RSLC_OP_DECL:
         dx_scan(e, e->c->prog.vars[n->a]->init);
         return;
      case RSLC_OP_INDEX:
         if (!dx_is_const(e, n->b))
         {
            uint32_t v = dx_root_var(e, n->a);
            if (v)
               e->vars[v].dynamic = true;
         }
         dx_scan(e, n->a);
         dx_scan(e, n->b);
         return;
      case RSLC_OP_CALL: case RSLC_OP_BUILTIN: case RSLC_OP_CONSTRUCT: case RSLC_OP_BLOCK:
      case RSLC_OP_SWITCH:
         cnt = n->b ? rslc_list_len(e->c, n->b) : 0;
         for (i = 0; i < cnt; i++)
            dx_scan(e, rslc_list_item(e->c, n->b, i));
         if (n->op == RSLC_OP_SWITCH)
            dx_scan(e, n->a);
         return;
      case RSLC_OP_FOR:
         cnt = rslc_list_len(e->c, n->b);
         for (i = 0; i < cnt; i++)
            dx_scan(e, rslc_list_item(e->c, n->b, i));
         return;
      default:
         dx_scan(e, n->a);
         dx_scan(e, n->b);
         dx_scan(e, n->c);
         return;
   }
}

/* ------------------------------------------------------------------ */
/* Blocks                                                              */
/* ------------------------------------------------------------------ */

/* The cb slot of a uniform block, as the HLSL printer registers it. */
static uint32_t dx_cb_slot(struct dx_emit *e, unsigned idx)
{
   const struct rslc_blockdecl *b = &e->c->prog.blocks[idx];
   if (!b->push && (idx >= 2 || b->binding != RSLC_UNSET))
      return b->binding == RSLC_UNSET ? 0 : b->binding;
   return b->push ? 1 : 0;
}

static bool dx_block_std140(struct dx_emit *e, unsigned idx)
{
   const struct rslc_blockdecl *b = &e->c->prog.blocks[idx];
   if (b->buffer)
      return b->std140;
   return b->push ? b->std140 : true;
}

/* ------------------------------------------------------------------ */
/* Interface                                                           */
/* ------------------------------------------------------------------ */

static uint8_t dx_ct_of(const struct rslc_type *t)
{
   while (t->kind == RSLC_TY_ARRAY)
      t = t->elem;
   return t->base == RSLC_BASE_INT ? DX_CT_SINT
      : t->base == RSLC_BASE_UINT || t->base == RSLC_BASE_BOOL ? DX_CT_UINT : DX_CT_FLOAT;
}

/* The components a register of the type holds. */
static unsigned dx_width(const struct rslc_type *t)
{
   while (t->kind == RSLC_TY_ARRAY)
      t = t->elem;
   if (t->kind == RSLC_TY_MAT)
      return t->rows;
   if (t->kind == RSLC_TY_VEC)
      return t->rows;
   return 1;
}

static uint8_t dx_interp(const struct rslc_var *v, uint8_t interp, uint8_t quals,
      const struct rslc_type *t)
{
   bool np = interp == RSLC_INTERP_NOPERSPECTIVE;
   while (t->kind == RSLC_TY_ARRAY)
      t = t->elem;
   (void)v;
   if (interp == RSLC_INTERP_FLAT || t->base == RSLC_BASE_INT || t->base == RSLC_BASE_UINT
         || t->base == RSLC_BASE_BOOL)
      return DX_INTERP_CONSTANT;
   if (quals & RSLC_VQ_SAMPLE)
      return np ? DX_INTERP_NOPERSPECTIVE_SAMPLE : DX_INTERP_LINEAR_SAMPLE;
   if (quals & RSLC_VQ_CENTROID)
      return np ? DX_INTERP_NOPERSPECTIVE_CENTROID : DX_INTERP_LINEAR_CENTROID;
   return np ? DX_INTERP_NOPERSPECTIVE : DX_INTERP_LINEAR;
}

/* Signature registers as fxc packs a stage's varyings: each element
 * goes into the register of its interpolation group that has the most
 * components left after its last used one and room for the element,
 * the lowest such register on a tie, or into a new register; elements
 * of more than one register, and system values, take registers of
 * their own (a pixel stage's integer system values share theirs). */
struct dx_packer
{
   uint8_t  end[DX_MAX_SIG];     /* one past the last used component */
   uint8_t  group[DX_MAX_SIG];
   uint32_t nreg;
};

#define DX_GROUP_ALONE 0xff
#define DX_GROUP_SGV   0xfe

static bool dx_pack(struct dx_emit *e, struct dx_packer *pk, unsigned n, unsigned rows,
      uint8_t group, uint32_t *reg, uint8_t *comp)
{
   uint32_t r, best = RSLC_UNSET;
   /* an element of several registers opens registers, which later
    * elements of its group may share */
   if (rows == 1 && group != DX_GROUP_ALONE)
      for (r = 0; r < pk->nreg; r++)
      {
         unsigned room = 4u - pk->end[r];
         if (pk->group[r] != group || room < n)
            continue;
         if (best == RSLC_UNSET || room > 4u - pk->end[best])
            best = r;
      }
   if (best != RSLC_UNSET)
   {
      *reg  = best;
      *comp = pk->end[best];
      pk->end[best] = (uint8_t)(pk->end[best] + n);
      return true;
   }
   if (pk->nreg + rows > DX_MAX_SIG)
      return dx_fail(e, "too many interface registers", 0);
   *reg  = pk->nreg;
   *comp = 0;
   for (r = 0; r < rows; r++)
   {
      pk->end[pk->nreg]   = (uint8_t)n;
      pk->group[pk->nreg] = group;
      pk->nreg++;
   }
   return true;
}

/* One signature element. */
static struct dx_sig *dx_sig_add(struct dx_emit *e, bool in, const char *name, uint32_t index,
      uint32_t reg, uint8_t comp, uint8_t sv, uint8_t ct, unsigned n, uint8_t interp)
{
   struct dx_sig *s;
   if ((in ? e->nisig : e->nosig) >= DX_MAX_SIG)
   {
      dx_fail(e, "too many interface elements", 0);
      return NULL;
   }
   s = in ? &e->isig[e->nisig++] : &e->osig[e->nosig++];
   memset(s, 0, sizeof(*s));
   s->name   = name;
   s->index  = index;
   s->reg    = reg;
   s->sv     = sv;
   s->ct     = ct;
   s->mask   = (uint8_t)(((1u << n) - 1) << comp);
   /* an input's components read; an output's never written, which fxc
    * counts as those outside its mask */
   s->rw     = in ? s->mask : (uint8_t)(~s->mask & 0xfu);
   s->interp = interp;
   return s;
}

/* A permanent home of n registers, in an indexable temporary when the
 * variable is indexed with a value that is not constant. */
static struct dx_place dx_home(struct dx_emit *e, uint32_t var, uint32_t n)
{
   struct dx_var *dv = &e->vars[var];
   if (dv->dynamic)
   {
      struct dx_place p = dx_place_of(DXO_XTEMP, 0);
      if (e->nxtemp == DX_MAX_XTEMP)
      {
         dx_fail(e, "too many indexed variables", 0);
         p.file = DXO_TEMP;
         return p;
      }
      p.arr = e->nxtemp;
      e->xtemp_len[e->nxtemp++] = n;
      return p;
   }
   return dx_place_of(DXO_TEMP, dx_temp(e, n));
}

static const char dx_texcoord[] = "TEXCOORD";
static const char dx_target[]   = "SV_Target";

/* An output element: the signature entry and the temporary it is kept
 * in until the stage ends. */
static bool dx_out_add(struct dx_emit *e, const char *name, uint32_t index, uint32_t reg,
      uint8_t comp, uint8_t sv, const struct rslc_type *t, unsigned n, struct dx_place home,
      uint8_t dist)
{
   uint32_t k = e->nosig;
   if (!dx_sig_add(e, false, name, index, reg, comp, sv, dx_ct_of(t), n, 0))
      return false;
   e->oc_home[k] = home;
   e->oc_n[k]    = (uint8_t)n;
   e->oc_comp[k] = comp;
   e->oc_dist[k] = dist;
   return true;
}

/* The input elements of a variable or io block member: rows registers
 * of n components. */
static bool dx_in_var(struct dx_emit *e, struct dx_packer *pk, bool pack, uint32_t loc,
      const struct rslc_type *t, uint8_t interp, struct dx_place *home)
{
   uint32_t rows = dx_slots(t), reg, k;
   unsigned n = dx_width(t);
   uint8_t comp;
   if (!pack)
   {
      reg  = pk->nreg;
      comp = 0;
      pk->nreg += rows;
   }
   else if (!dx_pack(e, pk, n, rows, interp, &reg, &comp))
      return false;
   *home = dx_place_of(DXO_INPUT, reg);
   home->comp = comp;
   for (k = 0; k < rows; k++)
      if (!dx_sig_add(e, true, dx_texcoord, loc + k, reg + k, comp, DX_SV_NONE, dx_ct_of(t), n,
               interp))
         return false;
   return true;
}

/* An input copied where main begins into the home of its variable. */
static bool dx_ic_add(struct dx_emit *e, struct dx_place home, uint32_t off, uint32_t vert,
      uint32_t reg, uint8_t comp, unsigned n)
{
   if (e->nic == DX_MAX_SIG * 6)
      return dx_fail(e, "too many interface elements", 0);
   e->ic_home[e->nic] = home;
   e->ic_off[e->nic]  = off;
   e->ic_vert[e->nic] = vert;
   e->ic_reg[e->nic]  = reg;
   e->ic_comp[e->nic] = comp;
   e->ic_n[e->nic]    = (uint8_t)n;
   e->nic++;
   return true;
}

/* A geometry stage's inputs: each element of a per-vertex array (an
 * in block's members, or a variable) packed as the vertex stage packs
 * its outputs, and copied for every vertex into the array's home. */
static bool dx_gs_inputs(struct dx_emit *e)
{
   const struct rslc_prog *pr = &e->c->prog;
   struct dx_packer pk;
   uint32_t i, k, vtx;
   memset(&pk, 0, sizeof(pk));
   for (i = 0; i < pr->ninputs; i++)
   {
      const struct rslc_var *v = pr->inputs[i];
      uint32_t loc = v->location == RSLC_UNSET ? 0 : v->location;
      struct dx_var *dv = &e->vars[v->id];
      const struct rslc_type *et;
      uint32_t per;
      if (v->quals & RSLC_VQ_PERVERTEX)
         continue;   /* gl_in: after the varyings, below */
      if (v->type->kind != RSLC_TY_ARRAY)
         return dx_fail(e, "a geometry stage input that is not per vertex", v->line);
      et  = v->type->elem;
      per = dx_slots(et);
      dv->home = dx_home(e, v->id, dx_slots(v->type));
      dv->has_home = true;
      if (v->quals & RSLC_VQ_IOBLOCK)
      {
         for (k = 0; k < et->nfields; k++)
         {
            const struct rslc_field *f = &et->fields[k];
            uint32_t reg;
            uint8_t comp;
            unsigned w = dx_width(f->type);
            if (dx_slots(f->type) != 1)
               return dx_fail(e, "an in block member of more than one location", v->line);
            if (!dx_pack(e, &pk, w, 1, dx_interp(v, f->interp, f->quals, f->type), &reg, &comp)
                  || !dx_sig_add(e, true, dx_texcoord, loc + k, reg, comp, DX_SV_NONE,
                     dx_ct_of(f->type), w, 0))
               return false;
            for (vtx = 0; vtx < e->gs_verts; vtx++)
               if (!dx_ic_add(e, dv->home, vtx * per + dx_field_slot(et, k), vtx, reg, comp, w))
                  return false;
         }
         continue;
      }
      {
         uint32_t reg, rows = per;
         uint8_t comp;
         unsigned w = dx_width(et);
         if (et->kind == RSLC_TY_STRUCT)
            return dx_fail(e, "a struct geometry stage input", v->line);
         if (!dx_pack(e, &pk, w, rows, dx_interp(v, v->interp, v->quals, et), &reg, &comp))
            return false;
         for (k = 0; k < rows; k++)
         {
            if (!dx_sig_add(e, true, dx_texcoord, loc + k, reg + k, comp, DX_SV_NONE, dx_ct_of(et),
                     w, 0))
               return false;
            for (vtx = 0; vtx < e->gs_verts; vtx++)
               if (!dx_ic_add(e, dv->home, vtx * per + k, vtx, reg + k, comp, w))
                  return false;
         }
      }
   }
   /* gl_in: the vertex stage's builtins, which follow its varyings */
   for (i = 0; i < pr->ninputs; i++)
   {
      const struct rslc_var *v = pr->inputs[i];
      const struct rslc_type *et;
      struct dx_var *dv = &e->vars[v->id];
      uint32_t per;
      if (!(v->quals & RSLC_VQ_PERVERTEX) || !e->used[v->id])
         continue;
      et  = v->type->elem;
      per = dx_slots(et);
      dv->home = dx_home(e, v->id, dx_slots(v->type));
      dv->has_home = true;
      for (k = 0; k < et->nfields; k++)
      {
         const struct rslc_field *f = &et->fields[k];
         const char *nm = f->name->str;
         uint32_t reg, n, d;
         uint8_t comp;
         bool clip = !strcmp(nm, "gl_ClipDistance"), cull = !strcmp(nm, "gl_CullDistance");
         if (!strcmp(nm, "gl_Position"))
         {
            if (!dx_pack(e, &pk, 4, 1, DX_GROUP_ALONE, &reg, &comp)
                  || !dx_sig_add(e, true, "SV_Position", 0, reg, 0, DX_SV_POSITION, DX_CT_FLOAT,
                     4, 0))
               return false;
            for (vtx = 0; vtx < e->gs_verts; vtx++)
               if (!dx_ic_add(e, dv->home, vtx * per + dx_field_slot(et, k), vtx, reg, 0, 4))
                  return false;
            continue;
         }
         if (!clip && !cull)
            continue;   /* gl_PointSize: Direct3D has none */
         n = clip ? pr->clip_in_n : pr->cull_in_n;
         if (!n)
            continue;
         if (n > 4)
            return dx_fail(e, "more than four clip or cull distances", v->line);
         if (!dx_pack(e, &pk, n, 1, DX_GROUP_ALONE, &reg, &comp)
               || !dx_sig_add(e, true, clip ? "SV_ClipDistance" : "SV_CullDistance", 0, reg, 0,
                  clip ? DX_SV_CLIP : DX_SV_CULL, DX_CT_FLOAT, n, 0))
            return false;
         for (vtx = 0; vtx < e->gs_verts; vtx++)
            for (d = 0; d < n; d++)
               if (!dx_ic_add(e, dv->home, vtx * per + dx_field_slot(et, k) + d, vtx, reg,
                        (uint8_t)d, 1))
                  return false;
      }
   }
   return true;
}

/* Sorts a signature by register and component, as fxc lists it,
 * carrying the outputs' homes along. */
static void dx_sig_sort(struct dx_emit *e, bool in)
{
   struct dx_sig *s = in ? e->isig : e->osig;
   uint32_t n = in ? e->nisig : e->nosig, i, j;
   for (i = 1; i < n; i++)
      for (j = i; j > 0; j--)
      {
         uint32_t ka = s[j - 1].reg == RSLC_UNSET ? ~0u : s[j - 1].reg * 4u + (s[j - 1].mask & -s[j - 1].mask);
         uint32_t kb = s[j].reg == RSLC_UNSET ? ~0u : s[j].reg * 4u + (s[j].mask & -s[j].mask);
         struct dx_sig t;
         if (ka <= kb)
            break;
         t = s[j - 1]; s[j - 1] = s[j]; s[j] = t;
         if (!in)
         {
            struct dx_place p = e->oc_home[j - 1];
            uint8_t b;
            e->oc_home[j - 1] = e->oc_home[j]; e->oc_home[j] = p;
            b = e->oc_n[j - 1]; e->oc_n[j - 1] = e->oc_n[j]; e->oc_n[j] = b;
            b = e->oc_comp[j - 1]; e->oc_comp[j - 1] = e->oc_comp[j]; e->oc_comp[j] = b;
            b = e->oc_dist[j - 1]; e->oc_dist[j - 1] = e->oc_dist[j]; e->oc_dist[j] = b;
         }
      }
}

/* The signatures and the homes of the interface, placed as fxc places
 * the HLSL printer's stage structs. */
static bool dx_interface(struct dx_emit *e)
{
   const struct rslc_prog *pr = &e->c->prog;
   bool frag = e->c->stage == RSLC_STAGE_FRAGMENT;
   struct dx_packer pk;
   uint32_t i, layer, pass;

   if (e->c->stage == RSLC_STAGE_COMPUTE)
      return true;

   /* inputs: the declared ones, then the builtins read; a vertex
    * stage's attributes one register each */
   memset(&pk, 0, sizeof(pk));
   if (e->c->stage == RSLC_STAGE_GEOMETRY && !dx_gs_inputs(e))
      return false;
   for (i = 0; e->c->stage != RSLC_STAGE_GEOMETRY && i < pr->ninputs; i++)
   {
      const struct rslc_var *v = pr->inputs[i];
      uint32_t loc = v->location == RSLC_UNSET ? 0 : v->location;
      struct dx_var *dv = &e->vars[v->id];
      if (dv->dynamic)
         return dx_fail(e, "a stage input indexed with a variable", v->line);
      if (v->quals & RSLC_VQ_IOBLOCK)
      {
         /* a member each, at its own place: the block's home is the
          * first, and a member's path finds it through its slot, so
          * every member must sit where its slot says */
         uint32_t k;
         dv->home = dx_place_of(DXO_TEMP, dx_temp(e, v->type->nfields));
         dv->has_home = true;
         for (k = 0; k < v->type->nfields; k++)
         {
            const struct rslc_field *f = &v->type->fields[k];
            struct dx_place h;
            if (dx_slots(f->type) != 1)
               return dx_fail(e, "an in block member of more than one location", v->line);
            if (!dx_in_var(e, &pk, frag, loc + k, f->type,
                     frag ? dx_interp(v, f->interp, f->quals, f->type) : 0, &h))
               return false;
            if (!dx_ic_add(e, dv->home, k, RSLC_UNSET, h.reg, h.comp, dx_width(f->type)))
               return false;
         }
         continue;
      }
      if (v->type->kind == RSLC_TY_STRUCT)
         return dx_fail(e, "a struct stage input", v->line);
      if (!dx_in_var(e, &pk, frag, loc, v->type,
               frag ? dx_interp(v, v->interp, v->quals, v->type) : 0, &dv->home))
         return false;
      dv->has_home = true;
   }
   /* a pixel stage reading gl_Layer: the position, then the layer, where
    * the geometry stage writes them, the position declared even when
    * unread */
   layer = 0;
   for (i = 1; frag && i < pr->nvars; i++)
   {
      const struct rslc_var *v = pr->vars[i];
      if (v->global && v->storage == RSLC_ST_BUILTIN && e->used[i]
            && v->builtin == RSLC_BI_LAYER)
         layer = i;
   }
   if (layer)
   {
      uint32_t reg;
      uint8_t comp;
      for (i = 1; i < pr->nvars; i++)
      {
         const struct rslc_var *v = pr->vars[i];
         if (v->global && v->storage == RSLC_ST_BUILTIN && e->used[i]
               && v->builtin == RSLC_BI_FRAGCOORD)
            break;
      }
      if (!dx_pack(e, &pk, 4, 1, DX_GROUP_ALONE, &reg, &comp)
            || !dx_sig_add(e, true, "SV_Position", 0, reg, 0, DX_SV_POSITION, DX_CT_FLOAT, 4,
               DX_INTERP_NOPERSPECTIVE))
         return false;
      if (i < pr->nvars)
      {
         e->vars[i].home     = dx_place_of(DXO_INPUT, reg);
         e->vars[i].has_home = true;
      }
      if (!dx_pack(e, &pk, 1, 1, DX_GROUP_ALONE, &reg, &comp)
            || !dx_sig_add(e, true, "SV_RenderTargetArrayIndex", 0, reg, 0, DX_SV_RTINDEX,
               DX_CT_UINT, 1, DX_INTERP_CONSTANT))
         return false;
      e->vars[layer].home     = dx_place_of(DXO_INPUT, reg);
      e->vars[layer].has_home = true;
   }
   for (i = 1; i < pr->nvars; i++)
   {
      const struct rslc_var *v = pr->vars[i];
      struct dx_var *dv = &e->vars[i];
      const char *name;
      uint8_t sv, group = DX_GROUP_ALONE;
      unsigned n = 1;
      uint32_t reg;
      uint8_t comp;
      if (!v->global || v->storage != RSLC_ST_BUILTIN || !e->used[i])
         continue;
      if (layer && (v->builtin == RSLC_BI_FRAGCOORD || v->builtin == RSLC_BI_LAYER))
         continue;
      switch (v->builtin)
      {
         case RSLC_BI_FRAGCOORD:     name = "SV_Position"; sv = DX_SV_POSITION; n = 4; break;
         case RSLC_BI_VERTEXINDEX:   name = "SV_VertexID"; sv = DX_SV_VERTEXID; break;
         case RSLC_BI_INSTANCEINDEX: name = "SV_InstanceID"; sv = DX_SV_INSTANCEID; break;
         case RSLC_BI_FRONTFACING:   name = "SV_IsFrontFace"; sv = DX_SV_FRONTFACE; group = DX_GROUP_SGV; break;
         case RSLC_BI_SAMPLEID:      name = "SV_SampleIndex"; sv = DX_SV_SAMPLEINDEX; group = DX_GROUP_SGV; break;
         case RSLC_BI_HELPERINVOCATION:
            return dx_fail(e, "no form of gl_HelperInvocation", v->line);
         case RSLC_BI_INVOCATIONID:
            /* vGSInstanceID: no signature element */
            dv->home = dx_place_of(DXO_GSINSTANCEID, 0);
            dv->has_home = true;
            continue;
         default:
            continue;
      }
      if (!frag)
      {
         reg  = pk.nreg++;
         comp = 0;
      }
      else if (!dx_pack(e, &pk, n, 1, group, &reg, &comp))
         return false;
      dv->home = dx_place_of(DXO_INPUT, reg);
      dv->home.comp = comp;
      dv->has_home = true;
      if (!dx_sig_add(e, true, name, 0, reg, comp, sv,
               v->builtin == RSLC_BI_FRAGCOORD ? DX_CT_FLOAT : DX_CT_UINT, n,
               v->builtin == RSLC_BI_FRAGCOORD ? DX_INTERP_NOPERSPECTIVE : DX_INTERP_CONSTANT))
         return false;
   }

   /* outputs: the declared ones, then the builtins written; a vertex
    * stage's packed, a pixel stage's colours at SV_Target<n> = o<n> */
   memset(&pk, 0, sizeof(pk));
   for (i = 0; i < pr->noutputs; i++)
   {
      const struct rslc_var *v = pr->outputs[i];
      uint32_t loc = v->location == RSLC_UNSET ? 0 : v->location;
      uint32_t n, k, reg;
      struct dx_var *dv = &e->vars[v->id];
      uint8_t comp;
      if (v->quals & RSLC_VQ_IOBLOCK)
      {
         dv->home = dx_place_of(DXO_TEMP, dx_temp(e, dx_slots(v->type)));
         dv->has_home = true;
         for (k = 0; k < v->type->nfields; k++)
         {
            const struct rslc_field *f = &v->type->fields[k];
            struct dx_place h = dv->home;
            unsigned w = dx_width(f->type);
            if (dx_slots(f->type) != 1)
               return dx_fail(e, "an out block member of more than one location", v->line);
            if (!dx_pack(e, &pk, w, 1, dx_interp(v, f->interp, f->quals, f->type), &reg, &comp))
               return false;
            h.reg += k;
            if (!dx_out_add(e, dx_texcoord, loc + k, reg, comp, DX_SV_NONE, f->type, w, h, 0))
               return false;
         }
         continue;
      }
      if (v->type->kind == RSLC_TY_STRUCT)
         return dx_fail(e, "a struct stage output", v->line);
      n = dx_slots(v->type);
      dv->home = dx_home(e, v->id, n);
      dv->has_home = true;
      if (frag)
      {
         for (k = 0; k < n; k++)
         {
            struct dx_place h = dv->home;
            h.reg += k;
            if (!dx_out_add(e, dx_target, loc + v->index + k, loc + v->index + k, 0, DX_SV_NONE,
                     v->type, dx_width(v->type), h, 0))
               return false;
         }
         continue;
      }
      if (!dx_pack(e, &pk, dx_width(v->type), n, dx_interp(v, v->interp, v->quals, v->type),
               &reg, &comp))
         return false;
      for (k = 0; k < n; k++)
      {
         struct dx_place h = dv->home;
         h.reg += k;
         if (!dx_out_add(e, dx_texcoord, loc + k, reg + k, comp, DX_SV_NONE, v->type,
                  dx_width(v->type), h, 0))
            return false;
      }
   }
   /* the position, the layer, then the rest: the layer where a pixel
    * stage reading it would place it, after the position */
   for (pass = 0; pass < 3; pass++)
   for (i = 1; i < pr->nvars; i++)
   {
      const struct rslc_var *v = pr->vars[i];
      struct dx_var *dv = &e->vars[i];
      uint32_t n, reg;
      uint8_t comp;
      if (!v->global || v->storage != RSLC_ST_BUILTIN || !e->used[i] || v->dir != RSLC_DIR_OUT)
         continue;
      if (pass != (v->builtin == RSLC_BI_POSITION ? 0u : v->builtin == RSLC_BI_LAYER ? 1u : 2u))
         continue;
      switch (v->builtin)
      {
         case RSLC_BI_POSITION:
            dv->home = dx_place_of(DXO_TEMP, dx_temp(e, 1));
            dv->has_home = true;
            if (!dx_pack(e, &pk, 4, 1, DX_GROUP_ALONE, &reg, &comp)
                  || !dx_out_add(e, "SV_Position", 0, reg, 0, DX_SV_POSITION, v->type, 4,
                     dv->home, 0))
               return false;
            break;
         case RSLC_BI_FRAGDEPTH:
            dv->home = dx_place_of(DXO_TEMP, dx_temp(e, 1));
            dv->has_home = true;
            if (!dx_out_add(e, "SV_Depth", 0, RSLC_UNSET, 0, DX_SV_NONE, v->type, 1, dv->home, 0))
               return false;
            break;
         case RSLC_BI_CLIPDISTANCE:
         case RSLC_BI_CULLDISTANCE:
            n = v->builtin == RSLC_BI_CLIPDISTANCE ? pr->clip_n : pr->cull_n;
            dv->home = dx_home(e, i, v->type->array_len);
            dv->has_home = true;
            if (!n)
               break;
            if (n > 4)
               return dx_fail(e, "more than four clip or cull distances", v->line);
            if (!dx_pack(e, &pk, n, 1, DX_GROUP_ALONE, &reg, &comp)
                  || !dx_out_add(e, v->builtin == RSLC_BI_CLIPDISTANCE ? "SV_ClipDistance"
                     : "SV_CullDistance", 0, reg, 0, v->builtin == RSLC_BI_CLIPDISTANCE
                     ? DX_SV_CLIP : DX_SV_CULL, v->type->elem, n, dv->home, (uint8_t)n))
               return false;
            break;
         case RSLC_BI_LAYER:
            dv->home = dx_place_of(DXO_TEMP, dx_temp(e, 1));
            dv->has_home = true;
            if (!dx_pack(e, &pk, 1, 1, DX_GROUP_ALONE, &reg, &comp)
                  || !dx_out_add(e, "SV_RenderTargetArrayIndex", 0, reg, 0, DX_SV_RTINDEX,
                     rslc_type_basic(e->c, RSLC_TY_SCALAR, RSLC_BASE_UINT, 1, 1), 1, dv->home, 0))
               return false;
            break;
         case RSLC_BI_POINTSIZE:
            /* Direct3D has no point size: kept, never output */
            dv->home = dx_place_of(DXO_TEMP, dx_temp(e, 1));
            dv->has_home = true;
            break;
         default:
            break;
      }
   }
   dx_sig_sort(e, true);
   dx_sig_sort(e, false);
   return true;
}

/* ------------------------------------------------------------------ */
/* Access paths                                                        */
/* ------------------------------------------------------------------ */

#define DXP_REGS 0   /* registers: a home plus registers past it */
#define DXP_CB   1   /* a uniform block, by byte offset */
#define DXP_RAW  2   /* a storage buffer, by byte offset */
#define DXP_TGSM 3   /* workgroup memory, by byte offset: block is its g register */

/* Where an lvalue, or a part of an aggregate, is. */
struct dx_path
{
   const struct rslc_type *type;   /* here, before the swizzle */
   struct dx_place place;          /* REGS: the home */
   uint32_t slot;                  /* REGS: registers past the home */
   uint32_t off;                   /* CB, RAW: bytes into the block */
   uint32_t dyn;                   /* a temporary adding registers (REGS, CB) or
                                    * bytes (RAW); RSLC_UNSET when none */
   uint32_t dcomp;                 /* a temporary choosing a vector's component;
                                    * RSLC_UNSET when none */
   uint8_t  dyn_comp;
   uint8_t  dcomp_c;
   uint8_t  kind;                  /* DXP_* */
   uint8_t  block;
   uint8_t  nswz;                  /* 0: no swizzle */
   uint8_t  swz[4];
   bool     std140;
};

static bool dx_rv(struct dx_emit *e, uint32_t node, struct dx_val *out);
static bool dx_stmt(struct dx_emit *e, uint32_t node);

/* A value in a fresh temporary of its own. */
static struct dx_opnd dx_fresh(struct dx_emit *e, struct dx_opnd v)
{
   struct dx_opnd t = dx_tmp(e, v.n);
   dx_mov(e, t, v);
   return t;
}

/* A one-component integer value in a temporary register, for an index. */
static bool dx_index_reg(struct dx_emit *e, struct dx_opnd v, uint32_t *reg, uint8_t *comp)
{
   if (v.file != DXO_TEMP || v.mod || v.rel != RSLC_UNSET)
      v = dx_fresh(e, dx_comp(v, 0));
   *reg  = v.idx[0];
   *comp = v.swz[0];
   return true;
}

/* path->dyn += index * scale. */
static bool dx_path_add_dyn(struct dx_emit *e, struct dx_path *p, struct dx_opnd index,
      uint32_t scale)
{
   struct dx_opnd t;
   if (scale == 1 && p->dyn == RSLC_UNSET)
      return dx_index_reg(e, index, &p->dyn, &p->dyn_comp);
   t = dx_tmp(e, 1);
   if (!dx_op3(e, DX_IMAD, t, dx_comp(index, 0), dx_immu(scale),
            p->dyn == RSLC_UNSET ? dx_immu(0) : dx_comp(dx_reg(DXO_TEMP, p->dyn, 4), p->dyn_comp)))
      return false;
   p->dyn = t.idx[0];
   p->dyn_comp = 0;
   return true;
}

/* A path over registers holding an evaluated value. */
static void dx_path_regs(struct dx_path *p, const struct rslc_type *t, struct dx_place place)
{
   memset(p, 0, sizeof(*p));
   p->kind  = DXP_REGS;
   p->type  = t;
   p->place = place;
   p->dyn   = RSLC_UNSET;
   p->dcomp = RSLC_UNSET;
}

/* Byte layout of a block path's type: the stride of an array or
 * matrix. */
static uint32_t dx_stride(const struct rslc_type *t, bool std140)
{
   uint32_t a, s, st;
   rslc_layout_of_type(t, std140, &a, &s, &st);
   return st;
}

/* The member k of a path's struct, or a constant element / column. */
static bool dx_path_member(struct dx_emit *e, struct dx_path *p, uint32_t k)
{
   const struct rslc_type *t = p->type;
   (void)e;
   if (p->kind == DXP_REGS)
      p->slot += dx_field_slot(t, k);
   else
      p->off += rslc_layout_field_offset(t, k, p->std140);
   p->type = t->fields[k].type;
   return true;
}

static bool dx_path_elem(struct dx_emit *e, struct dx_path *p, uint32_t k)
{
   const struct rslc_type *t = p->type;
   if (t->kind == RSLC_TY_VEC)
   {
      /* a component */
      uint8_t c = (uint8_t)k;
      if (p->nswz)
         c = p->swz[k];
      p->nswz   = 1;
      p->swz[0] = c;
      p->type   = rslc_type_basic(e->c, RSLC_TY_SCALAR, (enum rslc_base)t->base, 1, 1);
      return true;
   }
   if (t->kind == RSLC_TY_MAT)
   {
      if (p->kind == DXP_REGS)
         p->slot += k;
      else
         p->off += k * dx_stride(t, p->std140);
      p->type = rslc_type_basic(e->c, RSLC_TY_VEC, RSLC_BASE_FLOAT, t->rows, 1);
      return true;
   }
   if (p->kind == DXP_REGS)
      p->slot += k * dx_slots(t->elem);
   else
      p->off += k * dx_stride(t, p->std140);
   p->type = t->elem;
   return true;
}

/* An element chosen by a value. */
static bool dx_path_elem_dyn(struct dx_emit *e, struct dx_path *p, struct dx_opnd index,
      uint32_t line)
{
   const struct rslc_type *t = p->type;
   if (t->kind == RSLC_TY_VEC)
   {
      if (p->nswz)
         return dx_fail(e, "a swizzle indexed with a variable", line);
      if (!dx_index_reg(e, index, &p->dcomp, &p->dcomp_c))
         return false;
      p->type = rslc_type_basic(e->c, RSLC_TY_SCALAR, (enum rslc_base)t->base, 1, 1);
      return true;
   }
   if (p->kind == DXP_REGS)
   {
      uint32_t per = t->kind == RSLC_TY_MAT ? 1 : dx_slots(t->elem);
      if (p->place.file != DXO_XTEMP)
      {
         /* an evaluated aggregate: to an indexable temporary first */
         uint32_t n = dx_slots(t), i;
         struct dx_place x = dx_place_of(DXO_XTEMP, 0);
         if (e->nxtemp == DX_MAX_XTEMP)
            return dx_fail(e, "too many indexed values", line);
         x.arr = e->nxtemp;
         e->xtemp_len[e->nxtemp++] = n;
         for (i = 0; i < n; i++)
            if (!dx_mov(e, dx_at(&x, i, 4), dx_at(&p->place, p->slot + i, 4)))
               return false;
         p->place = x;
         p->slot  = 0;
      }
      if (!dx_path_add_dyn(e, p, index, per))
         return false;
   }
   else
   {
      uint32_t stride = dx_stride(t, p->std140);
      if (p->kind == DXP_CB)
      {
         if (stride & 15)
            return dx_fail(e, "a uniform array indexed with a variable whose stride is not 16 bytes",
                  line);
         if (!dx_path_add_dyn(e, p, index, stride / 16))
            return false;
      }
      else if (!dx_path_add_dyn(e, p, index, stride))
         return false;
   }
   p->type = t->kind == RSLC_TY_MAT ? rslc_type_basic(e->c, RSLC_TY_VEC, RSLC_BASE_FLOAT,
         t->rows, 1) : t->elem;
   return true;
}

/* The resource or view a storage buffer is bound as. */
static struct dx_opnd dx_buffer_opnd(struct dx_emit *e, unsigned block)
{
   const struct rslc_blockdecl *b = &e->c->prog.blocks[block];
   struct dx_opnd o = dx_reg(b->readonly ? DXO_RESOURCE : DXO_UAV,
         b->binding == RSLC_UNSET ? 0 : b->binding, 4);
   return o;
}

/* The resource, view or workgroup memory a byte-addressed path reads. */
static struct dx_opnd dx_raw_res(struct dx_emit *e, const struct dx_path *p)
{
   if (p->kind == DXP_TGSM)
      return dx_reg(DXO_TGSM, p->block, 4);
   return dx_buffer_opnd(e, p->block);
}

/* A storage buffer path's byte address, as a one-component operand. */
static struct dx_opnd dx_raw_addr(struct dx_emit *e, const struct dx_path *p, uint32_t extra)
{
   struct dx_opnd t;
   if (p->dyn == RSLC_UNSET)
      return dx_immu(p->off + extra);
   if (p->off + extra == 0)
      return dx_comp(dx_reg(DXO_TEMP, p->dyn, 4), p->dyn_comp);
   t = dx_tmp(e, 1);
   dx_op2(e, DX_IADD, t, dx_comp(dx_reg(DXO_TEMP, p->dyn, 4), p->dyn_comp),
         dx_immu(p->off + extra));
   return t;
}

/* The home of a path over registers, with its dynamic offset. */
static struct dx_place dx_path_place(const struct dx_path *p)
{
   struct dx_place pl = p->place;
   pl.reg += p->slot;
   if (p->dyn != RSLC_UNSET)
   {
      pl.rel      = p->dyn;
      pl.rel_comp = p->dyn_comp;
   }
   return pl;
}

/* One component of a vector chosen by a value: movc down the
 * components. */
static struct dx_opnd dx_pick(struct dx_emit *e, struct dx_opnd vec, unsigned n,
      uint32_t sel, uint8_t sel_c)
{
   struct dx_opnd r = dx_tmp(e, 1), c = dx_tmp(e, 1);
   struct dx_opnd s = dx_comp(dx_reg(DXO_TEMP, sel, 4), sel_c);
   unsigned k;
   dx_mov(e, r, dx_comp(vec, 0));
   for (k = 1; k < n; k++)
   {
      dx_op2(e, DX_IEQ, c, s, dx_immu(k));
      dx_op3(e, DX_MOVC, r, c, dx_comp(vec, k), r);
   }
   return r;
}

/* A bool read from a block is a 0 / 1 word: to a mask. */
static struct dx_opnd dx_from_laid_bool(struct dx_emit *e, struct dx_opnd v)
{
   struct dx_opnd t = dx_tmp(e, v.n);
   dx_op2(e, DX_INE, t, v, dx_splat(dx_immu(0), v.n));
   return t;
}

/* Reads the scalar or vector a path names. */
static bool dx_load(struct dx_emit *e, const struct dx_path *p, struct dx_opnd *out)
{
   unsigned n = dx_width(p->type), i;
   unsigned base_n = p->type->kind == RSLC_TY_SCALAR && p->nswz ? 4 : n;
   struct dx_opnd o;
   const struct rslc_type *vt = p->type;

   /* the vector the swizzle or the component choice reads from */
   if (p->nswz || p->dcomp != RSLC_UNSET)
      base_n = 4;
   switch (p->kind)
   {
      case DXP_REGS:
      {
         struct dx_place pl = dx_path_place(p);
         o = dx_at(&pl, 0, 4);
         if (pl.file == DXO_THREADIDINGROUPFLAT)
            o.n = 1;
         break;
      }
      case DXP_CB:
      {
         struct dx_place pl = dx_place_of(DXO_CB, p->off / 16);
         pl.arr = dx_cb_slot(e, p->block);
         e->cb_read[p->block] = true;
         if (p->dyn != RSLC_UNSET)
         {
            pl.rel = p->dyn;
            pl.rel_comp = p->dyn_comp;
            e->cb_dynamic[p->block] = true;
         }
         o = dx_at(&pl, 0, 4);
         /* the member's components within its register */
         for (i = 0; i < 4; i++)
            o.swz[i] = (uint8_t)(((p->off % 16) / 4 + i) & 3);
         break;
      }
      default:
      {
         /* ld_raw: n words from the address */
         unsigned words = p->nswz || p->dcomp != RSLC_UNSET
            ? dx_width(vt) : n;
         struct dx_opnd t, src[2];
         if (p->type->kind == RSLC_TY_SCALAR && !p->nswz)
            words = 1;
         else if (p->nswz)
         {
            /* the vector the swizzle reads: as wide as its highest component */
            words = 0;
            for (i = 0; i < p->nswz; i++)
               if (p->swz[i] + 1u > words)
                  words = p->swz[i] + 1u;
         }
         if (p->dcomp != RSLC_UNSET)
            words = 4;   /* a vector chosen into: the whole vector, whatever its width */
         t = dx_tmp(e, words);
         src[0] = dx_raw_addr(e, p, 0);
         src[1] = dx_raw_res(e, p);
         for (i = 0; i < 4; i++)
            src[1].swz[i] = (uint8_t)(i < words ? i : 0);
         if (!dx_inst(e, DX_LD_RAW, 0, &t, 1, src, 2))
            return false;
         o = t;
         for (i = 0; i < 4; i++)
            o.swz[i] = (uint8_t)(i < words ? i : words - 1);
         break;
      }
   }
   if (p->nswz)
   {
      struct dx_opnd s = o;
      for (i = 0; i < p->nswz; i++)
         s.swz[i] = o.swz[p->swz[i]];
      s.n = p->nswz;
      o = s;
      n = p->nswz;
   }
   else
      o.n = (uint8_t)(p->dcomp != RSLC_UNSET ? 4 : n);
   if (p->dcomp != RSLC_UNSET)
   {
      const struct rslc_type *full = p->type;
      (void)full;
      o = dx_pick(e, o, 4, p->dcomp, p->dcomp_c);
      n = 1;
   }
   (void)base_n;
   if (p->kind != DXP_REGS && p->type->base == RSLC_BASE_BOOL)
      o = dx_from_laid_bool(e, o);
   *out = o;
   return true;
}

/* Writes a scalar or vector to the place a path names. */
static bool dx_store(struct dx_emit *e, const struct dx_path *p, struct dx_opnd v, uint32_t line)
{
   unsigned n = v.n, i;
   uint8_t  comp[4];
   for (i = 0; i < n; i++)
      comp[i] = (uint8_t)(p->nswz ? p->swz[i] : i);
   if (p->kind == DXP_CB)
      return dx_fail(e, "a write to a uniform", line);
   if (p->kind == DXP_RAW || p->kind == DXP_TGSM)
   {
      struct dx_opnd dst, src[2], val = v;
      if (p->type->base == RSLC_BASE_BOOL)
      {
         struct dx_opnd t = dx_tmp(e, n);
         dx_op2(e, DX_AND, t, v, dx_splat(dx_immu(1), n));
         val = t;
      }
      if (p->dcomp != RSLC_UNSET)
      {
         /* a component chosen by a value: 4 * that past the vector */
         struct dx_path q = *p;
         struct dx_opnd t = dx_tmp(e, 1);
         dx_op3(e, DX_IMAD, t, dx_comp(dx_reg(DXO_TEMP, p->dcomp, 4), p->dcomp_c), dx_immu(4),
               p->dyn == RSLC_UNSET ? dx_immu(0) : dx_comp(dx_reg(DXO_TEMP, p->dyn, 4), p->dyn_comp));
         q.dyn = t.idx[0];
         q.dyn_comp = 0;
         dst = dx_raw_res(e, p);
         dst.n = 1; dst.swz[0] = 0;
         src[0] = dx_raw_addr(e, &q, 0);
         src[1] = dx_comp(val, 0);
         return dx_inst(e, DX_STORE_RAW, 0, &dst, 1, src, 2);
      }
      /* contiguous components go in one store; others one at a time */
      for (i = 0; i < n; i++)
         if (comp[i] != comp[0] + i)
            break;
      if (i == n)
      {
         dst = dx_raw_res(e, p);
         dst.n = (uint8_t)n;
         for (i = 0; i < 4; i++)
            dst.swz[i] = (uint8_t)i;
         src[0] = dx_raw_addr(e, p, 4u * comp[0]);
         src[1] = val;
         return dx_inst(e, DX_STORE_RAW, 0, &dst, 1, src, 2);
      }
      for (i = 0; i < n; i++)
      {
         dst = dx_raw_res(e, p);
         dst.n = 1; dst.swz[0] = 0;
         src[0] = dx_raw_addr(e, p, 4u * comp[i]);
         src[1] = dx_comp(val, i);
         if (!dx_inst(e, DX_STORE_RAW, 0, &dst, 1, src, 2))
            return false;
      }
      return true;
   }
   {
      struct dx_place pl = dx_path_place(p);
      struct dx_opnd  dst = dx_at(&pl, 0, 4);
      struct dx_opnd  src = v;
      if (pl.file == DXO_INPUT)
         return dx_fail(e, "a write to a stage input", line);
      if (p->dcomp != RSLC_UNSET)
      {
         /* each component, kept unless the value chooses it */
         unsigned w = dx_width(rslc_type_basic(e->c, RSLC_TY_VEC, RSLC_BASE_FLOAT, 4, 1));
         struct dx_opnd c = dx_tmp(e, 1);
         struct dx_opnd s = dx_comp(dx_reg(DXO_TEMP, p->dcomp, 4), p->dcomp_c);
         for (i = 0; i < w; i++)
         {
            struct dx_opnd d1 = dst, cur = dst;
            d1.n = 1; d1.swz[0] = (uint8_t)i;
            cur = dx_comp(cur, i);
            cur.swz[0] = (uint8_t)i;
            dx_op2(e, DX_IEQ, c, s, dx_immu(i));
            if (!dx_op3(e, DX_MOVC, d1, c, dx_comp(v, 0), cur))
               return false;
         }
         return true;
      }
      dst.n = (uint8_t)n;
      for (i = 0; i < n; i++)
         dst.swz[i] = comp[i];
      /* the source swizzle runs by the destination's component */
      src.n = 4;
      for (i = 0; i < 4; i++)
      {
         src.swz[i] = v.swz[0];
         src.imm[i] = v.imm[0];
      }
      for (i = 0; i < n; i++)
      {
         src.swz[comp[i]] = v.swz[i];
         src.imm[comp[i]] = v.imm[i];
      }
      if (v.file == DXO_IMM32)
      {
         /* an immediate keeps four values, one per destination component */
         src.n = 4;
      }
      return dx_mov(e, dst, src);
   }
}

/* Copies an aggregate (or leaf) from one path to another, leaf by
 * leaf. */
static bool dx_copy(struct dx_emit *e, const struct dx_path *dst, const struct dx_path *src,
      uint32_t line)
{
   const struct rslc_type *t = dst->type;
   uint32_t i, n;
   switch (t->kind)
   {
      case RSLC_TY_STRUCT:
         for (i = 0; i < t->nfields; i++)
         {
            struct dx_path d = *dst, s = *src;
            dx_path_member(e, &d, i);
            dx_path_member(e, &s, i);
            if (!dx_copy(e, &d, &s, line))
               return false;
         }
         return true;
      case RSLC_TY_ARRAY:
      case RSLC_TY_MAT:
         n = t->kind == RSLC_TY_MAT ? t->cols : t->array_len;
         /* registers to registers in one move each */
         for (i = 0; i < n; i++)
         {
            struct dx_path d = *dst, s = *src;
            dx_path_elem(e, &d, i);
            dx_path_elem(e, &s, i);
            if (!dx_copy(e, &d, &s, line))
               return false;
         }
         return true;
      default:
      {
         struct dx_opnd v;
         return dx_load(e, src, &v) && dx_store(e, dst, v, line);
      }
   }
}

/* A home for a variable met before its declaration ran: a global's or
 * a parameter's, made where its function begins. */
static bool dx_var_home(struct dx_emit *e, const struct rslc_var *v)
{
   struct dx_var *dv = &e->vars[v->id];
   if (dv->has_home)
      return true;
   dv->home = dx_home(e, v->id, dx_slots(v->type));
   dv->has_home = true;
   return !e->c->failed;
}

/* The path of a variable. */
static bool dx_path_var(struct dx_emit *e, const struct rslc_var *v, struct dx_path *p,
      uint32_t line)
{
   switch (v->storage)
   {
      case RSLC_ST_MEMBER:
      case RSLC_ST_BLOCK:
      {
         const struct rslc_blockdecl *b = &e->c->prog.blocks[v->block];
         memset(p, 0, sizeof(*p));
         p->kind   = b->buffer ? DXP_RAW : DXP_CB;
         p->block  = (uint8_t)v->block;
         p->std140 = dx_block_std140(e, v->block);
         p->dyn    = RSLC_UNSET;
         p->dcomp  = RSLC_UNSET;
         if (v->storage == RSLC_ST_MEMBER)
         {
            p->off  = rslc_layout_field_offset(b->type, v->member, p->std140);
            p->type = b->type->fields[v->member].type;
         }
         else
            p->type = b->type;
         return true;
      }
      case RSLC_ST_BUILTIN:
         if (e->c->stage == RSLC_STAGE_COMPUTE)
         {
            uint8_t f;
            switch (v->builtin)
            {
               case RSLC_BI_GLOBALINVOCATIONID:   f = DXO_THREADID; break;
               case RSLC_BI_WORKGROUPID:          f = DXO_GROUPID; break;
               case RSLC_BI_LOCALINVOCATIONID:    f = DXO_THREADIDINGROUP; break;
               case RSLC_BI_LOCALINVOCATIONINDEX: f = DXO_THREADIDINGROUPFLAT; break;
               default:
                  return dx_fail(e, "a builtin with no form here", line);
            }
            dx_path_regs(p, v->type, dx_place_of(f, 0));
            return true;
         }
         break;
      case RSLC_ST_SAMPLER:
      case RSLC_ST_IMAGE:
      case RSLC_ST_SUBPASS:
         return dx_fail(e, "a sampler or image used as a value", line);
      case RSLC_ST_SHARED:
      {
         /* g<n>, laid out std430 */
         struct dx_var *dv = &e->vars[v->id];
         if (!dv->has_home)
         {
            dv->home = dx_place_of(DXO_TGSM, e->ntgsm);
            if (e->ntgsm == DX_MAX_TGSM)
               return dx_fail(e, "too many workgroup variables", line);
            {
               uint32_t a, size, st;
               rslc_layout_of_type(v->type, false, &a, &size, &st);
               e->tgsm_size[e->ntgsm++] = (size + 3) & ~3u;
            }
            dv->has_home = true;
         }
         memset(p, 0, sizeof(*p));
         p->kind   = DXP_TGSM;
         p->block  = (uint8_t)dv->home.reg;
         p->std140 = false;
         p->dyn    = RSLC_UNSET;
         p->dcomp  = RSLC_UNSET;
         p->type   = v->type;
         return true;
      }
      default:
         break;
   }
   if (!dx_var_home(e, v))
      return false;
   dx_path_regs(p, v->type, e->vars[v->id].home);
   return true;
}

/* The path of an lvalue or of any expression: a non-lvalue base is
 * evaluated into temporaries. */
static bool dx_path_of(struct dx_emit *e, uint32_t node, struct dx_path *p)
{
   const struct rslc_node *n = rslc_node_at(e->c, node);
   switch (n->op)
   {
      case RSLC_OP_VAR:
      {
         const struct rslc_var *v = e->c->prog.vars[n->a];
         if (!e->vars[v->id].folded)
            return dx_path_var(e, v, p, n->line);
         break;
      }
      case RSLC_OP_MEMBER:
         if (!dx_path_of(e, n->a, p))
            return false;
         if (p->type->kind != RSLC_TY_STRUCT)
            return dx_fail(e, "internal: member of a non-struct", n->line);
         return dx_path_member(e, p, n->b);
      case RSLC_OP_INDEX:
      {
         struct rslc_cval cv;
         if (!dx_path_of(e, n->a, p))
            return false;
         if (dx_is_const(e, n->b) && rslc_const_eval(e->c, n->b, &cv))
            return dx_path_elem(e, p, cv.v[0]);
         {
            struct dx_val idx;
            if (!dx_rv(e, n->b, &idx))
               return false;
            return dx_path_elem_dyn(e, p, idx.v, n->line);
         }
      }
      case RSLC_OP_SWIZZLE:
      {
         const struct rslc_node *base = rslc_node_at(e->c, n->a);
         unsigned cnt = n->aux & 3u, i;
         uint8_t  s[4];
         if (!cnt)
            cnt = 4;
         if (base->type->kind == RSLC_TY_SCALAR)
            break;   /* a scalar splatted: an rvalue */
         if (!dx_path_of(e, n->a, p))
            return false;
         if (p->dcomp != RSLC_UNSET)
            break;
         for (i = 0; i < cnt; i++)
         {
            unsigned c = (n->aux >> (2 + 2 * i)) & 3u;
            s[i] = (uint8_t)(p->nswz ? p->swz[c] : c);
         }
         memcpy(p->swz, s, cnt);
         p->nswz = (uint8_t)cnt;
         p->type = n->type;
         return true;
      }
      default:
         break;
   }
   {
      struct dx_val v;
      if (!dx_rv(e, node, &v))
         return false;
      if (v.agg)
      {
         dx_path_regs(p, n->type, v.p);
         return true;
      }
      {
         struct dx_opnd t = v.v;
         if (t.file != DXO_TEMP || t.rel != RSLC_UNSET || t.mod)
            t = dx_fresh(e, v.v);
         dx_path_regs(p, n->type, dx_place_of(DXO_TEMP, t.idx[0]));
         /* the value's components as a swizzle of the register */
         p->nswz = t.n;
         memcpy(p->swz, t.swz, 4);
         if (n->type->kind != RSLC_TY_SCALAR && n->type->kind != RSLC_TY_VEC)
            p->nswz = 0;
         return true;
      }
   }
}

/* An expression read through its path: a vector operand or an
 * aggregate's registers. */
static bool dx_rv_path(struct dx_emit *e, const struct dx_path *p, struct dx_val *out)
{
   const struct rslc_type *t = p->type;
   memset(out, 0, sizeof(*out));
   if (t->kind == RSLC_TY_SCALAR || t->kind == RSLC_TY_VEC)
      return dx_load(e, p, &out->v);
   out->agg = true;
   if (p->kind == DXP_REGS)
   {
      out->p = dx_path_place(p);
      return true;
   }
   {
      struct dx_path d;
      dx_path_regs(&d, t, dx_place_of(DXO_TEMP, dx_temp(e, dx_slots(t))));
      out->p = d.place;
      return dx_copy(e, &d, p, 0);
   }
}

/* ------------------------------------------------------------------ */
/* Arithmetic                                                          */
/* ------------------------------------------------------------------ */

static bool dx_is_float(const struct rslc_type *t)
{
   return t->base == RSLC_BASE_FLOAT;
}

/* -v, folded into an immediate's bits. */
static struct dx_opnd dx_negate(struct dx_opnd o, bool flt)
{
   unsigned i;
   if (o.file == DXO_IMM32)
   {
      for (i = 0; i < 4; i++)
         o.imm[i] = flt ? o.imm[i] ^ 0x80000000u : (uint32_t)(-(int32_t)o.imm[i]);
      return o;
   }
   o.mod ^= 1;
   return o;
}

/* |v| of a float. */
static struct dx_opnd dx_fabs(struct dx_opnd o)
{
   unsigned i;
   if (o.file == DXO_IMM32)
   {
      for (i = 0; i < 4; i++)
         o.imm[i] &= 0x7fffffffu;
      return o;
   }
   o.mod = 2;
   return o;
}

/* Widens a scalar operand to n components; a vector stays. */
static struct dx_opnd dx_widen(struct dx_opnd o, unsigned n)
{
   if (o.n == 1 && n > 1)
      return dx_splat(o, n);
   return o;
}

/* The bits of 1 in a type's base: 1.0, 1, or true. */
static uint32_t dx_one_bits(const struct rslc_type *t)
{
   return t->base == RSLC_BASE_FLOAT ? 0x3f800000u
      : t->base == RSLC_BASE_BOOL ? 0xffffffffu : 1u;
}

/* Converts a scalar or vector between bases. */
static struct dx_opnd dx_convert(struct dx_emit *e, struct dx_opnd v, unsigned from, unsigned to)
{
   struct dx_opnd t;
   unsigned n = v.n, i;
   if (from == to || (from == RSLC_BASE_INT && to == RSLC_BASE_UINT)
         || (from == RSLC_BASE_UINT && to == RSLC_BASE_INT))
      return v;
   if (v.file == DXO_IMM32)
   {
      /* fold */
      for (i = 0; i < 4; i++)
      {
         uint32_t b = v.imm[i];
         float    f;
         memcpy(&f, &b, 4);
         if (to == RSLC_BASE_BOOL)
            b = from == RSLC_BASE_FLOAT ? (f != 0.0f ? ~0u : 0u) : (b ? ~0u : 0u);
         else if (to == RSLC_BASE_FLOAT)
         {
            f = from == RSLC_BASE_BOOL ? (b ? 1.0f : 0.0f)
               : from == RSLC_BASE_INT ? (float)(int32_t)b : (float)b;
            memcpy(&b, &f, 4);
         }
         else if (from == RSLC_BASE_FLOAT)
            b = to == RSLC_BASE_INT ? (uint32_t)(int32_t)f : (f <= 0.0f ? 0u : (uint32_t)f);
         else
            b = b ? 1u : 0u;   /* bool to int */
         v.imm[i] = b;
      }
      return v;
   }
   t = dx_tmp(e, n);
   if (to == RSLC_BASE_BOOL)
      dx_op2(e, from == RSLC_BASE_FLOAT ? DX_NE : DX_INE, t, v,
            dx_splat(dx_immu(0), n));
   else if (from == RSLC_BASE_BOOL)
      dx_op2(e, DX_AND, t, v, dx_splat(dx_immu(to == RSLC_BASE_FLOAT ? 0x3f800000u : 1u), n));
   else if (to == RSLC_BASE_FLOAT)
      dx_op1(e, from == RSLC_BASE_INT ? DX_ITOF : DX_UTOF, t, v);
   else
      dx_op1(e, to == RSLC_BASE_INT ? DX_FTOI : DX_FTOU, t, v);
   return t;
}

/* a / b and a % b of signed integers, truncating toward zero. */
static struct dx_opnd dx_sdivmod(struct dx_emit *e, struct dx_opnd a, struct dx_opnd b,
      unsigned n, bool mod)
{
   struct dx_opnd ua = dx_tmp(e, n), ub = dx_tmp(e, n), q = dx_tmp(e, n), r = dx_tmp(e, n);
   struct dx_opnd s = dx_tmp(e, n), d[2], src[2];
   dx_op2(e, DX_IMAX, ua, a, dx_negate(a, false));
   dx_op2(e, DX_IMAX, ub, b, dx_negate(b, false));
   d[0] = q; d[1] = r;
   src[0] = ua; src[1] = ub;
   dx_inst(e, DX_UDIV, 0, d, 2, src, 2);
   if (mod)
   {
      dx_op2(e, DX_ILT, s, a, dx_splat(dx_immu(0), n));
      dx_op3(e, DX_MOVC, r, s, dx_negate(r, false), r);
      return r;
   }
   dx_op2(e, DX_XOR, s, a, b);
   dx_op2(e, DX_ILT, s, s, dx_splat(dx_immu(0), n));
   dx_op3(e, DX_MOVC, q, s, dx_negate(q, false), q);
   return q;
}

/* A binary operator on two scalars or vectors of one base; the result
 * is n components (or one, for == and != of whole vectors). */
static bool dx_binop(struct dx_emit *e, unsigned op, unsigned base, struct dx_opnd a,
      struct dx_opnd b, unsigned n, bool reduce, struct dx_opnd *out, uint32_t line)
{
   bool flt  = base == RSLC_BASE_FLOAT;
   bool sint = base == RSLC_BASE_INT;
   struct dx_opnd t;
   a = dx_widen(a, n);
   b = dx_widen(b, n);
   t = dx_tmp(e, n);
   switch (op)
   {
      case RSLC_BIN_ADD:
         dx_op2(e, flt ? DX_ADD : DX_IADD, t, a, b);
         break;
      case RSLC_BIN_SUB:
         dx_op2(e, flt ? DX_ADD : DX_IADD, t, a, dx_negate(b, flt));
         break;
      case RSLC_BIN_MUL:
         if (flt)
            dx_op2(e, DX_MUL, t, a, b);
         else
         {
            struct dx_opnd d[2], s[2];
            d[0] = dx_reg(DXO_NULL, 0, n);
            d[1] = t;
            s[0] = a; s[1] = b;
            dx_inst(e, DX_IMUL, 0, d, 2, s, 2);
         }
         break;
      case RSLC_BIN_DIV:
      case RSLC_BIN_MOD:
         if (flt)
         {
            if (op == RSLC_BIN_MOD)
               return dx_fail(e, "% of floats", line);
            dx_op2(e, DX_DIV, t, a, b);
         }
         else if (sint)
            t = dx_sdivmod(e, a, b, n, op == RSLC_BIN_MOD);
         else
         {
            struct dx_opnd d[2], s[2];
            d[0] = op == RSLC_BIN_DIV ? t : dx_reg(DXO_NULL, 0, n);
            d[1] = op == RSLC_BIN_DIV ? dx_reg(DXO_NULL, 0, n) : t;
            s[0] = a; s[1] = b;
            dx_inst(e, DX_UDIV, 0, d, 2, s, 2);
         }
         break;
      case RSLC_BIN_LSHIFT:
         dx_op2(e, DX_ISHL, t, a, b);
         break;
      case RSLC_BIN_RSHIFT:
         dx_op2(e, sint ? DX_ISHR : DX_USHR, t, a, b);
         break;
      case RSLC_BIN_LT:
      case RSLC_BIN_GT:
      case RSLC_BIN_LE:
      case RSLC_BIN_GE:
      {
         bool swap = op == RSLC_BIN_GT || op == RSLC_BIN_LE;
         bool ge   = op == RSLC_BIN_GE || op == RSLC_BIN_LE;
         enum dx_op o = flt ? (ge ? DX_GE : DX_LT) : sint ? (ge ? DX_IGE : DX_ILT)
            : (ge ? DX_UGE : DX_ULT);
         dx_op2(e, o, t, swap ? b : a, swap ? a : b);
         break;
      }
      case RSLC_BIN_EQ:
      case RSLC_BIN_NE:
      {
         bool eq = op == RSLC_BIN_EQ;
         dx_op2(e, flt ? (eq ? DX_EQ : DX_NE) : (eq ? DX_IEQ : DX_INE), t, a, b);
         if (reduce && n > 1)
         {
            /* all equal, or any different */
            struct dx_opnd r = dx_tmp(e, 1);
            unsigned k;
            dx_op2(e, eq ? DX_AND : DX_OR, r, dx_comp(t, 0), dx_comp(t, 1));
            for (k = 2; k < n; k++)
               dx_op2(e, eq ? DX_AND : DX_OR, r, r, dx_comp(t, k));
            t = r;
         }
         break;
      }
      case RSLC_BIN_AND:
      case RSLC_BIN_BAND:
         dx_op2(e, DX_AND, t, a, b);
         break;
      case RSLC_BIN_OR:
      case RSLC_BIN_BOR:
         dx_op2(e, DX_OR, t, a, b);
         break;
      case RSLC_BIN_XOR:
      case RSLC_BIN_BXOR:
         dx_op2(e, DX_XOR, t, a, b);
         break;
      default:
         return dx_fail(e, "internal: binary operator", line);
   }
   *out = t;
   return !e->c->failed;
}

/* ------------------------------------------------------------------ */
/* Expressions                                                         */
/* ------------------------------------------------------------------ */

static bool dx_builtin(struct dx_emit *e, const struct rslc_node *n, struct dx_val *out);
static bool dx_call(struct dx_emit *e, const struct rslc_node *n, struct dx_val *out);

/* Whether evaluating a node can have an effect. */
static bool dx_pure(struct dx_emit *e, uint32_t node)
{
   const struct rslc_node *n;
   uint32_t i, cnt;
   if (!node)
      return true;
   n = rslc_node_at(e->c, node);
   switch (n->op)
   {
      case RSLC_OP_CONST: case RSLC_OP_VAR:
         return true;
      case RSLC_OP_ASSIGN: case RSLC_OP_CALL:
         return false;
      case RSLC_OP_UNARY:
         if (n->aux >= RSLC_UN_PREINC)
            return false;
         return dx_pure(e, n->a);
      case RSLC_OP_BUILTIN:
         if (n->a >= RSLC_BFN_ATOMICADD && n->a <= RSLC_BFN_IMAGESTORE)
            return false;
         if (n->a == RSLC_BFN_MODF || n->a == RSLC_BFN_FREXP || n->a == RSLC_BFN_UADDCARRY
               || n->a == RSLC_BFN_USUBBORROW || n->a == RSLC_BFN_UMULEXTENDED
               || n->a == RSLC_BFN_IMULEXTENDED || n->a == RSLC_BFN_EMITVERTEX
               || n->a == RSLC_BFN_ENDPRIMITIVE)
            return false;
         /* fall through */
      case RSLC_OP_CONSTRUCT:
         cnt = n->b ? rslc_list_len(e->c, n->b) : 0;
         for (i = 0; i < cnt; i++)
            if (!dx_pure(e, rslc_list_item(e->c, n->b, i)))
               return false;
         return true;
      default:
         return dx_pure(e, n->a) && dx_pure(e, n->b) && dx_pure(e, n->c);
   }
}

/* An immediate from a folded constant of up to four components. */
static struct dx_opnd dx_const_opnd(const struct rslc_type *t, const uint32_t *bits)
{
   uint32_t v[4];
   unsigned n = dx_width(t), i;
   for (i = 0; i < n; i++)
      v[i] = t->base == RSLC_BASE_BOOL ? (bits[i] ? ~0u : 0u) : bits[i];
   return dx_imm(v, n);
}

/* A constant node as a value: matrices in temporaries. */
static bool dx_const_val(struct dx_emit *e, uint32_t node, struct dx_val *out)
{
   const struct rslc_node *n = rslc_node_at(e->c, node);
   struct rslc_cval cv;
   memset(out, 0, sizeof(*out));
   if (n->op == RSLC_OP_CONST)
   {
      out->v = dx_const_opnd(n->type, &n->a);
      return true;
   }
   if (!rslc_const_eval(e->c, node, &cv))
      return dx_fail(e, "internal: constant", n->line);
   if (n->type->kind == RSLC_TY_MAT)
   {
      uint32_t k, r = dx_temp(e, n->type->cols);
      const struct rslc_type *col = rslc_type_basic(e->c, RSLC_TY_VEC, RSLC_BASE_FLOAT,
            n->type->rows, 1);
      for (k = 0; k < n->type->cols; k++)
         dx_mov(e, dx_reg(DXO_TEMP, r + k, n->type->rows),
               dx_const_opnd(col, cv.v + k * n->type->rows));
      out->agg = true;
      out->p   = dx_place_of(DXO_TEMP, r);
      return true;
   }
   out->v = dx_const_opnd(n->type, cv.v);
   return true;
}

/* A vector operand's value kept in a register of its own: what an
 * expression hands on when its source may change before it is read. */
static struct dx_opnd dx_own(struct dx_emit *e, struct dx_opnd v)
{
   if (v.file == DXO_IMM32)
      return v;
   return dx_fresh(e, v);
}

/* Column k of a matrix value. */
static struct dx_opnd dx_col(const struct dx_val *m, const struct rslc_type *t, uint32_t k)
{
   return dx_at(&m->p, k, t->rows);
}

/* Matrix arithmetic. */
static bool dx_mat_binop(struct dx_emit *e, const struct rslc_node *n, struct dx_val *a,
      struct dx_val *b, struct dx_val *out)
{
   const struct rslc_type *ta = rslc_node_at(e->c, n->a)->type;
   const struct rslc_type *tb = rslc_node_at(e->c, n->b)->type;
   const struct rslc_type *tr = n->type;
   uint32_t j, k;
   memset(out, 0, sizeof(*out));
   if (n->aux == RSLC_BIN_MUL && ta->kind == RSLC_TY_MAT && tb->kind != RSLC_TY_SCALAR)
   {
      if (tb->kind == RSLC_TY_VEC)
      {
         /* M * v: the columns weighed by v's components */
         struct dx_opnd r = dx_tmp(e, ta->rows);
         dx_op2(e, DX_MUL, r, dx_col(a, ta, 0), dx_splat(dx_comp(b->v, 0), ta->rows));
         for (k = 1; k < ta->cols; k++)
            dx_op3(e, DX_MAD, r, dx_col(a, ta, k), dx_splat(dx_comp(b->v, k), ta->rows), r);
         out->v = r;
         return true;
      }
      /* M * N: M times each column of N */
      {
         uint32_t base = dx_temp(e, tr->cols);
         for (j = 0; j < tr->cols; j++)
         {
            struct dx_opnd r = dx_reg(DXO_TEMP, base + j, ta->rows);
            struct dx_opnd cb = dx_col(b, tb, j);
            dx_op2(e, DX_MUL, r, dx_col(a, ta, 0), dx_splat(dx_comp(cb, 0), ta->rows));
            for (k = 1; k < ta->cols; k++)
               dx_op3(e, DX_MAD, r, dx_col(a, ta, k), dx_splat(dx_comp(cb, k), ta->rows), r);
         }
         out->agg = true;
         out->p   = dx_place_of(DXO_TEMP, base);
         return true;
      }
   }
   if (n->aux == RSLC_BIN_MUL && ta->kind == RSLC_TY_VEC && tb->kind == RSLC_TY_MAT)
   {
      /* v * M: v dotted with each column */
      struct dx_opnd r = dx_tmp(e, tb->cols);
      enum dx_op dp = tb->rows == 2 ? DX_DP2 : tb->rows == 3 ? DX_DP3 : DX_DP4;
      for (j = 0; j < tb->cols; j++)
      {
         struct dx_opnd d = dx_comp(r, j);
         d.swz[0] = (uint8_t)j;
         dx_op2(e, dp, d, a->v, dx_col(b, tb, j));
      }
      out->v = r;
      return true;
   }
   /* componentwise: column by column, a scalar against each */
   {
      const struct rslc_type *tm = ta->kind == RSLC_TY_MAT ? ta : tb;
      uint32_t base = dx_temp(e, tm->cols);
      enum dx_op op;
      bool sub = n->aux == RSLC_BIN_SUB;
      switch (n->aux)
      {
         case RSLC_BIN_ADD: case RSLC_BIN_SUB: op = DX_ADD; break;
         case RSLC_BIN_MUL: op = DX_MUL; break;
         case RSLC_BIN_DIV: op = DX_DIV; break;
         default:
            return dx_fail(e, "a matrix operator", n->line);
      }
      for (j = 0; j < tm->cols; j++)
      {
         struct dx_opnd x = ta->kind == RSLC_TY_MAT ? dx_col(a, ta, j)
            : dx_splat(dx_comp(a->v, 0), tm->rows);
         struct dx_opnd y = tb->kind == RSLC_TY_MAT ? dx_col(b, tb, j)
            : dx_splat(dx_comp(b->v, 0), tm->rows);
         if (sub)
            y = dx_negate(y, true);
         dx_op2(e, op, dx_reg(DXO_TEMP, base + j, tm->rows), x, y);
      }
      out->agg = true;
      out->p   = dx_place_of(DXO_TEMP, base);
      return true;
   }
}

/* The scalars of a constructor's arguments, one component at a time. */
static bool dx_construct(struct dx_emit *e, const struct rslc_node *n, struct dx_val *out)
{
   const struct rslc_type *t = n->type;
   uint32_t cnt = n->b ? rslc_list_len(e->c, n->b) : 0, i;
   memset(out, 0, sizeof(*out));
   if (t->kind == RSLC_TY_ARRAY || t->kind == RSLC_TY_STRUCT)
   {
      /* each argument into its element or member */
      uint32_t base = dx_temp(e, dx_slots(t));
      struct dx_path whole;
      dx_path_regs(&whole, t, dx_place_of(DXO_TEMP, base));
      for (i = 0; i < cnt; i++)
      {
         struct dx_path d = whole, s;
         if (t->kind == RSLC_TY_ARRAY)
            dx_path_elem(e, &d, i);
         else
            dx_path_member(e, &d, i);
         if (!dx_path_of(e, rslc_list_item(e->c, n->b, i), &s) || !dx_copy(e, &d, &s, n->line))
            return false;
      }
      out->agg = true;
      out->p   = whole.place;
      return true;
   }
   if (t->kind == RSLC_TY_MAT)
   {
      uint32_t base = dx_temp(e, t->cols), k;
      const struct rslc_node *a0 = rslc_node_at(e->c, rslc_list_item(e->c, n->b, 0));
      if (cnt == 1 && a0->type->kind == RSLC_TY_SCALAR)
      {
         /* the diagonal */
         struct dx_val s;
         struct dx_opnd v;
         if (!dx_rv(e, rslc_list_item(e->c, n->b, 0), &s))
            return false;
         v = dx_convert(e, s.v, a0->type->base, RSLC_BASE_FLOAT);
         for (k = 0; k < t->cols; k++)
         {
            dx_mov(e, dx_reg(DXO_TEMP, base + k, t->rows), dx_splat(dx_immf(0.0f), t->rows));
            if (k < t->rows)
            {
               struct dx_opnd d = dx_reg(DXO_TEMP, base + k, 1);
               d.swz[0] = (uint8_t)k;
               dx_mov(e, d, dx_comp(v, 0));
            }
         }
      }
      else if (cnt == 1 && a0->type->kind == RSLC_TY_MAT)
      {
         /* a matrix resized: its columns where they reach, identity past them */
         struct dx_val m;
         const struct rslc_type *st = a0->type;
         if (!dx_rv(e, rslc_list_item(e->c, n->b, 0), &m))
            return false;
         for (k = 0; k < t->cols; k++)
         {
            uint32_t r;
            for (r = 0; r < t->rows; r++)
            {
               struct dx_opnd d = dx_reg(DXO_TEMP, base + k, 1);
               d.swz[0] = (uint8_t)r;
               if (k < st->cols && r < st->rows)
                  dx_mov(e, d, dx_comp(dx_col(&m, st, k), r));
               else
                  dx_mov(e, d, dx_immf(k == r ? 1.0f : 0.0f));
            }
         }
      }
      else
      {
         /* scalars and vectors filling the columns in order */
         uint32_t at = 0;
         for (i = 0; i < cnt; i++)
         {
            const struct rslc_node *an = rslc_node_at(e->c, rslc_list_item(e->c, n->b, i));
            struct dx_val a;
            uint32_t w, c;
            if (!dx_rv(e, rslc_list_item(e->c, n->b, i), &a))
               return false;
            if (a.agg)
            {
               /* a matrix argument: column by column */
               for (c = 0; c < an->type->cols; c++)
               {
                  uint32_t r;
                  for (r = 0; r < an->type->rows; r++, at++)
                  {
                     struct dx_opnd d = dx_reg(DXO_TEMP, base + at / t->rows, 1);
                     d.swz[0] = (uint8_t)(at % t->rows);
                     dx_mov(e, d, dx_comp(dx_col(&a, an->type, c), r));
                  }
               }
               continue;
            }
            a.v = dx_convert(e, a.v, an->type->base, RSLC_BASE_FLOAT);
            w = a.v.n;
            for (c = 0; c < w; c++, at++)
            {
               struct dx_opnd d = dx_reg(DXO_TEMP, base + at / t->rows, 1);
               d.swz[0] = (uint8_t)(at % t->rows);
               dx_mov(e, d, dx_comp(a.v, c));
            }
         }
      }
      out->agg = true;
      out->p   = dx_place_of(DXO_TEMP, base);
      return true;
   }
   {
      /* a scalar or vector */
      unsigned w = dx_width(t), at = 0;
      struct dx_opnd r = dx_tmp(e, w);
      if (cnt == 1)
      {
         const struct rslc_node *an = rslc_node_at(e->c, rslc_list_item(e->c, n->b, 0));
         struct dx_val a;
         if (!dx_rv(e, rslc_list_item(e->c, n->b, 0), &a))
            return false;
         if (!a.agg && (an->type->kind == RSLC_TY_SCALAR || a.v.n >= w))
         {
            struct dx_opnd v = a.v;
            if (an->type->kind == RSLC_TY_SCALAR)
               v = dx_splat(dx_comp(v, 0), w);
            else
               v.n = (uint8_t)w;
            out->v = dx_convert(e, v, an->type->base, t->base);
            return true;
         }
      }
      for (i = 0; i < cnt && at < w; i++)
      {
         const struct rslc_node *an = rslc_node_at(e->c, rslc_list_item(e->c, n->b, i));
         struct dx_val a;
         unsigned c;
         if (!dx_rv(e, rslc_list_item(e->c, n->b, i), &a))
            return false;
         if (a.agg)
         {
            /* a matrix's components, column by column */
            uint32_t k, rr;
            for (k = 0; k < an->type->cols && at < w; k++)
               for (rr = 0; rr < an->type->rows && at < w; rr++, at++)
               {
                  struct dx_opnd d = dx_comp(r, at);
                  d.swz[0] = (uint8_t)at;
                  dx_mov(e, d, dx_convert(e, dx_comp(dx_col(&a, an->type, k), rr),
                           RSLC_BASE_FLOAT, t->base));
               }
            continue;
         }
         a.v = dx_convert(e, a.v, an->type->base, t->base);
         for (c = 0; c < a.v.n && at < w; c++, at++)
         {
            struct dx_opnd d = dx_comp(r, at);
            d.swz[0] = (uint8_t)at;
            dx_mov(e, d, dx_comp(a.v, c));
         }
      }
      out->v = r;
      return true;
   }
}

/* Stores a value to an lvalue node's path; aggregates register by
 * register. */
static bool dx_assign_to(struct dx_emit *e, const struct dx_path *p, const struct dx_val *v,
      uint32_t line)
{
   if (v->agg)
   {
      struct dx_path s;
      dx_path_regs(&s, p->type, v->p);
      return dx_copy(e, p, &s, line);
   }
   return dx_store(e, p, v->v, line);
}

/* a op b where either side may be a matrix or a scalar widened. */
static bool dx_rv_binary(struct dx_emit *e, const struct rslc_node *n, unsigned op,
      struct dx_val *a, struct dx_val *b, struct dx_val *out)
{
   const struct rslc_type *ta = rslc_node_at(e->c, n->a)->type;
   const struct rslc_type *tb = rslc_node_at(e->c, n->b)->type;
   const struct rslc_type *base = ta->kind == RSLC_TY_SCALAR ? tb : ta;
   unsigned w;
   bool reduce = op == RSLC_BIN_EQ || op == RSLC_BIN_NE;
   if (a->agg || b->agg)
   {
      struct rslc_node m = *n;
      m.aux = (uint16_t)op;
      if (reduce)
         return dx_fail(e, "== or != of aggregates", n->line);
      return dx_mat_binop(e, &m, a, b, out);
   }
   w = dx_width(base);
   if (reduce)
      w = dx_width(ta);
   memset(out, 0, sizeof(*out));
   return dx_binop(e, op, ta->base, a->v, b->v, w, reduce, &out->v, n->line);
}

static bool dx_rv(struct dx_emit *e, uint32_t node, struct dx_val *out)
{
   const struct rslc_node *n = rslc_node_at(e->c, node);
   const struct rslc_type *t = n->type;
   memset(out, 0, sizeof(*out));
   switch (n->op)
   {
      case RSLC_OP_CONST:
         return dx_const_val(e, node, out);
      case RSLC_OP_VAR:
         if (e->vars[n->a].folded)
            return dx_const_val(e, node, out);
         /* fall through */
      case RSLC_OP_MEMBER:
      case RSLC_OP_INDEX:
      {
         struct dx_path p;
         return dx_path_of(e, node, &p) && dx_rv_path(e, &p, out);
      }
      case RSLC_OP_SWIZZLE:
      {
         struct dx_val b;
         unsigned cnt = n->aux & 3u, i;
         struct dx_opnd s;
         if (!dx_rv(e, n->a, &b))
            return false;
         if (!cnt)
            cnt = 4;
         s = b.v;
         if (rslc_node_at(e->c, n->a)->type->kind == RSLC_TY_SCALAR)
         {
            out->v = dx_splat(dx_comp(s, 0), cnt);
            return true;
         }
         for (i = 0; i < cnt; i++)
         {
            unsigned c = (n->aux >> (2 + 2 * i)) & 3u;
            s.swz[i] = b.v.swz[c];
            s.imm[i] = b.v.imm[c];
         }
         s.n = (uint8_t)cnt;
         out->v = s;
         return true;
      }
      case RSLC_OP_CONVERT:
      {
         const struct rslc_type *ft = rslc_node_at(e->c, n->a)->type;
         if (!dx_rv(e, n->a, out))
            return false;
         if (out->agg)
            return true;   /* matrices are float alone */
         out->v = dx_convert(e, out->v, ft->base, t->base);
         return true;
      }
      case RSLC_OP_UNARY:
      {
         const struct rslc_type *at = rslc_node_at(e->c, n->a)->type;
         if (n->aux >= RSLC_UN_PREINC)
         {
            /* ++ and --: read, step, write back */
            struct dx_path p;
            struct dx_opnd cur, nv, one;
            uint32_t one_bits = dx_one_bits(at);
            bool inc  = n->aux == RSLC_UN_PREINC || n->aux == RSLC_UN_POSTINC;
            bool post = n->aux == RSLC_UN_POSTINC || n->aux == RSLC_UN_POSTDEC;
            if (!dx_path_of(e, n->a, &p) || !dx_load(e, &p, &cur))
               return false;
            cur = dx_own(e, cur);
            one = dx_imm(&one_bits, 1);
            if (!dx_binop(e, inc ? RSLC_BIN_ADD : RSLC_BIN_SUB, at->base, cur, one, cur.n,
                     false, &nv, n->line) || !dx_store(e, &p, nv, n->line))
               return false;
            out->v = post ? cur : nv;
            return true;
         }
         if (!dx_rv(e, n->a, out))
            return false;
         if (out->agg)
         {
            /* -M */
            uint32_t base = dx_temp(e, at->cols), k;
            for (k = 0; k < at->cols; k++)
               dx_mov(e, dx_reg(DXO_TEMP, base + k, at->rows), dx_negate(dx_col(out, at, k), true));
            out->p = dx_place_of(DXO_TEMP, base);
            return true;
         }
         switch (n->aux)
         {
            case RSLC_UN_NEG:
               if (dx_is_float(at) || out->v.file == DXO_IMM32)
                  out->v = dx_negate(out->v, dx_is_float(at));
               else
               {
                  struct dx_opnd r = dx_tmp(e, out->v.n);
                  dx_op1(e, DX_INEG, r, out->v);
                  out->v = r;
               }
               return true;
            case RSLC_UN_NOT:
            case RSLC_UN_BITNOT:
            {
               struct dx_opnd r = dx_tmp(e, out->v.n);
               dx_op1(e, DX_NOT, r, out->v);
               out->v = r;
               return true;
            }
            default:
               return dx_fail(e, "internal: unary operator", n->line);
         }
      }
      case RSLC_OP_BINARY:
      {
         struct dx_val a, b;
         if ((n->aux == RSLC_BIN_AND || n->aux == RSLC_BIN_OR) && !dx_pure(e, n->b))
         {
            /* a side effect on the right runs only when the left
             * does not decide */
            struct dx_opnd r = dx_tmp(e, 1);
            if (!dx_rv(e, n->a, &a))
               return false;
            dx_mov(e, r, a.v);
            dx_cond(e, DX_IF, n->aux == RSLC_BIN_AND, r);
            if (!dx_rv(e, n->b, &b))
               return false;
            dx_mov(e, r, b.v);
            dx_op0(e, DX_ENDIF, 0);
            out->v = r;
            return true;
         }
         if (!dx_rv(e, n->a, &a))
            return false;
         if (!a.agg && !dx_pure(e, n->b))
            a.v = dx_own(e, a.v);
         if (!dx_rv(e, n->b, &b))
            return false;
         return dx_rv_binary(e, n, n->aux, &a, &b, out);
      }
      case RSLC_OP_ASSIGN:
      {
         struct dx_path p;
         struct dx_val v;
         if (!dx_path_of(e, n->a, &p))
            return false;
         if (n->aux != RSLC_BIN_NONE)
         {
            struct dx_val cur, rhs;
            if (!dx_rv_path(e, &p, &cur))
               return false;
            if (!cur.agg)
               cur.v = dx_own(e, cur.v);
            if (!dx_rv(e, n->b, &rhs))
               return false;
            {
               /* the compound operator as a binary node of the same operands */
               struct rslc_node m = *n;
               m.op = RSLC_OP_BINARY;
               if (!dx_rv_binary(e, &m, n->aux, &cur, &rhs, &v))
                  return false;
            }
         }
         else if (!dx_rv(e, n->b, &v))
            return false;
         if (!dx_assign_to(e, &p, &v, n->line))
            return false;
         *out = v;
         return true;
      }
      case RSLC_OP_TERNARY:
      {
         struct dx_val c, a, b;
         if (!dx_rv(e, n->a, &c))
            return false;
         if (t->kind != RSLC_TY_SCALAR && t->kind != RSLC_TY_VEC)
         {
            /* aggregates: a branch each into the result's registers */
            struct dx_path r;
            struct dx_opnd cc = dx_own(e, dx_comp(c.v, 0));
            dx_path_regs(&r, t, dx_place_of(DXO_TEMP, dx_temp(e, dx_slots(t))));
            dx_cond(e, DX_IF, true, cc);
            if (!dx_rv(e, n->b, &a) || !dx_assign_to(e, &r, &a, n->line))
               return false;
            dx_op0(e, DX_ELSE, 0);
            if (!dx_rv(e, n->c, &b) || !dx_assign_to(e, &r, &b, n->line))
               return false;
            dx_op0(e, DX_ENDIF, 0);
            out->agg = true;
            out->p   = r.place;
            return true;
         }
         if (dx_pure(e, n->b) && dx_pure(e, n->c))
         {
            unsigned w = dx_width(t);
            struct dx_opnd r = dx_tmp(e, w);
            if (!dx_rv(e, n->b, &a) || !dx_rv(e, n->c, &b))
               return false;
            dx_op3(e, DX_MOVC, r, dx_splat(dx_comp(c.v, 0), w), dx_widen(a.v, w),
                  dx_widen(b.v, w));
            out->v = r;
            return true;
         }
         {
            unsigned w = dx_width(t);
            struct dx_opnd r = dx_tmp(e, w);
            struct dx_opnd cc = dx_own(e, dx_comp(c.v, 0));
            dx_cond(e, DX_IF, true, cc);
            if (!dx_rv(e, n->b, &a))
               return false;
            dx_mov(e, r, dx_widen(a.v, w));
            dx_op0(e, DX_ELSE, 0);
            if (!dx_rv(e, n->c, &b))
               return false;
            dx_mov(e, r, dx_widen(b.v, w));
            dx_op0(e, DX_ENDIF, 0);
            out->v = r;
            return true;
         }
      }
      case RSLC_OP_SEQ:
      {
         struct dx_val a;
         if (!dx_rv(e, n->a, &a))
            return false;
         return dx_rv(e, n->b, out);
      }
      case RSLC_OP_CONSTRUCT:
         if (t->kind != RSLC_TY_ARRAY && t->kind != RSLC_TY_STRUCT && dx_is_const(e, node))
            return dx_const_val(e, node, out);
         return dx_construct(e, n, out);
      case RSLC_OP_CALL:
         return dx_call(e, n, out);
      case RSLC_OP_BUILTIN:
         return dx_builtin(e, n, out);
      default:
         break;
   }
   return dx_fail(e, "internal: expression", n->line);
}

/* ------------------------------------------------------------------ */
/* Builtins                                                            */
/* ------------------------------------------------------------------ */

/* A texture argument: the t and s registers and the kind. */
struct dx_tex
{
   uint32_t t;
   uint8_t  kind;
   uint8_t  base;
};

static bool dx_tex_of(struct dx_emit *e, uint32_t node, struct dx_tex *tx)
{
   const struct rslc_node *n = rslc_node_at(e->c, node);
   uint32_t k = 0;
   const struct rslc_var *v;
   const struct rslc_type *t;
   if (n->op == RSLC_OP_INDEX)
   {
      struct rslc_cval cv;
      if (!dx_is_const(e, n->b) || !rslc_const_eval(e->c, n->b, &cv))
         return dx_fail(e, "a sampler array indexed with a variable", n->line);
      k = cv.v[0];
      n = rslc_node_at(e->c, n->a);
   }
   if (n->op != RSLC_OP_VAR)
      return dx_fail(e, "a sampler that is not a variable", n->line);
   v = e->c->prog.vars[n->a];
   t = v->type->kind == RSLC_TY_ARRAY ? v->type->elem : v->type;
   if (v->storage == RSLC_ST_PARAM)
   {
      /* a sampler parameter: the texture the call being expanded binds */
      if (v->type->kind == RSLC_TY_ARRAY)
         return dx_fail(e, "an array of samplers as a parameter", n->line);
      tx->t    = e->tex_bind[v->id];
      tx->kind = t->kind;
      tx->base = t->base;
      return true;
   }
   tx->t    = (v->binding == RSLC_UNSET ? 0 : v->binding) + k;
   tx->kind = t->kind;
   tx->base = t->base;
   return true;
}

static struct dx_opnd dx_tex_res(const struct dx_tex *tx)
{
   return dx_reg(DXO_RESOURCE, tx->t, 4);
}

static struct dx_opnd dx_tex_smp(const struct dx_tex *tx)
{
   struct dx_opnd s = dx_reg(DXO_SAMPLER, tx->t, 1);
   return s;
}

/* An immediate texel offset of up to three components, for the
 * extended opcode token. */
static bool dx_offset_token(struct dx_emit *e, uint32_t node, uint32_t *ext)
{
   struct rslc_cval cv;
   const struct rslc_node *n = rslc_node_at(e->c, node);
   unsigned k, w = dx_width(n->type);
   if (!dx_is_const(e, node) || !rslc_const_eval(e->c, node, &cv))
      return dx_fail(e, "a texel offset that is not constant", n->line);
   *ext = 1;   /* sample controls */
   for (k = 0; k < w && k < 3; k++)
      *ext |= (cv.v[k] & 15u) << (9 + 4 * k);
   return true;
}

/* An integer address for ld: the coordinates, the layer, then the mip
 * level in the last component the dimension reads. */
static struct dx_opnd dx_ld_addr(struct dx_emit *e, struct dx_opnd coord, unsigned cn,
      struct dx_opnd lod, unsigned lod_comp)
{
   struct dx_opnd a = dx_tmp(e, 4), d;
   d = a;
   d.n = (uint8_t)cn;
   dx_mov(e, d, coord);
   d = dx_comp(a, lod_comp);
   d.swz[0] = (uint8_t)lod_comp;
   dx_mov(e, d, dx_comp(lod, 0));
   return a;
}

static bool dx_texture(struct dx_emit *e, const struct rslc_node *n, unsigned bfn,
      struct dx_opnd *args, uint32_t cnt, struct dx_val *out)
{
   struct dx_tex tx;
   struct dx_opnd r = dx_tmp(e, 4), src[5];
   uint32_t ext = 0;
   bool arr;
   if (!dx_tex_of(e, rslc_list_item(e->c, n->b, 0), &tx))
      return false;
   arr = tx.kind == RSLC_TY_SAMPLER2DARRAY;
   switch (bfn)
   {
      case RSLC_BFN_TEXTURE:
      case RSLC_BFN_TEXTUREOFFSET:
      case RSLC_BFN_TEXTUREPROJ:
      case RSLC_BFN_TEXTUREPROJOFFSET:
      {
         struct dx_opnd coord = args[1];
         unsigned bias_at = bfn == RSLC_BFN_TEXTUREOFFSET || bfn == RSLC_BFN_TEXTUREPROJOFFSET
            ? 3 : 2;
         if (bfn == RSLC_BFN_TEXTUREOFFSET || bfn == RSLC_BFN_TEXTUREPROJOFFSET)
            if (!dx_offset_token(e, rslc_list_item(e->c, n->b, 2), &ext))
               return false;
         if (bfn == RSLC_BFN_TEXTUREPROJ || bfn == RSLC_BFN_TEXTUREPROJOFFSET)
         {
            struct dx_opnd c = dx_tmp(e, 2);
            dx_op2(e, DX_DIV, c, coord, dx_splat(dx_comp(coord, coord.n - 1), 2));
            coord = c;
         }
         src[0] = coord;
         src[1] = dx_tex_res(&tx);
         src[2] = dx_tex_smp(&tx);
         if (cnt > bias_at)
         {
            src[3] = dx_comp(args[bias_at], 0);
            if (!dx_inst_ext(e, DX_SAMPLE_B, 0, ext, &r, 1, src, 4))
               return false;
         }
         else if (!dx_inst_ext(e, DX_SAMPLE, 0, ext, &r, 1, src, 3))
            return false;
         break;
      }
      case RSLC_BFN_TEXTURELOD:
      case RSLC_BFN_TEXTURELODOFFSET:
      case RSLC_BFN_TEXTUREPROJLOD:
      case RSLC_BFN_TEXTUREPROJLODOFFSET:
      {
         struct dx_opnd coord = args[1];
         if (bfn == RSLC_BFN_TEXTURELODOFFSET || bfn == RSLC_BFN_TEXTUREPROJLODOFFSET)
            if (!dx_offset_token(e, rslc_list_item(e->c, n->b, 3), &ext))
               return false;
         if (bfn == RSLC_BFN_TEXTUREPROJLOD || bfn == RSLC_BFN_TEXTUREPROJLODOFFSET)
         {
            struct dx_opnd c = dx_tmp(e, 2);
            dx_op2(e, DX_DIV, c, coord, dx_splat(dx_comp(coord, coord.n - 1), 2));
            coord = c;
         }
         src[0] = coord;
         src[1] = dx_tex_res(&tx);
         src[2] = dx_tex_smp(&tx);
         src[3] = dx_comp(args[2], 0);
         if (!dx_inst_ext(e, DX_SAMPLE_L, 0, ext, &r, 1, src, 4))
            return false;
         break;
      }
      case RSLC_BFN_TEXTUREGRAD:
      case RSLC_BFN_TEXTUREGRADOFFSET:
         if (bfn == RSLC_BFN_TEXTUREGRADOFFSET
               && !dx_offset_token(e, rslc_list_item(e->c, n->b, 4), &ext))
            return false;
         src[0] = args[1];
         src[1] = dx_tex_res(&tx);
         src[2] = dx_tex_smp(&tx);
         src[3] = args[2];
         src[4] = args[3];
         if (!dx_inst_ext(e, DX_SAMPLE_D, 0, ext, &r, 1, src, 5))
            return false;
         break;
      case RSLC_BFN_TEXTUREGATHER:
      case RSLC_BFN_TEXTUREGATHEROFFSET:
      {
         unsigned comp_at = bfn == RSLC_BFN_TEXTUREGATHEROFFSET ? 3 : 2;
         struct rslc_cval cv;
         uint32_t comp = 0;
         if (bfn == RSLC_BFN_TEXTUREGATHEROFFSET
               && !dx_offset_token(e, rslc_list_item(e->c, n->b, 2), &ext))
            return false;
         if (cnt > comp_at)
         {
            if (!rslc_const_eval(e->c, rslc_list_item(e->c, n->b, comp_at), &cv))
               return dx_fail(e, "a gather component that is not constant", n->line);
            comp = cv.v[0] & 3u;
         }
         if (e->sm < 50 && comp)
            return dx_fail(e, "gathering a component past x needs shader model 5", n->line);
         src[0] = args[1];
         src[1] = dx_tex_res(&tx);
         src[2] = dx_tex_smp(&tx);
         if (e->sm >= 50)
         {
            src[2].n = 4;
            src[2].swz[0] = (uint8_t)comp;
         }
         if (!dx_inst_ext(e, DX_GATHER4, 0, ext, &r, 1, src, 3))
            return false;
         break;
      }
      case RSLC_BFN_TEXELFETCH:
      case RSLC_BFN_TEXELFETCHOFFSET:
      {
         struct dx_opnd addr;
         if (bfn == RSLC_BFN_TEXELFETCHOFFSET
               && !dx_offset_token(e, rslc_list_item(e->c, n->b, 3), &ext))
            return false;
         if (tx.kind == RSLC_TY_SAMPLERBUFFER)
         {
            src[0] = dx_splat(dx_comp(args[1], 0), 4);
            src[1] = dx_tex_res(&tx);
            if (!dx_inst(e, DX_LD, 0, &r, 1, src, 2))
               return false;
            break;
         }
         if (tx.kind == RSLC_TY_SAMPLER2DMSARRAY)
         {
            addr = dx_ld_addr(e, args[1], 3, dx_immu(0), 3);
            src[0] = addr;
            src[1] = dx_tex_res(&tx);
            src[2] = dx_comp(args[2], 0);
            if (!dx_inst(e, DX_LD_MS, 0, &r, 1, src, 3))
               return false;
            break;
         }
         addr = dx_ld_addr(e, args[1], arr ? 3 : 2, args[2], arr ? 3 : 2);
         src[0] = addr;
         src[1] = dx_tex_res(&tx);
         if (!dx_inst_ext(e, DX_LD, 0, ext, &r, 1, src, 2))
            return false;
         break;
      }
      case RSLC_BFN_TEXTURESIZE:
      case RSLC_BFN_TEXTUREQUERYLEVELS:
      {
         unsigned w = dx_width(n->type);
         if (tx.kind == RSLC_TY_SAMPLERBUFFER)
         {
            struct dx_opnd d = dx_tmp(e, 1);
            src[0] = dx_tex_res(&tx);
            if (e->sm < 50)
               return dx_fail(e, "a buffer's size needs shader model 5", n->line);
            if (!dx_inst(e, DX_BUFINFO, 0, &d, 1, src, 1))
               return false;
            out->v = d;
            return true;
         }
         src[0] = bfn == RSLC_BFN_TEXTURESIZE && cnt > 1 ? dx_comp(args[1], 0) : dx_immu(0);
         src[1] = dx_tex_res(&tx);
         if (!dx_inst(e, DX_RESINFO, DX_RESINFO_UINT, &r, 1, src, 2))
            return false;
         if (bfn == RSLC_BFN_TEXTUREQUERYLEVELS)
            out->v = dx_comp(r, 3);
         else
         {
            out->v = r;
            out->v.n = (uint8_t)w;
         }
         return true;
      }
      case RSLC_BFN_TEXTUREQUERYLOD:
      {
         if (e->sm < 41)
            return dx_fail(e, "textureQueryLod needs shader model 4.1", n->line);
         src[0] = args[1];
         src[1] = dx_tex_res(&tx);
         src[2] = dx_tex_smp(&tx);
         if (!dx_inst(e, DX_LOD, 0, &r, 1, src, 3))
            return false;
         /* the clamped level first, the unclamped one next, as GLSL */
         out->v = r;
         out->v.swz[0] = 0;
         out->v.swz[1] = 1;
         out->v.n = 2;
         return true;
      }
      default:
         return dx_fail(e, "a texture function with no form here", n->line);
   }
   out->v = r;
   out->v.n = 4;
   return true;
}

/* The view of an image argument. */
static bool dx_image_of(struct dx_emit *e, uint32_t node, const struct rslc_var **iv)
{
   const struct rslc_node *n = rslc_node_at(e->c, node);
   if (n->op != RSLC_OP_VAR || e->c->prog.vars[n->a]->storage != RSLC_ST_IMAGE)
      return dx_fail(e, "an image that is not a variable", n->line);
   *iv = e->c->prog.vars[n->a];
   return true;
}

/* imageLoad, imageStore and imageSize on u<binding>. */
static bool dx_image(struct dx_emit *e, const struct rslc_node *n, unsigned bfn,
      struct dx_val *out)
{
   const struct rslc_var *iv;
   struct dx_opnd u, src[3], r;
   struct dx_val coord, val;
   bool arr;
   if (!dx_image_of(e, rslc_list_item(e->c, n->b, 0), &iv))
      return false;
   arr = iv->type->kind == RSLC_TY_IMAGE2DARRAY;
   u = dx_reg(DXO_UAV, iv->binding == RSLC_UNSET ? 0 : iv->binding, 4);
   memset(out, 0, sizeof(*out));
   if (bfn == RSLC_BFN_IMAGESIZE)
   {
      r = dx_tmp(e, 4);
      src[0] = dx_immu(0);
      src[1] = u;
      if (!dx_inst(e, DX_RESINFO, DX_RESINFO_UINT, &r, 1, src, 2))
         return false;
      out->v = r;
      out->v.n = (uint8_t)(arr ? 3 : 2);
      return true;
   }
   if (!dx_rv(e, rslc_list_item(e->c, n->b, 1), &coord))
      return false;
   if (bfn == RSLC_BFN_IMAGELOAD)
   {
      if (iv->type->cols != RSLC_IMGFMT_R32F && iv->type->cols != RSLC_IMGFMT_R32I
            && iv->type->cols != RSLC_IMGFMT_R32UI)
         return dx_fail(e, "a typed load of a format other than r32", n->line);
      r = dx_tmp(e, 4);
      src[0] = dx_widen(coord.v, 4);
      src[1] = u;
      if (!dx_inst(e, DX_LD_UAV_TYPED, 0, &r, 1, src, 2))
         return false;
      out->v = r;
      return true;
   }
   coord.v = dx_own(e, coord.v);
   if (!dx_rv(e, rslc_list_item(e->c, n->b, 2), &val))
      return false;
   u.n = 4;
   src[0] = dx_widen(coord.v, 4);
   src[1] = val.v;
   out->v = dx_immu(0);
   return dx_inst(e, DX_STORE_UAV_TYPED, 0, &u, 1, src, 2);
}

/* An atomic on a storage buffer member: the old value back. */
static bool dx_atomic(struct dx_emit *e, const struct rslc_node *n, unsigned bfn,
      struct dx_val *out)
{
   struct dx_path p;
   struct dx_val  v, cmp;
   struct dx_opnd r = dx_tmp(e, 1), d[2], src[4];
   bool sint = n->type->base == RSLC_BASE_INT;
   enum dx_op op;
   unsigned ns = 0;
   if (!dx_path_of(e, rslc_list_item(e->c, n->b, 0), &p))
      return false;
   if (p.kind != DXP_RAW && p.kind != DXP_TGSM)
      return dx_fail(e, "an atomic on what is not a storage buffer member", n->line);
   if (p.kind == DXP_RAW && e->c->prog.blocks[p.block].readonly)
      return dx_fail(e, "an atomic on a readonly buffer", n->line);
   if (!dx_rv(e, rslc_list_item(e->c, n->b, 1), &v))
      return false;
   switch (bfn)
   {
      case RSLC_BFN_ATOMICADD:      op = DX_IMM_ATOMIC_IADD; break;
      case RSLC_BFN_ATOMICMIN:      op = sint ? DX_IMM_ATOMIC_IMIN : DX_IMM_ATOMIC_UMIN; break;
      case RSLC_BFN_ATOMICMAX:      op = sint ? DX_IMM_ATOMIC_IMAX : DX_IMM_ATOMIC_UMAX; break;
      case RSLC_BFN_ATOMICAND:      op = DX_IMM_ATOMIC_AND; break;
      case RSLC_BFN_ATOMICOR:       op = DX_IMM_ATOMIC_OR; break;
      case RSLC_BFN_ATOMICXOR:      op = DX_IMM_ATOMIC_XOR; break;
      case RSLC_BFN_ATOMICEXCHANGE: op = DX_IMM_ATOMIC_EXCH; break;
      default:                      op = DX_IMM_ATOMIC_CMP_EXCH; break;
   }
   d[0] = r;
   d[1] = dx_raw_res(e, &p);
   d[1].n = 1; d[1].swz[0] = 0;
   src[ns++] = dx_raw_addr(e, &p, 0);
   if (op == DX_IMM_ATOMIC_CMP_EXCH)
   {
      /* atomicCompSwap(mem, compare, data) */
      if (!dx_rv(e, rslc_list_item(e->c, n->b, 2), &cmp))
         return false;
      src[ns++] = dx_comp(v.v, 0);
      src[ns++] = dx_comp(cmp.v, 0);
   }
   else
      src[ns++] = dx_comp(v.v, 0);
   if (!dx_inst(e, op, 0, d, 2, src, ns))
      return false;
   out->v = r;
   return true;
}

/* GLSL's findMSB from firstbit_hi, which counts from the top. */
static struct dx_opnd dx_find_msb(struct dx_emit *e, struct dx_opnd v, unsigned w, bool sint)
{
   struct dx_opnd r = dx_tmp(e, w), m = dx_tmp(e, w), t = dx_tmp(e, w);
   dx_op1(e, sint ? DX_FIRSTBIT_SHI : DX_FIRSTBIT_HI, r, v);
   dx_op2(e, DX_IEQ, m, r, dx_splat(dx_immu(~0u), w));
   dx_op2(e, DX_IADD, t, dx_negate(r, false), dx_splat(dx_immu(31), w));
   dx_op3(e, DX_MOVC, r, m, dx_splat(dx_immu(~0u), w), t);
   return r;
}

static bool dx_write_outputs(struct dx_emit *e);

static bool dx_builtin(struct dx_emit *e, const struct rslc_node *n, struct dx_val *out)
{
   unsigned bfn = n->a;
   uint32_t cnt = n->b ? rslc_list_len(e->c, n->b) : 0, i;
   struct dx_opnd a[5];
   const struct rslc_type *t0 = cnt ? rslc_node_at(e->c, rslc_list_item(e->c, n->b, 0))->type
      : n->type;
   unsigned w = dx_width(n->type);
   unsigned w0 = dx_width(t0);
   bool flt = t0->base == RSLC_BASE_FLOAT, sint = t0->base == RSLC_BASE_INT;
   struct dx_opnd r;

   memset(out, 0, sizeof(*out));
   if (bfn >= RSLC_BFN_ATOMICADD && bfn <= RSLC_BFN_ATOMICCOMPSWAP)
      return dx_atomic(e, n, bfn, out);
   if (bfn == RSLC_BFN_SUBPASSLOAD || (bfn >= RSLC_BFN_SUBGROUPELECT && bfn <= RSLC_BFN_SUBGROUPMAX))
      return dx_fail(e, "no form of it in Direct3D bytecode", n->line);
   if (bfn >= RSLC_BFN_BARRIER && bfn <= RSLC_BFN_GROUPMEMORYBARRIER)
   {
      /* thread sync 1 << 11, workgroup memory 1 << 12, views in the
       * group 1 << 13, views everywhere 1 << 14 */
      uint32_t fl;
      switch (bfn)
      {
         case RSLC_BFN_BARRIER:             fl = (1u << 11) | (1u << 12); break;
         case RSLC_BFN_MEMORYBARRIERSHARED: fl = 1u << 12; break;
         case RSLC_BFN_GROUPMEMORYBARRIER:  fl = (1u << 12) | (1u << 13); break;
         case RSLC_BFN_MEMORYBARRIER:       fl = (1u << 12) | (1u << 14); break;
         default:                           fl = 1u << 14; break;
      }
      if (e->c->stage != RSLC_STAGE_COMPUTE)
      {
         if (bfn == RSLC_BFN_BARRIER)
            return dx_fail(e, "barrier() outside a compute stage", n->line);
         fl &= 1u << 14;   /* only device memory orders outside a workgroup */
         if (!fl)
            return true;
      }
      out->v = dx_immu(0);
      return dx_op0(e, DX_SYNC, fl);
   }
   if (bfn >= RSLC_BFN_IMAGESTORE && bfn <= RSLC_BFN_IMAGESIZE)
      return dx_image(e, n, bfn, out);
   if (bfn == RSLC_BFN_EMITVERTEX || bfn == RSLC_BFN_ENDPRIMITIVE)
   {
      /* the outputs written, then the vertex; stream 0 on model 5 */
      struct dx_opnd m = dx_reg(DXO_STREAM, 0, 0);
      out->v = dx_immu(0);
      if (e->c->stage != RSLC_STAGE_GEOMETRY)
         return dx_fail(e, "EmitVertex and EndPrimitive belong to a geometry stage", n->line);
      if (bfn == RSLC_BFN_EMITVERTEX && !dx_write_outputs(e))
         return false;
      if (e->sm >= 50)
         return dx_inst(e, bfn == RSLC_BFN_EMITVERTEX ? DX_EMIT_STREAM : DX_CUT_STREAM, 0,
               NULL, 0, &m, 1);
      return dx_op0(e, bfn == RSLC_BFN_EMITVERTEX ? DX_EMIT : DX_CUT, 0);
   }

   /* the arguments' values; samplers stay unevaluated */
   if (cnt > 5)
      return dx_fail(e, "internal: builtin arity", n->line);
   for (i = 0; i < cnt; i++)
   {
      const struct rslc_node *an = rslc_node_at(e->c, rslc_list_item(e->c, n->b, i));
      struct dx_val v;
      memset(&a[i], 0, sizeof(a[i]));
      if (RSLC_TY_IS_SAMPLER(an->type->kind)
            || (an->type->kind == RSLC_TY_ARRAY && RSLC_TY_IS_SAMPLER(an->type->elem->kind)))
         continue;
      /* out parameters are written below */
      if ((bfn == RSLC_BFN_MODF && i == 1) || (bfn == RSLC_BFN_FREXP && i == 1)
            || ((bfn == RSLC_BFN_UADDCARRY || bfn == RSLC_BFN_USUBBORROW) && i == 2)
            || ((bfn == RSLC_BFN_UMULEXTENDED || bfn == RSLC_BFN_IMULEXTENDED) && i >= 2))
         continue;
      if (!dx_rv(e, rslc_list_item(e->c, n->b, i), &v))
         return false;
      if (v.agg)
      {
         if (bfn != RSLC_BFN_TRANSPOSE && bfn != RSLC_BFN_MATRIXCOMPMULT)
            return dx_fail(e, "a matrix argument with no form here", n->line);
         a[i] = dx_reg(DXO_TEMP, 0, 4);
         out->p = v.p;   /* kept for the matrix builtins below */
         continue;
      }
      a[i] = v.v;
      if (i + 1 < cnt && !dx_pure(e, rslc_list_item(e->c, n->b, i + 1)))
         a[i] = dx_own(e, a[i]);
   }
   if (bfn >= RSLC_BFN_TEXTURESIZE && bfn <= RSLC_BFN_TEXTUREGATHEROFFSET)
      return dx_texture(e, n, bfn, a, cnt, out);

   r = dx_tmp(e, w);
   switch (bfn)
   {
      case RSLC_BFN_RADIANS:
         dx_op2(e, DX_MUL, r, a[0], dx_splat(dx_immf(0.0174532925f), w));
         break;
      case RSLC_BFN_DEGREES:
         dx_op2(e, DX_MUL, r, a[0], dx_splat(dx_immf(57.2957795f), w));
         break;
      case RSLC_BFN_SIN:
      case RSLC_BFN_COS:
      case RSLC_BFN_TAN:
      {
         struct dx_opnd d[2], c = dx_tmp(e, w);
         d[0] = bfn == RSLC_BFN_COS ? dx_reg(DXO_NULL, 0, w) : r;
         d[1] = bfn == RSLC_BFN_SIN ? dx_reg(DXO_NULL, 0, w) : bfn == RSLC_BFN_COS ? r : c;
         dx_inst(e, DX_SINCOS, 0, d, 2, a, 1);
         if (bfn == RSLC_BFN_TAN)
            dx_op2(e, DX_DIV, r, r, c);
         break;
      }
      case RSLC_BFN_POW:
         dx_op1(e, DX_LOG, r, a[0]);
         dx_op2(e, DX_MUL, r, r, dx_widen(a[1], w));
         dx_op1(e, DX_EXP, r, r);
         break;
      case RSLC_BFN_EXP:
         dx_op2(e, DX_MUL, r, a[0], dx_splat(dx_immf(1.44269504f), w));
         dx_op1(e, DX_EXP, r, r);
         break;
      case RSLC_BFN_LOG:
         dx_op1(e, DX_LOG, r, a[0]);
         dx_op2(e, DX_MUL, r, r, dx_splat(dx_immf(0.693147181f), w));
         break;
      case RSLC_BFN_EXP2: dx_op1(e, DX_EXP, r, a[0]); break;
      case RSLC_BFN_LOG2: dx_op1(e, DX_LOG, r, a[0]); break;
      case RSLC_BFN_SQRT: dx_op1(e, DX_SQRT, r, a[0]); break;
      case RSLC_BFN_INVERSESQRT: dx_op1(e, DX_RSQ, r, a[0]); break;
      case RSLC_BFN_ABS:
         if (flt)
            dx_mov(e, r, dx_fabs(a[0]));
         else
            dx_op2(e, DX_IMAX, r, a[0], dx_negate(a[0], false));
         break;
      case RSLC_BFN_SIGN:
      {
         struct dx_opnd p = dx_tmp(e, w), q = dx_tmp(e, w);
         dx_op2(e, flt ? DX_LT : DX_ILT, p, dx_splat(dx_immu(0), w), a[0]);
         dx_op2(e, flt ? DX_LT : DX_ILT, q, a[0], dx_splat(dx_immu(0), w));
         dx_op2(e, DX_IADD, r, dx_negate(p, false), q);
         if (flt)
            dx_op1(e, DX_ITOF, r, r);
         break;
      }
      case RSLC_BFN_FLOOR: dx_op1(e, DX_ROUND_NI, r, a[0]); break;
      case RSLC_BFN_CEIL:  dx_op1(e, DX_ROUND_PI, r, a[0]); break;
      case RSLC_BFN_TRUNC: dx_op1(e, DX_ROUND_Z, r, a[0]); break;
      case RSLC_BFN_ROUND:
      case RSLC_BFN_ROUNDEVEN: dx_op1(e, DX_ROUND_NE, r, a[0]); break;
      case RSLC_BFN_FRACT: dx_op1(e, DX_FRC, r, a[0]); break;
      case RSLC_BFN_MOD:
      {
         /* x - y * floor(x / y) */
         struct dx_opnd y = dx_widen(a[1], w);
         dx_op2(e, DX_DIV, r, a[0], y);
         dx_op1(e, DX_ROUND_NI, r, r);
         dx_op3(e, DX_MAD, r, dx_negate(y, true), r, a[0]);
         break;
      }
      case RSLC_BFN_MODF:
      {
         struct dx_path p;
         struct dx_opnd ip = dx_tmp(e, w);
         dx_op1(e, DX_ROUND_Z, ip, a[0]);
         dx_op2(e, DX_ADD, r, a[0], dx_negate(ip, true));
         if (!dx_path_of(e, rslc_list_item(e->c, n->b, 1), &p) || !dx_store(e, &p, ip, n->line))
            return false;
         break;
      }
      case RSLC_BFN_MIN:
      case RSLC_BFN_MAX:
      {
         bool mn = bfn == RSLC_BFN_MIN;
         dx_op2(e, flt ? (mn ? DX_MIN : DX_MAX) : sint ? (mn ? DX_IMIN : DX_IMAX)
               : (mn ? DX_UMIN : DX_UMAX), r, a[0], dx_widen(a[1], w));
         break;
      }
      case RSLC_BFN_CLAMP:
         dx_op2(e, flt ? DX_MAX : sint ? DX_IMAX : DX_UMAX, r, a[0], dx_widen(a[1], w));
         dx_op2(e, flt ? DX_MIN : sint ? DX_IMIN : DX_UMIN, r, r, dx_widen(a[2], w));
         break;
      case RSLC_BFN_MIX:
      {
         const struct rslc_type *t2 = rslc_node_at(e->c, rslc_list_item(e->c, n->b, 2))->type;
         if (t2->base == RSLC_BASE_BOOL)
         {
            dx_op3(e, DX_MOVC, r, dx_widen(a[2], w), a[1], a[0]);
            break;
         }
         {
            /* x + (y - x) * a */
            struct dx_opnd d = dx_tmp(e, w);
            dx_op2(e, DX_ADD, d, a[1], dx_negate(a[0], true));
            dx_op3(e, DX_MAD, r, d, dx_widen(a[2], w), a[0]);
         }
         break;
      }
      case RSLC_BFN_STEP:
         /* x >= edge ? 1 : 0 */
         dx_op2(e, DX_GE, r, a[1], dx_widen(a[0], w));
         dx_op2(e, DX_AND, r, r, dx_splat(dx_immf(1.0f), w));
         break;
      case RSLC_BFN_SMOOTHSTEP:
      {
         struct dx_opnd d = dx_tmp(e, w), t = dx_tmp(e, w);
         struct dx_opnd e0 = dx_widen(a[0], w), e1 = dx_widen(a[1], w);
         dx_op2(e, DX_ADD, d, e1, dx_negate(e0, true));
         dx_op2(e, DX_ADD, t, a[2], dx_negate(e0, true));
         dx_op2f(e, DX_DIV, DX_SATURATE, t, t, d);
         dx_op3(e, DX_MAD, d, dx_negate(t, true), dx_splat(dx_immf(2.0f), w),
               dx_splat(dx_immf(3.0f), w));
         dx_op2(e, DX_MUL, t, t, t);
         dx_op2(e, DX_MUL, r, t, d);
         break;
      }
      case RSLC_BFN_ISNAN:
         dx_op2(e, DX_NE, r, a[0], a[0]);
         break;
      case RSLC_BFN_ISINF:
         dx_op2(e, DX_EQ, r, dx_fabs(a[0]), dx_splat(dx_immu(0x7f800000u), w));
         break;
      case RSLC_BFN_FLOATBITSTOINT: case RSLC_BFN_FLOATBITSTOUINT:
      case RSLC_BFN_INTBITSTOFLOAT: case RSLC_BFN_UINTBITSTOFLOAT:
         out->v = a[0];
         return true;
      case RSLC_BFN_FMA:
         dx_op3(e, DX_MAD, r, a[0], a[1], a[2]);
         break;
      case RSLC_BFN_LDEXP:
      {
         struct dx_opnd x = dx_tmp(e, w);
         dx_op1(e, DX_ITOF, x, a[1]);
         dx_op1(e, DX_EXP, x, x);
         dx_op2(e, DX_MUL, r, a[0], x);
         break;
      }
      case RSLC_BFN_PACKUNORM4X8:
      case RSLC_BFN_PACKUNORM2X16:
      {
         /* round(clamp(v, 0, 1) * max), then the fields */
         bool four = bfn == RSLC_BFN_PACKUNORM4X8;
         unsigned k, nc = four ? 4 : 2;
         struct dx_opnd s = dx_tmp(e, nc);
         dx_op2f(e, DX_MUL, DX_SATURATE, s, a[0], dx_splat(dx_immf(1.0f), nc));
         dx_op2(e, DX_MUL, s, s, dx_splat(dx_immf(four ? 255.0f : 65535.0f), nc));
         dx_op1(e, DX_ROUND_NE, s, s);
         dx_op1(e, DX_FTOU, s, s);
         dx_mov(e, r, dx_comp(s, 0));
         for (k = 1; k < nc; k++)
         {
            if (e->sm >= 50)
               dx_op4(e, DX_BFI, r, dx_immu(four ? 8 : 16), dx_immu(k * (four ? 8 : 16)),
                     dx_comp(s, k), r);
            else
            {
               struct dx_opnd sh = dx_tmp(e, 1);
               dx_op2(e, DX_ISHL, sh, dx_comp(s, k), dx_immu(k * (four ? 8 : 16)));
               dx_op2(e, DX_OR, r, r, sh);
            }
         }
         break;
      }
      case RSLC_BFN_UNPACKUNORM4X8:
      case RSLC_BFN_UNPACKUNORM2X16:
      {
         bool four = bfn == RSLC_BFN_UNPACKUNORM4X8;
         unsigned k, nc = four ? 4 : 2;
         struct dx_opnd u = dx_tmp(e, nc);
         for (k = 0; k < nc; k++)
         {
            struct dx_opnd d = dx_comp(u, k);
            d.swz[0] = (uint8_t)k;
            dx_op2(e, DX_USHR, d, dx_comp(a[0], 0), dx_immu(k * (four ? 8 : 16)));
         }
         dx_op2(e, DX_AND, u, u, dx_splat(dx_immu(four ? 0xffu : 0xffffu), nc));
         dx_op1(e, DX_UTOF, r, u);
         dx_op2(e, DX_MUL, r, r, dx_splat(dx_immf(four ? 1.0f / 255.0f : 1.0f / 65535.0f), nc));
         break;
      }
      case RSLC_BFN_PACKHALF2X16:
      {
         struct dx_opnd h = dx_tmp(e, 2);
         if (e->sm < 50)
            return dx_fail(e, "packHalf2x16 needs shader model 5", n->line);
         dx_op1(e, DX_F32TOF16, h, a[0]);
         dx_op4(e, DX_BFI, r, dx_immu(16), dx_immu(16), dx_comp(h, 1), dx_comp(h, 0));
         break;
      }
      case RSLC_BFN_UNPACKHALF2X16:
      {
         struct dx_opnd h = dx_tmp(e, 2), d;
         if (e->sm < 50)
            return dx_fail(e, "unpackHalf2x16 needs shader model 5", n->line);
         d = dx_comp(h, 0);
         dx_op2(e, DX_AND, d, dx_comp(a[0], 0), dx_immu(0xffffu));
         d = dx_comp(h, 1);
         d.swz[0] = 1;
         dx_op2(e, DX_USHR, d, dx_comp(a[0], 0), dx_immu(16));
         dx_op1(e, DX_F16TOF32, r, h);
         break;
      }
      case RSLC_BFN_LENGTH:
      case RSLC_BFN_DISTANCE:
      {
         struct dx_opnd v = a[0];
         if (bfn == RSLC_BFN_DISTANCE)
         {
            struct dx_opnd d = dx_tmp(e, w0);
            dx_op2(e, DX_ADD, d, a[0], dx_negate(a[1], true));
            v = d;
         }
         if (w0 == 1)
         {
            dx_mov(e, r, dx_fabs(v));
            break;
         }
         dx_op2(e, w0 == 2 ? DX_DP2 : w0 == 3 ? DX_DP3 : DX_DP4, r, v, v);
         dx_op1(e, DX_SQRT, r, r);
         break;
      }
      case RSLC_BFN_DOT:
         if (w0 == 1)
            dx_op2(e, DX_MUL, r, a[0], a[1]);
         else
            dx_op2(e, w0 == 2 ? DX_DP2 : w0 == 3 ? DX_DP3 : DX_DP4, r, a[0], a[1]);
         break;
      case RSLC_BFN_CROSS:
      {
         /* a.yzx * b.zxy - a.zxy * b.yzx */
         struct dx_opnd p = dx_tmp(e, 3), ay = a[0], bz = a[1], az = a[0], by = a[1];
         static const uint8_t yzx[3] = { 1, 2, 0 }, zxy[3] = { 2, 0, 1 };
         unsigned k;
         for (k = 0; k < 3; k++)
         {
            ay.swz[k] = a[0].swz[yzx[k]]; ay.imm[k] = a[0].imm[yzx[k]];
            bz.swz[k] = a[1].swz[zxy[k]]; bz.imm[k] = a[1].imm[zxy[k]];
            az.swz[k] = a[0].swz[zxy[k]]; az.imm[k] = a[0].imm[zxy[k]];
            by.swz[k] = a[1].swz[yzx[k]]; by.imm[k] = a[1].imm[yzx[k]];
         }
         dx_op2(e, DX_MUL, p, az, by);
         dx_op3(e, DX_MAD, r, ay, bz, dx_negate(p, true));
         break;
      }
      case RSLC_BFN_NORMALIZE:
      {
         struct dx_opnd l = dx_tmp(e, 1);
         if (w0 == 1)
         {
            struct dx_opnd z = dx_tmp(e, 1);
            /* sign(x) */
            dx_op2(e, DX_LT, l, dx_immu(0), a[0]);
            dx_op2(e, DX_LT, z, a[0], dx_immu(0));
            dx_op2(e, DX_IADD, r, dx_negate(l, false), z);
            dx_op1(e, DX_ITOF, r, r);
            break;
         }
         dx_op2(e, w0 == 2 ? DX_DP2 : w0 == 3 ? DX_DP3 : DX_DP4, l, a[0], a[0]);
         dx_op1(e, DX_RSQ, l, l);
         dx_op2(e, DX_MUL, r, a[0], dx_splat(l, w));
         break;
      }
      case RSLC_BFN_FACEFORWARD:
      {
         /* dot(Nref, I) < 0 ? N : -N */
         struct dx_opnd d = dx_tmp(e, 1);
         dx_op2(e, w0 == 1 ? DX_MUL : w0 == 2 ? DX_DP2 : w0 == 3 ? DX_DP3 : DX_DP4, d, a[2], a[1]);
         dx_op2(e, DX_LT, d, d, dx_immu(0));
         dx_op3(e, DX_MOVC, r, dx_splat(d, w), a[0], dx_negate(a[0], true));
         break;
      }
      case RSLC_BFN_REFLECT:
      {
         /* I - 2 * dot(N, I) * N */
         struct dx_opnd d = dx_tmp(e, 1);
         dx_op2(e, w0 == 1 ? DX_MUL : w0 == 2 ? DX_DP2 : w0 == 3 ? DX_DP3 : DX_DP4, d, a[1], a[0]);
         dx_op2(e, DX_ADD, d, d, d);
         dx_op3(e, DX_MAD, r, dx_negate(dx_splat(d, w), true), a[1], a[0]);
         break;
      }
      case RSLC_BFN_LESSTHAN: case RSLC_BFN_LESSTHANEQUAL: case RSLC_BFN_GREATERTHAN:
      case RSLC_BFN_GREATERTHANEQUAL: case RSLC_BFN_EQUAL: case RSLC_BFN_NOTEQUAL:
      {
         static const uint8_t ops[6] = { RSLC_BIN_LT, RSLC_BIN_LE, RSLC_BIN_GT, RSLC_BIN_GE,
            RSLC_BIN_EQ, RSLC_BIN_NE };
         return dx_binop(e, ops[bfn - RSLC_BFN_LESSTHAN], t0->base, a[0], a[1], w, false,
               &out->v, n->line);
      }
      case RSLC_BFN_ANY:
      case RSLC_BFN_ALL:
      {
         unsigned k;
         dx_op2(e, bfn == RSLC_BFN_ANY ? DX_OR : DX_AND, r, dx_comp(a[0], 0), dx_comp(a[0], 1));
         for (k = 2; k < w0; k++)
            dx_op2(e, bfn == RSLC_BFN_ANY ? DX_OR : DX_AND, r, r, dx_comp(a[0], k));
         break;
      }
      case RSLC_BFN_NOT:
         dx_op1(e, DX_NOT, r, a[0]);
         break;
      case RSLC_BFN_BITFIELDEXTRACT:
         if (e->sm < 50)
            return dx_fail(e, "bitfieldExtract needs shader model 5", n->line);
         dx_op3(e, sint ? DX_IBFE : DX_UBFE, r, dx_splat(dx_comp(a[2], 0), w),
               dx_splat(dx_comp(a[1], 0), w), a[0]);
         break;
      case RSLC_BFN_BITFIELDINSERT:
         if (e->sm < 50)
            return dx_fail(e, "bitfieldInsert needs shader model 5", n->line);
         dx_op4(e, DX_BFI, r, dx_splat(dx_comp(a[3], 0), w), dx_splat(dx_comp(a[2], 0), w),
               a[1], a[0]);
         break;
      case RSLC_BFN_BITFIELDREVERSE:
         if (e->sm < 50)
            return dx_fail(e, "bitfieldReverse needs shader model 5", n->line);
         dx_op1(e, DX_BFREV, r, a[0]);
         break;
      case RSLC_BFN_BITCOUNT:
         if (e->sm < 50)
            return dx_fail(e, "bitCount needs shader model 5", n->line);
         dx_op1(e, DX_COUNTBITS, r, a[0]);
         break;
      case RSLC_BFN_FINDLSB:
         if (e->sm < 50)
            return dx_fail(e, "findLSB needs shader model 5", n->line);
         dx_op1(e, DX_FIRSTBIT_LO, r, a[0]);
         break;
      case RSLC_BFN_FINDMSB:
         if (e->sm < 50)
            return dx_fail(e, "findMSB needs shader model 5", n->line);
         out->v = dx_find_msb(e, a[0], w, sint);
         return true;
      case RSLC_BFN_DFDX: case RSLC_BFN_DFDY:
      case RSLC_BFN_DFDXFINE: case RSLC_BFN_DFDYFINE:
      case RSLC_BFN_DFDXCOARSE: case RSLC_BFN_DFDYCOARSE:
      {
         bool x = bfn == RSLC_BFN_DFDX || bfn == RSLC_BFN_DFDXFINE || bfn == RSLC_BFN_DFDXCOARSE;
         enum dx_op op = x ? DX_DERIV_RTX : DX_DERIV_RTY;
         if (e->sm >= 50 && (bfn == RSLC_BFN_DFDXFINE || bfn == RSLC_BFN_DFDYFINE))
            op = x ? DX_DERIV_RTX_FINE : DX_DERIV_RTY_FINE;
         else if (e->sm >= 50 && (bfn == RSLC_BFN_DFDXCOARSE || bfn == RSLC_BFN_DFDYCOARSE))
            op = x ? DX_DERIV_RTX_COARSE : DX_DERIV_RTY_COARSE;
         dx_op1(e, op, r, a[0]);
         break;
      }
      case RSLC_BFN_FWIDTH: case RSLC_BFN_FWIDTHFINE: case RSLC_BFN_FWIDTHCOARSE:
      {
         struct dx_opnd y = dx_tmp(e, w);
         enum dx_op ox = DX_DERIV_RTX, oy = DX_DERIV_RTY;
         if (e->sm >= 50 && bfn == RSLC_BFN_FWIDTHFINE)
         {
            ox = DX_DERIV_RTX_FINE;
            oy = DX_DERIV_RTY_FINE;
         }
         else if (e->sm >= 50 && bfn == RSLC_BFN_FWIDTHCOARSE)
         {
            ox = DX_DERIV_RTX_COARSE;
            oy = DX_DERIV_RTY_COARSE;
         }
         dx_op1(e, ox, r, a[0]);
         dx_op1(e, oy, y, a[0]);
         dx_op2(e, DX_ADD, r, dx_fabs(r), dx_fabs(y));
         break;
      }
      case RSLC_BFN_TRANSPOSE:
      {
         const struct rslc_type *mt = t0;
         uint32_t base = dx_temp(e, mt->rows), j, k;
         struct dx_val m;
         m.agg = true;
         m.p = out->p;
         for (j = 0; j < mt->rows; j++)
            for (k = 0; k < mt->cols; k++)
            {
               struct dx_opnd d = dx_reg(DXO_TEMP, base + j, 1);
               d.swz[0] = (uint8_t)k;
               dx_mov(e, d, dx_comp(dx_col(&m, mt, k), j));
            }
         memset(out, 0, sizeof(*out));
         out->agg = true;
         out->p = dx_place_of(DXO_TEMP, base);
         return true;
      }
      default:
         return dx_fail(e, "a builtin function with no form here yet", n->line);
   }
   out->v = r;
   return !e->c->failed;
}

/* ------------------------------------------------------------------ */
/* Calls                                                               */
/* ------------------------------------------------------------------ */

/* The textures a call binds its callee's sampler parameters to. */
static bool dx_call_key(struct dx_emit *e, const struct rslc_func *f, uint32_t args,
      uint32_t *tex)
{
   uint32_t i, k = 0;
   memset(tex, 0xff, DX_MAX_TEXPARAMS * sizeof(*tex));
   for (i = 0; i < f->nparams; i++)
   {
      const struct rslc_type *pt = f->params[i]->type;
      struct dx_tex tx;
      if (!RSLC_TY_IS_SAMPLER(pt->kind))
         continue;
      if (k == DX_MAX_TEXPARAMS)
         return dx_fail(e, "too many sampler parameters", f->line);
      if (!dx_tex_of(e, rslc_list_item(e->c, args, i), &tx))
         return false;
      tex[k++] = tx.t;
   }
   return true;
}

/* What a body's returns need: how many there are, and whether one sits
 * in a loop or switch of the body. */
static void dx_returns(struct dx_emit *e, uint32_t node, uint32_t depth, uint32_t *count,
      bool *deep)
{
   const struct rslc_node *n;
   uint32_t i, cnt;
   if (!node)
      return;
   n = rslc_node_at(e->c, node);
   switch (n->op)
   {
      case RSLC_OP_RETURN:
         (*count)++;
         if (depth)
            *deep = true;
         break;
      case RSLC_OP_BLOCK:
         cnt = n->b ? rslc_list_len(e->c, n->b) : 0;
         for (i = 0; i < cnt; i++)
            dx_returns(e, rslc_list_item(e->c, n->b, i), depth, count, deep);
         break;
      case RSLC_OP_IF:
         dx_returns(e, n->b, depth, count, deep);
         dx_returns(e, n->c, depth, count, deep);
         break;
      case RSLC_OP_FOR:
         dx_returns(e, rslc_list_item(e->c, n->b, 3), depth + 1, count, deep);
         break;
      case RSLC_OP_WHILE:
         dx_returns(e, n->b, depth + 1, count, deep);
         break;
      case RSLC_OP_DO:
         dx_returns(e, n->a, depth + 1, count, deep);
         break;
      case RSLC_OP_SWITCH:
         cnt = n->b ? rslc_list_len(e->c, n->b) : 0;
         for (i = 0; i < cnt; i++)
            dx_returns(e, rslc_list_item(e->c, n->b, i), depth + 1, count, deep);
         break;
      default:
         break;
   }
}

/* Whether a body's last statement is a return. */
static bool dx_ends_in_return(struct dx_emit *e, uint32_t node)
{
   const struct rslc_node *n = rslc_node_at(e->c, node);
   while (n->op == RSLC_OP_BLOCK)
   {
      uint32_t cnt = n->b ? rslc_list_len(e->c, n->b) : 0;
      if (!cnt)
         return false;
      n = rslc_node_at(e->c, rslc_list_item(e->c, n->b, cnt - 1));
   }
   return n->op == RSLC_OP_RETURN;
}

/* After a loop or switch of an expanded body closes: a return that left
 * it leaves the next one out too, up to the wrapper. */
static bool dx_return_cascade(struct dx_emit *e)
{
   const struct dx_inline *in;
   if (!e->ninl)
      return true;
   in = &e->inl[e->ninl - 1];
   if (in->flag == RSLC_UNSET || e->nctl < in->nctl)
      return true;
   return dx_cond(e, DX_BREAKC, true, dx_comp(dx_reg(DXO_TEMP, in->flag, 1), 0));
}

/* A call, expanded where it is: the arguments, in order, then the
 * callee's parameters in fresh registers of the caller's, the body, the
 * out parameters back. */
static bool dx_call(struct dx_emit *e, const struct rslc_node *n, struct dx_val *out)
{
   const struct rslc_func *f = e->c->prog.funcs[n->a];
   uint32_t cnt = n->b ? rslc_list_len(e->c, n->b) : 0, i, k, returns = 0;
   uint32_t tex[DX_MAX_TEXPARAMS];
   struct dx_path  *outs;
   struct dx_val   *vals;
   struct dx_place *homes;
   struct dx_inline *in;
   bool deep = false, trailing, wrapped, ok;
   if (e->ninl == DX_MAX_INLINE)
      return dx_fail(e, "calls nested too deep", n->line);
   if (!dx_call_key(e, f, n->b, tex))
      return false;
   outs  = (struct dx_path*)rslc_scratch_alloc(e->c, (cnt ? cnt : 1) * sizeof(*outs));
   vals  = (struct dx_val*)rslc_scratch_alloc(e->c, (cnt ? cnt : 1) * sizeof(*vals));
   homes = (struct dx_place*)rslc_scratch_alloc(e->c, (cnt ? cnt : 1) * sizeof(*homes));
   if (!outs || !vals || !homes)
      return false;
   /* the arguments, left to right; a call among them is expanded whole
    * before this callee's parameters take their homes */
   for (i = 0; i < cnt; i++)
   {
      const struct rslc_var *pv = f->params[i];
      uint32_t an = rslc_list_item(e->c, n->b, i);
      if (RSLC_TY_IS_SAMPLER(pv->type->kind))
         continue;
      if (pv->dir != RSLC_DIR_IN && !dx_path_of(e, an, &outs[i]))
         return false;
      if (pv->dir == RSLC_DIR_IN && !dx_rv(e, an, &vals[i]))
         return false;
   }
   for (i = 0; i < cnt; i++)
   {
      const struct rslc_var *pv = f->params[i];
      struct dx_var *dv = &e->vars[pv->id];
      struct dx_path home;
      if (RSLC_TY_IS_SAMPLER(pv->type->kind))
         continue;
      /* an indexed parameter keeps its indexable temporary: the callee
       * is never active twice */
      if (!dv->dynamic || !dv->has_home)
         homes[i] = dx_home(e, pv->id, dx_slots(pv->type));
      else
         homes[i] = dv->home;
      dx_path_regs(&home, pv->type, homes[i]);
      if (pv->dir == RSLC_DIR_INOUT && !dx_copy(e, &home, &outs[i], n->line))
         return false;
      if (pv->dir == RSLC_DIR_IN && !dx_assign_to(e, &home, &vals[i], n->line))
         return false;
   }
   for (i = 0, k = 0; i < f->nparams; i++)
   {
      struct dx_var *dv = &e->vars[f->params[i]->id];
      if (RSLC_TY_IS_SAMPLER(f->params[i]->type->kind))
      {
         e->tex_bind[f->params[i]->id] = tex[k++];
         continue;
      }
      dv->home     = homes[i];
      dv->has_home = true;
   }

   dx_returns(e, f->body, 0, &returns, &deep);
   trailing = dx_ends_in_return(e, f->body);
   wrapped  = returns > (trailing ? 1u : 0u);
   in = &e->inl[e->ninl++];
   memset(in, 0, sizeof(*in));
   in->f    = f;
   in->flag = RSLC_UNSET;
   if (f->ret->kind != RSLC_TY_VOID)
      in->ret = dx_temp(e, dx_slots(f->ret));
   ok = true;
   if (wrapped)
   {
      if (e->nctl == DX_MAX_LOOPS)
         return dx_fail(e, "loops nested too deep", n->line);
      if (deep)
      {
         in->flag = dx_temp(e, 1);
         ok = dx_mov(e, dx_reg(DXO_TEMP, in->flag, 1), dx_immu(0));
      }
      memset(&e->ctl[e->nctl], 0, sizeof(e->ctl[0]));
      e->nctl++;
      in->nctl    = e->nctl;
      in->wrapped = true;
      ok = ok && dx_op0(e, DX_LOOP, 0);
   }
   ok = ok && dx_stmt(e, f->body);
   if (wrapped)
   {
      /* the body's last return has left already */
      e->nctl--;
      ok = ok && (trailing || dx_op0(e, DX_BREAK, 0)) && dx_op0(e, DX_ENDLOOP, 0);
   }
   e->ninl--;
   if (!ok)
      return false;

   for (i = 0; i < cnt; i++)
   {
      const struct rslc_var *pv = f->params[i];
      struct dx_path home;
      if (pv->dir == RSLC_DIR_IN || RSLC_TY_IS_SAMPLER(pv->type->kind))
         continue;
      dx_path_regs(&home, pv->type, homes[i]);
      if (!dx_copy(e, &outs[i], &home, n->line))
         return false;
   }
   memset(out, 0, sizeof(*out));
   if (f->ret->kind == RSLC_TY_VOID)
      out->v = dx_immu(0);
   else if (f->ret->kind == RSLC_TY_SCALAR || f->ret->kind == RSLC_TY_VEC)
      out->v = dx_reg(DXO_TEMP, in->ret, dx_width(f->ret));
   else
   {
      out->agg = true;
      out->p   = dx_place_of(DXO_TEMP, in->ret);
   }
   return true;
}

/* ------------------------------------------------------------------ */
/* Statements                                                          */
/* ------------------------------------------------------------------ */

/* The outputs to their registers, where the stage ends. */
static bool dx_write_outputs(struct dx_emit *e)
{
   uint32_t k;
   for (k = 0; k < e->nosig; k++)
   {
      const struct dx_sig *s = &e->osig[k];
      struct dx_opnd d;
      if (s->reg == RSLC_UNSET)
      {
         d = dx_reg(DXO_ODEPTH, 0, 1);
         if (!dx_mov(e, d, dx_at(&e->oc_home[k], 0, 1)))
            return false;
         continue;
      }
      if (e->oc_dist[k])
      {
         unsigned i;
         for (i = 0; i < e->oc_dist[k]; i++)
         {
            d = dx_reg(DXO_OUTPUT, s->reg, 1);
            d.swz[0] = (uint8_t)(e->oc_comp[k] + i);
            if (!dx_mov(e, d, dx_at(&e->oc_home[k], i, 1)))
               return false;
         }
         continue;
      }
      d = dx_reg(DXO_OUTPUT, s->reg, e->oc_n[k]);
      {
         unsigned i;
         for (i = 0; i < 4; i++)
            d.swz[i] = (uint8_t)((e->oc_comp[k] + i) & 3);
      }
      if (!dx_mov(e, d, dx_at(&e->oc_home[k], 0, e->oc_n[k])))
         return false;
   }
   return true;
}

static bool dx_stmt_list(struct dx_emit *e, uint32_t list)
{
   uint32_t i, cnt = list ? rslc_list_len(e->c, list) : 0;
   for (i = 0; i < cnt; i++)
      if (!dx_stmt(e, rslc_list_item(e->c, list, i)))
         return false;
   return true;
}

/* A continue from the innermost loop: a for's iteration or a do's test
 * runs first, as DXBC's continue goes straight to the loop's top. */
static bool dx_continue(struct dx_emit *e, uint32_t line)
{
   uint32_t i = e->nctl;
   while (i--)
   {
      const struct dx_ctl *c = &e->ctl[i];
      if (!c->is_loop)
         continue;
      if (c->iter)
      {
         struct dx_val v;
         if (!dx_rv(e, c->iter, &v))
            return false;
      }
      if (c->cond)
      {
         struct dx_val v;
         if (!dx_rv(e, c->cond, &v))
            return false;
         if (!dx_cond(e, DX_BREAKC, false, dx_comp(v.v, 0)))
            return false;
      }
      return dx_op0(e, DX_CONTINUE, 0);
   }
   return dx_fail(e, "continue outside a loop", line);
}

/* Whether a statement ends its case section: nothing after it runs. */
static bool dx_ends_flow(struct dx_emit *e, uint32_t node)
{
   const struct rslc_node *n = rslc_node_at(e->c, node);
   switch (n->op)
   {
      case RSLC_OP_BREAK: case RSLC_OP_CONTINUE: case RSLC_OP_RETURN:
         return true;
      case RSLC_OP_BLOCK:
      {
         uint32_t cnt = n->b ? rslc_list_len(e->c, n->b) : 0;
         return cnt && dx_ends_flow(e, rslc_list_item(e->c, n->b, cnt - 1));
      }
      case RSLC_OP_IF:
         return n->c && dx_ends_flow(e, n->b) && dx_ends_flow(e, n->c);
      default:
         return false;
   }
}

static bool dx_stmt(struct dx_emit *e, uint32_t node)
{
   const struct rslc_node *n = rslc_node_at(e->c, node);
   uint32_t mark = e->ntemp;
   struct rslc_mark smark;
   bool ok = true;

   rslc_scratch_mark(e->c, &smark);
   switch (n->op)
   {
      case RSLC_OP_BLOCK:
         ok = dx_stmt_list(e, n->b);
         if (n->aux & RSLC_BLOCK_GROUP)
            mark = e->ntemp;   /* declarators of the enclosing scope */
         break;
      case RSLC_OP_DECL:
      {
         const struct rslc_var *v = e->c->prog.vars[n->a];
         struct dx_var *dv = &e->vars[v->id];
         if (v->storage == RSLC_ST_CONST && v->init && v->type->kind != RSLC_TY_ARRAY
               && v->type->kind != RSLC_TY_STRUCT && dx_is_const(e, v->init))
         {
            dv->folded = true;
            break;
         }
         /* a home each time the declaration is reached: an expanded
          * call's locals are its own; an indexed one keeps its
          * indexable temporary */
         if (!dv->has_home || !dv->dynamic)
         {
            dv->home = dx_home(e, v->id, dx_slots(v->type));
            dv->has_home = true;
         }
         mark = e->ntemp;   /* the home stays with the scope */
         if (v->init)
         {
            struct dx_path p;
            struct dx_val val;
            dx_path_regs(&p, v->type, dv->home);
            ok = dx_rv(e, v->init, &val) && dx_assign_to(e, &p, &val, n->line);
         }
         break;
      }
      case RSLC_OP_EXPR:
      {
         struct dx_val v;
         ok = dx_rv(e, n->a, &v);
         break;
      }
      case RSLC_OP_IF:
      {
         struct dx_val c;
         ok = dx_rv(e, n->a, &c);
         if (!ok)
            break;
         ok = dx_cond(e, DX_IF, true, dx_comp(c.v, 0));
         {
            uint32_t m = e->ntemp;
            ok = ok && dx_stmt(e, n->b);
            e->ntemp = m;
         }
         if (ok && n->c)
         {
            uint32_t m = e->ntemp;
            ok = dx_op0(e, DX_ELSE, 0) && dx_stmt(e, n->c);
            e->ntemp = m;
         }
         ok = ok && dx_op0(e, DX_ENDIF, 0);
         break;
      }
      case RSLC_OP_FOR:
      case RSLC_OP_WHILE:
      case RSLC_OP_DO:
      {
         uint32_t init = 0, cond = 0, iter = 0, body;
         struct dx_ctl *c;
         if (n->op == RSLC_OP_FOR)
         {
            init = rslc_list_item(e->c, n->b, 0);
            cond = rslc_list_item(e->c, n->b, 1);
            iter = rslc_list_item(e->c, n->b, 2);
            body = rslc_list_item(e->c, n->b, 3);
         }
         else if (n->op == RSLC_OP_WHILE)
         {
            cond = n->a;
            body = n->b;
         }
         else
         {
            body = n->a;
            cond = n->b;
         }
         if (init && !dx_stmt(e, init))
         {
            ok = false;
            break;
         }
         if (e->nctl == DX_MAX_LOOPS)
         {
            ok = dx_fail(e, "loops nested too deep", n->line);
            break;
         }
         c = &e->ctl[e->nctl++];
         memset(c, 0, sizeof(*c));
         c->is_loop = true;
         c->iter    = iter;
         c->cond    = n->op == RSLC_OP_DO ? cond : 0;
         ok = dx_op0(e, DX_LOOP, 0);
         if (ok && cond && n->op != RSLC_OP_DO)
         {
            struct dx_val cv;
            uint32_t m = e->ntemp;
            ok = dx_rv(e, cond, &cv) && dx_cond(e, DX_BREAKC, false, dx_comp(cv.v, 0));
            e->ntemp = m;
         }
         if (ok)
         {
            uint32_t m = e->ntemp;
            ok = dx_stmt(e, body);
            e->ntemp = m;
         }
         if (ok && iter)
         {
            struct dx_val iv;
            ok = dx_rv(e, iter, &iv);
         }
         if (ok && n->op == RSLC_OP_DO)
         {
            struct dx_val cv;
            ok = dx_rv(e, cond, &cv) && dx_cond(e, DX_BREAKC, false, dx_comp(cv.v, 0));
         }
         e->nctl--;
         ok = ok && dx_op0(e, DX_ENDLOOP, 0) && dx_return_cascade(e);
         break;
      }
      case RSLC_OP_SWITCH:
      {
         struct dx_val sel;
         uint32_t nst = n->b ? rslc_list_len(e->c, n->b) : 0, i;
         bool open = false;      /* a section runs on into the next */
         bool any = false;       /* the open section has statements */
         ok = dx_rv(e, n->a, &sel);
         if (!ok)
            break;
         ok = dx_inst(e, DX_SWITCH, 0, NULL, 0, &sel.v, 1);
         if (e->nctl == DX_MAX_LOOPS)
         {
            ok = dx_fail(e, "loops nested too deep", n->line);
            break;
         }
         memset(&e->ctl[e->nctl], 0, sizeof(e->ctl[0]));
         e->nctl++;
         for (i = 0; ok && i < nst; i++)
         {
            uint32_t sn = rslc_list_item(e->c, n->b, i);
            const struct rslc_node *cs = rslc_node_at(e->c, sn);
            if (cs->op == RSLC_OP_CASE)
            {
               struct dx_opnd lit;
               if (open && any)
               {
                  ok = dx_fail(e, "a case section that falls through into the next", cs->line);
                  break;
               }
               if (cs->a)
               {
                  struct rslc_cval cv;
                  if (!rslc_const_eval(e->c, cs->a, &cv))
                  {
                     ok = dx_fail(e, "a case label that is not constant", cs->line);
                     break;
                  }
                  lit = dx_immu(cv.v[0]);
                  ok = dx_inst(e, DX_CASE, 0, NULL, 0, &lit, 1);
               }
               else
                  ok = dx_op0(e, DX_DEFAULT, 0);
               open = true;
               any  = false;
               continue;
            }
            if (!open)
               continue;   /* unreachable after a break */
            ok = dx_stmt(e, sn);
            any = true;
            if (dx_ends_flow(e, sn))
               open = false;
         }
         if (ok && open)
            ok = dx_op0(e, DX_BREAK, 0);
         e->nctl--;
         ok = ok && dx_op0(e, DX_ENDSWITCH, 0) && dx_return_cascade(e);
         break;
      }
      case RSLC_OP_BREAK:
         ok = dx_op0(e, DX_BREAK, 0);
         break;
      case RSLC_OP_CONTINUE:
         ok = dx_continue(e, n->line);
         break;
      case RSLC_OP_RETURN:
         if (!e->ninl)
            ok = (e->c->stage == RSLC_STAGE_GEOMETRY || dx_write_outputs(e))
               && dx_op0(e, DX_RET, 0);
         else
         {
            const struct dx_inline *in = &e->inl[e->ninl - 1];
            if (n->a)
            {
               struct dx_val v;
               struct dx_path p;
               dx_path_regs(&p, in->f->ret, dx_place_of(DXO_TEMP, in->ret));
               ok = dx_rv(e, n->a, &v) && dx_assign_to(e, &p, &v, n->line);
            }
            /* out of the wrapper; from a loop or switch inside it, by
             * the flag */
            if (ok && in->wrapped && e->nctl > in->nctl)
               ok = dx_mov(e, dx_reg(DXO_TEMP, in->flag, 1), dx_immu(~0u));
            if (ok && in->wrapped)
               ok = dx_op0(e, DX_BREAK, 0);
         }
         break;
      case RSLC_OP_DISCARD:
      {
         struct dx_opnd t = dx_immu(~0u);
         ok = dx_cond(e, DX_DISCARD, true, t);
         break;
      }
      case RSLC_OP_CASE:
         break;
      default:
         ok = dx_fail(e, "a statement with no form here", n->line);
         break;
   }
   rslc_scratch_release(e->c, &smark);
   e->ntemp = mark;
   if (!ok && !e->c->failed)
      dx_fail(e, "cannot emit a statement", n->line);
   return ok;
}

/* ------------------------------------------------------------------ */
/* Declarations                                                        */
/* ------------------------------------------------------------------ */

static bool dx_decl1(struct dx_emit *e, enum dx_op op, uint32_t flags, struct dx_opnd o,
      bool dst)
{
   unsigned len = 1 + dx_opnd_len(&o);
   if (!dx_w(e, (uint32_t)op | flags | ((uint32_t)len << 24)))
      return false;
   return dx_put_opnd(e, &o, dst, 0);
}

/* The return type token of a resource: four of the base. */
static uint32_t dx_ret_type(uint8_t base)
{
   uint32_t t = base == RSLC_BASE_INT ? 3u : base == RSLC_BASE_UINT ? 4u : 5u;
   return t | (t << 4) | (t << 8) | (t << 12);
}

/* A destination writing a signature element's mask: its components. */
static unsigned dx_mask_opnd(struct dx_opnd *o, uint8_t mask)
{
   unsigned n = 0, c;
   for (c = 0; c < 4; c++)
      if (mask & (1u << c))
         o->swz[n++] = (uint8_t)c;
   o->n = (uint8_t)n;
   return n;
}

static bool dx_decls(struct dx_emit *e)
{
   const struct rslc_prog *pr = &e->c->prog;
   uint32_t i, k;
   uint32_t flags = 1u << 11;   /* refactoring allowed */

   if (e->early_z)
      flags |= 1u << 13;   /* force early depth / stencil */
   if (!dx_op0(e, DX_DCL_GLOBAL_FLAGS, flags))
      return false;

   /* uniform blocks, those the code reads: as fxc does, as one declared
    * is a binding the root signature or the pipeline layout must have */
   for (i = 0; i < RSLC_MAX_BLOCKS; i++)
   {
      const struct rslc_blockdecl *b = &pr->blocks[i];
      struct dx_opnd o;
      uint32_t a, size, st;
      if (!pr->has_block[i] || b->buffer || !e->cb_read[i])
         continue;
      rslc_layout_of_type(b->type, dx_block_std140(e, i), &a, &size, &st);
      o = dx_reg(DXO_CB, dx_cb_slot(e, i), 4);
      o.idx[1] = (size + 15) / 16;
      if (!o.idx[1])
         o.idx[1] = 1;
      if (!dx_w(e, DX_DCL_CONSTANT_BUFFER | (e->cb_dynamic[i] ? 1u << 11 : 0) | (4u << 24))
            || !dx_put_opnd(e, &o, false, 0))
         return false;
   }
   /* samplers, then textures */
   for (i = 0; i < pr->nsamplers; i++)
   {
      const struct rslc_var *v = pr->samplers[i];
      const struct rslc_type *t = v->type->kind == RSLC_TY_ARRAY ? v->type->elem : v->type;
      uint32_t count = v->type->kind == RSLC_TY_ARRAY ? v->type->array_len : 1;
      if (!e->used[v->id] || t->kind == RSLC_TY_SAMPLERBUFFER || t->kind == RSLC_TY_SAMPLER2DMSARRAY)
         continue;
      for (k = 0; k < count; k++)
      {
         struct dx_opnd s = dx_reg(DXO_SAMPLER, (v->binding == RSLC_UNSET ? 0 : v->binding) + k, 0);
         if (!dx_decl1(e, DX_DCL_SAMPLER, 0, s, false))
            return false;
      }
   }
   for (i = 0; i < pr->nsamplers; i++)
   {
      const struct rslc_var *v = pr->samplers[i];
      const struct rslc_type *t = v->type->kind == RSLC_TY_ARRAY ? v->type->elem : v->type;
      uint32_t count = v->type->kind == RSLC_TY_ARRAY ? v->type->array_len : 1;
      uint32_t dim;
      if (!e->used[v->id])
         continue;
      dim = t->kind == RSLC_TY_SAMPLER2DARRAY ? DX_DIM_TEX2DARRAY
         : t->kind == RSLC_TY_SAMPLER2DMSARRAY ? DX_DIM_TEX2DMSARRAY
         : t->kind == RSLC_TY_SAMPLERBUFFER ? DX_DIM_BUFFER : DX_DIM_TEX2D;
      for (k = 0; k < count; k++)
      {
         struct dx_opnd o = dx_reg(DXO_RESOURCE, (v->binding == RSLC_UNSET ? 0 : v->binding) + k, 0);
         unsigned len = 1 + dx_opnd_len(&o) + 1;
         if (!dx_w(e, DX_DCL_RESOURCE | (dim << 11) | ((uint32_t)len << 24))
               || !dx_put_opnd(e, &o, false, 0) || !dx_w(e, dx_ret_type(t->base)))
            return false;
      }
   }
   /* storage buffers */
   for (i = 0; i < RSLC_MAX_BLOCKS; i++)
   {
      const struct rslc_blockdecl *b = &pr->blocks[i];
      struct dx_opnd o;
      if (!pr->has_block[i] || !b->buffer)
         continue;
      o = dx_buffer_opnd(e, i);
      o.n = 0;
      if (b->readonly)
      {
         if (!dx_decl1(e, DX_DCL_RESOURCE_RAW, 0, dx_reg(DXO_RESOURCE, o.idx[0], 0), false))
            return false;
      }
      else if (!dx_decl1(e, DX_DCL_UAV_RAW, b->coherent ? 1u << 16 : 0,
               dx_reg(DXO_UAV, o.idx[0], 0), false))
         return false;
   }
   /* images */
   for (i = 1; i < pr->nvars; i++)
   {
      const struct rslc_var *v = pr->vars[i];
      struct dx_opnd o;
      uint32_t dim, ret;
      unsigned len;
      if (v->storage != RSLC_ST_IMAGE || !e->used[i])
         continue;
      o = dx_reg(DXO_UAV, v->binding == RSLC_UNSET ? 0 : v->binding, 0);
      dim = v->type->kind == RSLC_TY_IMAGE2DARRAY ? DX_DIM_TEX2DARRAY : DX_DIM_TEX2D;
      /* rgba8 is read and written as unorm */
      ret = v->type->cols == RSLC_IMGFMT_RGBA8 ? 0x1111u : dx_ret_type(v->type->base);
      len = 1 + dx_opnd_len(&o) + 1;
      if (!dx_w(e, DX_DCL_UAV_TYPED | (dim << 11) | ((uint32_t)len << 24))
            || !dx_put_opnd(e, &o, false, 0) || !dx_w(e, ret))
         return false;
   }
   /* inputs and outputs */
   for (i = 0; i < e->nisig; i++)
   {
      const struct dx_sig *s = &e->isig[i];
      struct dx_opnd o = dx_reg(DXO_INPUT, s->reg, 4);
      unsigned n = dx_mask_opnd(&o, s->mask);
      bool frag = e->c->stage == RSLC_STAGE_FRAGMENT;
      uint32_t sv = s->sv;
      enum dx_op op;
      uint32_t fl = 0;
      if (e->c->stage == RSLC_STAGE_GEOMETRY)
      {
         o.twod   = 1;
         o.idx[1] = o.idx[0];
         o.idx[0] = e->gs_verts;
      }
      if (frag)
      {
         op = s->sv ? (s->sv == DX_SV_POSITION || s->sv == DX_SV_RTINDEX ? DX_DCL_INPUT_PS_SIV
               : DX_DCL_INPUT_PS_SGV) : DX_DCL_INPUT_PS;
         if (op != DX_DCL_INPUT_PS_SGV)
            fl = (uint32_t)s->interp << 11;
      }
      else if (e->c->stage == RSLC_STAGE_GEOMETRY)
      {
         /* a vertex's position is interpreted; its clip distances are
          * plain values here, as fxc declares them */
         if (sv != DX_SV_POSITION)
            sv = 0;
         op = sv ? DX_DCL_INPUT_SIV : DX_DCL_INPUT;
      }
      else
         op = sv ? DX_DCL_INPUT_SGV : DX_DCL_INPUT;
      if (!dx_w(e, (uint32_t)op | fl | ((uint32_t)(1 + dx_opnd_len(&o) + (sv ? 1 : 0)) << 24))
            || !dx_put_opnd(e, &o, true, n))
         return false;
      if (sv && !dx_w(e, sv))
         return false;
   }
   if (e->c->stage == RSLC_STAGE_GEOMETRY)
   {
      static const uint8_t in_prim[8]  = { 0, 1, 2, 6, 3, 7, 0, 0 };
      static const uint8_t out_topo[8] = { 0, 1, 3, 0, 0, 0, 3, 5 };
      for (i = 1; i < pr->nvars; i++)
      {
         const struct rslc_var *v = pr->vars[i];
         if (v->global && v->storage == RSLC_ST_BUILTIN && e->used[i]
               && v->builtin == RSLC_BI_INVOCATIONID
               && !dx_decl1(e, DX_DCL_INPUT, 0, dx_reg(DXO_GSINSTANCEID, 0, 0), true))
            return false;
      }
      if (pr->gs_invocations > 1
            && (!dx_w(e, DX_DCL_GS_INSTANCES | (2u << 24)) || !dx_w(e, pr->gs_invocations)))
         return false;
      if (!dx_op0(e, DX_DCL_GS_INPUT_PRIMITIVE, (uint32_t)in_prim[pr->gs_in] << 11))
         return false;
      if (e->sm >= 50 && !dx_decl1(e, DX_DCL_STREAM, 0, dx_reg(DXO_STREAM, 0, 0), false))
         return false;
      if (!dx_op0(e, DX_DCL_GS_OUTPUT_TOPOLOGY, (uint32_t)out_topo[pr->gs_out] << 11))
         return false;
   }
   for (i = 0; i < e->nosig; i++)
   {
      const struct dx_sig *s = &e->osig[i];
      struct dx_opnd o;
      unsigned n;
      if (s->reg == RSLC_UNSET)
      {
         if (!dx_decl1(e, DX_DCL_OUTPUT, 0, dx_reg(DXO_ODEPTH, 0, 1), true))
            return false;
         continue;
      }
      o = dx_reg(DXO_OUTPUT, s->reg, 4);
      n = dx_mask_opnd(&o, s->mask);
      if (s->sv)
      {
         if (!dx_w(e, DX_DCL_OUTPUT_SIV | ((uint32_t)(1 + dx_opnd_len(&o) + 1) << 24))
               || !dx_put_opnd(e, &o, true, n) || !dx_w(e, s->sv))
            return false;
      }
      else if (!dx_decl1(e, DX_DCL_OUTPUT, 0, o, true))
         return false;
   }
   if (e->c->stage == RSLC_STAGE_GEOMETRY
         && (!dx_w(e, DX_DCL_MAX_OUTPUT_VERTEX_COUNT | (2u << 24)) || !dx_w(e, pr->gs_max_vertices)))
      return false;
   if (e->c->stage == RSLC_STAGE_COMPUTE)
   {
      /* the thread ids read, workgroup memory, the group's size */
      for (i = 1; i < pr->nvars; i++)
      {
         const struct rslc_var *v = pr->vars[i];
         struct dx_opnd o;
         uint8_t f;
         if (!v->global || v->storage != RSLC_ST_BUILTIN || !e->used[i])
            continue;
         switch (v->builtin)
         {
            case RSLC_BI_GLOBALINVOCATIONID:   f = DXO_THREADID; break;
            case RSLC_BI_WORKGROUPID:          f = DXO_GROUPID; break;
            case RSLC_BI_LOCALINVOCATIONID:    f = DXO_THREADIDINGROUP; break;
            case RSLC_BI_LOCALINVOCATIONINDEX: f = DXO_THREADIDINGROUPFLAT; break;
            default: continue;
         }
         o = dx_reg(f, 0, f == DXO_THREADIDINGROUPFLAT ? 1 : 3);
         if (!dx_decl1(e, DX_DCL_INPUT, 0, o, true))
            return false;
      }
      for (i = 0; i < e->ntgsm; i++)
      {
         struct dx_opnd g = dx_reg(DXO_TGSM, i, 0);
         if (!dx_w(e, DX_DCL_TGSM_RAW | ((uint32_t)(1 + dx_opnd_len(&g) + 1) << 24))
               || !dx_put_opnd(e, &g, false, 0) || !dx_w(e, e->tgsm_size[i]))
            return false;
      }
      if (!dx_w(e, DX_DCL_THREAD_GROUP | (4u << 24))
            || !dx_w(e, pr->cs_local[0] ? pr->cs_local[0] : 1)
            || !dx_w(e, pr->cs_local[1] ? pr->cs_local[1] : 1)
            || !dx_w(e, pr->cs_local[2] ? pr->cs_local[2] : 1))
         return false;
   }
   if (e->max_temp && (!dx_w(e, DX_DCL_TEMPS | (2u << 24)) || !dx_w(e, e->max_temp)))
      return false;
   for (i = 0; i < e->nxtemp; i++)
      if (!dx_w(e, DX_DCL_INDEXABLE_TEMP | (4u << 24)) || !dx_w(e, i)
            || !dx_w(e, e->xtemp_len[i]) || !dx_w(e, 4))
         return false;
   return true;
}

/* ------------------------------------------------------------------ */
/* Entry                                                               */
/* ------------------------------------------------------------------ */

static bool dx_put_chunk(struct rslc_ctx *c, struct rslc_pbuf *b, const char *tag,
      const struct rslc_pbuf *body)
{
   uint32_t len = (uint32_t)body->len;
   return rslc_pbuf_put(c, b, tag, 4) && rslc_pbuf_put(c, b, &len, 4)
      && rslc_pbuf_put(c, b, body->data, body->len);
}

bool rslc_emit_dxbc(struct rslc_ctx *c, struct rslc_result *result)
{
   const struct rslc_prog *pr = &c->prog;
   struct dx_emit *e = (struct dx_emit*)rslc_arena_zalloc(c, sizeof(*e));
   struct rslc_pbuf decl, isgn, osgn, shex, dxbc, blob;
   uint8_t *active[2];
   uint32_t i, prog_type, total, hdr[8];
   bool ok;

   if (!e)
      return false;
   if (!pr->main_fn)
   {
      rslc_err(c, "no main() in stage", NULL, 0);
      return false;
   }
   e->c     = c;
   e->sm    = c->opts->version ? c->opts->version : 50;
   if (e->sm != 40 && e->sm != 41 && e->sm != 50)
   {
      rslc_err(c, "dxbc: shader model 4.0, 4.1 or 5.0", NULL, 0);
      return false;
   }
   if (c->stage == RSLC_STAGE_GEOMETRY)
   {
      if (!pr->gs_in || !pr->gs_out || !pr->gs_max_vertices)
      {
         rslc_err(c, "dxbc: a geometry stage needs its primitives and max_vertices", NULL, 0);
         return false;
      }
      if (pr->gs_invocations > 1 && e->sm < 50)
      {
         rslc_err(c, "dxbc: geometry instancing needs shader model 5", NULL, 0);
         return false;
      }
      e->gs_verts = RSLC_GS_IN_VERTICES(pr->gs_in);
   }
   if (c->stage == RSLC_STAGE_COMPUTE && e->sm < 50)
   {
      rslc_err(c, "dxbc: a compute stage needs shader model 5", NULL, 0);
      return false;
   }
   e->vars     = (struct dx_var*)rslc_arena_zalloc(c, (pr->nvars + 1) * sizeof(*e->vars));
   e->tex_bind = (uint32_t*)rslc_arena_zalloc(c, (pr->nvars + 1) * sizeof(uint32_t));
   e->reach    = (uint8_t*)rslc_arena_zalloc(c, pr->nfuncs + 1);
   e->used     = (uint8_t*)rslc_arena_zalloc(c, pr->nvars + 1);
   if (!e->vars || !e->tex_bind || !e->reach || !e->used)
      return false;
   rslc_reach_funcs(c, e->reach);
   rslc_reach_vars(c, e->reach, e->used);
   e->early_z = pr->early_fragment_tests;

   for (i = 1; i < pr->nfuncs; i++)
      if (e->reach[i] && pr->funcs[i]->body)
         dx_scan(e, pr->funcs[i]->body);
   for (i = 1; i < pr->nvars; i++)
   {
      const struct rslc_var *v = pr->vars[i];
      if (!v->global)
         continue;
      if (v->init)
         dx_scan(e, v->init);
      if (v->storage == RSLC_ST_CONST && v->init && v->type->kind != RSLC_TY_ARRAY
            && v->type->kind != RSLC_TY_STRUCT && dx_is_const(e, v->init))
         e->vars[i].folded = true;
   }

   if (!rslc_pbuf_init(c, &e->code, 0, 4096))
      return false;
   e->out = &e->code;
   ok = dx_interface(e);
   /* the globals' homes, below every function's */
   for (i = 1; ok && i < pr->nvars; i++)
   {
      const struct rslc_var *v = pr->vars[i];
      if (v->global && !e->vars[i].folded && !e->vars[i].has_home
            && (v->storage == RSLC_ST_GLOBAL || v->storage == RSLC_ST_CONST))
         ok = dx_var_home(e, v);
   }
   if (ok)
   {
      e->ntemp = e->max_temp;
      e->out   = &e->code;
      /* in blocks into their temporaries */
      for (i = 0; ok && i < e->nic; i++)
      {
         struct dx_place src = dx_place_of(DXO_INPUT, e->ic_reg[i]);
         struct dx_opnd  so;
         src.comp = e->ic_comp[i];
         so = dx_at(&src, 0, e->ic_n[i]);
         if (e->ic_vert[i] != RSLC_UNSET)
         {
            /* v[vertex][register] */
            so.twod   = 1;
            so.idx[1] = so.idx[0];
            so.idx[0] = e->ic_vert[i];
         }
         ok = dx_mov(e, dx_at(&e->ic_home[i], e->ic_off[i], e->ic_n[i]), so);
      }
      /* outputs and globals start at zero, as HLSL's statics do */
      for (i = 0; ok && i < e->nosig; i++)
      {
         uint32_t k, regs = e->oc_dist[i] ? e->oc_dist[i] : 1;
         for (k = 0; ok && k < regs; k++)
            ok = dx_mov(e, dx_at(&e->oc_home[i], k, 4), dx_splat(dx_immu(0), 4));
      }
      for (i = 1; ok && i < pr->nvars; i++)
      {
         const struct rslc_var *v = pr->vars[i];
         struct dx_path p;
         struct dx_val val;
         uint32_t mark = e->ntemp;
         if (!v->global || e->vars[i].folded || !e->vars[i].has_home
               || (v->storage != RSLC_ST_GLOBAL && v->storage != RSLC_ST_CONST))
            continue;
         dx_path_regs(&p, v->type, e->vars[i].home);
         if (v->init)
            ok = dx_rv(e, v->init, &val) && dx_assign_to(e, &p, &val, v->line);
         else
         {
            uint32_t k, n = dx_slots(v->type);
            for (k = 0; ok && k < n; k++)
               ok = dx_mov(e, dx_at(&e->vars[i].home, k, 4), dx_splat(dx_immu(0), 4));
         }
         e->ntemp = mark;
      }
      ok = ok && dx_stmt(e, pr->main_fn->body)
         && (c->stage == RSLC_STAGE_GEOMETRY || dx_write_outputs(e)) && dx_op0(e, DX_RET, 0);
   }
   if (!ok)
   {
      if (!c->failed)
         rslc_err(c, "dxbc: emission failed", NULL, 0);
      rslc_pbuf_free(c, &e->code);
      return false;
   }

   /* the declarations, the program, the signatures, the container */
   memset(&decl, 0, sizeof(decl));
   memset(&isgn, 0, sizeof(isgn));
   memset(&osgn, 0, sizeof(osgn));
   memset(&shex, 0, sizeof(shex));
   memset(&dxbc, 0, sizeof(dxbc));
   ok = rslc_pbuf_init(c, &decl, 0, 1024) && rslc_pbuf_init(c, &isgn, 0, 256)
      && rslc_pbuf_init(c, &osgn, 0, 256) && rslc_pbuf_init(c, &shex, 0, 4096)
      && rslc_pbuf_init(c, &dxbc, 0, 8192);
   if (ok)
   {
      e->out = &decl;
      ok = dx_decls(e);
   }
   ok = ok && dx_sig_chunk(e, &isgn, e->isig, e->nisig, false)
      && dx_sig_chunk(e, &osgn, e->osig, e->nosig, c->stage == RSLC_STAGE_GEOMETRY && e->sm >= 50);
   if (ok)
   {
      uint32_t words = (uint32_t)((decl.len + e->code.len) / 4) + 2;
      uint32_t ver;
      prog_type = c->stage == RSLC_STAGE_FRAGMENT ? 0u : c->stage == RSLC_STAGE_VERTEX ? 1u
         : c->stage == RSLC_STAGE_GEOMETRY ? 2u : 5u;
      ver = (prog_type << 16) | ((e->sm / 10) << 4) | (e->sm % 10);
      ok = rslc_pbuf_put(c, &shex, &ver, 4) && rslc_pbuf_put(c, &shex, &words, 4)
         && rslc_pbuf_put(c, &shex, decl.data, decl.len)
         && rslc_pbuf_put(c, &shex, e->code.data, e->code.len);
   }
   if (ok)
   {
      /* header, three chunk offsets, the chunks */
      uint32_t off = 32 + 3 * 4;
      uint32_t offs[3];
      offs[0] = off;
      off += 8 + (uint32_t)isgn.len;
      offs[1] = off;
      off += 8 + (uint32_t)osgn.len;
      offs[2] = off;
      off += 8 + (uint32_t)shex.len;
      total = off;
      memcpy(hdr, "DXBC", 4);
      hdr[1] = hdr[2] = hdr[3] = hdr[4] = 0;
      hdr[5] = 1;
      hdr[6] = total;
      hdr[7] = 3;
      ok = rslc_pbuf_put(c, &dxbc, hdr, 32) && rslc_pbuf_put(c, &dxbc, offs, 12)
         && dx_put_chunk(c, &dxbc, "ISGN", &isgn)
         && dx_put_chunk(c, &dxbc, c->stage == RSLC_STAGE_GEOMETRY && e->sm >= 50 ? "OSG5" : "OSGN",
            &osgn)
         && dx_put_chunk(c, &dxbc, e->sm >= 50 ? "SHEX" : "SHDR", &shex);
      if (ok)
         dx_checksum(dxbc.data, total);
   }
   active[0] = active[1] = NULL;
   ok = ok && rslc_blob_begin(c, &blob, dxbc.len, active, e->used)
      && rslc_pbuf_put(c, &blob, dxbc.data, dxbc.len);
   rslc_pbuf_free(c, &decl);
   rslc_pbuf_free(c, &isgn);
   rslc_pbuf_free(c, &osgn);
   rslc_pbuf_free(c, &shex);
   rslc_pbuf_free(c, &dxbc);
   rslc_pbuf_free(c, &e->code);
   if (!ok)
      return false;
   rslc_blob_end(c, &blob, RSLC_DST_DXBC, result);
   {
      const struct rslc_blob_hdr *h = (const struct rslc_blob_hdr*)result->blob;
      result->spirv     = (const uint32_t*)((const uint8_t*)result->blob + h->payload_off);
      result->spirv_len = h->payload_len / 4;
   }
   return true;
}

#endif /* RSLC_EXTENDED && !RSLC_NO_DXBC */
