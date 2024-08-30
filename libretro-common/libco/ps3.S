.globl .co_swap_asm
.globl co_swap_asm
.type .co_swap_asm, @function
.type co_swap_asm, @function
.co_swap_asm:
co_swap_asm:
      mfcr    8
      std     1,40(4)
      mflr    9
      std     14,72(4)
      std     15,80(4)
      std     16,88(4)
      std     17,96(4)
      std     18,104(4)
      std     19,112(4)
      std     20,120(4)
      std     21,128(4)
      std     22,136(4)
      std     23,144(4)
      std     24,152(4)
      std     25,160(4)
      std     26,168(4)
      std     27,176(4)
      std     28,184(4)
      std     29,192(4)
      std     30,200(4)
      std     31,208(4)
      std     9,32(4)
      ld      7,32(3)
      ld      1,40(3)
      bl      swap
      trap
swap: stw     8,48(4)
      lwz     6,48(3)
      mtctr   7
      ld      14,72(3)
      ld      15,80(3)
      ld      16,88(3)
      ld      17,96(3)
      ld      18,104(3)
      ld      19,112(3)
      ld      20,120(3)
      ld      21,128(3)
      ld      22,136(3)
      ld      23,144(3)
      ld      24,152(3)
      ld      25,160(3)
      ld      26,168(3)
      ld      27,176(3)
      ld      28,184(3)
      ld      29,192(3)
      ld      30,200(3)
      ld      31,208(3)
      mtcr    6
      bctr
