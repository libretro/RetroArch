/* adapted from https://github.com/dimok789/homebrew_launcher */

/****************************************************************************
 *
 * Copyright (C) 2015 Dimok
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 ****************************************************************************/

#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <inttypes.h>
#include <wiiu/os.h>

#include "hbl.h"

#ifdef WIIU_LOG_RPX
#include "../verbosity.h"
#endif

#define MEM_AREA_TABLE              ((s_mem_area*)(MEM_BASE + 0x1600))
#define ELF_DATA_ADDR               (*(volatile unsigned int*)(MEM_BASE + 0x1300 + 0x00))
#define ELF_DATA_SIZE               (*(volatile unsigned int*)(MEM_BASE + 0x1300 + 0x04))
#define RPX_MAX_SIZE                (*(volatile unsigned int*)(MEM_BASE + 0x1300 + 0x0C))
#define RPX_MAX_CODE_SIZE           (*(volatile unsigned int*)(MEM_BASE + 0x1300 + 0x10))
#define APP_BASE_MEM                ((unsigned char*)(MEM_BASE + 0x2000))

typedef struct _s_mem_area
{
   unsigned int        address;
   unsigned int        size;
   struct _s_mem_area *next;
} s_mem_area;

void SC0x25_KernelCopyData(unsigned int addr, unsigned int src, unsigned int len);

typedef struct _memory_values_t
{
    unsigned int start_address;
    unsigned int end_address;
} memory_values_t;

static const memory_values_t mem_vals_540[] =
{
    { 0x2E609EFC, 0x2FF82000 }, /* 26083 kB */
    { 0x29030800, 0x293F6000 }, /* 3864 kB */
    { 0x288EEC30, 0x28B06800 }, /* 2144 kB */
    { 0x2D3B966C, 0x2D894000 }, /* 4971 kB */
    { 0x2CB56370, 0x2D1EF000 }, /* 6756 kB */
    { 0x2D8AD3D8, 0x2E000000 }, /* 7499 kB */
    { 0x2970200C, 0x298B9800 }, /* 1759 kB */
    { 0x2A057B68, 0x2A1B9000 }, /* 1414 kB */
    { 0x2ABBCC4C, 0x2ACB9000 }, /* 1010 kB */
    {0, 0}
};

static inline void memoryAddArea(int start, int end, int cur_index)
{
    /* Create and copy new memory area */
    s_mem_area * mem_area = MEM_AREA_TABLE;
    mem_area[cur_index].address = start;
    mem_area[cur_index].size    = end - start;
    mem_area[cur_index].next    = 0;

    /* Fill pointer to this area in the previous area */
    if (cur_index > 0)
    {
        mem_area[cur_index - 1].next = &mem_area[cur_index];
    }
}
void *getApplicationEndAddr(void)
{
   extern u32 _end[];
   if((u32)_end >= 0x01000000)
      return APP_BASE_MEM;
   return _end;
}

/* Create memory areas arrays */
static void memoryInitAreaTable(u32 args_size)
{
    u32 ApplicationMemoryEnd = (u32)getApplicationEndAddr() + args_size;

    /* This one seems to be available on every firmware and therefore its our code area but also our main RPX area behind our code */
    /* 22876 kB - our application      ok  */
    memoryAddArea(ApplicationMemoryEnd + 0x30000000, 0x30000000 + 0x01E20000, 0);

    const memory_values_t * mem_vals = mem_vals_540;

    /* Fill entries */
    int i = 0;
    while (mem_vals[i].start_address)
    {
        memoryAddArea(mem_vals[i].start_address, mem_vals[i].end_address, i + 1);
        i++;
    }
}

static int HomebrewCopyMemory(u8 *address, u32 bytes, u32 args_size)
{
   args_size += 0x7;
   args_size &= ~0x7;
   if (args_size > 0x10000)
      args_size = 0x10000;

   memoryInitAreaTable(args_size);

   RPX_MAX_SIZE = 0x40000000;
   RPX_MAX_CODE_SIZE = 0x03000000;

   /* check if we load an RPX or an ELF */
   if (*(u16 *)&address[7] != 0xCAFE)
   {
      /* assume ELF */
      printf("loading ELF file \n");

      ELF_DATA_ADDR = (u32)getApplicationEndAddr() + args_size;
      if (ELF_DATA_ADDR >= 0x01000000)
         return -1;
   }
   else
   {
      /* RPX */
      printf("loading RPX file \n");

      ELF_DATA_ADDR = MEM_AREA_TABLE->address;
   }

   /*! if we load an ELF file */
   if (ELF_DATA_ADDR < 0x01000000)
   {

      if ((ELF_DATA_ADDR + bytes) > 0x01000000)
         return -1;

      memcpy((void *)ELF_DATA_ADDR, address, bytes);
      ELF_DATA_SIZE = bytes;
   }
   else
   {
      DCFlushRange(address, bytes);

      u32 done = 0;
      u32 addressPhysical = (u32)OSEffectiveToPhysical(address);

      s_mem_area *mem_map = MEM_AREA_TABLE;
      u32 mapPosition = 0;

      while ((done < bytes) && mem_map)
      {
         if (mapPosition >= mem_map->size)
         {
            mem_map = mem_map->next;

            if (!mem_map)
               return -1;

            mapPosition = 0;
         }

         u32 blockSize = bytes - done;

         if ((mapPosition + blockSize) > mem_map->size)
            blockSize = mem_map->size - mapPosition;

         SC0x25_KernelCopyData(mem_map->address + mapPosition, (addressPhysical + done), blockSize);

         mapPosition += blockSize;
         done += blockSize;
      }

      ELF_DATA_SIZE = done;
   }
   return bytes;
}

#ifdef WIIU_LOG_RPX

#define LINE_LEN 32
/**
 * This is called between when the RPX is read off the storage medium and
 * when it is sent to the loader. It prints a hexdump to the logger, which
 * can then be parsed out by a script.
 *
 * If we can at least semi-reliably generate the "System Memory Error", this
 * can be useful in identifying if the problem is corrupt file i/o vs in-memory
 * corruption.
 */
void log_rpx(const char *filepath, unsigned char *buf, size_t len)
{
  unsigned int line_buffer[LINE_LEN];
  int i, offset;

  RARCH_LOG("=== BEGIN file=%s size=%d ===\n", filepath, len);
  for(i = 0; i < len; i++)
  {
    offset = i % LINE_LEN;
    line_buffer[offset] = buf[i];

    if(offset == (LINE_LEN-1)) {
      RARCH_LOG("%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\n",
	line_buffer[0], line_buffer[1], line_buffer[2], line_buffer[3],
	line_buffer[4], line_buffer[5], line_buffer[6], line_buffer[7],
	line_buffer[8], line_buffer[9], line_buffer[10], line_buffer[11],
	line_buffer[12], line_buffer[13], line_buffer[14], line_buffer[15],
	line_buffer[16], line_buffer[17], line_buffer[18], line_buffer[19],
	line_buffer[20], line_buffer[21], line_buffer[22], line_buffer[23],
	line_buffer[24], line_buffer[25], line_buffer[26], line_buffer[27],
	line_buffer[28], line_buffer[29], line_buffer[30], line_buffer[31]);
    }
  }
  if((len % LINE_LEN) != 0) {
    for(i = (LINE_LEN - (len % LINE_LEN)); i < LINE_LEN; i++)
    {
      line_buffer[i] = 0;
    }
    RARCH_LOG("%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\n",
        line_buffer[0], line_buffer[1], line_buffer[2], line_buffer[3],
	line_buffer[4], line_buffer[5], line_buffer[6], line_buffer[7],
	line_buffer[8], line_buffer[9], line_buffer[10], line_buffer[11],
	line_buffer[16], line_buffer[17], line_buffer[18], line_buffer[19],
	line_buffer[20], line_buffer[21], line_buffer[22], line_buffer[23],
	line_buffer[24], line_buffer[25], line_buffer[26], line_buffer[27],
	line_buffer[28], line_buffer[29], line_buffer[30], line_buffer[31]);

  }
  RARCH_LOG("=== END %s ===\n", filepath);

}

#endif

int HBL_loadToMemory(const char *filepath, u32 args_size)
{
   if (!filepath || !*filepath)
      return -1;

   printf("Loading file %s\n", filepath);

   FILE *fp = fopen(filepath, "rb");

   if (!fp)
   {
      printf("failed to open file %s\n", filepath);
      return -1;
   }

   u32 bytesRead = 0;
   fseek(fp, 0, SEEK_END);
   u32 fileSize = ftell(fp);
   fseek(fp, 0, SEEK_SET);

   size_t buffer_size = (fileSize + 0x3f) & ~0x3f;
   unsigned char *buffer = (unsigned char *) memalign(0x40, buffer_size);

   if (!buffer)
   {
      printf("Not enough memory\n");
      return -1;
   }

   memset(buffer, 0, buffer_size);

   /* Copy rpl in memory */
   while (bytesRead < fileSize)
   {
      printf("progress: %f        \r", 100.0f * (f32)bytesRead / (f32)fileSize);

      u32 blockSize = 0x8000;

      if (blockSize > (fileSize - bytesRead))
         blockSize = fileSize - bytesRead;

      int ret = fread(buffer + bytesRead, 1, blockSize, fp);

      if (ret <= 0)
      {
         printf("Failure on reading file %s\n", filepath);
         break;
      }

      bytesRead += ret;
   }

   printf("progress: %f         \n", 100.0f * (f32)bytesRead / (f32)fileSize);

   if (bytesRead != fileSize)
   {
      free(buffer);
      printf("File loading not finished for file %s, finished %" PRIi32 " of %" PRIi32 " bytes\n", filepath, bytesRead,
             fileSize);
      printf("File read failure");
      return -1;
   }
#ifdef WIIU_LOG_RPX
   log_rpx(filepath, buffer, bytesRead);
#endif

   int ret = HomebrewCopyMemory(buffer, bytesRead, args_size);

   free(buffer);

   if (ret < 0)
   {
      printf("Not enough memory");
      return -1;
   }

   return fileSize;
}
