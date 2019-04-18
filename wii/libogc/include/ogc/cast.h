#ifndef __CAST_H__
#define __CAST_H__

#include <gctypes.h>

#define	GQR2			914
#define	GQR3			915
#define	GQR4			916
#define	GQR5			917
#define	GQR6			918
#define	GQR7			919

#define GQR_TYPE_F32	0
#define GQR_TYPE_U8		4
#define GQR_TYPE_U16	5
#define GQR_TYPE_S8		6
#define GQR_TYPE_S16	7

#define GQR_CAST_U8		2
#define GQR_CAST_U16	3
#define GQR_CAST_S8		4
#define GQR_CAST_S16	5

#ifdef __cplusplus
extern "C" {
#endif

#ifdef GEKKO

#define __set_gqr(_reg,_val)	asm volatile("mtspr %0,%1" : : "i"(_reg), "b"(_val))

// does a default init
static inline void CAST_Init()
{
	__asm__ __volatile__ (
		"li		3,0x0004\n\
		 oris	3,3,0x0004\n\
		 mtspr	914,3\n\
		 li		3,0x0005\n\
		 oris	3,3,0x0005\n\
		 mtspr	915,3\n\
		 li		3,0x0006\n\
		 oris	3,3,0x0006\n\
		 mtspr	916,3\n\
		 li		3,0x0007\n\
		 oris	3,3,0x0007\n\
		 mtspr	917,3\n"
		 : : : "r3"
	);
}

static inline void CAST_SetGQR2(u32 type,u32 scale)
{
	register u32 val = (((((scale)<<8)|(type))<<16)|(((scale)<<8)|(type)));
	__set_gqr(GQR2,val);
}

static inline void CAST_SetGQR3(u32 type,u32 scale)
{
	register u32 val = (((((scale)<<8)|(type))<<16)|(((scale)<<8)|(type)));
	__set_gqr(GQR3,val);
}

static inline void CAST_SetGQR4(u32 type,u32 scale)
{
	register u32 val = (((((scale)<<8)|(type))<<16)|(((scale)<<8)|(type)));
	__set_gqr(GQR4,val);
}

static inline void CAST_SetGQR5(u32 type,u32 scale)
{
	register u32 val = (((((scale)<<8)|(type))<<16)|(((scale)<<8)|(type)));
	__set_gqr(GQR5,val);
}

static inline void CAST_SetGQR6(u32 type,u32 scale)
{
	register u32 val = (((((scale)<<8)|(type))<<16)|(((scale)<<8)|(type)));
	__set_gqr(GQR6,val);
}

static inline void CAST_SetGQR7(u32 type,u32 scale)
{
	register u32 val = (((((scale)<<8)|(type))<<16)|(((scale)<<8)|(type)));
	__set_gqr(GQR7,val);
}

/******************************************************************/
/*																  */
/* cast from int to float										  */
/*																  */
/******************************************************************/

static inline f32 __castu8f32(register u8 *in)
{
	register f32 rval;
	__asm__ __volatile__ (
		"psq_l	%[rval],0(%[in]),1,2" : [rval]"=f"(rval) : [in]"r"(in)
	);
	return rval;
}

static inline f32 __castu16f32(register u16 *in)
{
	register f32 rval;
	__asm__ __volatile__ (
		"psq_l	%[rval],0(%[in]),1,3" : [rval]"=f"(rval) : [in]"r"(in)
	);
	return rval;
}

static inline f32 __casts8f32(register s8 *in)
{
	register f32 rval;
	__asm__ __volatile__ (
		"psq_l	%[rval],0(%[in]),1,4" : [rval]"=f"(rval) : [in]"r"(in)
	);
	return rval;
}

static inline f32 __casts16f32(register s16 *in)
{
	register f32 rval;
	__asm__ __volatile__ (
		"psq_l	%[rval],0(%[in]),1,5" : [rval]"=f"(rval) : [in]"r"(in)
	);
	return rval;
}

static inline void castu8f32(register u8 *in,register volatile f32 *out)
{
	*out = __castu8f32(in);
}

static inline void castu16f32(register u16 *in,register volatile f32 *out)
{
	*out = __castu16f32(in);
}

static inline void casts8f32(register s8 *in,register volatile f32 *out)
{
	*out = __casts8f32(in);
}

static inline void casts16f32(register s16 *in,register volatile f32 *out)
{
	*out = __casts16f32(in);
}

/******************************************************************/
/*																  */
/* cast from float to int										  */
/*																  */
/******************************************************************/

static inline u8 __castf32u8(register f32 in)
{
	f32 a;
	register u8 rval;
	register f32 *ptr = &a;

	__asm__ __volatile__ (
		"psq_st	%[in],0(%[ptr]),1,2\n"
		"lbz	%[out],0(%[ptr])\n"
		: [out]"=r"(rval), [ptr]"+r"(ptr) : [in]"f"(in)
	);
	return rval;
}

static inline u16 __castf32u16(register f32 in)
{
	f32 a;
	register u16 rval;
	register f32 *ptr = &a;

	__asm__ __volatile__ (
		"psq_st	%[in],0(%[ptr]),1,3\n"
		"lhz	%[out],0(%[ptr])\n"
		: [out]"=r"(rval), [ptr]"+r"(ptr) : [in]"f"(in)
	);
	return rval;
}

static inline s8 __castf32s8(register f32 in)
{
	f32 a;
	register s8 rval;
	register f32 *ptr = &a;

	__asm__ __volatile__ (
		"psq_st	%[in],0(%[ptr]),1,4\n"
		"lbz	%[out],0(%[ptr])\n"
		: [out]"=r"(rval), [ptr]"+r"(ptr) : [in]"f"(in)
	);
	return rval;
}

static inline s16 __castf32s16(register f32 in)
{
	f32 a;
	register s16 rval;
	register f32 *ptr = &a;

	__asm__ __volatile__ (
		"psq_st	%[in],0(%[ptr]),1,5\n"
		"lha	%[out],0(%[ptr])\n"
		: [out]"=r"(rval), [ptr]"+r"(ptr) : [in]"f"(in)
	);
	return rval;
}

static inline void castf32u8(register f32 *in,register vu8 *out)
{
	*out = __castf32u8(*in);
}

static inline void castf32u16(register f32 *in,register vu16 *out)
{
	*out = __castf32u16(*in);
}

static inline void castf32s8(register f32 *in,register vs8 *out)
{
	*out = __castf32s8(*in);
}

static inline void castf32s16(register f32 *in,register vs16 *out)
{
	*out = __castf32s16(*in);
}

#endif //GEKKO

#ifdef __cplusplus
}
#endif

#endif
