
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <wiiu/gx2.h>
#include <wiiu/system/memory.h>
#include <wiiu/shader_utils.h>
#include <wiiu/wiiu_dbg.h>

/* this is a hack for elf builds since their data section is below 0x10000000
 * and thus can't be accessed by the GX2 hardware */
#ifndef GX2_CAN_ACCESS_DATA_SECTION
typedef struct
{
   void* vs_program;
   void* ps_program;
   void* gs_program;
   void* gs_copy_program;
}org_programs_t;
#endif

void GX2InitShader(GX2Shader* shader)
{
   if (shader->fs.program)
      return;

   shader->fs.size = GX2CalcFetchShaderSizeEx(shader->vs.attribVarCount,
                                                    GX2_FETCH_SHADER_TESSELLATION_NONE, GX2_TESSELLATION_MODE_DISCRETE);
#ifdef GX2_CAN_ACCESS_DATA_SECTION
   shader->fs.program = MEM2_alloc(shader->fs.size, GX2_SHADER_ALIGNMENT);
#else
   shader->fs.program = MEM2_alloc(shader->fs.size + sizeof(org_programs_t), GX2_SHADER_ALIGNMENT);
#endif
   GX2InitFetchShaderEx(&shader->fs, (uint8_t*)shader->fs.program,
                        shader->vs.attribVarCount,
                        shader->attribute_stream,
                        GX2_FETCH_SHADER_TESSELLATION_NONE, GX2_TESSELLATION_MODE_DISCRETE);
   GX2Invalidate(GX2_INVALIDATE_MODE_CPU_SHADER, shader->fs.program, shader->fs.size);

#ifndef GX2_CAN_ACCESS_DATA_SECTION
   org_programs_t* org = (org_programs_t*)(shader->fs.program + shader->fs.size);
   org->vs_program = shader->vs.program;
   org->ps_program = shader->ps.program;
   org->gs_program = shader->gs.program;
   org->gs_copy_program = shader->gs.copyProgram;

   shader->vs.program = MEM2_alloc(shader->vs.size, GX2_SHADER_ALIGNMENT);
   memcpy(shader->vs.program, org->vs_program, shader->vs.size);
   GX2Invalidate(GX2_INVALIDATE_MODE_CPU_SHADER, shader->vs.program, shader->vs.size);

   shader->ps.program = MEM2_alloc(shader->ps.size, GX2_SHADER_ALIGNMENT);
   memcpy(shader->ps.program, org->ps_program, shader->ps.size);
   GX2Invalidate(GX2_INVALIDATE_MODE_CPU_SHADER, shader->ps.program, shader->ps.size);

   if(org->gs_program)
   {
      shader->gs.program = MEM2_alloc(shader->gs.size, GX2_SHADER_ALIGNMENT);
      memcpy(shader->gs.program, org->gs_program, shader->gs.size);
      GX2Invalidate(GX2_INVALIDATE_MODE_CPU_SHADER, shader->gs.program, shader->gs.size);

      shader->gs.copyProgram = MEM2_alloc(shader->gs.copyProgramSize, GX2_SHADER_ALIGNMENT);
      memcpy(shader->gs.copyProgram, org->gs_copy_program, shader->gs.copyProgramSize);
      GX2Invalidate(GX2_INVALIDATE_MODE_CPU_SHADER, shader->gs.copyProgram, shader->gs.copyProgramSize);
   }
#endif

}

void GX2DestroyShader(GX2Shader* shader)
{
#ifndef GX2_CAN_ACCESS_DATA_SECTION
   MEM2_free(shader->vs.program);
   MEM2_free(shader->ps.program);
   MEM2_free(shader->gs.program);
   MEM2_free(shader->gs.copyProgram);

   org_programs_t* org = (org_programs_t*)(shader->fs.program + shader->fs.size);

   shader->vs.program = org->vs_program;
   shader->ps.program = org->ps_program;
   shader->gs.program = org->gs_program;
   shader->gs.copyProgram = org->gs_copy_program;
#endif

   MEM2_free(shader->fs.program);
   shader->fs.program = NULL;
}

void GX2SetShader(GX2Shader* shader)
{
   GX2SetVertexShader(&shader->vs);
   GX2SetPixelShader(&shader->ps);
   GX2SetFetchShader(&shader->fs);
   if(shader->gs.program)
      GX2SetGeometryShader(&shader->gs);
}


void check_shader_verbose(u32* shader, u32 shader_size, u32* org, u32 org_size, const char* name)
{
   printf("%s :\n", name);
   DEBUG_VAR(shader_size);
   DEBUG_VAR(org_size);

   if(shader_size != org_size)
      printf("size mismatch : 0x%08X should be 0x%08X\n", shader_size, org_size);

   for(int i = 0; i < shader_size / 4; i+=4)
   {
      printf("0x%08X 0x%08X 0x%08X 0x%08X          0x%08X 0x%08X 0x%08X 0x%08X\n",
             shader[i], shader[i+1], shader[i+2], shader[i+3],
             org[i], org[i+1], org[i+2], org[i+3]);
   }

   for(int i = 0; i < shader_size / 4; i++)
   {
      if (shader[i] != org[i])
      {
         printf("%i(%X): 0x%08X(0x%08X) should be 0x%08X(0x%08X) \n", i, i, shader[i], __builtin_bswap32(shader[i]) , org[i], __builtin_bswap32(org[i]));
      }
   }
}
void check_shader(const void* shader_, u32 shader_size, const void* org_, u32 org_size, const char* name)
{
   u32* shader = (u32*)shader_;
   u32* org = (u32*)org_;
   bool different = false;
   printf("%-20s : ", name);
   if(shader_size != org_size)
   {
      different = true;
      printf("\nsize mismatch : 0x%08X should be 0x%08X", shader_size, org_size);
   }


   for(int i = 0; i < shader_size / 4; i++)
   {
      if (shader[i] != org[i])
      {
         different = true;
         printf("\n%i(%X): 0x%08X(0x%08X) should be 0x%08X(0x%08X)", i, i, shader[i], __builtin_bswap32(shader[i]) , org[i], __builtin_bswap32(org[i]));
      }
   }

   if(!different)
      printf("no errors");

   printf("\n");

}

