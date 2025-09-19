#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <wiiu/types.h>
#include <wiiu/system/memory.h>
#include <wiiu/shader_utils.h>
#include <wiiu/wiiu_dbg.h>
#include <gx2/ra_shaders.h>
#include <gx2/enum.h>
#include <gx2/mem.h>

void GX2InitShader(GX2Shader *shader)
{
   if (shader->fs.program)
      return;

   shader->fs.size = GX2CalcFetchShaderSizeEx(shader->vs.attribVarCount,
                     GX2_FETCH_SHADER_TESSELLATION_NONE, GX2_TESSELLATION_MODE_DISCRETE);
   shader->fs.program = MEM2_alloc(shader->fs.size, GX2_SHADER_PROGRAM_ALIGNMENT);
   GX2InitFetchShaderEx(&shader->fs, (uint8_t *)shader->fs.program,
                        shader->vs.attribVarCount,
                        shader->attribute_stream,
                        GX2_FETCH_SHADER_TESSELLATION_NONE, GX2_TESSELLATION_MODE_DISCRETE);
   GX2Invalidate(GX2_INVALIDATE_MODE_CPU_SHADER, shader->fs.program, shader->fs.size);
}

void GX2DestroyShader(GX2Shader *shader)
{
   MEM2_free(shader->fs.program);
   shader->fs.program = NULL;
}

void GX2SetShader(GX2Shader *shader)
{
   GX2SetVertexShader(&shader->vs);
   GX2SetPixelShader(&shader->ps);
   GX2SetFetchShader(&shader->fs);

   if (shader->gs.program)
      GX2SetGeometryShader(&shader->gs);
}

#if 0
static void dump_vs_data(GX2VertexShader* vs)
{
   unsigned i;

   DEBUG_INT(vs->size);
   DEBUG_VAR(vs->mode);
   DEBUG_INT(vs->uniformBlockCount);

   for (i = 0; i < vs->uniformBlockCount; i++)
   {
      DEBUG_STR(vs->uniformBlocks[i].name);
      DEBUG_INT(vs->uniformBlocks[i].offset);
      DEBUG_INT(vs->uniformBlocks[i].size);
   }
   DEBUG_INT(vs->uniformVarCount);
   for (i = 0; i < vs->uniformVarCount; i++)
   {
      DEBUG_STR(vs->uniformVars[i].name);
      DEBUG_INT(vs->uniformVars[i].offset);
      DEBUG_INT(vs->uniformVars[i].type);
      DEBUG_INT(vs->uniformVars[i].count);
      DEBUG_INT(vs->uniformVars[i].block);
   }
   DEBUG_INT(vs->initialValueCount);
   for (i = 0; i < vs->initialValueCount; i++)
   {
      DEBUG_INT(vs->initialValues[i].offset);
      DEBUG_FLOAT(vs->initialValues[i].value[0]);
      DEBUG_FLOAT(vs->initialValues[i].value[1]);
      DEBUG_FLOAT(vs->initialValues[i].value[2]);
      DEBUG_FLOAT(vs->initialValues[i].value[3]);
   }
   DEBUG_INT(vs->loopVarCount);
   for (i = 0; i < vs->loopVarCount; i++)
   {
      DEBUG_INT(vs->loopVars[i].offset);
      DEBUG_VAR(vs->loopVars[i].value);
   }
   DEBUG_INT(vs->samplerVarCount);
   for (i = 0; i < vs->samplerVarCount; i++)
   {
      DEBUG_STR(vs->samplerVars[i].name);
      DEBUG_INT(vs->samplerVars[i].type);
      DEBUG_INT(vs->samplerVars[i].location);
   }

   for (i = 0; i < vs->attribVarCount; i++)
   {
      DEBUG_STR(vs->attribVars[i].name);
      DEBUG_VAR(vs->attribVars[i].type);
      DEBUG_INT(vs->attribVars[i].location);
      DEBUG_INT(vs->attribVars[i].count);
   }
}

static void dump_ps_data(GX2PixelShader* ps)
{
   unsigned i;

   DEBUG_INT(ps->size);
   DEBUG_VAR(ps->mode);
   DEBUG_INT(ps->uniformBlockCount);
   for (i = 0; i < ps->uniformBlockCount; i++)
   {
      DEBUG_STR(ps->uniformBlocks[i].name);
      DEBUG_INT(ps->uniformBlocks[i].offset);
      DEBUG_INT(ps->uniformBlocks[i].size);
   }
   DEBUG_INT(ps->uniformVarCount);
   for (i = 0; i < ps->uniformVarCount; i++)
   {
      DEBUG_STR(ps->uniformVars[i].name);
      DEBUG_INT(ps->uniformVars[i].offset);
      DEBUG_INT(ps->uniformVars[i].type);
      DEBUG_INT(ps->uniformVars[i].count);
      DEBUG_INT(ps->uniformVars[i].block);
   }
   DEBUG_INT(ps->initialValueCount);
   for (i = 0; i < ps->initialValueCount; i++)
   {
      DEBUG_INT(ps->initialValues[i].offset);
      DEBUG_FLOAT(ps->initialValues[i].value[0]);
      DEBUG_FLOAT(ps->initialValues[i].value[1]);
      DEBUG_FLOAT(ps->initialValues[i].value[2]);
      DEBUG_FLOAT(ps->initialValues[i].value[3]);
   }
   DEBUG_INT(ps->loopVarCount);
   for (i = 0; i < ps->loopVarCount; i++)
   {
      DEBUG_INT(ps->loopVars[i].offset);
      DEBUG_VAR(ps->loopVars[i].value);
   }
   DEBUG_INT(ps->samplerVarCount);
   for (i = 0; i < ps->samplerVarCount; i++)
   {
      DEBUG_STR(ps->samplerVars[i].name);
      DEBUG_INT(ps->samplerVars[i].type);
      DEBUG_INT(ps->samplerVars[i].location);
   }
}

static void check_shader_verbose(u32 *shader, u32 shader_size, u32 *org, u32 org_size, const char *name)
{
   unsigned i;

   printf("%s :\n", name);
   DEBUG_VAR(shader_size);
   DEBUG_VAR(org_size);

   if (shader_size != org_size)
      printf("size mismatch : 0x%08lX should be 0x%08lX\n", shader_size, org_size);

   for (i = 0; i < shader_size / 4; i += 4)
   {
      printf("0x%08lX 0x%08lX 0x%08lX 0x%08lX          0x%08lX 0x%08lX 0x%08lX 0x%08lX\n",
             shader[i], shader[i + 1], shader[i + 2], shader[i + 3],
             org[i], org[i + 1], org[i + 2], org[i + 3]);
   }

   for (i = 0; i < shader_size / 4; i++)
   {
      if (shader[i] != org[i])
         printf("%i(%X): 0x%08lX(0x%08lX) should be 0x%08lX(0x%08lX) \n", i, i, shader[i], __builtin_bswap32(shader[i]), org[i],
                __builtin_bswap32(org[i]));
   }
}

static void check_shader(const void *shader_, u32 shader_size, const void *org_, u32 org_size, const char *name)
{
   unsigned i;
   bool different = false;
   u32 *shader    = (u32 *)shader_;
   u32 *org       = (u32 *)org_;

   printf("%-20s : ", name);

   if (shader_size != org_size)
   {
      different = true;
      printf("\nsize mismatch : 0x%08lX should be 0x%08lX", shader_size, org_size);
   }

   for (i = 0; i < shader_size / 4; i++)
   {
      if (shader[i] != org[i])
      {
         different = true;
         printf("\n%i(%X): 0x%08lX(0x%08lX) should be 0x%08lX(0x%08lX)", i, i, shader[i], __builtin_bswap32(shader[i]), org[i],
                __builtin_bswap32(org[i]));
      }
   }

   if (!different)
      printf("no errors");

   printf("\n");
}
#endif

#define MAKE_MAGIC(c0,c1,c2,c3) ((c0 << 24) |(c1 << 16) |(c2 << 8) |(c3 << 0))

#define GFD_FILE_MAJOR_VERSION         7
#define GFD_FILE_GPU_VERSION           2
#define GFD_BLOCK_MAJOR_VERSION        1

#define GFD_FILE_MAGIC                 MAKE_MAGIC('G','f','x','2')
#define GFD_BLOCK_MAGIC                MAKE_MAGIC('B','L','K','{')
#define GFD_BLOCK_RELOCATIONS_MAGIC    MAKE_MAGIC('}','B','L','K')
#define GFD_RELOCATIONS_TYPE_MASK      0xFFF00000
#define GFD_RELOCATIONS_VALUE_MASK     (~GFD_RELOCATIONS_TYPE_MASK)
#define GFD_RELOCATIONS_DATA           0xD0600000
#define GFD_RELOCATIONS_TEXT           0xCA700000

typedef enum
{
   GFD_BLOCK_TYPE_END_OF_FILE           = 1,
   GFD_BLOCK_TYPE_PADDING               = 2,
   GFD_BLOCK_TYPE_VERTEX_SHADER_HEADER  = 3,
   GFD_BLOCK_TYPE_VERTEX_SHADER_PROGRAM = 5,
   GFD_BLOCK_TYPE_PIXEL_SHADER_HEADER   = 6,
   GFD_BLOCK_TYPE_PIXEL_SHADER_PROGRAM  = 7,
} GFDBlockType;

typedef struct
{
   uint32_t magic;
   uint32_t headerSize;
   uint32_t majorVersion;
   uint32_t minorVersion;
   uint32_t gpuVersion;
   uint32_t align;
   uint32_t unk1;
   uint32_t unk2;
} GFDFileHeader;

typedef struct
{
   uint32_t magic;
   uint32_t headerSize;
   uint32_t majorVersion;
   uint32_t minorVersion;
   GFDBlockType type;
   uint32_t dataSize;
   uint32_t id;
   uint32_t index;
} GFDBlockHeader;

typedef struct
{
   uint32_t magic;
   uint32_t headerSize;
   uint32_t unk1;
   uint32_t dataSize;
   uint32_t dataOffset;
   uint32_t textSize;
   uint32_t textOffset;
   uint32_t patchBase;
   uint32_t patchCount;
   uint32_t patchOffset;
} GFDRelocationHeader;

typedef struct
{
   GFDBlockHeader header;
   u8 data[];
} GFDBlock;

void gfd_free(GFDFile* gfd)
{
   if (gfd)
   {
      MEM2_free(gfd->data);
      free(gfd);
   }
}

static bool gfd_relocate_block(GFDBlock* block)
{
   unsigned i;
   GFDRelocationHeader* rel = (GFDRelocationHeader*)(block->data + block->header.dataSize) - 1;

   if (rel->magic != GFD_BLOCK_RELOCATIONS_MAGIC)
      return false;

   if ((rel->patchOffset & GFD_RELOCATIONS_TYPE_MASK) != GFD_RELOCATIONS_DATA)
      return false;

   u32* patches = (u32*)(block->data + (rel->patchOffset & GFD_RELOCATIONS_VALUE_MASK));

   for (i = 0; i < rel->patchCount; i++)
   {
      if (patches[i])
      {
         if ((patches[i] & GFD_RELOCATIONS_TYPE_MASK) != GFD_RELOCATIONS_DATA)
            return false;

         u32* ptr = (u32*)(block->data + (patches[i] & GFD_RELOCATIONS_VALUE_MASK));
         if ((((*ptr) & GFD_RELOCATIONS_TYPE_MASK) != GFD_RELOCATIONS_DATA) &&
            (((*ptr) & GFD_RELOCATIONS_TYPE_MASK) != GFD_RELOCATIONS_TEXT))
            return false;
         *ptr = (u32)block->data + ((*ptr) & GFD_RELOCATIONS_VALUE_MASK);
      }
   }

   return true;
}

GFDFile *gfd_open(const char *filename)
{
   int size;
   GFDFile* gfd = (GFDFile*)calloc(1, sizeof(*gfd));
   FILE     *fp = fopen(filename, "rb");

   if (!fp)
      goto error;

   fseek(fp, 0, SEEK_END);
   size      = ftell(fp);
   fseek(fp, 0, SEEK_SET);
   gfd->data = MEM2_alloc(size, GX2_SHADER_PROGRAM_ALIGNMENT);
   fread(gfd->data, 1, size, fp);
   fclose(fp);

   GFDFileHeader *header = (GFDFileHeader *)gfd->data;

   if (header->magic != GFD_FILE_MAGIC)
      goto error;

   if (header->headerSize != sizeof(GFDFileHeader))
      goto error;

   if (header->majorVersion != GFD_FILE_MAJOR_VERSION)
      goto error;

   if (header->gpuVersion != GFD_FILE_GPU_VERSION)
      goto error;

   if (!header->align)
      goto error;

   GFDBlock *block = (GFDBlock *)(gfd->data + header->headerSize);

   while (block->header.type != GFD_BLOCK_TYPE_END_OF_FILE)
   {
      if (block->header.magic != GFD_BLOCK_MAGIC)
         goto error;

      if (block->header.headerSize != sizeof(GFDBlockHeader))
         goto error;

      if (block->header.majorVersion != GFD_BLOCK_MAJOR_VERSION)
         goto error;

      switch (block->header.type)
      {
         case GFD_BLOCK_TYPE_VERTEX_SHADER_HEADER:
            if (gfd->vs)
               continue;

            gfd->vs = (GX2VertexShader*)block->data;
            if (!gfd_relocate_block(block))
               goto error;

            break;

         case GFD_BLOCK_TYPE_VERTEX_SHADER_PROGRAM:
            if (gfd->vs->program)
               continue;

            GX2Invalidate(GX2_INVALIDATE_MODE_CPU_SHADER, block->data, block->header.dataSize);
            gfd->vs->program = block->data;
            break;

         case GFD_BLOCK_TYPE_PIXEL_SHADER_HEADER:
            if (gfd->ps)
               continue;

            gfd->ps = (GX2PixelShader*)block->data;
            if (!gfd_relocate_block(block))
               goto error;

            break;

         case GFD_BLOCK_TYPE_PIXEL_SHADER_PROGRAM:
            if (gfd->ps->program)
               continue;

            GX2Invalidate(GX2_INVALIDATE_MODE_CPU_SHADER, block->data, block->header.dataSize);
            gfd->ps->program = block->data;
            break;

         default:
            break;
      }

      block = (GFDBlock *)((u8 *)block + block->header.headerSize + block->header.dataSize);
   }

   if (!gfd->vs)
      goto error;

   if (!gfd->vs->program)
      goto error;

   if (!gfd->ps)
      goto error;

   if (!gfd->ps->program)
      goto error;

   return gfd;

error:
   gfd_free(gfd);

   return NULL;
}
