/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2026 - The RetroArch team
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

/* Lowers Vulkan flavoured SPIR-V, as emitted by glslang for .slang shaders,
 * into SPIR-V that is legal for the OpenGL execution environment defined by
 * GL_ARB_gl_spirv and OpenGL 4.6.
 *
 * Only two things actually differ for the modules the slang filter chain
 * produces:
 *
 *   1. The *PushConstant* Storage Class does not exist in OpenGL SPIR-V.
 *      Appendix A.spv.4 of ARB_gl_spirv restricts Storage Class to
 *      UniformConstant/Input/Uniform/Output/Workgroup/Private/Function/
 *      AtomicCounter/Image. The push constant block is therefore turned into
 *      an ordinary *Uniform* Block, i.e. a second UBO, and given a *Binding*
 *      decoration, which ARB_gl_spirv requires on uniform block variables.
 *
 *   2. *DescriptorSet* must always be 0 if present. slang mandates set #0 for
 *      every resource already, so this is validated, never rewritten.
 *
 * Anything the transform cannot prove safe is rejected so the caller falls
 * back to the SPIRV-Cross path. Being conservative costs a slower compile;
 * being wrong costs a broken shader.
 *
 * Two subtleties are worth spelling out.
 *
 * Layout: a push constant block follows std430 packing whereas a uniform
 * block follows std140. Section 7.6.2.spv of ARB_gl_spirv only requires the
 * explicit Offset/ArrayStride/MatrixStride decorations to obey the alignment
 * rules, not the packing rules, and for scalars, vectors and matrices std430
 * and std140 alignment are identical. The two layouts only diverge for arrays
 * and nested structs, whose base alignment std140 rounds up to 16, so a push
 * constant block containing either is rejected rather than relaid out.
 *
 * Pointer identity: rewriting the Storage Class of an OpTypePointer can
 * produce a declaration identical to one the module already has for the UBO,
 * and section 2.8 of the SPIR-V specification requires non-aggregate types to
 * be unique. (SPV_KHR_variable_pointers relaxes this for pointers, but that
 * extension is not in play here.) Redundant pointer declarations are
 * therefore dropped and their uses redirected at the pre-existing type. This
 * is sound because the Logical addressing model, which is validated below and
 * which ARB_gl_spirv mandates, confines pointer values to OpVariable and the
 * access chain instructions.
 */

#include <stdint.h>
#include <string.h>

#include <boolean.h>

#include "spirv_opengl.h"

#define SPV_MAGIC                          0x07230203u
#define SPV_VERSION_1_0                    0x00010000u
#define SPV_HEADER_WORDS                   5

#define SPV_OP_UNDEF                       1
#define SPV_OP_NAME                        5
#define SPV_OP_MEMBER_NAME                 6
#define SPV_OP_EXTENSION                   10
#define SPV_OP_MEMORY_MODEL                14
#define SPV_OP_CAPABILITY                  17
#define SPV_OP_TYPE_BOOL                   20
#define SPV_OP_TYPE_INT                    21
#define SPV_OP_TYPE_FLOAT                  22
#define SPV_OP_TYPE_VECTOR                 23
#define SPV_OP_TYPE_MATRIX                 24
#define SPV_OP_TYPE_SAMPLER                26
#define SPV_OP_TYPE_ARRAY                  28
#define SPV_OP_TYPE_RUNTIME_ARRAY          29
#define SPV_OP_TYPE_STRUCT                 30
#define SPV_OP_TYPE_POINTER                32
#define SPV_OP_TYPE_FUNCTION               33
#define SPV_OP_TYPE_FORWARD_POINTER        39
#define SPV_OP_CONSTANT_NULL               46
#define SPV_OP_FUNCTION_PARAMETER          55
#define SPV_OP_VARIABLE                    59
#define SPV_OP_ACCESS_CHAIN                65
#define SPV_OP_IN_BOUNDS_ACCESS_CHAIN      66
#define SPV_OP_PTR_ACCESS_CHAIN            67
#define SPV_OP_IN_BOUNDS_PTR_ACCESS_CHAIN  70
#define SPV_OP_DECORATE                    71
#define SPV_OP_MEMBER_DECORATE             72
#define SPV_OP_DECORATION_GROUP            73
#define SPV_OP_GROUP_DECORATE              74
#define SPV_OP_GROUP_MEMBER_DECORATE       75

#define SPV_DEC_BLOCK                      2
#define SPV_DEC_GLSL_SHARED                8
#define SPV_DEC_GLSL_PACKED                9
#define SPV_DEC_BUILTIN                    11
#define SPV_DEC_BINDING                    33
#define SPV_DEC_DESCRIPTOR_SET             34

#define SPV_BUILTIN_VERTEX_INDEX           42
#define SPV_BUILTIN_INSTANCE_INDEX         43

#define SPV_SC_UNIFORM_CONSTANT            0
#define SPV_SC_INPUT                       1
#define SPV_SC_UNIFORM                     2
#define SPV_SC_OUTPUT                      3
#define SPV_SC_WORKGROUP                   4
#define SPV_SC_PRIVATE                     6
#define SPV_SC_FUNCTION                    7
#define SPV_SC_PUSH_CONSTANT               9
#define SPV_SC_ATOMIC_COUNTER              10
#define SPV_SC_IMAGE                       11

/* Distinct pointee types a push constant block may be accessed through.
 * A block would need this many distinct member types to overflow. */
#define SPV_MAX_PUSH_POINTERS              32
/* Members we are willing to inspect in a push constant block. */
#define SPV_MAX_STRUCT_MEMBERS             256

struct spv_ptr_remap
{
   /* OpTypePointer PushConstant result <id>. */
   uint32_t from;
   /* Equivalent Uniform pointer to redirect to, or 'from' when the
    * declaration is simply rewritten in place. */
   uint32_t to;
   /* Pointee type <id>. */
   uint32_t pointee;
   bool     drop;
};

struct spv_scan
{
   const uint32_t      *words;
   size_t               count;
   /* Word offset one past the final annotation instruction. */
   size_t               annotation_end;
   /* Result <id> of the single PushConstant OpVariable, 0 if none. */
   uint32_t             push_var_id;
   /* <id> of the struct type the push constant variable points at. */
   uint32_t             push_type_id;
   unsigned             push_var_count;
   unsigned             ptr_count;
   bool                 has_annotations;
   struct spv_ptr_remap ptrs[SPV_MAX_PUSH_POINTERS];
};

/* ARB_gl_spirv Appendix A.spv.3. Deliberately a subset: what a .slang shader
 * plausibly needs, and nothing whose GL version requirements would have to be
 * reasoned about here. Notably this excludes VariablePointers, which the
 * pointer deduplication below depends on being absent. */
static bool spirv_opengl_capability_ok(uint32_t cap)
{
   switch (cap)
   {
      case 0:  /* Matrix                            */
      case 1:  /* Shader                            */
      case 10: /* Float64                           */
      case 25: /* ImageGatherExtended               */
      case 28: /* UniformBufferArrayDynamicIndexing */
      case 29: /* SampledImageArrayDynamicIndexing  */
      case 32: /* ClipDistance                      */
      case 33: /* CullDistance                      */
      case 34: /* ImageCubeArray                    */
      case 35: /* SampleRateShading                 */
      case 37: /* SampledRect                       */
      case 43: /* Sampled1D                         */
      case 45: /* SampledCubeArray                  */
      case 46: /* SampledBuffer                     */
      case 49: /* StorageImageExtendedFormats       */
      case 50: /* ImageQuery                        */
      case 51: /* DerivativeControl                 */
      case 52: /* InterpolationFunction             */
      case 57: /* MultiViewport                     */
         return true;
      default:
         break;
   }

   return false;
}

static bool spirv_opengl_storage_class_ok(uint32_t sc)
{
   switch (sc)
   {
      case SPV_SC_UNIFORM_CONSTANT:
      case SPV_SC_INPUT:
      case SPV_SC_UNIFORM:
      case SPV_SC_OUTPUT:
      case SPV_SC_WORKGROUP:
      case SPV_SC_PRIVATE:
      case SPV_SC_FUNCTION:
      case SPV_SC_ATOMIC_COUNTER:
      case SPV_SC_IMAGE:
      /* Rewritten to Uniform below. */
      case SPV_SC_PUSH_CONSTANT:
         return true;
      default:
         break;
   }

   return false;
}

/* std140 and std430 agree on alignment for everything but arrays and structs,
 * so a push constant block made purely of scalars, vectors and matrices
 * already satisfies section 7.6.2.spv once it becomes a uniform block. */
static bool spirv_opengl_member_type_ok(uint32_t op)
{
   switch (op)
   {
      /* OpTypeBool is deliberately absent: it has no defined size or
       * layout in a block, so encountering one means the module is not
       * shaped the way this code assumes. */
      case SPV_OP_TYPE_INT:
      case SPV_OP_TYPE_FLOAT:
      case SPV_OP_TYPE_VECTOR:
      case SPV_OP_TYPE_MATRIX:
         return true;
      default:
         break;
   }

   return false;
}

/* Returns the opcode defining <id>, or 0. */
static uint32_t spirv_opengl_defining_op(const uint32_t *words, size_t count,
      uint32_t id)
{
   size_t offset = SPV_HEADER_WORDS;

   while (offset < count)
   {
      uint32_t word0 = words[offset];
      uint32_t op    = word0 & 0xffffu;
      uint32_t len   = word0 >> 16;

      if (len < 1 || offset + len > count)
         break;

      switch (op)
      {
         case SPV_OP_TYPE_BOOL:
         case SPV_OP_TYPE_INT:
         case SPV_OP_TYPE_FLOAT:
         case SPV_OP_TYPE_VECTOR:
         case SPV_OP_TYPE_MATRIX:
         case SPV_OP_TYPE_SAMPLER:
         case SPV_OP_TYPE_ARRAY:
         case SPV_OP_TYPE_RUNTIME_ARRAY:
         case SPV_OP_TYPE_STRUCT:
         case SPV_OP_TYPE_POINTER:
            if (len >= 2 && words[offset + 1] == id)
               return op;
            break;
         default:
            break;
      }
      offset += len;
   }


   return 0;
}

/* The annotation section precedes the type section, so decorations cannot be
 * matched against ids discovered later in the same pass. */
static bool spirv_opengl_has_decoration(const uint32_t *words, size_t count,
      uint32_t id, uint32_t decoration)
{
   size_t offset = SPV_HEADER_WORDS;

   while (offset < count)
   {
      uint32_t word0 = words[offset];
      uint32_t op    = word0 & 0xffffu;
      uint32_t len   = word0 >> 16;

      if (len < 1 || offset + len > count)
         break;

      if (     op == SPV_OP_DECORATE
            && len >= 3
            && words[offset + 1] == id
            && words[offset + 2] == decoration)
         return true;
      offset += len;
   }


   return false;
}

/* Every member of the push constant block struct must be a plain scalar,
 * vector or matrix. See the layout note at the top of this file. */
static bool spirv_opengl_push_block_ok(const uint32_t *words, size_t count,
      uint32_t struct_id)
{
   size_t offset = SPV_HEADER_WORDS;

   while (offset < count)
   {
      uint32_t word0 = words[offset];
      uint32_t op    = word0 & 0xffffu;
      uint32_t len   = word0 >> 16;

      if (len < 1 || offset + len > count)
         return false;

      if (     op == SPV_OP_TYPE_STRUCT
            && len >= 2
            && words[offset + 1] == struct_id)
      {
         uint32_t i;
         uint32_t members = len - 2;

         if (members == 0 || members > SPV_MAX_STRUCT_MEMBERS)
            return false;

         for (i = 0; i < members; i++)
         {
            if (!spirv_opengl_member_type_ok(spirv_opengl_defining_op(
                        words, count, words[offset + 2 + i])))
               return false;
         }

         return true;
      }
      offset += len;
   }


   return false;
}

static struct spv_ptr_remap *spirv_opengl_find_ptr(struct spv_scan *scan,
      uint32_t id)
{
   unsigned i;

   for (i = 0; i < scan->ptr_count; i++)
   {
      if (scan->ptrs[i].from == id)
         return &scan->ptrs[i];
   }

   return NULL;
}

static bool spirv_opengl_is_push_ptr(struct spv_scan *scan, uint32_t id)
{
   return id != 0 && spirv_opengl_find_ptr(scan, id) != NULL;
}

/* Under the Logical addressing model, and with VariablePointers absent, a
 * pointer value can only be produced by OpVariable or an access chain. The
 * instructions below are therefore the only ones that may legitimately name a
 * push constant pointer type outside of those two, and none of them should.
 * Anything else means the module is shaped in a way this code did not
 * anticipate, so reject it. Only operand positions that are unambiguously
 * <id>s are examined, so a literal can never be mistaken for a reference. */
static bool spirv_opengl_ptr_uses_ok(struct spv_scan *scan)
{
   const uint32_t *words = scan->words;
   size_t          count = scan->count;
   size_t          offset = SPV_HEADER_WORDS;

   while (offset < count)
   {
      uint32_t word0 = words[offset];
      uint32_t op    = word0 & 0xffffu;
      uint32_t len   = word0 >> 16;

      if (len < 1 || offset + len > count)
         return false;

      switch (op)
      {
         /* Result type, or the type being declared. */
         case SPV_OP_UNDEF:
         case SPV_OP_CONSTANT_NULL:
         case SPV_OP_FUNCTION_PARAMETER:
            if (len >= 2 && spirv_opengl_is_push_ptr(scan, words[offset + 1]))
               return false;
            break;

         /* Decoration and debug targets. A dropped pointer declaration must
          * not leave one of these dangling. */
         case SPV_OP_NAME:
         case SPV_OP_MEMBER_NAME:
         case SPV_OP_DECORATE:
         case SPV_OP_MEMBER_DECORATE:
            if (len >= 2 && spirv_opengl_is_push_ptr(scan, words[offset + 1]))
               return false;
            break;

         /* Pointee of another pointer, i.e. a pointer to a pointer. */
         case SPV_OP_TYPE_POINTER:
            if (len >= 4 && spirv_opengl_is_push_ptr(scan, words[offset + 3]))
               return false;
            break;

         /* Element type and length are both <id>s. */
         case SPV_OP_TYPE_ARRAY:
            if (     len >= 4
                  && (     spirv_opengl_is_push_ptr(scan, words[offset + 2])
                        || spirv_opengl_is_push_ptr(scan, words[offset + 3])))
               return false;
            break;

         case SPV_OP_TYPE_RUNTIME_ARRAY:
            if (len >= 3 && spirv_opengl_is_push_ptr(scan, words[offset + 2]))
               return false;
            break;

         /* Return type followed by parameter types, or member types. */
         case SPV_OP_TYPE_FUNCTION:
         case SPV_OP_TYPE_STRUCT:
         {
            uint32_t i;
            for (i = 2; i < len; i++)
            {
               if (spirv_opengl_is_push_ptr(scan, words[offset + i]))
                  return false;
            }
            break;
         }

         default:
            break;
      }
      offset += len;
   }


   return true;
}

static bool spirv_opengl_scan(struct spv_scan *scan)
{
   const uint32_t *words = scan->words;
   size_t          count = scan->count;
   size_t          offset = SPV_HEADER_WORDS;

   while (offset < count)
   {
      uint32_t word0 = words[offset];
      uint32_t op    = word0 & 0xffffu;
      uint32_t len   = word0 >> 16;

      if (len < 1 || offset + len > count)
         return false;

      switch (op)
      {
         case SPV_OP_CAPABILITY:
            if (len < 2 || !spirv_opengl_capability_ok(words[offset + 1]))
               return false;
            break;

         /* SPIR-V extensions carry environment rules of their own; refuse to
          * reason about them. */
         case SPV_OP_EXTENSION:
            return false;

         case SPV_OP_MEMORY_MODEL:
            /* Addressing model must be Logical. */
            if (len < 3 || words[offset + 1] != 0)
               return false;
            break;

         /* Separate samplers are not part of OpenGL SPIR-V. */
         case SPV_OP_TYPE_SAMPLER:
            return false;

         /* Decoration groups would make the decoration analysis unsound, and
          * glslang never emits them. */
         case SPV_OP_DECORATION_GROUP:
         case SPV_OP_GROUP_DECORATE:
         case SPV_OP_GROUP_MEMBER_DECORATE:
            return false;

         case SPV_OP_TYPE_FORWARD_POINTER:
         case SPV_OP_PTR_ACCESS_CHAIN:
         case SPV_OP_IN_BOUNDS_PTR_ACCESS_CHAIN:
            return false;

         case SPV_OP_TYPE_POINTER:
            if (len < 4)
               return false;
            if (!spirv_opengl_storage_class_ok(words[offset + 2]))
               return false;
            if (words[offset + 2] == SPV_SC_PUSH_CONSTANT)
            {
               if (scan->ptr_count >= SPV_MAX_PUSH_POINTERS)
                  return false;
               scan->ptrs[scan->ptr_count].from    = words[offset + 1];
               scan->ptrs[scan->ptr_count].to      = words[offset + 1];
               scan->ptrs[scan->ptr_count].pointee = words[offset + 3];
               scan->ptrs[scan->ptr_count].drop    = false;
               scan->ptr_count++;
            }
            break;

         case SPV_OP_VARIABLE:
            if (len < 4)
               return false;
            if (!spirv_opengl_storage_class_ok(words[offset + 3]))
               return false;
            if (words[offset + 3] == SPV_SC_PUSH_CONSTANT)
            {
               struct spv_ptr_remap *ptr;

               if (++scan->push_var_count > 1)
                  return false;

               if (!(ptr = spirv_opengl_find_ptr(scan, words[offset + 1])))
                  return false;

               scan->push_var_id  = words[offset + 2];
               scan->push_type_id = ptr->pointee;
            }
            break;

         case SPV_OP_DECORATE:
            if (len < 3)
               return false;
            scan->has_annotations = true;
            scan->annotation_end  = offset + len;

            switch (words[offset + 2])
            {
               case SPV_DEC_DESCRIPTOR_SET:
                  if (len < 4 || words[offset + 3] != 0)
                     return false;
                  break;
               case SPV_DEC_BUILTIN:
                  if (     len >= 4
                        && (     words[offset + 3] == SPV_BUILTIN_VERTEX_INDEX
                              || words[offset + 3] == SPV_BUILTIN_INSTANCE_INDEX))
                     return false;
                  break;
               case SPV_DEC_GLSL_SHARED:
               case SPV_DEC_GLSL_PACKED:
                  return false;
               default:
                  break;
            }
            break;

         case SPV_OP_MEMBER_DECORATE:
            if (len < 4)
               return false;
            scan->has_annotations = true;
            scan->annotation_end  = offset + len;

            switch (words[offset + 3])
            {
               case SPV_DEC_BUILTIN:
                  if (     len >= 5
                        && (     words[offset + 4] == SPV_BUILTIN_VERTEX_INDEX
                              || words[offset + 4] == SPV_BUILTIN_INSTANCE_INDEX))
                     return false;
                  break;
               case SPV_DEC_GLSL_SHARED:
               case SPV_DEC_GLSL_PACKED:
                  return false;
               default:
                  break;
            }
            break;

         default:
            break;
      }
      offset += len;
   }


   return true;
}

/* Point each push constant pointer type at an equivalent Uniform pointer if
 * the module already declares one, so that rewriting the Storage Class cannot
 * introduce a duplicate non-aggregate type. */
static void spirv_opengl_dedup_pointers(struct spv_scan *scan)
{
   const uint32_t *words  = scan->words;
   size_t          count  = scan->count;
   size_t          offset = SPV_HEADER_WORDS;
   unsigned        i;

   while (offset < count)
   {
      uint32_t word0 = words[offset];
      uint32_t op    = word0 & 0xffffu;
      uint32_t len   = word0 >> 16;

      if (len < 1 || offset + len > count)
         break;

      if (op == SPV_OP_TYPE_POINTER && len >= 4
            && words[offset + 2] == SPV_SC_UNIFORM)
      {
         for (i = 0; i < scan->ptr_count; i++)
         {
            if (     !scan->ptrs[i].drop
                  && scan->ptrs[i].pointee == words[offset + 3])
            {
               scan->ptrs[i].to   = words[offset + 1];
               scan->ptrs[i].drop = true;
            }
         }
      }
      offset += len;
   }

   /* Two push constant pointers sharing a pointee would collide with each
    * other once both are rewritten in place. */
   for (i = 0; i < scan->ptr_count; i++)
   {
      unsigned j;

      if (scan->ptrs[i].drop)
         continue;

      for (j = i + 1; j < scan->ptr_count; j++)
      {
         if (     !scan->ptrs[j].drop
               && scan->ptrs[j].pointee == scan->ptrs[i].pointee)
         {
            scan->ptrs[j].to   = scan->ptrs[i].from;
            scan->ptrs[j].drop = true;
         }
      }
   }

}

size_t spirv_opengl_lower(
      const uint32_t *in_words, size_t in_count,
      uint32_t *out_words, size_t out_capacity,
      unsigned push_binding)
{
   struct spv_scan scan;
   size_t offset;
   size_t out_offset;

   if (!in_words || !out_words || in_count < SPV_HEADER_WORDS)
      return 0;
   if (in_words[0] != SPV_MAGIC)
      return 0;
   /* ARB_gl_spirv mandates SPIR-V 1.0 support and nothing beyond it. */
   if (in_words[1] > SPV_VERSION_1_0)
      return 0;
   if (out_capacity < in_count + SPIRV_OPENGL_LOWER_EXTRA_WORDS)
      return 0;

   memset(&scan, 0, sizeof(scan));
   scan.words = in_words;
   scan.count = in_count;

   if (!spirv_opengl_scan(&scan))
      return 0;

   if (!scan.push_var_id)
   {
      /* Nothing to rewrite; a module without push constants is already
       * legal for OpenGL. */
      if (scan.ptr_count)
         return 0;
      memcpy(out_words, in_words, in_count * sizeof(uint32_t));
      return in_count;
   }

   if (!scan.has_annotations)
      return 0;
   /* Only a Block decorated struct can become a uniform block. */
   if (!spirv_opengl_has_decoration(in_words, in_count,
            scan.push_type_id, SPV_DEC_BLOCK))
      return 0;
   /* A bound push constant variable is not something glslang emits, and
    * would mean the analysis has missed something. */
   if (spirv_opengl_has_decoration(in_words, in_count,
            scan.push_var_id, SPV_DEC_BINDING))
      return 0;
   if (!spirv_opengl_push_block_ok(in_words, in_count, scan.push_type_id))
      return 0;
   if (!spirv_opengl_ptr_uses_ok(&scan))
      return 0;

   spirv_opengl_dedup_pointers(&scan);

   memcpy(out_words, in_words, SPV_HEADER_WORDS * sizeof(uint32_t));

   offset     = SPV_HEADER_WORDS;
   out_offset = SPV_HEADER_WORDS;

   while (offset < in_count)
   {
      uint32_t word0             = in_words[offset];
      uint32_t op                = word0 & 0xffffu;
      uint32_t len               = word0 >> 16;
      struct spv_ptr_remap *ptr  = NULL;
      bool drop                  = false;

      switch (op)
      {
         case SPV_OP_TYPE_POINTER:
            if ((ptr = spirv_opengl_find_ptr(&scan, in_words[offset + 1])))
               drop = ptr->drop;
            break;
         case SPV_OP_VARIABLE:
         case SPV_OP_ACCESS_CHAIN:
         case SPV_OP_IN_BOUNDS_ACCESS_CHAIN:
            if (len >= 2)
               ptr = spirv_opengl_find_ptr(&scan, in_words[offset + 1]);
            break;
         default:
            break;
      }

      if (!drop)
      {
         memcpy(&out_words[out_offset], &in_words[offset],
               len * sizeof(uint32_t));

         if (ptr)
         {
            if (op == SPV_OP_TYPE_POINTER)
               out_words[out_offset + 2] = SPV_SC_UNIFORM;
            else
               out_words[out_offset + 1] = ptr->to;
         }

         if (op == SPV_OP_VARIABLE
               && out_words[out_offset + 3] == SPV_SC_PUSH_CONSTANT)
            out_words[out_offset + 3] = SPV_SC_UNIFORM;

         out_offset += len;
      }

      offset += len;

      /* Splice on the two decorations the rewritten block now needs. They
       * belong at the end of the annotation section. */
      if (offset == scan.annotation_end)
      {
         out_words[out_offset++] = (4u << 16) | SPV_OP_DECORATE;
         out_words[out_offset++] = scan.push_var_id;
         out_words[out_offset++] = SPV_DEC_BINDING;
         out_words[out_offset++] = (uint32_t)push_binding;

         out_words[out_offset++] = (4u << 16) | SPV_OP_DECORATE;
         out_words[out_offset++] = scan.push_var_id;
         out_words[out_offset++] = SPV_DEC_DESCRIPTOR_SET;
         out_words[out_offset++] = 0;
      }
   }

   return out_offset;
}
