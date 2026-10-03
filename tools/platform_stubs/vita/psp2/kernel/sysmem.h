/* Compile-only Vita stub for the matrix's gxm video lane, carrying
 * the memory-block calls that driver names. Each declaration is
 * the shape the real header gives it. */
#ifndef STUB_PSP2_KERNEL_SYSMEM
#define STUB_PSP2_KERNEL_SYSMEM
#include <psp2/types.h>

#define SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW           (0x09408060)
#define SCE_KERNEL_MEMBLOCK_TYPE_USER_RW                 (0x0C20D060)
#define SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE         (0x0C208060)

typedef struct SceKernelAllocMemBlockOpt {
	SceSize size;
	SceUInt32 attr;
	SceSize alignment;
	SceUInt32 uidBaseBlock;
	const char *strBaseBlockName;
	int flags;
	int reserved[10];
} SceKernelAllocMemBlockOpt;

typedef SceUInt32 SceKernelMemBlockType;

typedef enum SceKernelModel {
   SCE_KERNEL_MODEL_VITATV = 0x20000
} SceKernelModel;

int sceKernelFreeMemBlock(SceUID uid);
int sceKernelGetMemBlockBase(SceUID uid, void **base);
int sceKernelGetModelForCDialog(void);
int sceKernelOpenMemBlock(const char *name, int flags);
SceUID sceKernelAllocMemBlock(const char *name, SceKernelMemBlockType type, SceSize size, SceKernelAllocMemBlockOpt *opt);

#endif
