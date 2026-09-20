/* Hermetic compile-only stand-in: only the names psp1_gfx.c uses. */
#ifndef STUB_psp_pspgu_h
#define STUB_psp_pspgu_h

#define GU_FALSE            0
#define GU_TRUE             1

#define GU_DIRECT           0
#define GU_CALL             1
#define GU_SEND             2
#define GU_TAIL             0
#define GU_HEAD             1

#define GU_PSM_5650         0
#define GU_PSM_5551         1
#define GU_PSM_4444         2
#define GU_PSM_8888         3
#define GU_PSM_T16          5
#define GU_PSM_T32          6

#define GU_NEAREST          0
#define GU_LINEAR           1
#define GU_CLAMP            1

#define GU_ADD              0
#define GU_FIX              10

#define GU_TFX_REPLACE      3
#define GU_TCC_RGB          0

#define GU_SPRITES          6

#define GU_ALPHA_TEST       0x0002
#define GU_DEPTH_TEST       0x0003
#define GU_SCISSOR_TEST     0x0005
#define GU_BLEND            0x0008
#define GU_TEXTURE_2D       0x000c

#define GU_COLOR_BUFFER_BIT 1
#define GU_DEPTH_BUFFER_BIT 2

#define GU_TEXTURE_32BITF   0x0018
#define GU_VERTEX_32BITF    0x0180
#define GU_TRANSFORM_2D     0x800000

/* Stores the state of the GE; the GE writes it, so its size matters to
 * any caller reserving storage for it. */
typedef struct PspGeContext
{
   unsigned int context[512];
} PspGeContext;

void  sceGuInit(void);
void  sceGuTerm(void);
void  sceGuDisplay(int state);
void  sceGuStart(int cid, void *list);
int   sceGuFinish(void);
int   sceGuSync(int mode, int what);
void  sceGuCallList(const void *list);
void  sceGuCallMode(int mode);
void  sceGuSendList(int mode, const void *list, PspGeContext *context);
void *sceGuSwapBuffers(void);

void  sceGuDrawBuffer(int psm, void *fbp, int fbw);
void  sceGuDispBuffer(int width, int height, void *dispbp, int dispbw);
void  sceGuClear(int flags);
void  sceGuClearColor(unsigned int color);
void  sceGuScissor(int x, int y, int w, int h);
void  sceGuEnable(int state);
void  sceGuDisable(int state);

void  sceGuTexMode(int tpsm, int maxmips, int a2, int swizzle);
void  sceGuTexFunc(int tfx, int tcc);
void  sceGuTexFilter(int min, int mag);
void  sceGuTexWrap(int u, int v);
void  sceGuTexImage(int mipmap, int width, int height, int tbw,
      const void *tbp);
void  sceGuClutMode(int cpsm, unsigned int shift, unsigned int mask,
      unsigned int a3);
void  sceGuClutLoad(int num_blocks, const void *cbp);

void  sceGuBlendFunc(int op, int src, int dest, unsigned int srcfix,
      unsigned int destfix);
void  sceGuDrawArray(int prim, int vtype, int count, const void *indices,
      const void *vertices);
void  sceGuCopyImage(int psm, int sx, int sy, int width, int height,
      int srcw, void *src, int dx, int dy, int destw, void *dest);

#endif
