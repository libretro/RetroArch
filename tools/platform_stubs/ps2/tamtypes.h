/* Compile-only PS2 stub. The PS2SDK scalar names live here, as
 * they do in the real headers, so the stubs that need them do not
 * each declare their own and collide. */
#ifndef STUB_PS2_TAMTYPES
#define STUB_PS2_TAMTYPES

typedef signed int s128 __attribute__((mode(TI)));
typedef signed short s16;
typedef signed int s32;
typedef signed char s8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;
typedef unsigned char u8;
typedef volatile s128 vs128 __attribute__((mode(TI)));

#endif
