/* author: Shawn Anastasio */

#define LIBCO_C
#include "libco.h"
#include "settings.h"

#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ppc64_context {
  //GPRs
  uint64_t gprs[32];
  uint64_t lr;
  uint64_t ccr;

  //FPRs
  uint64_t fprs[32];

  #ifdef __ALTIVEC__
  //Altivec (VMX)
  uint64_t vmx[12 * 2];
  uint32_t vrsave;
  #endif
};

static thread_local struct ppc64_context* co_active_handle = 0;

#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define ALIGN(p, x) ((void*)((uintptr_t)(p) & ~((x) - 1)))

#define MIN_STACK    0x10000lu
#define MIN_STACK_FRAME 0x20lu
#define STACK_ALIGN     0x10lu

void swap_context(struct ppc64_context* read, struct ppc64_context* write);
__asm__(
  ".text\n"
  ".align 4\n"
  ".type swap_context @function\n"
  "swap_context:\n"
  ".cfi_startproc\n"

  //save GPRs
  "std 1, 8(4)\n"
  "std 2, 16(4)\n"
  "std 12, 96(4)\n"
  "std 13, 104(4)\n"
  "std 14, 112(4)\n"
  "std 15, 120(4)\n"
  "std 16, 128(4)\n"
  "std 17, 136(4)\n"
  "std 18, 144(4)\n"
  "std 19, 152(4)\n"
  "std 20, 160(4)\n"
  "std 21, 168(4)\n"
  "std 22, 176(4)\n"
  "std 23, 184(4)\n"
  "std 24, 192(4)\n"
  "std 25, 200(4)\n"
  "std 26, 208(4)\n"
  "std 27, 216(4)\n"
  "std 28, 224(4)\n"
  "std 29, 232(4)\n"
  "std 30, 240(4)\n"
  "std 31, 248(4)\n"

  //save LR
  "mflr 5\n"
  "std 5, 256(4)\n"

  //save CCR
  "mfcr 5\n"
  "std 5, 264(4)\n"

  //save FPRs
  "stfd 14, 384(4)\n"
  "stfd 15, 392(4)\n"
  "stfd 16, 400(4)\n"
  "stfd 17, 408(4)\n"
  "stfd 18, 416(4)\n"
  "stfd 19, 424(4)\n"
  "stfd 20, 432(4)\n"
  "stfd 21, 440(4)\n"
  "stfd 22, 448(4)\n"
  "stfd 23, 456(4)\n"
  "stfd 24, 464(4)\n"
  "stfd 25, 472(4)\n"
  "stfd 26, 480(4)\n"
  "stfd 27, 488(4)\n"
  "stfd 28, 496(4)\n"
  "stfd 29, 504(4)\n"
  "stfd 30, 512(4)\n"
  "stfd 31, 520(4)\n"

  #ifdef __ALTIVEC__
  //save VMX
  "li 5, 528\n"
  "stvxl 20, 4, 5\n"
  "addi 5, 5, 16\n"
  "stvxl 21, 4, 5\n"
  "addi 5, 5, 16\n"
  "stvxl 22, 4, 5\n"
  "addi 5, 5, 16\n"
  "stvxl 23, 4, 5\n"
  "addi 5, 5, 16\n"
  "stvxl 24, 4, 5\n"
  "addi 5, 5, 16\n"
  "stvxl 25, 4, 5\n"
  "addi 5, 5, 16\n"
  "stvxl 26, 4, 5\n"
  "addi 5, 5, 16\n"
  "stvxl 27, 4, 5\n"
  "addi 5, 5, 16\n"
  "stvxl 28, 4, 5\n"
  "addi 5, 5, 16\n"
  "stvxl 29, 4, 5\n"
  "addi 5, 5, 16\n"
  "stvxl 30, 4, 5\n"
  "addi 5, 5, 16\n"
  "stvxl 31, 4, 5\n"
  "addi 5, 5, 16\n"

  //save VRSAVE
  "mfvrsave 5\n"
  "stw 5, 736(4)\n"
  #endif

  //restore GPRs
  "ld 1, 8(3)\n"
  "ld 2, 16(3)\n"
  "ld 12, 96(3)\n"
  "ld 13, 104(3)\n"
  "ld 14, 112(3)\n"
  "ld 15, 120(3)\n"
  "ld 16, 128(3)\n"
  "ld 17, 136(3)\n"
  "ld 18, 144(3)\n"
  "ld 19, 152(3)\n"
  "ld 20, 160(3)\n"
  "ld 21, 168(3)\n"
  "ld 22, 176(3)\n"
  "ld 23, 184(3)\n"
  "ld 24, 192(3)\n"
  "ld 25, 200(3)\n"
  "ld 26, 208(3)\n"
  "ld 27, 216(3)\n"
  "ld 28, 224(3)\n"
  "ld 29, 232(3)\n"
  "ld 30, 240(3)\n"
  "ld 31, 248(3)\n"

  //restore LR
  "ld 5, 256(3)\n"
  "mtlr 5\n"

  //restore CCR
  "ld 5, 264(3)\n"
  "mtcr 5\n"

  //restore FPRs
  "lfd 14, 384(3)\n"
  "lfd 15, 392(3)\n"
  "lfd 16, 400(3)\n"
  "lfd 17, 408(3)\n"
  "lfd 18, 416(3)\n"
  "lfd 19, 424(3)\n"
  "lfd 20, 432(3)\n"
  "lfd 21, 440(3)\n"
  "lfd 22, 448(3)\n"
  "lfd 23, 456(3)\n"
  "lfd 24, 464(3)\n"
  "lfd 25, 472(3)\n"
  "lfd 26, 480(3)\n"
  "lfd 27, 488(3)\n"
  "lfd 28, 496(3)\n"
  "lfd 29, 504(3)\n"
  "lfd 30, 512(3)\n"
  "lfd 31, 520(3)\n"

  #ifdef __ALTIVEC__
  //restore VMX
  "li 5, 528\n"
  "lvxl 20, 3, 5\n"
  "addi 5, 5, 16\n"
  "lvxl 21, 3, 5\n"
  "addi 5, 5, 16\n"
  "lvxl 22, 3, 5\n"
  "addi 5, 5, 16\n"
  "lvxl 23, 3, 5\n"
  "addi 5, 5, 16\n"
  "lvxl 24, 3, 5\n"
  "addi 5, 5, 16\n"
  "lvxl 25, 3, 5\n"
  "addi 5, 5, 16\n"
  "lvxl 26, 3, 5\n"
  "addi 5, 5, 16\n"
  "lvxl 27, 3, 5\n"
  "addi 5, 5, 16\n"
  "lvxl 28, 3, 5\n"
  "addi 5, 5, 16\n"
  "lvxl 29, 3, 5\n"
  "addi 5, 5, 16\n"
  "lvxl 30, 3, 5\n"
  "addi 5, 5, 16\n"
  "lvxl 31, 3, 5\n"
  "addi 5, 5, 16\n"

  //restore VRSAVE
  "lwz 5, 720(3)\n"
  "mtvrsave 5\n"
  #endif

  //branch to LR
  "blr\n"

  ".cfi_endproc\n"
  ".size swap_context, .-swap_context\n"
);

cothread_t co_active() {
  if(!co_active_handle) {
    co_active_handle = (struct ppc64_context*)malloc(MIN_STACK + sizeof(struct ppc64_context));
  }
  return (cothread_t)co_active_handle;
}

cothread_t co_derive(void* memory, unsigned int size, void (*coentry)(void)) {
  uint8_t* sp;
  struct ppc64_context* context = (struct ppc64_context*)memory;

  //save current context into new context to initialize it
  swap_context(context, context);

  //align stack
  sp = (uint8_t*)memory + size - STACK_ALIGN;
  sp = (uint8_t*)ALIGN(sp, STACK_ALIGN);

  //write 0 for initial backchain
  *(uint64_t*)sp = 0;

  //create new frame with backchain
  sp -= MIN_STACK_FRAME;
  *(uint64_t*)sp = (uint64_t)(sp + MIN_STACK_FRAME);

  //update context with new stack (r1) and entrypoint (r12, lr)
  context->gprs[ 1] = (uint64_t)sp;
  context->gprs[12] = (uint64_t)coentry;
  context->lr       = (uint64_t)coentry;

  return (cothread_t)memory;
}

cothread_t co_create(unsigned int size, void (*coentry)(void)) {
  void* memory = malloc(size);
  if(!memory) return (cothread_t)0;
  return co_derive(memory, size, coentry);
}

void co_delete(cothread_t handle) {
  free(handle);
}

void co_switch(cothread_t to) {
  struct ppc64_context* from = co_active_handle;
  co_active_handle = (struct ppc64_context*)to;
  swap_context((struct ppc64_context*)to, from);
}

int co_serializable() {
  return 1;
}

#ifdef __cplusplus
}
#endif
