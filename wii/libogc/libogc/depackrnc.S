#include <asm.h>

/**********************************/
/* register map					  */
/*								  */
/* r0  	= tmp					  */
/* r3   = in			= r2	  */
/* r4   = out			= r4	  */
/* r5   = A				= r5	  */
/* r6   = X				= r6	  */
/* r7   = Y				= r7	  */
/* r8   = carry flag	= r3	  */
/* r10  = tmp			= r8	  */
/* r11  = store data	= r9	  */
/* r29  = copy in(r3)	= r0	  */
/* r30  = copy out(r4)	= r1	  */
/*								  */
/**********************************/

#define LOAD				\
	lbzu		r5,1(r29);	\
	slwi		r5,r5,1;	\
	insrwi		r5,r8,1,31;	\
	extrwi		r8,r5,1,23;	\
	clrlwi.		r5,r5,24

#define GETBIT				\
	slwi		r5,r5,1;	\
	extrwi		r8,r5,1,23;	\
	clrlwi.		r5,r5,24

#define COPYRAW				\
	lbzu		r11,1(r29);	\
	stbu		r11,1(r30)

	//r3 is input, r4 is output
	.globl depackrnc2
depackrnc2:
	mflr	r0
	stw		r0,4(sp)
	stwu	sp,-64(sp)
	stw		r3,8(sp)
	stw		r4,12(sp)
	stw		r29,48(sp)
	mr		r29,r3
	stw		r30,52(sp)
	mr		r30,r4

	li		r8,1
	addi	r30,r30,-1
	addi	r29,r29,17

	LOAD
	GETBIT
	b		_xloop

_fetch3:
	LOAD
	b		_back3
_fetch4:
	LOAD
	b		_back4
_fetch5:
	LOAD
	b		_back5
_fetch6:
	LOAD
	b		_back6
_fetch7:
	LOAD
	b		_back7

_raw:
	li		r7,4
_x4bits:
	GETBIT
	beq		_fetch7
_back7:
	slwi	r6,r6,1				//ROL(X)
	insrwi	r6,r8,1,31
	extrwi	r8,r6,1,23
	clrlwi	r6,r6,24

	subic.	r7,r7,1
	bne		_x4bits
	addi	r7,r6,3

	slwi	r7,r7,1				//ROL(Y)
	insrwi	r7,r8,1,31
	extrwi	r8,r7,1,23
	clrlwi	r7,r7,24
_rawlpb:
	COPYRAW
	COPYRAW

	subic.	r7,r7,1
	bne		_rawlpb
	b		_xloop

_fetch0:
	LOAD
	rlwinm.	r0,r8,0,31,31
	bne		_smalls
_getlen:
	GETBIT
	beq		_fetch3
_back3:
	slwi	r7,r7,1				//ROL(Y)
	insrwi	r7,r8,1,31
	extrwi	r8,r7,1,23
	clrlwi	r7,r7,24

	GETBIT
	beq		_fetch4
_back4:
	rlwinm.	r0,r8,0,31,31
	beq		_copy

	GETBIT
	beq		_fetch5
_back5:
	subic	r7,r7,1
	slwi	r7,r7,1				//ROL(Y)
	insrwi	r7,r8,1,31
	extrwi	r8,r7,1,23
	clrlwi	r7,r7,24

	cmpwi	r7,9
	beq		_raw

_copy:
	GETBIT
	beq		_fetch6
_back6:
	rlwinm.	r0,r8,0,31,31
	beq		_bytedisp

	GETBIT
	bne		_skip0

	LOAD
_skip0:
	slwi	r6,r6,1				//ROL(X)
	insrwi	r6,r8,1,31
	extrwi	r8,r6,1,23
	clrlwi	r6,r6,24

	slwi	r5,r5,1				//ROL(A)
	insrwi	r5,r8,1,31
	extrwi	r8,r5,1,23
	clrlwi.	r5,r5,24
	bne		_skip1

	LOAD
_skip1:
	rlwinm.	r0,r8,0,31,31
	bne		_bigdisp
	cmpwi	r6,0
	bne		_bytedisp
	addic	r6,r6,1

_another:
	GETBIT
	bne		_dispx

	LOAD
_dispx:
	slwi	r6,r6,1				//ROL(X)
	insrwi	r6,r8,1,31
	extrwi	r8,r6,1,23
	clrlwi	r6,r6,24

_bytedisp:
	lbzu	r8,1(r29)
	andi.	r10,r30,0xff
	sub		r8,r10,r8
	srwi	r10,r30,8
	sub		r10,r10,r6
	slwi	r10,r10,8
	add		r8,r8,r10
	subic	r8,r8,1
_bytelp:
	lbzu	r11,1(r8)
	stbu	r11,1(r30)

	subic.	r7,r7,1
	bne		_bytelp
	b		_xloop

_getbits:
	LOAD
	rlwinm.	r0,r8,0,31,31
	bne		_string
_xbyte:
	COPYRAW
_xloop:
	GETBIT
	rlwinm.	r0,r8,0,31,31
	bne		_chkz

	COPYRAW

	GETBIT
	rlwinm.	r0,r8,0,31,31
	beq		_xbyte

	li		r8,1
_chkz:
	rlwinm.	r0,r5,0,24,31
	beq		_getbits

_string:
	li		r6,0
	li		r7,2
	GETBIT
	beq		_fetch0
	rlwinm.	r0,r8,0,31,31
	beq		_getlen
_smalls:
	GETBIT
	beq		_fetch1
_back1:
	rlwinm.	r0,r8,0,31,31
	beq		_bytedisp

	addic	r7,r7,1

	GETBIT
	beq		_fetch2
_back2:
	rlwinm.	r0,r8,0,31,31
	beq		_copy
	lbzu	r7,1(r29)
	cmpwi	r7,0
	beq		_overnout
	addic	r7,r7,8
	b		_copy

_bigdisp:
	GETBIT
	bne		_skip2
	LOAD
_skip2:
	slwi	r6,r6,1				//ROL(X)
	insrwi	r6,r8,1,31
	extrwi	r8,r6,1,23
	clrlwi	r6,r6,24
	ori		r6,r6,4

	GETBIT
	bne		_skip3
	LOAD
_skip3:
	rlwinm.	r0,r8,0,31,31
	bne		_bytedisp
	b		_another

_fetch1:
	LOAD
	b		_back1
_fetch2:
	LOAD
	b		_back2
_overnout:
	GETBIT
	bne		_check4end
	LOAD
_check4end:
	rlwinm.	r0,r8,0,31,31
	bne		_xloop

	lwz		r29,48(sp)
	lwz		r30,52(sp)
	lwz		r4,12(sp)
	lwz		r3,8(sp)
	lwz		r0,68(sp)
	addi	sp,sp,64
	mtlr	r0
	blr
