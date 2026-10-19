/* Copyright (C) 2026 The RetroArch team
 * SPDX-License-Identifier: GPL-3.0-or-later */

   .section ".rodata.sceResident","a"
ps3_userinfo_name:
   .asciz "cellUserInfo"

   .section ".rodata.sceFNID","a"
   .align 2
ps3_userinfo_fnid:
   .long 0x2b761140

   .section ".data.sceFStub.cellUserInfo","aw"
   .align 2
ps3_userinfo_stub:
   .long __ps3_user_info_get_stat

   .section ".lib.stub","aw"
   .align 2
   .long 0x2c000001
   .short 0x0009, 1
   .long 0, 0
   .long ps3_userinfo_name, ps3_userinfo_fnid, ps3_userinfo_stub
   .long 0, 0, 0, 0

   .section ".sceStub.text","ax"
   .align 2
   .globl __ps3_user_info_get_stat
__ps3_user_info_get_stat:
   mflr 0
   std 0,16(1)
   std 2,40(1)
   stdu 1,-128(1)
   lis 12,ps3_userinfo_stub@ha
   lwz 12,ps3_userinfo_stub@l(12)
   lwz 0,0(12)
   lwz 2,4(12)
   mtctr 0
   bctrl
   addi 1,1,128
   ld 2,40(1)
   ld 0,16(1)
   mtlr 0
   blr

   .section ".opd","aw"
   .align 3
   .globl ps3_user_info_get_stat
ps3_user_info_get_stat:
   .quad __ps3_user_info_get_stat,.TOC.@tocbase,0
