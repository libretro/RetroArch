/* Compile-only Vita stub. SceUID lives here, as it does in the real
 * headers, so the kernel stubs that need it do not each declare their
 * own and collide under -pedantic. */
#ifndef STUB_PSP2_TYPES
#define STUB_PSP2_TYPES
typedef int SceUID;
#endif
