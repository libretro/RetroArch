/* encoding_crc32.c's VPCLMULQDQ, as GCC builds it, through any
 * assembler with AVX-512F.
 *
 * GCC hands the instruction to the system assembler, and binutils
 * before 2.30 cannot encode it on ZMM, so the file spells it out in
 * EVEX bytes with a gas macro. Here the macro is run over every
 * destination, source and source register, and each encoding is
 * checked against the assembler's own vpclmulqdq where the assembler
 * has one (CRC32_GAS_NATIVE, probed by the Makefile), and against the
 * EVEX fields of the instruction as the SDM gives them everywhere.
 * The Makefile also fails the build if the assembly GCC makes of the
 * file names the instruction on ZMM, YMM or XMM16-31, and if
 * encoding_rzstd.c and encoding_crc32.c cannot share a unity build
 * with a <cpuid.h> that has no include guard. */

#include <stdio.h>
#include <string.h>

#include "../../../encodings/encoding_crc32.c"

#if defined(CRC32_VPCLMUL_GAS)

#define CRC32_GAS_REGS "0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15," \
   "16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31"

/* imm, src2, src1, dst in AT&T order; dst outermost. Emitted from
 * inside a function, as the file's own uses are, so the macro text
 * goes through the same extended-asm %% handling; the function jumps
 * over the tables, which stay in .text where instructions belong */
#define CRC32_GAS_TABLE(name, insn, imm) \
   ".globl " name "\n" \
   name ":\n\t" \
   ".irp d," CRC32_GAS_REGS "\n\t" \
   ".irp a," CRC32_GAS_REGS "\n\t" \
   ".irp b," CRC32_GAS_REGS "\n\t" \
   insn " " imm ", %%zmm\\b, %%zmm\\a, %%zmm\\d\n\t" \
   ".endr\n\t.endr\n\t.endr\n\t"

extern const unsigned char crc32_gas_macro[];
#if defined(CRC32_GAS_NATIVE)
extern const unsigned char crc32_gas_native[];
#endif

void crc32_gas_tables(void)
{
   __asm__ __volatile__(CRC32_VPCLMUL_GAS
         "jmp 9f\n"
         CRC32_GAS_TABLE("crc32_gas_macro", "crc32_vpclmulqdq", "0x11")
#if defined(CRC32_GAS_NATIVE)
         CRC32_GAS_TABLE("crc32_gas_native", "vpclmulqdq", "$0x11")
#endif
         "9:\n\t"
         : : );
}

/* SDM vol. 2, 2.7: EVEX is 62h then P0 (R X B R' 0 0 m m), P1
 * (W v v v v 1 p p), P2 (z L' L b V' a a a), the register-sourced
 * bits held inverted; for a register r/m, X extends B to five bits.
 * VPCLMULQDQ zmm is EVEX.512.66.0F3A.WIG 44 /r ib. */
static void crc32_gas_expect(unsigned char *o, unsigned d, unsigned s1,
      unsigned s2)
{
   unsigned R  = !(d  & 8),  R1 = !(d  & 16);
   unsigned B  = !(s2 & 8),  X  = !(s2 & 16);
   unsigned V  = (~s1) & 15, V1 = !(s1 & 16);

   o[0] = 0x62;
   o[1] = (unsigned char)((R << 7) | (X << 6) | (B << 5) | (R1 << 4)
         | 3 /* mm = 0F3A */);
   o[2] = (unsigned char)((0 << 7) /* W */ | (V << 3) | (1 << 2)
         | 1 /* pp = 66 */);
   o[3] = (unsigned char)((2 << 5) /* L'L = 512 */ | (V1 << 3));
   o[4] = 0x44;
   o[5] = (unsigned char)(0xC0 | ((d & 7) << 3) | (s2 & 7));
   o[6] = 0x11;
}

int main(void)
{
   unsigned d, a, b, bad = 0;
   const unsigned char *m = crc32_gas_macro;

   crc32_gas_tables();
   for (d = 0; d < 32; d++)
      for (a = 0; a < 32; a++)
         for (b = 0; b < 32; b++, m += 7)
         {
            unsigned char e[7];
            crc32_gas_expect(e, d, a, b);
            if (memcmp(m, e, 7)
#if defined(CRC32_GAS_NATIVE)
                  || memcmp(m, crc32_gas_native + (m - crc32_gas_macro), 7)
#endif
               )
            {
               if (bad++ < 8)
                  printf("FAIL: vpclmulqdq $0x11, %%zmm%u, %%zmm%u, "
                        "%%zmm%u encoded %02x %02x %02x %02x %02x %02x "
                        "%02x\n", b, a, d,
                        m[0], m[1], m[2], m[3], m[4], m[5], m[6]);
            }
         }

   if (bad)
   {
      printf("%u of 32768 encodings wrong\n", bad);
      return 1;
   }
#if defined(CRC32_GAS_NATIVE)
   printf("[pass] crc32_gas_test: 32768 encodings, as the assembler's\n");
#else
   printf("[pass] crc32_gas_test: 32768 encodings, as the SDM's\n");
#endif
   return 0;
}

#else

int main(void)
{
   printf("[pass] crc32_gas_test: this build does not spell VPCLMULQDQ "
         "out\n");
   return 0;
}

#endif
