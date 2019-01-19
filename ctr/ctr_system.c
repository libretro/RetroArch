#include <3ds.h>
#include <sys/iosupport.h>
#include <3ds/services/apt.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "ctr_debug.h"

#define CTR_APPMEMALLOC_PTR ((u32*)0x1FF80040)

u32 __stacksize__      = 0x00400000;
u32 __linear_heap_size = 0x01000000;

u32 __heap_size;
u32 __linear_heap;
u32 __heapBase;

u32 __stack_bottom;
u32 __stack_size_extra;

extern u32 __linear_heap_size_hbl;
extern u32 __heap_size_hbl;

extern void (*__system_retAddr)(void);

void envDestroyHandles(void);
void __appExit();
void __libc_fini_array(void);

void __appInit();
void __libc_init_array(void);
void __system_initSyscalls(void);
void __system_initArgv();

void __ctru_exit(int rc);
int __libctru_gtod(struct _reent* ptr, struct timeval* tp, struct timezone* tz);
void (*__system_retAddr)(void);
extern void* __service_ptr;

Result __sync_init(void) __attribute__((weak));

void __system_allocateHeaps(void)
{
   extern char* fake_heap_end;
   u32 tmp = 0;

   MemInfo stack_memInfo;
   PageInfo stack_pageInfo;

   register u32 sp_val __asm__("sp");

   svcQueryMemory(&stack_memInfo, &stack_pageInfo, sp_val);

   __stacksize__      += 0xFFF;
   __stacksize__      &= ~0xFFF;
   __stack_size_extra  = __stacksize__ > stack_memInfo.size ? __stacksize__ - stack_memInfo.size : 0;
   __stack_bottom      = stack_memInfo.base_addr - __stack_size_extra;

   if (__stack_size_extra)
   {
      svcControlMemory(&tmp, __stack_bottom, 0x0, __stack_size_extra, MEMOP_ALLOC, MEMPERM_READ | MEMPERM_WRITE);
      memset((void*)__stack_bottom, 0xFC, __stack_size_extra);
   }

   /* setup the application heap */
   __heapBase  = 0x08000000;
   __heap_size = 0;

   /* Allocate the linear heap */
   svcControlMemory(&__linear_heap, 0x0, 0x0, __linear_heap_size, MEMOP_ALLOC_LINEAR, MEMPERM_READ | MEMPERM_WRITE);

   /* Set up newlib heap */
   fake_heap_end = (char*)0x13F00000;
}

void __attribute__((weak)) __libctru_init(void (*retAddr)(void))
{
   /* Store the return address */
   __system_retAddr = NULL;
   if (envIsHomebrew()) {
      __system_retAddr = retAddr;
   }

   /* Initialize the synchronization subsystem */
   __sync_init();

   /* Initialize newlib support system calls */
   __system_initSyscalls();

   /* Allocate application and linear heaps */
   __system_allocateHeaps();
}

Result __sync_fini(void) __attribute__((weak));

extern char** __system_argv;

void __attribute__((noreturn)) __libctru_exit(int rc)
{
   u32 tmp = 0;

   if (__system_argv)
      free(__system_argv);

   /* Unmap the linear heap */
   svcControlMemory(&tmp, __linear_heap, 0x0, __linear_heap_size, MEMOP_FREE, 0x0);

   /* Unmap the application heap */
   svcControlMemory(&tmp, __heapBase, 0x0, __heap_size, MEMOP_FREE, 0x0);

   if (__stack_size_extra)
      svcControlMemory(&tmp, __stack_bottom, 0x0, __stack_size_extra, MEMOP_FREE, 0x0);

   /* Close some handles */
   envDestroyHandles();

   if (__sync_fini)
      __sync_fini();

   /* Jump to the loader if it provided a callback */
   if (__system_retAddr)
      __system_retAddr();

   /* Since above did not jump, end this process */
   svcExitProcess();
}

#include <3ds/types.h>

#include <string.h>

/* System globals we define here */
int __system_argc;
char** __system_argv;
extern const char* __system_arglist;

// sha-256 of "ARGV"
char __argv_hmac[0x20] = {0x1d, 0x78, 0xff, 0xb9, 0xc5, 0xbc, 0x78, 0xb7, 0xac, 0x29, 0x1d, 0x3e, 0x16, 0xd0, 0xcf, 0x53,
                         0xef, 0x12, 0x58, 0x83, 0xb6, 0x9e, 0x2f, 0x79, 0x47, 0xf9, 0x35, 0x61, 0xeb, 0x50, 0xd7, 0x67};

Result APT_ReceiveDeliverArg_(void* param, size_t param_size, void* hmac, size_t hmac_size, u64* source_pid, bool* received)
{
	u32 cmdbuf[16];
	cmdbuf[0]=IPC_MakeHeader(0x35,2,0);
	cmdbuf[1]=param_size;
	cmdbuf[2]=hmac_size;

	u32 saved_threadstorage[4];
	u32* staticbufs = getThreadStaticBuffers();
   saved_threadstorage[0]=staticbufs[0];
	saved_threadstorage[1]=staticbufs[1];
   saved_threadstorage[2]=staticbufs[2];
	saved_threadstorage[3]=staticbufs[3];
   staticbufs[0]=IPC_Desc_StaticBuffer(param_size, 0);
	staticbufs[1]=(u32)param;
   staticbufs[2]=IPC_Desc_StaticBuffer(hmac_size, 2);
	staticbufs[3]=(u32)hmac;

	Result ret = aptSendCommand(cmdbuf);
   staticbufs[0]=saved_threadstorage[0];
	staticbufs[1]=saved_threadstorage[1];
   staticbufs[2]=saved_threadstorage[2];
	staticbufs[3]=saved_threadstorage[3];

   if(R_FAILED(ret))
      return ret;

   if(source_pid)
      *source_pid = ((u64*)cmdbuf)[1];
   if(received)
      *received = ((bool*)cmdbuf)[16];

	return cmdbuf[1];
}

void __system_initArgv(void)
{
   int i;

   struct
   {
      u32 argc;
      char args[];
   }*arg_struct = (void*)__system_arglist;

   static u8 param[0x300];
   u8 hmac[0x20];
   bool received;

   if(!__service_ptr
      && R_SUCCEEDED(APT_ReceiveDeliverArg_(&param, sizeof(param), hmac, sizeof(hmac), NULL, &received))
      && received
      && !memcmp(hmac, __argv_hmac, sizeof(__argv_hmac)))
      arg_struct = (void*)param;

   __system_argc = 0;

   if (arg_struct)
      __system_argc = arg_struct->argc;

   if (__system_argc)
   {
      __system_argv = (char**) malloc((__system_argc + 1) * sizeof(char**));
      __system_argv[0] = arg_struct->args;
      for (i = 1; i < __system_argc; i++)
         __system_argv[i] = __system_argv[i - 1] + strlen(__system_argv[i - 1]) + 1;

      i = __system_argc - 1;
      __system_argc = 1;
      while (i)
      {
         if(__system_argv[i] && isalnum(__system_argv[i][0])
               && strncmp(__system_argv[i], "3dslink:/", 9))
         {
            __system_argv[1] = __system_argv[i];
            __system_argc = 2;
            break;
         }
         i--;
      }
   }
   else
   {
      __system_argc = 1;
      __system_argv = (char**) malloc(sizeof(char**) * 2);
      __system_argv[0] = "sdmc:/retroarch/retroarch";
   }
   __system_argv[__system_argc] = NULL;
}

void initSystem(void (*retAddr)(void))
{
   __libctru_init(retAddr);
   __appInit();
   __system_initArgv();
   __libc_init_array();
}

void __attribute__((noreturn)) __ctru_exit(int rc)
{
   __libc_fini_array();
   __appExit();
   __libctru_exit(rc);
}

typedef union
{
   struct
   {
      unsigned description : 10;
      unsigned module      : 8;
      unsigned             : 3;
      unsigned summary     : 6;
      unsigned level       : 5;
   };
   Result val;
} ctr_result_value;

void dump_result_value(Result val)
{
   ctr_result_value res;
   res.val = val;
   printf("result      : 0x%08X\n", val);
   printf("description : %u\n", res.description);
   printf("module      : %u\n", res.module);
   printf("summary     : %u\n", res.summary);
   printf("level       : %u\n", res.level);
}

bool select_pressed = false;

void wait_for_input(void)
{
   printf("\n\nPress Start.\n\n");
   fflush(stdout);

   while (aptMainLoop())
   {
      u32 kDown;

      hidScanInput();

      kDown = hidKeysDown();

      if (kDown & KEY_START)
         break;

      if (kDown & KEY_SELECT)
         exit(0);

#if 0
      select_pressed = true;
#endif

      svcSleepThread(1000000);
   }
}

long sysconf(int name)
{
   switch (name)
   {
      case _SC_NPROCESSORS_ONLN:
         return 2;
   }

   return -1;
}
