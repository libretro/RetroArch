#ifndef __GX_H__
#define __GX_H__

/*!
 * \file gx.h
 * \brief GX subsystem
 *
 */

#include <gctypes.h>
#include "lwp.h"
#include "gx_struct.h"
#include "gu.h"

#define GX_FALSE			0
#define GX_TRUE				1
#define GX_DISABLE			0
#define GX_ENABLE			1

/*! \addtogroup clipmode Clipping mode
 * @{
 */
#define GX_CLIP_DISABLE		1
#define GX_CLIP_ENABLE		0
/*! @} */

#define GX_FIFO_MINSIZE		(64*1024)			/*!< Smallest usable graphics FIFO size. */
#define GX_FIFO_HIWATERMARK	(16*1024)			/*!< Default hi watermark for FIFO buffer control. */
#define GX_FIFO_OBJSIZE		128

#define GX_PERSPECTIVE		0
#define GX_ORTHOGRAPHIC		1

#define GX_MT_NULL			0
#define GX_MT_XF_FLUSH		1
#define GX_MT_DL_SAVE_CTX	2

#define GX_XF_FLUSH_NONE	0
#define GX_XF_FLUSH_SAFE	1

/*! \addtogroup channelid Color channel ID
 * @{
 */
#define GX_COLOR0			0
#define GX_COLOR1			1
#define GX_ALPHA0			2
#define GX_ALPHA1			3
#define GX_COLOR0A0			4
#define GX_COLOR1A1			5
#define GX_COLORZERO		6
#define GX_ALPHA_BUMP		7
#define GX_ALPHA_BUMPN		8
#define GX_COLORNULL		0xff
/*! @} */

/*! \addtogroup mtxtype Matrix type
 * @{
 */
#define GX_MTX2x4			0
#define GX_MTX3x4			1
/*! @} */

/*! \addtogroup vtxfmt Vertex format index
 * @{
 */
#define GX_VTXFMT0			0
#define GX_VTXFMT1			1
#define GX_VTXFMT2			2
#define GX_VTXFMT3			3
#define GX_VTXFMT4			4
#define GX_VTXFMT5			5
#define GX_VTXFMT6			6
#define GX_VTXFMT7			7
#define GX_MAXVTXFMT		8

/*! @} */

/*! \addtogroup vtxattrin Vertex data input type
 * @{
 */
#define GX_NONE				0			/*!< Input data is not used */
#define GX_DIRECT			1			/*!< Input data is set direct */
#define GX_INDEX8			2			/*!< Input data is set by a 8bit index */
#define GX_INDEX16			3			/*!< Input data is set by a 16bit index */

/*! @} */

/*! \addtogroup compsize Number of components in an attribute
 * @{
 */
#define GX_U8				0			/*!< Unsigned 8-bit integer */
#define GX_S8				1			/*!< Signed 8-bit integer */
#define GX_U16				2			/*!< Unsigned 16-bit integer */
#define GX_S16				3			/*!< Signed 16-bit integer */
#define GX_F32				4			/*!< 32-bit floating-point */
#define GX_RGB565			0			/*!< 16-bit RGB */
#define GX_RGB8				1			/*!< 24-bit RGB */
#define GX_RGBX8			2			/*!< 32-bit RGBX */
#define GX_RGBA4			3			/*!< 16-bit RGBA */
#define GX_RGBA6			4			/*!< 24-bit RGBA */
#define GX_RGBA8			5			/*!< 32-bit RGBA */
/*! @} */

/*! \addtogroup comptype Attribute component type
 * @{
 */
#define GX_POS_XY			0			/*!< X,Y position */
#define GX_POS_XYZ			1			/*!< X,Y,Z position */
#define GX_NRM_XYZ			0			/*!< X,Y,Z normal */
#define GX_NRM_NBT			1
#define GX_NRM_NBT3			2
#define GX_CLR_RGB			0			/*!< RGB color */
#define GX_CLR_RGBA			1			/*!< RGBA color */
#define GX_TEX_S			0			/*!< One texture dimension */
#define GX_TEX_ST			1			/*!< Two texture dimensions */
/*! @} */

/*! \addtogroup vtxattr Vertex attribute array type
 * @{
 */
#define GX_VA_PTNMTXIDX			0
#define GX_VA_TEX0MTXIDX		1
#define GX_VA_TEX1MTXIDX		2
#define GX_VA_TEX2MTXIDX		3
#define GX_VA_TEX3MTXIDX		4
#define GX_VA_TEX4MTXIDX		5
#define GX_VA_TEX5MTXIDX		6
#define GX_VA_TEX6MTXIDX		7
#define GX_VA_TEX7MTXIDX		8
#define GX_VA_POS				9
#define GX_VA_NRM				10
#define GX_VA_CLR0				11
#define GX_VA_CLR1				12
#define GX_VA_TEX0				13
#define GX_VA_TEX1				14
#define GX_VA_TEX2				15
#define GX_VA_TEX3				16
#define GX_VA_TEX4				17
#define GX_VA_TEX5				18
#define GX_VA_TEX6				19
#define GX_VA_TEX7				20
#define GX_POSMTXARRAY			21
#define GX_NRMMTXARRAY			22
#define GX_TEXMTXARRAY			23
#define GX_LIGHTARRAY			24
#define GX_VA_NBT				25
#define GX_VA_MAXATTR			26
#define GX_VA_NULL				0xff
/*! @} */

/*! \addtogroup primtype Primitive type
 * \brief Collection of primitive types that can be drawn by the GP.
 *
 * \note Which type you use depends on your needs; however, performance can increase by using triangle strips or fans instead of discrete triangles.
 * @{
 */
#define GX_POINTS				0xB8			/*!< Draws a series of points. Each vertex is a single point. */
#define GX_LINES				0xA8			/*!< Draws a series of unconnected line segments. Each pair of vertices makes a line. */
#define GX_LINESTRIP			0xB0			/*!< Draws a series of lines. Each vertex (besides the first) makes a line between it and the previous. */
#define GX_TRIANGLES			0x90			/*!< Draws a series of unconnected triangles. Three vertices make a single triangle. */
#define GX_TRIANGLESTRIP		0x98			/*!< Draws a series of triangles. Each triangle (besides the first) shares a side with the previous triangle.
												* Each vertex (besides the first two) completes a triangle. */
#define GX_TRIANGLEFAN			0xA0			/*!< Draws a single triangle fan. The first vertex is the "centerpoint". The second and third vertex complete
												* the first triangle. Each subsequent vertex completes another triangle which shares a side with the previous
												* triangle (except the first triangle) and has the centerpoint vertex as one of the vertices. */
#define GX_QUADS				0x80			/*!< Draws a series of unconnected quads. Every four vertices completes a quad. Internally, each quad is
												* translated into a pair of triangles. */
/*! @} */

#define GX_SRC_REG			0
#define GX_SRC_VTX			1

/*! \addtogroup lightid Light ID
 * @{
 */
#define GX_LIGHT0			0x001			/*!< Light 0 */
#define GX_LIGHT1			0x002			/*!< Light 2 */
#define GX_LIGHT2			0x004			/*!< Light 3 */
#define GX_LIGHT3			0x008			/*!< Light 4 */
#define GX_LIGHT4			0x010			/*!< Light 5 */
#define GX_LIGHT5			0x020			/*!< Light 6 */
#define GX_LIGHT6			0x040			/*!< Light 7 */
#define GX_LIGHT7			0x080			/*!< Light 8 */
#define GX_MAXLIGHT			0x100			/*!< All lights */
#define GX_LIGHTNULL		0x000			/*!< No lights */
/*! @} */

/*! \addtogroup difffn Diffuse function
 * @{
 */
#define GX_DF_NONE			0
#define GX_DF_SIGNED		1
#define GX_DF_CLAMP			2
/*! @} */

/*! \addtogroup attenfunc Attenuation function
 * @{
 */
#define GX_AF_SPEC			0			/*!< Specular computation */
#define GX_AF_SPOT			1			/*!< Spot light attenuation */
#define GX_AF_NONE			2			/*!< No attenuation */
/*! @} */

/* pos,nrm,tex,dtt matrix */
/*! \addtogroup pnmtx Position-normal matrix index
 * @{
 */
#define GX_PNMTX0			0
#define GX_PNMTX1			3
#define GX_PNMTX2			6
#define GX_PNMTX3			9
#define GX_PNMTX4			12
#define GX_PNMTX5			15
#define GX_PNMTX6			18
#define GX_PNMTX7			21
#define GX_PNMTX8			24
#define GX_PNMTX9			27
/*! @} */

/*! \addtogroup texmtx Texture matrix index
 * @{
 */
#define GX_TEXMTX0			30
#define GX_TEXMTX1			33
#define GX_TEXMTX2			36
#define GX_TEXMTX3			39
#define GX_TEXMTX4			42
#define GX_TEXMTX5			45
#define GX_TEXMTX6			48
#define GX_TEXMTX7			51
#define GX_TEXMTX8			54
#define GX_TEXMTX9			57
#define GX_IDENTITY			60
/*! @} */

/*! \addtogroup dttmtx Post-transform texture matrix index
 * @{
 */
#define GX_DTTMTX0			64
#define GX_DTTMTX1			67
#define GX_DTTMTX2			70
#define GX_DTTMTX3			73
#define GX_DTTMTX4			76
#define GX_DTTMTX5			79
#define GX_DTTMTX6			82
#define GX_DTTMTX7			85
#define GX_DTTMTX8			88
#define GX_DTTMTX9			91
#define GX_DTTMTX10			94
#define GX_DTTMTX11			97
#define GX_DTTMTX12			100
#define GX_DTTMTX13			103
#define GX_DTTMTX14			106
#define GX_DTTMTX15			109
#define GX_DTTMTX16			112
#define GX_DTTMTX17			115
#define GX_DTTMTX18			118
#define GX_DTTMTX19			121
#define GX_DTTIDENTITY		125
/*! @} */

/* tex coord id
   used by: XF: 0x1040,0x1050
            BP: 0x30
*/
/*! \addtogroup texcoordid texture coordinate slot
 * @{
 */
#define GX_TEXCOORD0		0x0
#define GX_TEXCOORD1		0x1
#define GX_TEXCOORD2		0x2
#define GX_TEXCOORD3		0x3
#define GX_TEXCOORD4		0x4
#define GX_TEXCOORD5		0x5
#define GX_TEXCOORD6		0x6
#define GX_TEXCOORD7		0x7
#define GX_MAXCOORD			0x8
#define GX_TEXCOORDNULL		0xff
/*! @} */

/* tex format */
#define _GX_TF_ZTF			0x10
#define _GX_TF_CTF			0x20

/*! \addtogroup texfmt Texture format
 * @{
 */

#define GX_TF_I4			0x0
#define GX_TF_I8			0x1
#define GX_TF_IA4			0x2
#define GX_TF_IA8			0x3
#define GX_TF_RGB565		0x4
#define GX_TF_RGB5A3		0x5
#define GX_TF_RGBA8			0x6
#define GX_TF_CI4			0x8
#define GX_TF_CI8			0x9
#define GX_TF_CI14			0xa
#define GX_TF_CMPR			0xE			/*!< Compressed */

#define GX_TL_IA8			0x00
#define GX_TL_RGB565		0x01
#define GX_TL_RGB5A3		0x02

#define GX_CTF_R4			(0x0|_GX_TF_CTF)			/*!< For copying 4 bits from red */
#define GX_CTF_RA4			(0x2|_GX_TF_CTF)			/*!< For copying 4 bits from red, 4 bits from alpha */
#define GX_CTF_RA8			(0x3|_GX_TF_CTF)			/*!< For copying 8 bits from red, 8 bits from alpha */
#define GX_CTF_YUVA8		(0x6|_GX_TF_CTF)
#define GX_CTF_A8			(0x7|_GX_TF_CTF)			/*!< For copying 8 bits from alpha */
#define GX_CTF_R8			(0x8|_GX_TF_CTF)			/*!< For copying 8 bits from red */
#define GX_CTF_G8			(0x9|_GX_TF_CTF)			/*!< For copying 8 bits from green */
#define GX_CTF_B8			(0xA|_GX_TF_CTF)			/*!< For copying 8 bits from blue */
#define GX_CTF_RG8			(0xB|_GX_TF_CTF)			/*!< For copying 8 bits from red, 8 bits from green */
#define GX_CTF_GB8			(0xC|_GX_TF_CTF)			/*!< For copying 8 bits from green, 8 bits from blue */

/*! \addtogroup ztexfmt Z Texture format
 * @{
 */

#define GX_TF_Z8			(0x1|_GX_TF_ZTF)			/*!< For texture copy, specifies upper 8 bits of Z */
#define GX_TF_Z16			(0x3|_GX_TF_ZTF)			/*!< For texture copy, specifies upper 16 bits of Z */
#define GX_TF_Z24X8			(0x6|_GX_TF_ZTF)			/*!< For texture copy, copies 24 Z bits and 0xFF */

/*! @} */

#define GX_CTF_Z4			(0x0|_GX_TF_ZTF|_GX_TF_CTF)			/*!< For copying 4 upper bits from Z */
#define GX_CTF_Z8M			(0x9|_GX_TF_ZTF|_GX_TF_CTF)			/*!< For copying the middle 8 bits of Z */
#define GX_CTF_Z8L			(0xA|_GX_TF_ZTF|_GX_TF_CTF)			/*!< For copying the lower 8 bits of Z */
#define GX_CTF_Z16L			(0xC|_GX_TF_ZTF|_GX_TF_CTF)			/*!< For copying the lower 16 bits of Z */

#define GX_TF_A8			GX_CTF_A8

/*! @} */

/* gx tlut size */
#define GX_TLUT_16			1	// number of 16 entry blocks.
#define GX_TLUT_32			2
#define GX_TLUT_64			4
#define GX_TLUT_128			8
#define GX_TLUT_256			16
#define GX_TLUT_512			32
#define GX_TLUT_1K			64
#define GX_TLUT_2K			128
#define GX_TLUT_4K			256
#define GX_TLUT_8K			512
#define GX_TLUT_16K			1024

/*! \addtogroup ztexop Z Texture operator
 * @{
 */

#define GX_ZT_DISABLE		0
#define GX_ZT_ADD			1			/*!< Add a Z texel to reference Z */
#define GX_ZT_REPLACE		2			/*!< Replace reference Z with Z texel */
#define GX_MAX_ZTEXOP		3

/*! @} */

/*! \addtogroup texgentyp Texture coordinate generation type
 * @{
 */
#define GX_TG_MTX3x4		0			/*!< 2x4 matrix multiply on the input attribute and generate S,T texture coordinates. */
#define GX_TG_MTX2x4		1			/*!< 3x4 matrix multiply on the input attribute and generate S,T,Q coordinates; S,T are then divided
										* by Q to produce the actual 2D texture coordinates. */
#define GX_TG_BUMP0			2			/*!< Use light 0 in the bump map calculation. */
#define GX_TG_BUMP1			3			/*!< Use light 1 in the bump map calculation. */
#define GX_TG_BUMP2			4			/*!< Use light 2 in the bump map calculation. */
#define GX_TG_BUMP3			5			/*!< Use light 3 in the bump map calculation. */
#define GX_TG_BUMP4			6			/*!< Use light 4 in the bump map calculation. */
#define GX_TG_BUMP5			7			/*!< Use light 5 in the bump map calculation. */
#define GX_TG_BUMP6			8			/*!< Use light 6 in the bump map calculation. */
#define GX_TG_BUMP7			9			/*!< Use light 7 in the bump map calculation. */
#define GX_TG_SRTG			10			/*!< Coordinates generated from vertex lighting results; one of the color channel results is converted
										* into texture coordinates. */
/*! @} */

/*! \addtogroup texgensrc Texture coordinate source
 * @{
 */
#define GX_TG_POS			0
#define GX_TG_NRM			1
#define GX_TG_BINRM			2
#define GX_TG_TANGENT		3
#define GX_TG_TEX0			4
#define GX_TG_TEX1			5
#define GX_TG_TEX2			6
#define GX_TG_TEX3			7
#define GX_TG_TEX4			8
#define GX_TG_TEX5			9
#define GX_TG_TEX6			10
#define GX_TG_TEX7			11
#define GX_TG_TEXCOORD0		12
#define GX_TG_TEXCOORD1		13
#define GX_TG_TEXCOORD2		14
#define GX_TG_TEXCOORD3		15
#define GX_TG_TEXCOORD4		16
#define GX_TG_TEXCOORD5		17
#define GX_TG_TEXCOORD6		18
#define GX_TG_COLOR0		19
#define GX_TG_COLOR1		20
/*! @} */

/*! \addtogroup compare Compare type
 * @{
 */
#define GX_NEVER			0
#define GX_LESS				1
#define GX_EQUAL			2
#define GX_LEQUAL			3
#define GX_GREATER			4
#define GX_NEQUAL			5
#define GX_GEQUAL			6
#define GX_ALWAYS			7
/*! @} */

/* Text Wrap Mode */
#define GX_CLAMP			0
#define GX_REPEAT			1
#define GX_MIRROR			2
#define GX_MAXTEXWRAPMODE	3

/*! \addtogroup blendmode Blending type
 * @{
 */
#define GX_BM_NONE			0			/*!< Write input directly to EFB */
#define GX_BM_BLEND			1			/*!< Blend using blending equation */
#define GX_BM_LOGIC			2			/*!< Blend using bitwise operation */
#define GX_BM_SUBTRACT		3			/*!< Input subtracts from existing pixel */
#define GX_MAX_BLENDMODE	4
/*! @} */

/*! \addtogroup blendfactor Blending control
 * \details Each pixel (source or destination) is multiplied by any of these controls.
 * @{
 */
#define GX_BL_ZERO			0			/*!< 0.0 */
#define GX_BL_ONE			1			/*!< 1.0 */
#define GX_BL_SRCCLR		2			/*!< source color */
#define GX_BL_INVSRCCLR		3			/*!< 1.0 - (source color) */
#define GX_BL_SRCALPHA		4			/*!< source alpha */
#define GX_BL_INVSRCALPHA	5			/*!< 1.0 - (source alpha) */
#define GX_BL_DSTALPHA		6			/*!< framebuffer alpha */
#define GX_BL_INVDSTALPHA	7			/*!< 1.0 - (FB alpha) */
#define GX_BL_DSTCLR		GX_BL_SRCCLR
#define GX_BL_INVDSTCLR		GX_BL_INVSRCCLR
/*! @} */

/*! \addtogroup logicop Logical operation type
 * \details Destination (dst) acquires the value of one of these operations, given in C syntax.
 * @{
 */
#define GX_LO_CLEAR			0			/*!< 0 */
#define GX_LO_AND			1			/*!< src & dst */
#define GX_LO_REVAND		2			/*!< src & ~dst */
#define GX_LO_COPY			3			/*!< src */
#define GX_LO_INVAND		4			/*!< ~src & dst */
#define GX_LO_NOOP			5			/*!< dst */
#define GX_LO_XOR			6			/*!< src ^ dst */
#define GX_LO_OR			7			/*!< src | dst */
#define GX_LO_NOR			8			/*!< ~(src | dst) */
#define GX_LO_EQUIV			9			/*!< ~(src ^ dst) */
#define GX_LO_INV			10			/*!< ~dst */
#define GX_LO_REVOR			11			/*!< src | ~dst */
#define GX_LO_INVCOPY		12			/*!< ~src */
#define GX_LO_INVOR			13			/*!< ~src | dst */
#define GX_LO_NAND			14			/*!< ~(src & dst) */
#define GX_LO_SET			15			/*!< 1 */
/*! @} */

/*! \addtogroup texoff Texture offset value
 * \brief Used for texturing points or lines.
 * @{
 */
#define GX_TO_ZERO			0
#define GX_TO_SIXTEENTH		1
#define GX_TO_EIGHTH		2
#define GX_TO_FOURTH		3
#define GX_TO_HALF			4
#define GX_TO_ONE			5
#define GX_MAX_TEXOFFSET	6
/*! @} */

/*! \addtogroup tevdefmode TEV combiner operation
 * \brief Color/Alpha combiner modes for GX_SetTevOp().
 *
 * \details For these equations, <i>Cv</i> is the output color for the stage, <i>Cr</i> is the output color of previous stage, and <i>Ct</i> is the texture color. <i>Av</i> is the output
 * alpha for a stage, <i>Ar</i> is the output alpha of previous stage, and <i>At</i> is the texture alpha. As a special case, rasterized color
 * (<tt>GX_CC_RASC</tt>) is used as <i>Cr</i> and rasterized alpha (<tt>GX_CA_RASA</tt>) is used as <i>Ar</i> at the first TEV stage because there is no previous stage.
 *
 * @{
 */

#define GX_MODULATE			0			/*!< <i>Cv</i>=<i>CrCt</i>; <i>Av</i>=<i>ArAt</i> */
#define GX_DECAL			1			/*!< <i>Cv</i>=(1-<i>At</i>)<i>Cr</i> + <i>AtCt</i>; <i>Av</i>=<i>Ar</i> */
#define GX_BLEND			2			/*!< <i>Cv=(1-<i>Ct</i>)<i>Cr</i> + <i>Ct</i>; <i>Av</i>=<i>AtAr</i> */
#define GX_REPLACE			3			/*!< <i>Cv=<i>Ct</i>; <i>Ar=<i>At</i> */
#define GX_PASSCLR			4			/*!< <i>Cv=<i>Cr</i>; <i>Av=<i>Ar</i> */

/*! @} */

/*! \addtogroup tevcolorarg TEV color combiner input
 * @{
 */

#define GX_CC_CPREV			0				/*!< Use the color value from previous TEV stage */
#define GX_CC_APREV			1				/*!< Use the alpha value from previous TEV stage */
#define GX_CC_C0			2				/*!< Use the color value from the color/output register 0 */
#define GX_CC_A0			3				/*!< Use the alpha value from the color/output register 0 */
#define GX_CC_C1			4				/*!< Use the color value from the color/output register 1 */
#define GX_CC_A1			5				/*!< Use the alpha value from the color/output register 1 */
#define GX_CC_C2			6				/*!< Use the color value from the color/output register 2 */
#define GX_CC_A2			7				/*!< Use the alpha value from the color/output register 2 */
#define GX_CC_TEXC			8				/*!< Use the color value from texture */
#define GX_CC_TEXA			9				/*!< Use the alpha value from texture */
#define GX_CC_RASC			10				/*!< Use the color value from rasterizer */
#define GX_CC_RASA			11				/*!< Use the alpha value from rasterizer */
#define GX_CC_ONE			12
#define GX_CC_HALF			13
#define GX_CC_KONST			14
#define GX_CC_ZERO			15				/*!< Use to pass zero value */

/*! @} */

/*! \addtogroup tevalphaarg TEV alpha combiner input
 * @{
 */

#define GX_CA_APREV			0				/*!< Use the alpha value from previous TEV stage */
#define GX_CA_A0			1				/*!< Use the alpha value from the color/output register 0 */
#define GX_CA_A1			2				/*!< Use the alpha value from the color/output register 1 */
#define GX_CA_A2			3				/*!< Use the alpha value from the color/output register 2 */
#define GX_CA_TEXA			4				/*!< Use the alpha value from texture */
#define GX_CA_RASA			5				/*!< Use the alpha value from rasterizer */
#define GX_CA_KONST			6
#define GX_CA_ZERO			7				/*!< Use to pass zero value */

/*! @} */

/*! \addtogroup tevstage TEV stage
 * \details The GameCube's Graphics Processor (GP) can use up to 16 stages to compute a texel for a particular surface.
 * By default, each texture will use two stages, but it can be configured through various functions calls.
 *
 * \note This is different from \ref texmapid s, where textures are loaded into.
 * @{
 */

#define GX_TEVSTAGE0		0
#define GX_TEVSTAGE1		1
#define GX_TEVSTAGE2		2
#define GX_TEVSTAGE3		3
#define GX_TEVSTAGE4		4
#define GX_TEVSTAGE5		5
#define GX_TEVSTAGE6		6
#define GX_TEVSTAGE7		7
#define GX_TEVSTAGE8		8
#define GX_TEVSTAGE9		9
#define GX_TEVSTAGE10		10
#define GX_TEVSTAGE11		11
#define GX_TEVSTAGE12		12
#define GX_TEVSTAGE13		13
#define GX_TEVSTAGE14		14
#define GX_TEVSTAGE15		15
#define GX_MAX_TEVSTAGE		16

/*! @} */

/*! \addtogroup tevop TEV combiner operator
 * @{
 */

#define GX_TEV_ADD				0
#define GX_TEV_SUB				1
#define GX_TEV_COMP_R8_GT		8
#define GX_TEV_COMP_R8_EQ		9
#define GX_TEV_COMP_GR16_GT		10
#define GX_TEV_COMP_GR16_EQ		11
#define GX_TEV_COMP_BGR24_GT	12
#define GX_TEV_COMP_BGR24_EQ	13
#define GX_TEV_COMP_RGB8_GT		14
#define GX_TEV_COMP_RGB8_EQ		15
#define GX_TEV_COMP_A8_GT		GX_TEV_COMP_RGB8_GT	 // for alpha channel
#define GX_TEV_COMP_A8_EQ		GX_TEV_COMP_RGB8_EQ  // for alpha channel

/*! @} */

/*! \addtogroup tevbias TEV bias value
 * @{
 */

#define GX_TB_ZERO				0
#define GX_TB_ADDHALF			1
#define GX_TB_SUBHALF			2
#define GX_MAX_TEVBIAS			3

/*! @} */

/*! \addtogroup tevclampmode TEV clamping mode
 * \note These modes are used for a function which is not implementable on production (i.e. retail) GameCube hardware.
 * @{
 */

#define GX_TC_LINEAR			0
#define GX_TC_GE				1
#define GX_TC_EQ				2
#define GX_TC_LE				3
#define GX_MAX_TEVCLAMPMODE		4

/*! @} */

/*! \addtogroup tevscale TEV scale value
 * @{
 */

#define GX_CS_SCALE_1			0
#define GX_CS_SCALE_2			1
#define GX_CS_SCALE_4			2
#define GX_CS_DIVIDE_2			3
#define GX_MAX_TEVSCALE			4

/*! @} */

/*! \addtogroup tevcoloutreg TEV color/output register
 * @{
 */

#define GX_TEVPREV				0			/*!< Default register for passing results from one stage to another. */
#define GX_TEVREG0				1
#define GX_TEVREG1				2
#define GX_TEVREG2				3
#define GX_MAX_TEVREG			4

/*! @} */

/*! \addtogroup cullmode Backface culling mode
 * @{
 */
#define GX_CULL_NONE			0			/*!< Do not cull any primitives. */
#define GX_CULL_FRONT			1			/*!< Cull front-facing primitives. */
#define GX_CULL_BACK			2			/*!< Cull back-facing primitives. */
#define GX_CULL_ALL				3			/*!< Cull all primitives. */
/*! @} */

/*! \addtogroup texmapid texture map slot
 * \brief Texture map slots to hold textures in.
 *
 * \details The GameCube's Graphics Processor (GP) can apply up to eight textures to a single surface. Those textures
 * are assigned one of these slots. Various operations used on or with a particular texture will also take one of these
 * items, including operations regarding texture coordinate generation (although not necessarily on the same slot).
 *
 * \note This is different from \ref tevstage s, which are the actual quanta for work with textures.
 * @{
 */
#define GX_TEXMAP0				0			/*!< Texture map slot 0 */
#define GX_TEXMAP1				1			/*!< Texture map slot 1 */
#define GX_TEXMAP2				2			/*!< Texture map slot 2 */
#define GX_TEXMAP3				3			/*!< Texture map slot 3 */
#define GX_TEXMAP4				4			/*!< Texture map slot 4 */
#define GX_TEXMAP5				5			/*!< Texture map slot 5 */
#define GX_TEXMAP6				6			/*!< Texture map slot 6 */
#define GX_TEXMAP7				7			/*!< Texture map slot 7 */
#define GX_MAX_TEXMAP			8
#define GX_TEXMAP_NULL			0xff			/*!< No texmap */
#define GX_TEXMAP_DISABLE		0x100			/*!< Disable texmap lookup for this texmap slot (use bitwise OR with a texture map slot). */
/*! @} */

/*! \addtogroup alphaop Alpha combine control
 * @{
 */
#define GX_AOP_AND				0
#define GX_AOP_OR				1
#define GX_AOP_XOR				2
#define GX_AOP_XNOR				3
#define GX_MAX_ALPHAOP			4
/*! @} */

/*! \addtogroup tevkcolorid TEV constant color register
 * @{
 */
#define GX_KCOLOR0				0			/*!< Constant register 0 */
#define GX_KCOLOR1				1			/*!< Constant register 1 */
#define GX_KCOLOR2				2			/*!< Constant register 2 */
#define GX_KCOLOR3				3			/*!< Constant register 3 */
#define GX_KCOLOR_MAX			4
/*! @} */

/*! \addtogroup tevkcolorsel TEV constant color selection
 * @{
 */
#define GX_TEV_KCSEL_1					0x00			/*!< constant 1.0 */
#define GX_TEV_KCSEL_7_8				0x01			/*!< constant 7/8 */
#define GX_TEV_KCSEL_3_4				0x02			/*!< constant 3/4 */
#define GX_TEV_KCSEL_5_8				0x03			/*!< constant 5/8 */
#define GX_TEV_KCSEL_1_2				0x04			/*!< constant 1/2 */
#define GX_TEV_KCSEL_3_8				0x05			/*!< constant 3/8 */
#define GX_TEV_KCSEL_1_4				0x06			/*!< constant 1/4 */
#define GX_TEV_KCSEL_1_8				0x07			/*!< constant 1/8 */
#define GX_TEV_KCSEL_K0					0x0C			/*!< K0[RGB] register */
#define GX_TEV_KCSEL_K1					0x0D			/*!< K1[RGB] register */
#define GX_TEV_KCSEL_K2					0x0E			/*!< K2[RGB] register */
#define GX_TEV_KCSEL_K3					0x0F			/*!< K3[RGB] register */
#define GX_TEV_KCSEL_K0_R				0x10			/*!< K0[RRR] register */
#define GX_TEV_KCSEL_K1_R				0x11			/*!< K1[RRR] register */
#define GX_TEV_KCSEL_K2_R				0x12			/*!< K2[RRR] register */
#define GX_TEV_KCSEL_K3_R				0x13			/*!< K3[RRR] register */
#define GX_TEV_KCSEL_K0_G				0x14			/*!< K0[GGG] register */
#define GX_TEV_KCSEL_K1_G				0x15			/*!< K1[GGG] register */
#define GX_TEV_KCSEL_K2_G				0x16			/*!< K2[GGG] register */
#define GX_TEV_KCSEL_K3_G				0x17			/*!< K3[GGG] register */
#define GX_TEV_KCSEL_K0_B				0x18			/*!< K0[BBB] register */
#define GX_TEV_KCSEL_K1_B				0x19			/*!< K1[BBB] register */
#define GX_TEV_KCSEL_K2_B				0x1A			/*!< K2[BBB] register */
#define GX_TEV_KCSEL_K3_B				0x1B			/*!< K3[RBB] register */
#define GX_TEV_KCSEL_K0_A				0x1C			/*!< K0[AAA] register */
#define GX_TEV_KCSEL_K1_A				0x1D			/*!< K1[AAA] register */
#define GX_TEV_KCSEL_K2_A				0x1E			/*!< K2[AAA] register */
#define GX_TEV_KCSEL_K3_A				0x1F			/*!< K3[AAA] register */
/*! @} */

/*! \addtogroup tevkalphasel TEV constant alpha selection
 * @{
 */
#define GX_TEV_KASEL_1					0x00			/*!< constant 1.0 */
#define GX_TEV_KASEL_7_8				0x01			/*!< constant 7/8 */
#define GX_TEV_KASEL_3_4				0x02			/*!< constant 3/4 */
#define GX_TEV_KASEL_5_8				0x03			/*!< constant 5/8 */
#define GX_TEV_KASEL_1_2				0x04			/*!< constant 1/2 */
#define GX_TEV_KASEL_3_8				0x05			/*!< constant 3/8 */
#define GX_TEV_KASEL_1_4				0x06			/*!< constant 1/4 */
#define GX_TEV_KASEL_1_8				0x07			/*!< constant 1/8 */
#define GX_TEV_KASEL_K0_R				0x10			/*!< K0[R] register */
#define GX_TEV_KASEL_K1_R				0x11			/*!< K1[R] register */
#define GX_TEV_KASEL_K2_R				0x12			/*!< K2[R] register */
#define GX_TEV_KASEL_K3_R				0x13			/*!< K3[R] register */
#define GX_TEV_KASEL_K0_G				0x14			/*!< K0[G] register */
#define GX_TEV_KASEL_K1_G				0x15			/*!< K1[G] register */
#define GX_TEV_KASEL_K2_G				0x16			/*!< K2[G] register */
#define GX_TEV_KASEL_K3_G				0x17			/*!< K3[G] register */
#define GX_TEV_KASEL_K0_B				0x18			/*!< K0[B] register */
#define GX_TEV_KASEL_K1_B				0x19			/*!< K1[B] register */
#define GX_TEV_KASEL_K2_B				0x1A			/*!< K2[B] register */
#define GX_TEV_KASEL_K3_B				0x1B			/*!< K3[B] register */
#define GX_TEV_KASEL_K0_A				0x1C			/*!< K0[A] register */
#define GX_TEV_KASEL_K1_A				0x1D			/*!< K1[A] register */
#define GX_TEV_KASEL_K2_A				0x1E			/*!< K2[A] register */
#define GX_TEV_KASEL_K3_A				0x1F			/*!< K3[A] register */
/*! @} */

/*! \addtogroup tevswapsel TEV color swap table entry
 * @{
 */

#define GX_TEV_SWAP0					0
#define GX_TEV_SWAP1					1
#define GX_TEV_SWAP2					2
#define GX_TEV_SWAP3					3
#define GX_MAX_TEVSWAP					4

/*! @} */

/* tev color chan */
#define GX_CH_RED						0
#define GX_CH_GREEN						1
#define GX_CH_BLUE						2
#define GX_CH_ALPHA						3

/*! \addtogroup indtexstage Indirect texture stage
 * @{
 */
#define GX_INDTEXSTAGE0					0
#define GX_INDTEXSTAGE1					1
#define GX_INDTEXSTAGE2					2
#define GX_INDTEXSTAGE3					3
#define GX_MAX_INDTEXSTAGE				4
/*! @} */

/*! \addtogroup indtexformat Indirect texture format
 * \details Bits for the indirect offsets are extracted from the high end of each component byte. Bits for the bump alpha
 * are extraced off the low end of the byte. For <tt>GX_ITF_8</tt>, the byte is duplicated for the offset and the bump alpha.
 * @{
 */
#define GX_ITF_8						0
#define GX_ITF_5						1
#define GX_ITF_4						2
#define GX_ITF_3						3
#define GX_MAX_ITFORMAT					4
/*! @} */

/*! \addtogroup indtexbias Indirect texture bias select
 * \brief Indicates which components of the indirect offset should receive a bias value.
 *
 * \details The bias is fixed at -128 for <tt>GX_ITF_8</tt> and +1 for the other formats. The bias happens prior to the indirect matrix multiply.
 * @{
 */
#define GX_ITB_NONE						0
#define GX_ITB_S						1
#define GX_ITB_T						2
#define GX_ITB_ST						3
#define GX_ITB_U						4
#define GX_ITB_SU						5
#define GX_ITB_TU						6
#define GX_ITB_STU						7
#define GX_MAX_ITBIAS					8
/*! @} */

/*! \addtogroup indtexmtx Indirect texture matrix
 * @{
 */
#define GX_ITM_OFF						0			/*!< Specifies a matrix of all zeroes. */
#define GX_ITM_0						1			/*!< Specifies indirect matrix 0, indirect scale 0. */
#define GX_ITM_1						2			/*!< Specifies indirect matrix 1, indirect scale 1. */
#define GX_ITM_2						3			/*!< Specifies indirect matrix 2, indirect scale 2. */
#define GX_ITM_S0						5			/*!< Specifies dynamic S-type matrix, indirect scale 0. */
#define GX_ITM_S1						6			/*!< Specifies dynamic S-type matrix, indirect scale 1. */
#define GX_ITM_S2						7			/*!< Specifies dynamic S-type matrix, indirect scale 2. */
#define GX_ITM_T0						9			/*!< Specifies dynamic T-type matrix, indirect scale 0. */
#define GX_ITM_T1						10			/*!< Specifies dynamic T-type matrix, indirect scale 1. */
#define GX_ITM_T2						11			/*!< Specifies dynamic T-type matrix, indirect scale 2. */
/*! @} */

/*! \addtogroup indtexwrap Indirect texture wrap value
 * \brief Indicates whether the regular texture coordinate should be wrapped before being added to the offset.
 *
 * \details <tt>GX_ITW_OFF</tt> specifies no wrapping. <tt>GX_ITW_0</tt> will zero out the regular texture coordinate.
 * @{
 */
#define GX_ITW_OFF						0
#define GX_ITW_256						1
#define GX_ITW_128						2
#define GX_ITW_64						3
#define GX_ITW_32						4
#define GX_ITW_16						5
#define GX_ITW_0						6
#define GX_MAX_ITWRAP					7
/*! @} */

/*! \addtogroup indtexalphasel Indirect texture bump alpha select
 * \brief Indicates which offset component should provide the "bump" alpha output for the given TEV stage.
 *
 * \note Bump alpha is not available for TEV stage 0.
 * @{
 */
#define GX_ITBA_OFF						0
#define GX_ITBA_S						1
#define GX_ITBA_T						2
#define GX_ITBA_U						3
#define GX_MAX_ITBALPHA					4
/*! @} */

/*! \addtogroup indtexscale Indirect texture scale
 * \brief Specifies an additional scale value that may be applied to the texcoord used for an indirect initial lookup (not a TEV stage regular lookup).
 *
 * \details The scale value is a fraction; thus <tt>GX_ITS_32</tt> means to divide the texture coordinate values by 32.
 * @{
 */
#define GX_ITS_1						0
#define GX_ITS_2						1
#define GX_ITS_4						2
#define GX_ITS_8						3
#define GX_ITS_16						4
#define GX_ITS_32						5
#define GX_ITS_64						6
#define GX_ITS_128						7
#define GX_ITS_256						8
#define GX_MAX_ITSCALE					9
/*! @} */

/*! \addtogroup fogtype Fog equation control
 * @{
 */
#define GX_FOG_NONE						0

#define GX_FOG_PERSP_LIN				2
#define GX_FOG_PERSP_EXP				4
#define GX_FOG_PERSP_EXP2				5
#define GX_FOG_PERSP_REVEXP				6
#define GX_FOG_PERSP_REVEXP2			7

#define GX_FOG_ORTHO_LIN				10
#define GX_FOG_ORTHO_EXP				12
#define GX_FOG_ORTHO_EXP2				13
#define GX_FOG_ORTHO_REVEXP				14
#define GX_FOG_ORTHO_REVEXP2			15

#define GX_FOG_LIN						GX_FOG_PERSP_LIN
#define GX_FOG_EXP						GX_FOG_PERSP_EXP
#define GX_FOG_EXP2						GX_FOG_PERSP_EXP2
#define GX_FOG_REVEXP  					GX_FOG_PERSP_REVEXP
#define GX_FOG_REVEXP2 					GX_FOG_PERSP_REVEXP2
/*! @} */

/* pixel format */
#define GX_PF_RGB8_Z24					0
#define GX_PF_RGBA6_Z24					1
#define GX_PF_RGB565_Z16				2
#define GX_PF_Z24						3
#define GX_PF_Y8						4
#define GX_PF_U8						5
#define GX_PF_V8						6
#define GX_PF_YUV420					7

/*! \addtogroup zfmt Compressed Z format
 * @{
 */
#define GX_ZC_LINEAR					0
#define GX_ZC_NEAR						1
#define GX_ZC_MID						2
#define GX_ZC_FAR						3
/*! @} */

/*! \addtogroup xfbclamp XFB clamp modes
 * @{
 */

#define GX_CLAMP_NONE					0
#define GX_CLAMP_TOP					1
#define GX_CLAMP_BOTTOM					2

/*! @} */

/*! \addtogroup gammamode Gamma values
 * @{
 */

#define GX_GM_1_0						0
#define GX_GM_1_7						1
#define GX_GM_2_2						2

/*! @} */

/*! \addtogroup copymode EFB copy mode
 * \brief Controls whether all lines, only even lines, or only odd lines are copied from the EFB.
 * @{
 */
#define GX_COPY_PROGRESSIVE				0
#define GX_COPY_INTLC_EVEN				2
#define GX_COPY_INTLC_ODD				3
/*! @} */

/*! \addtogroup alphareadmode Alpha read mode
 * @{
 */
#define GX_READ_00						0			/*!< Always read 0x00. */
#define GX_READ_FF						1			/*!< Always read 0xFF. */
#define GX_READ_NONE					2			/*!< Always read the real alpha value. */
/*! @} */

/*! \addtogroup texcachesize Texture cache size
 * \brief Size of texture cache regions.
 * @{
 */
#define GX_TEXCACHE_32K					0
#define GX_TEXCACHE_128K				1
#define GX_TEXCACHE_512K				2
#define GX_TEXCACHE_NONE				3
/*! @} */

/*! \addtogroup distattnfn Brightness decreasing function
 * \brief Type of the brightness decreasing function by distance.
 * @{
 */
#define GX_DA_OFF						0
#define GX_DA_GENTLE					1
#define GX_DA_MEDIUM					2
#define GX_DA_STEEP						3
/*! @} */

/*! \addtogroup spotfn Spot illumination distribution function
 * @{
 */
#define GX_SP_OFF						0
#define GX_SP_FLAT						1
#define GX_SP_COS						2
#define GX_SP_COS2						3
#define GX_SP_SHARP						4
#define GX_SP_RING1						5
#define GX_SP_RING2						6
/*! @} */

/*! \addtogroup texfilter Texture filter types
 * @{
 */
#define GX_NEAR							0			/*!< Point sampling, no mipmap */
#define GX_LINEAR						1			/*!< Bilinear filtering, no mipmap */
#define GX_NEAR_MIP_NEAR				2			/*!< Point sampling, discrete mipmap */
#define GX_LIN_MIP_NEAR					3			/*!< Bilinear filtering, discrete mipmap */
#define GX_NEAR_MIP_LIN					4			/*!< Point sampling, linear mipmap */
#define GX_LIN_MIP_LIN					5			/*!< Trilinear filtering */
/*! @} */

/*! \addtogroup anisotropy Maximum anisotropy filter control
 * @{
 */
#define GX_ANISO_1						0
#define GX_ANISO_2						1
#define GX_ANISO_4						2
#define GX_MAX_ANISOTROPY				3
/*! @} */

/*! \addtogroup vcachemetrics Vertex cache performance counter
 * @{
 */
#define GX_VC_POS						0
#define GX_VC_NRM						1
#define GX_VC_CLR0						2
#define GX_VC_CLR1						3
#define GX_VC_TEX0						4
#define GX_VC_TEX1						5
#define GX_VC_TEX2						6
#define GX_VC_TEX3						7
#define GX_VC_TEX4						8
#define GX_VC_TEX5						9
#define GX_VC_TEX6						10
#define GX_VC_TEX7						11
#define GX_VC_ALL						15
/*! @} */

/*! \addtogroup perf0metrics Performance counter 0 metric
 * \details Performance counter 0 is used to measure attributes dealing with geometry and primitives, such as triangle counts and clipping ratios.
 *
 * \note <tt>GX_PERF0_XF_*</tt> measure how many GP cycles are spent in each stage of the XF.<br><br>
 *
 * \note The triangle metrics (<tt>GX_PERF0_TRIANGLES_*</tt>) allow counting triangles under specific conditions or with specific attributes.<br><br>
 *
 * \note <tt>GX_PERF0_TRIANGLES_*TEX</tt> count triangles based on the number of texture coordinates supplied; <tt>GX_PERF0_TRIANGLES_*CLR</tt> count
 * triangles based on the number of colors supplied.<br><br>
 *
 * \note The quad metrics allow you to count the number of quads (2x2 pixels) the GP processes. The term <i>coverage</i> is used to indicate how many
 * pixels in the quad are actually part of the triangle being rasterized. For example, a coverage of 4 means all pixels in the quad intersect the
 * triangle. A coverage of 1 indicates that only 1 pixel in the quad intersected the triangle.
 * @{
 */
#define GX_PERF0_VERTICES				0			/*!< Number of vertices processed by the GP. */
#define GX_PERF0_CLIP_VTX				1			/*!< Number of vertices that were clipped by the GP. */
#define GX_PERF0_CLIP_CLKS				2			/*!< Number of GP clocks spent clipping. */
#define GX_PERF0_XF_WAIT_IN				3			/*!< Number of cycles the XF is waiting on input. If the XF is waiting a large percentage
													* of the total time, it may indicate that the CPU is not supplying data fast enough to
													* keep the GP busy. */
#define GX_PERF0_XF_WAIT_OUT			4			/*!< Number of cycles the XF waits to send its output to the rest of the GP pipeline. If
													* the XF cannot output, it may indicate that the GP is currently fill-rate limited. */
#define GX_PERF0_XF_XFRM_CLKS			5			/*!< Number of cycles the transform engine is busy. */
#define GX_PERF0_XF_LIT_CLKS			6			/*!< Number of cycles the lighting engine is busy. */
#define GX_PERF0_XF_BOT_CLKS			7			/*!< Number of cycles the bottom of the pipe (result combiner) is busy. */
#define GX_PERF0_XF_REGLD_CLKS			8			/*!< Number of cycles are spent loading XF state registers. */
#define GX_PERF0_XF_REGRD_CLKS			9			/*!< Number of cycles the XF reads the state registers. */
#define GX_PERF0_CLIP_RATIO				10
#define GX_PERF0_TRIANGLES				11			/*!< Number of triangles. */
#define GX_PERF0_TRIANGLES_CULLED		12			/*!< Number of triangles that <i>failed</i> the front-face/back-face culling test. */
#define GX_PERF0_TRIANGLES_PASSED		13			/*!< Number of triangles that <i>passed</i> the front-face/back-face culling test. */
#define GX_PERF0_TRIANGLES_SCISSORED	14			/*!< Number of triangles that are scissored. */
#define GX_PERF0_TRIANGLES_0TEX			15
#define GX_PERF0_TRIANGLES_1TEX			16
#define GX_PERF0_TRIANGLES_2TEX			17
#define GX_PERF0_TRIANGLES_3TEX			18
#define GX_PERF0_TRIANGLES_4TEX			19
#define GX_PERF0_TRIANGLES_5TEX			20
#define GX_PERF0_TRIANGLES_6TEX			21
#define GX_PERF0_TRIANGLES_7TEX			22
#define GX_PERF0_TRIANGLES_8TEX			23
#define GX_PERF0_TRIANGLES_0CLR			24
#define GX_PERF0_TRIANGLES_1CLR			25
#define GX_PERF0_TRIANGLES_2CLR			26
#define GX_PERF0_QUAD_0CVG				27			/*!< Number of quads having zero coverage. */
#define GX_PERF0_QUAD_NON0CVG			28			/*!< Number of quads having coverage greater than zero. */
#define GX_PERF0_QUAD_1CVG				29			/*!< Number of quads with 1 pixel coverage. */
#define GX_PERF0_QUAD_2CVG				30			/*!< Number of quads with 2 pixel coverage. */
#define GX_PERF0_QUAD_3CVG				31			/*!< Number of quads with 3 pixel coverage. */
#define GX_PERF0_QUAD_4CVG				32			/*!< Number of quads with 4 pixel coverage. */
#define GX_PERF0_AVG_QUAD_CNT			33			/*!< Average quad count; average based on what is unknown */
#define GX_PERF0_CLOCKS					34			/*!< Number of GP clocks that have elapsed since the previous call to GX_ReadGP0Metric(). */
#define GX_PERF0_NONE					35			/*!< Disables performance measurement for perf0 and resets the counter. */
/*! @} */

/*! \addtogroup perf1metrics Performance counter 1 metric
 * \details Performance counter 1 is used for measuring texturing and caching performance as well as FIFO performance.
 *
 * \note <tt>GX_PERF1_TC_*</tt> can be used to compute the texture cache (TC) miss rate. The <tt>TC_CHECK*</tt> parameters count how many texture cache lines are
 * accessed for each pixel. In the worst case, for a mipmap, up to 8 cache lines may be accessed to produce one textured pixel.
 * <tt>GX_PERF1_TC_MISS</tt> counts how many of those accesses missed the texture cache. To compute the miss rate, divide <tt>GX_PERF1_TC_MISS</tt> by the sum of all four
 * <tt>GX_PERF1_TC_CHECK*</tt> values.<br><br>
 *
 * \note <tt>GX_PERF1_VC_*</tt> count different vertex cache stall conditions.
 * @{
 */
#define GX_PERF1_TEXELS					0			/*!< Number of texels processed by the GP. */
#define GX_PERF1_TX_IDLE				1			/*!< Number of clocks that the texture unit (TX) is idle. */
#define GX_PERF1_TX_REGS				2			/*!< Number of GP clocks spent writing to state registers in the TX unit. */
#define GX_PERF1_TX_MEMSTALL			3			/*!< Number of GP clocks the TX unit is stalled waiting for main memory. */
#define GX_PERF1_TC_CHECK1_2			4
#define GX_PERF1_TC_CHECK3_4			5
#define GX_PERF1_TC_CHECK5_6			6
#define GX_PERF1_TC_CHECK7_8			7
#define GX_PERF1_TC_MISS				8			/*!< Number of texture cache misses in total? */
#define GX_PERF1_VC_ELEMQ_FULL			9
#define GX_PERF1_VC_MISSQ_FULL			10
#define GX_PERF1_VC_MEMREQ_FULL			11
#define GX_PERF1_VC_STATUS7				12
#define GX_PERF1_VC_MISSREP_FULL		13
#define GX_PERF1_VC_STREAMBUF_LOW		14
#define GX_PERF1_VC_ALL_STALLS			15
#define GX_PERF1_VERTICES				16			/*!< Number of vertices processed by the GP. */
#define GX_PERF1_FIFO_REQ				17			/*!< Number of lines (32B) read from the GP FIFO. */
#define GX_PERF1_CALL_REQ				18			/*!< Number of lines (32B) read from called display lists. */
#define GX_PERF1_VC_MISS_REQ			19			/*!< Number vertex cache miss request. Each miss requests a 32B transfer from main memory. */
#define GX_PERF1_CP_ALL_REQ				20			/*!< Counts all requests (32B/request) from the GP Command Processor (CP). It should be equal to
													* the sum of counts returned by <tt>GX_PERF1_FIFO_REQ</tt>, <tt>GX_PERF1_CALL_REQ</tt>, and <tt>GX_PERF1_VC_MISS_REQ</tt>. */
#define GX_PERF1_CLOCKS					21			/*!< Number of GP clocks that have elapsed since the last call to GX_ReadGP1Metric(). */
#define GX_PERF1_NONE					22			/*!< Disables performance measurement for perf1 and resets the counter. */
/*! @} */

/*! \addtogroup tlutname TLUT name
 * \brief Name of Texture Look-Up Table (TLUT) in texture memory.
 *
 * \details Each table <tt>GX_TLUT0</tt>-<tt>GX_TLUT15</tt> contains 256 entries,16b per entry. <tt>GX_BIGTLUT0</tt>-<tt>3</tt>
 * contains 1024 entries, 16b per entry. Used for configuring texture memory in GX_Init().
 * @{
 */
#define GX_TLUT0						 0
#define GX_TLUT1						 1
#define GX_TLUT2						 2
#define GX_TLUT3						 3
#define GX_TLUT4						 4
#define GX_TLUT5						 5
#define GX_TLUT6						 6
#define GX_TLUT7						 7
#define GX_TLUT8						 8
#define GX_TLUT9						 9
#define GX_TLUT10						10
#define GX_TLUT11						11
#define GX_TLUT12						12
#define GX_TLUT13						13
#define GX_TLUT14						14
#define GX_TLUT15						15
#define GX_BIGTLUT0						16
#define GX_BIGTLUT1						17
#define GX_BIGTLUT2						18
#define GX_BIGTLUT3						19
/*! @} */

#define GX_MAX_VTXDESC					GX_VA_MAXATTR
#define GX_MAX_VTXDESC_LISTSIZE			(GX_VA_MAXATTR+1)

#define GX_MAX_VTXATTRFMT				GX_VA_MAXATTR
#define GX_MAX_VTXATTRFMT_LISTSIZE		(GX_VA_MAXATTR+1)

#define GX_MAX_Z24						0x00ffffff

#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */

typedef union _wgpipe
{
	vu8 U8;
	vs8 S8;
	vu16 U16;
	vs16 S16;
	vu32 U32;
	vs32 S32;
	vf32 F32;
} WGPipe;

/*! \typedef struct _gx_color GXColor
 * \brief Structure used to pass colors to some GX functions.
 */
typedef struct _gx_color {
 	u8 r;			/*!< Red color component. */
 	u8 g;			/*!< Green color component. */
 	u8 b;			/*!< Blue alpha component. */
	u8 a;			/*!< Alpha component. If a function does not use the alpha value, it is safely ignored. */
} GXColor;

/*! \typedef struct _gx_colors10 GXColorS10
 * \brief Structure used to pass signed 10-bit colors to some GX functions.
 */
typedef struct _gx_colors10 {
 	s16 r;			/*!< Red color component. */
 	s16 g;			/*!< Green color component. */
 	s16 b;			/*!< Blue color component. */
	s16 a;			/*!< Alpha component. If a function does not use the alpha value, it is safely ignored. */
} GXColorS10;

/*! \typedef struct _gx_texobj GXTexObj
 * \brief Object containing information about a texture.
 *
 * \details This structure contains precompiled register state setting commands and data. The application must use the GX_InitTexObj*()
 * function to initialize or change this object. The proper size of the object is returned by
 *
 * \code sizeof(GXTexObj) \endcode
 *
 * \details but the internal data representation is not visible to the application.
 */
typedef struct _gx_texobj {
	u32 val[8];
} GXTexObj;

/*! \typedef struct _gx_tlutobj GXTlutObj
 * \brief Object containing information on a TLUT.
 *
 * \details This structure contains precompiled register state setting commands and data. The application must use the GX_InitTlutObj()
 * function to initialize or change this object. The proper size of the object is returned by
 *
 * \code sizeof(GXTlutObj) \endcode
 *
 * \details but the internal data representation is not visible to the application.
 */
typedef struct _gx_tlutobj {
	u32 val[3];
} GXTlutObj;

/*! \typedef struct _gx_texreg GXTexRegion
 * \brief Object containing information on a texture cache region.
 *
 * \details This structure contains precompiled register state setting commands and data. The application must use the
 * GX_InitTexCacheRegion() function to initialize or change this object. The proper size of the object is returned by
 *
 * \code sizeof(GXTexRegion) \endcode
 *
 * \details but the internal data representation is not visible to the application.
 */
typedef struct _gx_texreg {
	u32 val[4];
} GXTexRegion;

/*! \typedef struct _gx_tlutreg GXTlutRegion
 * \brief Object containing information on a TLUT cache region.
 *
 * \details This structure contains precompiled register state setting commands and data. The application must use the GX_InitTlutRegion()
 * function to initialize or change this object. The proper size of the object is returned by
 *
 * \code sizeof(GXTlutRegion) \endcode
 *
 * \details but the internal data representation is not visible to the application.
 */
typedef struct _gx_tlutreg {
	u32 val[4];
} GXTlutRegion;

/*! \typedef _gx_litobj GXLightObj
 * \brief Object containing information on a light.
 *
 * \details This structure contains precompiled register state setting commands and data. The application must use the GX_InitLight*() functions
 * to initialize or change this object. The proper size of the object is returned by
 *
 * \code sizeof(GXLightObj) \endcode
 *
 * \details but the internal data representation is not visible to the application.
 */
typedef struct _gx_litobj {
	u32 val[16];
} GXLightObj;

typedef struct _vtx {
	f32 x,y,z;
	u16 s,t;
	u32 rgba;
} Vtx;

/*! \struct GXVtxDesc
 * \brief Structure describing how a single vertex attribute will be referenced.
 *
 * \details An array of these structures can be used to describe all the attributes in a vertex. The attribute <tt>GX_VA_NULL</tt> should be
 * used to mark the end of the array.
 */
typedef struct {
	u8 attr;			/*!< \ref vtxattr for this element. */
	u8 type;			/*!< \ref vtxattrin for this element. */
} GXVtxDesc;

/*! \struct GXVtxAttrFmt
 * \brief Structure describing the attribute format for one attribute.
 *
 * \details An array of these structures can be used to describe the format of all attributes in a vertex. The attribute <tt>GX_VA_NULL</tt>
 * should be used to mark the end of the array.
 */
typedef struct {
	u32 vtxattr;			/*!< \ref vtxattr for this element. */
	u32 comptype;			/*!< \ref comptype for this element. */
	u32 compsize;			/*!< \ref compsize for this element. */
	u32 frac;				/*!< Number of fractional bits for a fixed-point number. */
} GXVtxAttrFmt;

/*! \struct GXFifoObj
 * \brief Object describing a graphics FIFO.
 *
 * \details This structure contains precompiled register state setting commands and data. The application must use the GX_InitFifo*() functions
 * to initialize or change this object. The proper size of the object is returned by
 *
 * \code sizeof(GXFifoObj) \endcode
 *
 * but the internal data representation is not visible to the application.
 */
typedef struct {
	u8 pad[GX_FIFO_OBJSIZE];
} GXFifoObj;

typedef struct {
	u8 dummy[4];
} GXTexReg;

/*! \struct GXFogAdjTbl
 * \brief Fog range adjustment parameter table.
 */
typedef struct {
	u16 r[10];			/*!< u4.8 format range parameter. */
} GXFogAdjTbl;

/*! \typedef void (*GXBreakPtCallback)(void)
 * \brief function pointer typedef for the GP breakpoint-token callback
 */
typedef void (*GXBreakPtCallback)(void);

/*! \typedef void (*GXDrawDoneCallback)(void)
 * \brief function pointer typedef for the GP drawdone-token callback
 */
typedef void (*GXDrawDoneCallback)(void);

/*! \typedef void (*GXDrawSyncCallback)(u16 token)
 * \brief function pointer typedef for the drawsync-token callback
 * \param[out] token tokenvalue most recently encountered.
 */
typedef void (*GXDrawSyncCallback)(u16 token);

/*! \typedef GXTexRegion* (*GXTexRegionCallback)(GXTexObj *obj,u8 mapid)
 * \brief function pointer typedef for the texture region callback
 * \param[out] token tokenvalue most recently encountered.
 */
typedef GXTexRegion* (*GXTexRegionCallback)(GXTexObj *obj,u8 mapid);

/*! \typedef GXTlutRegion* (*GXTlutRegionCallback)(u32 tlut_name)
 * \brief function pointer typedef for the TLUT region callback
 * \param[out] token tokenvalue most recently encountered.
 */
typedef GXTlutRegion* (*GXTlutRegionCallback)(u32 tlut_name);

extern WGPipe* const wgPipe;
/*!
 * \fn GXFifoObj* GX_Init(void *base,u32 size)
 * \brief Initializes the graphics processor to its initial state.
 *
 * \details This function sets the default state of the graphics processor and should be called before any other GX functions.
 * This function sets up an immediate-mode method of communicating graphics commands from the CPU to the Graphics Processor
 * (GP). This function will initialize a FIFO and attach it to both the CPU and GP. The CPU will write commands to the FIFO
 * and the GP will read the commands. This function returns a pointer to the initialized FIFO. The application must allocate
 * the memory for the FIFO. The parameter \a base is a pointer to the allocated main memory and must be aligned to 32B. \a size
 * is the size of the FIFO in bytes and must be a multiple of 32B. Refer to additional notes in GX_InitFifoBase() concerning
 * the FIFO memory.
 *
 * \note It is also possible to override the default immediate-mode style and instead buffer the graphics for frame <i>n+1</i>
 * while the GP is reading the graphics for frame <i>n</i>. See GX_SetCPUFifo() and GX_SetGPFifo() for further information.<br><br>
 *
 * \note This function also designates the calling thread as the default GX thread; i.e., it assumes the calling thread is the
 * one responsible for generating graphics data. This thread will be the thread to be suspended when the FIFO gets too full.
 * The current GX thread can be changed by calling GX_SetCurrentGXThread().
 *
 * \param[in] base pointer to the GX FIFO buffer base address. Must be aligned on a 32 Byte boundery.
 * \param[in] size size of buffer. Must be a multiple of 32.
 *
 * \return pointer to the intialized GXFifoObj object.
 */
GXFifoObj* GX_Init(void *base,u32 size);

/*!
 * \fn void GX_InitFifoBase(GXFifoObj *fifo,void *base,u32 size)
 * \brief Describes the area of main memory that will be used for this \a fifo.
 *
 * The Graphics FIFO is the mechanism used to communicate graphics commands from the CPU to the Graphics Processor (GP). The FIFO
 * base pointer should be 32-byte aligned. memalign() can return 32-byte aligned pointers. The size should also be a multiple of
 * 32B.
 *
 * The CPU's write-gather pipe is used to write data to the FIFO. Therefore, the FIFO memory area must be forced out of the CPU
 * cache prior to being used. DCInvalidateRange() may be used for this purpose. Due to the mechanics of flushing the write-gather
 * pipe, the FIFO memory area should be at least 32 bytes larger than the maximum expected amount of data stored. Up to 32 NOPs
 * may be written at the end during flushing.
 *
 * \note GX_Init() also takes the arguments \a base and \a size and initializes a FIFO using these values and attaches the FIFO
 * to both the CPU and GP. The application must allocate the memory for the graphics FIFO before calling GX_Init(). Therefore, it
 * is not necessary to call this function unless you want to resize the default FIFO sometime after GX_Init() has been called or
 * you are creating a new FIFO. The minimum size is 64kB defined by <tt>GX_FIFO_MINSIZE</tt>.<br><br>
 *
 * \note This function will also set the read and write pointers for the FIFO to the base address, so ordinarily it is not
 * necessary to call GX_InitFifoPtrs() when initializing the FIFO. In addition, This function sets the FIFO's high water mark to
 * (size-16kB) and the low water mark to (size/2), so it is also not necessary to call GX_InitFifoLimits().
 *
 * \param[in] fifo the fifo struct to use
 * \param[in] base ptr to the base of allocation; must be 32-byte aligned
 * \param[in] size size of the FIFO in bytes; must be multiple of 32; size must be <tt>GX_FIFO_MINSIZE</tt> or larger
 *
 * \return none
 */
void GX_InitFifoBase(GXFifoObj *fifo,void *base,u32 size);

/*!
 * \fn void GX_InitFifoLimits(GXFifoObj *fifo,u32 hiwatermark,u32 lowatermark)
 * \brief Sets the high and low water mark for the \a fifo.
 *
 * \details The high and low water marks are used in <i>immediate-mode</i>, i.e. when the \a fifo is attached to both the CPU and
 * Graphics Processor (GP) (see GX_SetCPUFifo() and GX_SetGPFifo()).
 *
 * The hardware keeps track of the number of bytes between the read and write pointers. This number represents how full the FIFO is,
 * and when it is greater than or equal to the \a hiwatermark, the hardware issues an interrupt. The GX API will suspend sending
 * graphics to the Graphics FIFO until it has emptied to a certain point. The \a lowatermark is used to set the point at which the
 * FIFO is empty enough to resume sending graphics commands to the  FIFO. Both \a hiwatermark and \a lowatermark should be in
 * multiples of 32B. The count for \a lowatermark should be less than \a hiwatermark. Of course, \a hiwatermark and \a lowatermark
 * must be less than the size of the FIFO.
 *
 * \note When the FIFO is only attached to the CPU or only attached to the GP, the high and low water mark interrupts are disabled.
 *
 * \param[in] fifo the fifo struct to use
 * \param[in] hiwatermark number of bytes to be queued before libogc stops writing commands to the FIFO
 * \param[in] lowatermark number of bytes to be queued before libogc resumes writing commands to the FIFO
 *
 * \return none
 */
void GX_InitFifoLimits(GXFifoObj *fifo,u32 hiwatermark,u32 lowatermark);

/*!
 * \fn void GX_InitFifoPtrs(GXFifoObj *fifo,void *rd_ptr,void *wt_ptr)
 * \brief Sets the \a fifo read and write pointers.
 *
 * \note This is normally done only during initialization of the FIFO. After that, the graphics hardware manages the FIFO pointers.
 *
 * \param[in] fifo the fifo struct to use
 * \param[in] rd_ptr the pointer to use for the FIFO read pointer; must be 32-byte aligned
 * \param[in] wt_ptr the pointer to use for the FIFO write pointer; must be 32-byte aligned
 *
 * \return none
 */
void GX_InitFifoPtrs(GXFifoObj *fifo,void *rd_ptr,void *wt_ptr);

/*!
 * \fn void GX_GetFifoPtrs(GXFifoObj *fifo,void **rd_ptr,void **wt_ptr)
 * \brief Returns the current value of the Graphics FIFO read and write pointers.
 *
 * \note See GX_EnableBreakPt() for an example of why you would do this.
 *
 * \param[in] fifo pointer to a FIFO struct
 * \param[out] rd_ptr address of the FIFO read pointer
 * \param[out] wt_ptr address of the FIFO write pointer
 *
 * \return none
 */
void GX_GetFifoPtrs(GXFifoObj *fifo,void **rd_ptr,void **wt_ptr);

/*!
 * \fn void GX_SetCPUFifo(GXFifoObj *fifo)
 * \brief Attaches a FIFO to the CPU.
 *
 * \note If the FIFO being attached is one already attached to the GP, the FIFO can be considered to be in immediate mode. If not,
 * the CPU can write commands, and the GP will execute them when the GP attaches to this FIFO (multi-buffered mode).
 *
 * \param[in] fifo fifo struct containing info on the FIFO to attach
 *
 * \return none
 */
void GX_SetCPUFifo(GXFifoObj *fifo);

/*!
 * \fn void GX_SetGPFifo(GXFifoObj *fifo)
 * \brief Attaches \a fifo to the GP.
 *
 * \note If the FIFO is also attached to the CPU, the system is in immediate-mode, and the fifo acts like a true FIFO. In immediate-mode,
 * graphics commands are fed directly from the CPU to the GP. In immediate-mode the FIFO's high and low water marks are active. The high
 * and low water marks implement the flow-control mechanism between the CPU and GP. When the FIFO becomes more full than the high water
 * mark, the CPU will stop writing graphics commands into the FIFO.  When the FIFO empties to a point lower than the low water mark, the
 * CPU will resume writing graphics commands into the FIFO. The high and low water marks are set when intializing the FIFO using
 * GX_InitFifoLimits().<br><br>
 *
 * \note If the FIFO is only attached to the GP, the FIFO acts like a buffer. In this case, high and low water marks are disabled, and
 * the GP reads the FIFO until it is empty. Before attaching a new FIFO to the GP, you should make sure the previous FIFO is empty, using
 * the \a cmdIdle status returned by GX_GetGPStatus().<br><br>
 *
 * \note The break point mechanism can be used to force the FIFO to stop reading commands at a certain point; see GX_EnableBreakPt().
 *
 * \param[in] fifo struct containing info on the FIFO to attach
 *
 * \return none
 */
void GX_SetGPFifo(GXFifoObj *fifo);

/*!
 * \fn void GX_GetCPUFifo(GXFifoObj *fifo)
 * \brief Copies the information from the currently attached CPU FIFO into \a fifo.
 *
 * \param[out] fifo the object to copy the current CPU FIFO object data into
 *
 * \return none
 */
void GX_GetCPUFifo(GXFifoObj *fifo);

/*!
 * \fn void GX_GetGPFifo(GXFifoObj *fifo)
 * \brief Copies the information from the currently attached GP FIFO info \a fifo.
 *
 * \param[out] fifo the object to copy the current GP FIFO object data into
 *
 * \return none
 */
void GX_GetGPFifo(GXFifoObj *fifo);

/*!
 * \fn void* GX_GetFifoBase(GXFifoObj *fifo)
 * \brief Get the base address for a given \a fifo.
 *
 * \param[in] fifo the object to get the address from
 *
 * \return pointer to the base address of the FIFO in main memory.
 */
void* GX_GetFifoBase(GXFifoObj *fifo);

/*!
 * \fn u32 GX_GetFifoCount(GXFifoObj *fifo)
 * \brief Returns number of cache lines in the FIFO.
 *
 * \note The count is incorrect if an overflow has occurred (i.e. you have written more data than the size of the fifo), as the
 * hardware cannot detect an overflow in general.
 *
 * \param[in] fifo the FIFO to get the count from
 *
 * \return number of cache lines in the FIFO
 */
u32 GX_GetFifoCount(GXFifoObj *fifo);

/*!
 * \fn u32 GX_GetFifoSize(GXFifoObj *fifo)
 * \brief Get the size of a given \a fifo.
 *
 * \param[in] fifo the object to get the size from
 *
 * \return size of the FIFO, in bytes
 */
u32 GX_GetFifoSize(GXFifoObj *fifo);

/*!
 * \fn u8 GX_GetFifoWrap(GXFifoObj *fifo)
 * \brief Returns a non-zero value if the write pointer has passed the TOP of the FIFO.
 *
 * \details Returns true only if the FIFO is attached to the CPU and the FIFO write pointer has passed the top of the FIFO. Use the
 * return value to detect whether or not an overflow has occured by initializing the FIFO's write pointer to the base of the FIFO
 * before sending any commands to the FIFO.
 *
 * \note If the FIFO write pointer is not explicitly set to the base of the FIFO, you cannot rely on this function to detect overflows.
 *
 * \param[in] fifo the object to get the wrap status from
 *
 * \return wrap value
 */
u8 GX_GetFifoWrap(GXFifoObj *fifo);

/*!
 * \fn GXDrawDoneCallback GX_SetDrawDoneCallback(GXDrawDoneCallback cb)
 * \brief Installs a callback that is invoked whenever a DrawDone command is encountered by the GP.
 *
 * \details The DrawDone command is sent by GX_SetDrawDone().
 *
 * \note By the time the callback is invoked, the GP will already have resumed reading from the FIFO, if there are any commands in it.
 *
 * \param[in] cb callback to be invoked when DrawDone is encountered
 *
 * \return pointer to the previous callback
 */
GXDrawDoneCallback GX_SetDrawDoneCallback(GXDrawDoneCallback cb);

/*!
 * \fn GXBreakPtCallback GX_SetBreakPtCallback(GXBreakPtCallback cb)
 * \brief Registers \a cb as a function to be invoked when a break point is encountered.
 *
 * \warning The callback will run with interrupts disabled, so it should terminate as quickly as possible.
 *
 * \param[in] cb function to be invoked when the breakpoint is encountered; NULL means no function will run
 *
 * \return pointer to the previous callback function
 */
GXBreakPtCallback GX_SetBreakPtCallback(GXBreakPtCallback cb);

/*!
 * \fn void GX_AbortFrame()
 * \brief Aborts the current frame.
 *
 * \details This command will reset the entire graphics pipeline, including any commands in the graphics FIFO.
 *
 * \note Texture memory will not be reset, so currently loaded textures will still be valid; however, when loading texture using
 * GX_PreloadEntireTexture() or TLUTs using GX_LoadTlut(), you must make sure the command completed. You can use the draw sync mechanism to
 * do this; see GX_SetDrawSync() and GX_GetDrawSync().
 *
 * \return none
 */
void GX_AbortFrame();

/*!
 * \fn void GX_Flush()
 * \brief Flushes all commands to the GP.
 *
 * \details Specifically, it flushes the write-gather FIFO in the CPU to make sure that all commands are sent to the GP.
 *
 * \return none
 */
void GX_Flush();

/*!
 * \fn void GX_SetMisc(u32 token,u32 value)
 * \brief Sets miscellanous settings in the GP.
 *
 * \param[in] token setting to change
 * \param[in] value value to change the setting to
 *
 * \return none
 */
void GX_SetMisc(u32 token,u32 value);

/*!
 * \fn void GX_SetDrawDone()
 * \brief Sends a DrawDone command to the GP.
 *
 * \details When all previous commands have been processed and the pipeline is empty, a <i>DrawDone</i> status bit will be set,
 * and an interrupt will occur. You can receive notification of this event by installing a callback on the interrupt with
 * GX_SetDrawDoneCallback(), or you can poll the status bit with GX_WaitDrawDone(). This function also flushes the write-gather
 * FIFO in the CPU to make sure that all commands are sent to the graphics processor.
 *
 * \note This function is normally used in multibuffer mode (see GX_SetCPUFifo()). In immediate mode, the GX_DrawDone() command
 * can be used, which both sends the command and stalls until the DrawDone status is true.
 *
 * \return none
 */
void GX_SetDrawDone();

/*!
 * \fn void GX_WaitDrawDone()
 * \brief Stalls until <i>DrawDone</i> is encountered by the GP.
 *
 * \details It means all graphics commands sent before this DrawDone command have executed and the last pixel has been written to
 * the frame buffer. You may want to execute some non-graphics operations between executing GX_SetDrawDone() and this function, but
 * if you simply want to wait and have nothing to execute, you can use GX_DrawDone().
 *
 * \note This function is normally used in immediate mode (see GX_SetCPUFifo()). In multibuffer mode, sending the '<i>done</i>' command is
 * separated from polling the '<i>done</i>' status (see GX_SetDrawDone() and GX_WaitDrawDone()).
 *
 * \return none
 */
void GX_WaitDrawDone();

/*!
 * \fn u16 GX_GetDrawSync()
 * \brief Returns the value of the token register, which is written using the GX_SetDrawSync() function.
 *
 * \return the value of the token register.
 */
u16 GX_GetDrawSync();

/*!
 * \fn void GX_SetDrawSync(u16 token)
 * \brief This function sends a token into the command stream.
 *
 * \details When the token register is set, an interrupt will also be received by the CPU. You can install a callback on this interrupt
 *          with GX_SetDrawSyncCallback(). Draw syncs can be used to notify the CPU that the graphics processor is finished using a shared
 *          resource (a vertex array for instance).
 *
 * \param[in] token 16-bit value to write to the token register.
 *
 * \return none
 */
void GX_SetDrawSync(u16 token);

/*!
 * \fn GXDrawSyncCallback GX_SetDrawSyncCallback(GXDrawSyncCallback cb)
 * \brief Installs a callback that is invoked whenever a DrawSync token is encountered by the graphics pipeline.
 *
 * \details The callback's argument is the value of the token most recently encountered. Since it is possible to
 *          miss tokens (graphics processing does not stop while the callback is running), your code should be
 *          capable of deducing if any tokens have been missed.
 *
 * \param[in] cb callback to be invoked when the DrawSync tokens are encountered in the graphics pipeline.
 *
 * \return pointer to the previously set callback function.
 */
GXDrawSyncCallback GX_SetDrawSyncCallback(GXDrawSyncCallback cb);

/*!
 * \fn void GX_DisableBreakPt()
 * \brief Allows reads from the FIFO currently attached to the Graphics Processor (GP) to resume.
 *
 * \details See GX_EnableBreakPt() for an explanation of the FIFO break point feature.
 *
 * \note The breakpoint applies to the FIFO currently attached to the Graphics Processor (GP) (see GX_SetGPFifo()).
 *
 * \return none
 */
void GX_DisableBreakPt();

/*!
 * \fn void GX_EnableBreakPt(void *break_pt)
 * \brief Sets a breakpoint that causes the GP to halt when encountered.
 *
 * \note The break point feature allows the application to have two frames of graphics in the FIFO at the same time, overlapping
 * one frame's processing by the graphics processor with another frame's processing by the CPU. For example, suppose you finish
 * writing the graphics commands for one frame and are ready to start on the next. First, execute a GX_Flush() command to make
 * sure all the data in the CPU write gatherer is flushed into the FIFO. This will also align the FIFO write pointer to a 32B
 * boundary. Next, read the value of the current write pointer using GX_GetFifoPtrs(). Write the value of the write pointer as
 * the break point address using GX_EnableBreakPt(). When the FIFO read pointer reaches the break point address the hardware
 * will disable reads from the FIFO.  The status \a brkpt, returned by GX_GetGPStatus(), can be polled to detect when the break point
 * is reached. The application can then decide when to disable the break point, using GX_DisableBreakPt(), which will allow the FIFO
 * to resume reading graphics commands.<br><br>
 *
 * \note FIFO reads will stall when the GP FIFO read pointer is equal to the break point address \a break_pt. To re-enable reads of
 * the GP FIFO, use GX_DisableBreakPt().<br><br>
 *
 * \note Use GX_SetBreakPtCallback() to set what function runs when the breakpoint is encountered.
 *
 * \param[in] break_pt address for GP to break on when read.
 *
 * \return none
 */
void GX_EnableBreakPt(void *break_pt);

/*!
 * \fn void GX_DrawDone()
 * \brief Sends a DrawDone command to the GP and stalls until its subsequent execution.
 *
 * \note This function is equivalent to calling GX_SetDrawDone() then GX_WaitDrawDone().
 *
 * \return none
 */
void GX_DrawDone();

/*!
 * \fn void GX_TexModeSync()
 * \brief Inserts a synchronization command into the graphics FIFO. When the Graphics Processor sees this command, it will
 * allow the texture pipeline to flush before continuing.
 *
 * \details This command is necessary when changing the usage of regions of texture memory from preloaded or TLUT to cached areas.
 * It makes sure that the texture pipeline is finished with that area of the texture memory prior to changing its usage.
 * This function should be called prior to drawing any primitives that uses the texture memory region in its new mode. It is not
 * necessary to call this command when changing texture memory regions from cached to preloaded (or TLUT), since the commands to
 * load the regions with data will cause the necessary synchronization to happen automatically.
 *
 * \return none
 */
void GX_TexModeSync();

/*!
 * \fn void GX_InvVtxCache();
 * \brief Invalidates the vertex cache.
 *
 * \details Specifically, this functions invalidates the vertex cache tags. This function should be used whenever you relocate or modify
 * data that is read by, or may be cached by, the vertex cache. The invalidate is very fast, taking only two Graphics Processor (GP) clock
 * cycles to complete.
 *
 * \note The vertex cache is used to cache indexed attribute data. Any attribute that is set to <tt>GX_INDEX8</tt> or <tt>GX_INDEX16</tt> in the current
 * vertex descriptor (see GX_SetVtxDesc()) is indexed. Direct data bypasses the vertex cache. Direct data is any attribute that is set to
 * <tt>GX_DIRECT</tt> in the current vertex descriptor.
 *
 * \return none
 */
void GX_InvVtxCache();

/*!
 * \fn void GX_ClearVtxDesc()
 * \brief Clears all vertex attributes of the current vertex descriptor to GX_NONE.
 *
 * \note The same functionality can be obtained using GX_SetVtxDescv(), however using GX_ClearVtxDesc() is much more efficient.
 *
 * \return none
 */
void GX_ClearVtxDesc();

/*!
 * \fn void GX_LoadProjectionMtx(Mtx44 mt,u8 type)
 * \brief Sets the projection matrix.
 *
 * \note Only two types of projection matrices are supported: <tt>GX_PERSPECTIVE</tt> or <tt>GX_ORTHOGRAPHIC</tt>.
 *
 * \param[in] mt matrix to use for the perspective
 * \param[in] type which perspective type to use
 *
 * \return none
 */
void GX_LoadProjectionMtx(Mtx44 mt,u8 type);

/*!
 * \fn void GX_SetViewport(f32 xOrig,f32 yOrig,f32 wd,f32 ht,f32 nearZ,f32 farZ)
 * \brief Sets the viewport rectangle in screen coordinates.
 *
 * \details The screen origin (\a xOrig = 0.0f, \a yOrig = 0.0f) is at the top left corner of the display. Floating point arguments allow the
 * viewport to be adjusted by 1/2 line for interlaced field rendering modes; see GX_SetViewportJitter(). The viewport depth parameters are normalized coordinates
 * from 0.0f - 1.0f. The GX API will convert the depth range values to proper scale values depending on the type and format of the Z-buffer.
 *
 * \note You should avoid using negative values for \a xOrig or \a yOrig. While this may work, it may cause problems with points and lines being clipped incorrectly. If
 * you need to shift the viewport up or left, consider using GX_SetScissorBoxOffset() instead.
 *
 * \param[in] xOrig left-most X coordinate on the screen
 * \param[in] yOrig top-most Y coordinate on the screen
 * \param[in] wd width of the viewport
 * \param[in] ht height of the viewport
 * \param[in] nearZ value to use for near depth scale
 * \param[in] farZ value to use for far depth scale
 *
 * \return none
 */
void GX_SetViewport(f32 xOrig,f32 yOrig,f32 wd,f32 ht,f32 nearZ,f32 farZ);

/*!
 * \fn void GX_SetViewportJitter(f32 xOrig,f32 yOrig,f32 wd,f32 ht,f32 nearZ,f32 farZ,u32 field)
 * \brief Sets the viewport and adjusts the viewport's line offset for interlaced field rendering.
 *
 * \details Depending on whether the viewport starts on an even or odd line, and whether the next \a field to be rendered is
 * even or odd, the viewport may be adjusted by half a line. This has the same effect as slightly tilting the camera down and is necessary
 * for interlaced rendering. No other camera adjustment (i.e. don't change the projection matrix) is needed for interlaced field rendering.
 *
 * \note To set a viewport <i>without</i> jitter, use GX_SetViewport().
 *
 * \param[in] xOrig left-most X coordinate on the screen
 * \param[in] yOrig top-most Y coordinate on the screen
 * \param[in] wd width of the viewport
 * \param[in] ht height of the viewport
 * \param[in] nearZ value to use for near depth scale
 * \param[in] farZ value to use for far depth scale
 * \param[in] field whether the next field is even (0) or odd (1)
 *
 * \return none
 */
void GX_SetViewportJitter(f32 xOrig,f32 yOrig,f32 wd,f32 ht,f32 nearZ,f32 farZ,u32 field);

/*!
 * \fn void GX_SetChanCtrl(s32 channel,u8 enable,u8 ambsrc,u8 matsrc,u8 litmask,u8 diff_fn,u8 attn_fn)
 * \brief Sets the lighting controls for a particular color channel.
 *
 * \details The color channel can have one or more (up to 8) lights associated with it, set using \a litmask. The \a diff_fn and \a attn_fn parameters
 * control the lighting equation for all lights associated with this channel; the \a ambsrc and \a matsrc can be used to select whether the input
 * source colors come from register colors or vertex colors. When the channel \a enable is set to <tt>GX_FALSE</tt>, the material color source (set by \a matsrc)
 * is passed through as the channel's output color. When the channel \a enable is <tt>GX_TRUE</tt>, the output color depends on the settings of the other
 * controls (i.e., the lighting equation). GX_Init() sets the \a enable for all channels to <tt>GX_FALSE</tt>. This function only configures the lighting
 * channel; to output the result of the channel computation, use GX_SetNumChans().
 *
 * \note Even though channels <tt>GX_COLOR0</tt> and <tt>GX_ALPHA0</tt> are controlled separately for lighting, they are rasterized together as one RGBA color, effectively
 * <tt>GX_COLOR0A0</tt>. The same is true for <tt>GX_COLOR1</tt> and <tt>GX_ALPHA1</tt>-- effectively, they are rasterized as <tt>GX_COLOR1A1</tt>. Since there is only one rasterizer for
 * color in the graphics hardware, you must choose which color to rasterize for each stage in the Texture Environment (TEV) unit. This is accomplished
 * using GX_SetTevOrder().<br><br>
 *
 * \note In order to use a vertex color in channel <tt>GX_COLOR1A1</tt>, two colors per vertex must be supplied, i.e. both <tt>GX_VA_CLR0</tt> and <tt>GX_VA_CLR1</tt> must be
 * enabled in the current vertex descriptor. If only <tt>GX_VA_CLR0</tt> <b>or</b> <tt>GX_VA_CLR1</tt> is enabled in the current vertex descriptor, the vertex color is
 * directed to the channel <tt>GX_VA_COLOR0A0</tt>.<br><br>
 *
 * \note When \a ambsrc is set to <tt>GX_SRC_REG</tt>, the color set by GX_SetChanAmbColor() is used as the ambient color. When \a matsrc is <tt>GX_SRC_REG</tt>, the color set
 * by GX_SetChanMatColor() is used as the material color.
 *
 * \param[in] channel color channel to use
 * \param[in] enable whether or not to enable lighting for this channel
 * \param[in] ambsrc source for the ambient color
 * \param[in] matsrc source for the material color
 * \param[in] litmask \ref lightid or IDs to associate with this channel
 * \param[in] diff_fn \ref difffn to use
 * \param[in] attn_fn \ref attenfunc to use
 *
 * \return none
 */
void GX_SetChanCtrl(s32 channel,u8 enable,u8 ambsrc,u8 matsrc,u8 litmask,u8 diff_fn,u8 attn_fn);

/*!
 * \fn void GX_SetChanAmbColor(s32 channel,GXColor color)
 * \brief Sets the ambient color register for color channel \a chan.
 *
 * \details This color will be used by the channel as the ambient color if the ambient source, set by GX_SetChanCtrl(), is <tt>GX_SRC_REG</tt>.
 *
 * \param[in] channel channel to set
 * \param[in] color color to set it to
 *
 * \return none
 */
void GX_SetChanAmbColor(s32 channel,GXColor color);

/*!
 * \fn void GX_SetChanMatColor(s32 channel,GXColor color)
 * \brief Sets the material color register for color channel \a chan.
 *
 * \details This color will be used by the channel as the material color if the material source, set by GX_SetChanCtrl(), is <tt>GX_SRC_REG</tt>.
 *
 * \param[in] channel channel to set
 * \param[in] color color to set it to
 *
 * \return none
 */
void GX_SetChanMatColor(s32 channel,GXColor color);

/*!
 * \fn void GX_SetArray(u32 attr,void *ptr,u8 stride)
 * \brief Sets the array base pointer and stride for a single attribute.
 *
 * \details The array base and stride are used to compute the address of indexed attribute data using the equation:<br><br>
 *
 * &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<i>attr_addr</i> = \a ptr + <i>attr_idx</i> * \a stride
 *
 * When drawing a graphics primitive that has been enabled to use indexed attributes (see GX_SetVtxDesc()), <i>attr_idx</i> is supplied in the vertex
 * data. The format and size of the data in the array must also be described using GX_SetVtxAttrFmt(). You can also index other data, such as
 * matrices (see GX_LoadPosMtxIdx(), GX_LoadNrmMtxIdx3x3(), and GX_LoadTexMtxIdx()), and light objects (see GX_LoadLightObjIdx()). In the case
 * of matrices and light objects, the size and format of the data to be loaded is implied by the function call.
 *
 * There is a base pointer, \a ptr, for each type of attribute as well as for light data and matrices. Each attribute can be stored in its own
 * array for maximum data compression (i.e., removal of redundant attribute data). The \a stride is in byte units and is the distance between
 * attributes in the array.
 *
 * \note Indexed data is loaded into a vertex cache in the graphics processor. The vertex cache fetches 32B of data for each cache miss;
 * therefore, there is a small performance benefit to aligning attribute arrays to 32B, and possibly for arranging vertex data so that it
 * doesn't span 32B boundaries. Conveniently enough, memalign() returns 32-byte aligned pointers. For static data arrays, you can use the
 * ATTRIBUTE_ALIGN(32) attribute macro to align the \a ptr to 32B.
 *
 * \param[in] attr \ref vtxattr that the array is storing
 * \param[in] ptr pointer to the array itself
 * \param[in] stride stride (in bytes) between each element in the array
 *
 * \return none
 */
void GX_SetArray(u32 attr,void *ptr,u8 stride);

/*!
 * \fn void GX_SetVtxAttrFmt(u8 vtxfmt,u32 vtxattr,u32 comptype,u32 compsize,u32 frac)
 * \brief Sets the attribute format (\a vtxattr) for a single attribute in the Vertex Attribute Table (VAT).
 *
 * \details Each attribute format describes the data type (comptype), number of elements (compsize), and fixed point format (frac), if required. The
 * are eight vertex formats available in the VAT. The vertex format describes the format of all attributes in a vertex. The application can pre-program
 * all eight vertex formats and then select one of them during the actual drawing of geometry (See GX_Begin()). Note that all vertices used to draw a
 * particular graphics primitive will have the same format. The vertex format set using this function, along with the current vertex descriptor set
 * using GX_SetVtxDesc(), completely define the vertex data format.
 *
 * \note The vertex format allows data to be sent to the graphics processor in its most <i>quantized</i> format. The graphics hardware will <i>inverse-quantize</i>
 * the data (into floating point format) before it is used. The vertex attribute format is used to communicate the data quantization format to the hardware.<br><br>
 *
 * \note <tt>GX_VA_NRM</tt> and <tt>GX_VA_NBT</tt> attributes share the same type. Also, the frac for these attributes is fixed according to the type. The component count
 * (compsize) for <tt>GX_VA_NRM</tt> must be set to <tt>GX_NRM_XYZ</tt>. The component count for <tt>GX_VA_NBT</tt> must be set to <tt>GX_NRM_NBT</tt> or <tt>GX_NRM_NBT3</tt>.
 *
 * \param[in] vtxfmt \ref vtxfmt
 * \param[in] vtxattr \ref vtxattr
 * \param[in] comptype \ref comptype
 * \param[in] compsize \ref compsize
 * \param[in] frac number of fractional bits in a fixed-point number
 *
 * \return none
 */
void GX_SetVtxAttrFmt(u8 vtxfmt,u32 vtxattr,u32 comptype,u32 compsize,u32 frac);

/*!
 * \fn void GX_SetVtxAttrFmtv(u8 vtxfmt,GXVtxAttrFmt *attr_list)
 * \brief Sets multiple attribute formats within a single vertex format.
 *
 * \details This is useful when you need to set all the attributes in a vertex format at once (e.g., during graphics initialization).
 *
 * \note The constant <tt>GX_MAX_VTXATTRFMT_LISTSIZE</tt> should be used to allocate the list. You can get a current vertex format using GX_GetVtxAttrFmtv().
 *
 * \param[in] vtxfmt \ref vtxfmt
 * \param[in] attr_list pointer to array of GXVtxAttrFmt structs to draw from
 *
 * \return none
 */
void GX_SetVtxAttrFmtv(u8 vtxfmt,GXVtxAttrFmt *attr_list);

/*!
 * \fn void GX_SetVtxDesc(u8 attr,u8 type)
 * \brief Sets the \a type of a single attribute (\a attr) in the current vertex descriptor.
 *
 * \details The current vertex descriptor defines which attributes are present in a vertex and how each attribute is referenced. The current
 * vertex descriptor is used by the Graphics Processor (GP) to interpret the graphics command stream produced by the GX API. In particular,
 * the current vertex descriptor is used to parse the vertex data that is present in the command stream.
 *
 * \param[in] attr \ref vtxattr
 * \param[in] type \ref vtxattrin
 *
 * \return none
 */
void GX_SetVtxDesc(u8 attr,u8 type);

/*!
 * \fn void GX_SetVtxDescv(GXVtxDesc *attr_list)
 * \brief Sets the type of multiple attributes.
 *
 * \details This function is used when more than one attribute needs to be set (e.g., during initialization of geometry).
 *
 * \note The constant <tt>GX_MAX_VTXATTRFMT_LISTSIZE</tt> can be used to allocate memory for \a attr_list
 *
 * \param[in] attr_list array of pointers to GXVtxDesc structs; last element of the array should be <tt>GX_VA_NULL</tt>
 *
 * \return none
 */
void GX_SetVtxDescv(GXVtxDesc *attr_list);

/*!
 * \fn void GX_GetVtxDescv(GXVtxDesc *attr_list)
 * \brief Gets the type of multiple attributes.
 *
 * \details This function saves the attributes that are current set. This is usually used in conjunction with GX_SetVtxDescv
 *
 * \note The constant <tt>GX_MAX_VTXATTRFMT_LISTSIZE</tt> must be used to allocate memory for \a attr_list
 *
 * \param[in] attr_list array of pointers to GXVtxDesc structs
 *
 * \return none
 */
void GX_GetVtxDescv(GXVtxDesc *attr_list);

/*!
 * \fn u32 GX_EndDispList()
 * \brief Ends a display list and resumes writing graphics commands to the CPU FIFO.
 *
 * \details This function returns the size of the display list written to the display list buffer since GX_BeginDispList() was called. If
 * the display list size exceeds the size of the buffer allocated, a zero length size will be returned. The display list size is a
 * multiple of 32B and any unsed commands in the last 32B will be padded with <tt>GX_NOP</tt>. The size returned should be passed to
 * GX_CallDispList() when the display list needs to be executed.
 *
 * \note Due to the mechanics of flushing the write-gather pipe (which is used to create the display list), the display buffer should be
 * at least 32 bytes larger than the maximum expected amount of data stored. This function calls GX_Flush(), and thus it is not necessary
 * to call GX_Flush() explicitly after creating the display list.<br><br>
 *
 * \note A display list cannot be nested; i.e., no display list functions (GX_BeginDispList(), GX_EndDispList() and GX_CallDispList()) can
 * be called between a GX_BeginDispList() and GX_EndDispList() pair.<br><br>
 *
 * \note To execute a display list, use GX_CallDispList().
 *
 * \return 0 if display list size exceeds buffer, otherwise gives list size in bytes
 *
 * \bug Specifying a display list buffer size for GX_BeginDispList() the exact size that the display list will be (after padding) will cause
 * this function to return a very large (and very incorrect) value.
 */
u32 GX_EndDispList();

/*!
 * \fn void GX_Begin(u8 primitve,u8 vtxfmt,u16 vtxcnt)
 * \brief Begins drawing of a graphics primitive.
 *
 * \details To draw a graphics primitive, a stream of vertex data matching the description of both GX_SetVtxDesc() and GX_SetVtxAttrFmt() is
 * enclosed between GX_Begin()/GX_End() pairs. The number of vertices between GX_Begin() and GX_End() must match that specified by the \a vtxcnt
 * parameter. The type of the primitive will determine the minimum number of vertices required. For example, a <tt>GX_TRIANGLES</tt> primitive must
 * have at least 3 vertices.
 *
 * \note Primitives in which the vertex order is clockwise to the viewer are considered front-facing (for culling purposes).
 *
 * \param[in] primitve \ref primtype to draw
 * \param[in] vtxfmt \ref vtxfmt to use
 * \param[in] vtxcnt number of vertices being drawn; maximum is 65536
 */
void GX_Begin(u8 primitve,u8 vtxfmt,u16 vtxcnt);

/*!
 * \fn void GX_BeginDispList(void *list,u32 size)
 * \brief Begins a display list and disables writes to the FIFO currently attached to the CPU.
 *
 * \details After this function is called, GX API functions that normally send command or data into the CPU FIFO will send them to the
 * display list buffer instead of the FIFO until GX_EndDispList() is called. Writes to the CPU FIFO will be re-enabled when the function
 * GX_EndDispList() executes.
 *
 * Basically you can put most of GX API commands into a display list. However, since the use of display list can bypass all state
 * coherences controlled by GX API in run-time, sometimes it brings some state collisions or incoherences that may lead to unexpected
 * behavior or even graphics pipeline hang. The most recommended safe way is putting only primitives (regions enclosed by GX_Begin() and
 * GX_End()) that don't cause any state collisions.
 *
 * \note The application is expected to allocate the memory for the display list buffer. If the display list exceeds the maximum size
 * of the buffer, GX_EndDispList() will return 0. The address of the buffer must be 32-byte aligned; memalign() can return 32-byte-aligned
 * pointers. You can use the macro ATTRIBUTE_ALIGN(32) to align statically allocated buffers.<br><br>
 *
 * \note The CPU's write-gather pipe is used to write graphics commands to the display list. Therefore, the display list buffer must be
 * forced out of the CPU cache prior to being filled. DCInvalidateRange() may be used for this purpose. In addition, due to the mechanics
 * of flushing the write-gather pipe, the display list buffer should be at least 63 bytes larger than the maximum expected amount of data
 * stored.<br><br>
 *
 * \note A display list cannot be nested; i.e., no display list functions (GX_BeginDispList(), GX_EndDispList() and GX_CallDispList()) can
 * be called between a GX_BeginDispList() and GX_EndDispList() pair.<br><br>
 *
 * \note To execute a display list, use GX_CallDispList().
 *
 * \param[in] list 32-byte aligned buffer to hold the list
 * \param[in] size size of the buffer, multiple of 32
 *
 * \return none
 */
void GX_BeginDispList(void *list,u32 size);

/*!
 * \fn void GX_CallDispList(void *list,u32 nbytes)
 * \brief Causes the GP to execute graphics commands from the display \a list instead of from the GP FIFO.
 *
 * \details When the number of bytes specified by \a nbytes have been read, the graphics processor will resume executing
 * commands from the graphics FIFO. Graphics commands from a display list are prefetched into a separate 4KB FIFO. This prevents
 * any data prefetched for the main graphics command stream from being lost during the display list call.
 *
 * \note A display list cannot call another display list.<br><br>
 *
 * \note The display list must be padded to a length of 32B. All the data in the display list is interpreted by the graphics
 * processor, so any unused memory at the end of a display list should be set to GX_NOP. If you create the display list using
 * GX_BeginDispList()/GX_EndDispList(), this padding will be inserted automatically.
 *
 * \param[in] list 32-byte aligned pointer to the display list buffer
 * \param[in] nbytes number of bytes in the display list. Use the return value of GX_EndDispList() here.
 *
 * \return none
 */
void GX_CallDispList(void *list,u32 nbytes);

/*!
 * \fn static inline void GX_End()
 * \brief Used to end the drawing of a graphics primitive. This does nothing in libogc.
 *
 * \return none
 */
static inline void GX_End()
{
}

static inline void GX_Position3f32(f32 x,f32 y,f32 z)
{
	wgPipe->F32 = x;
	wgPipe->F32 = y;
	wgPipe->F32 = z;
}

static inline void GX_Position3u16(u16 x,u16 y,u16 z)
{
	wgPipe->U16 = x;
	wgPipe->U16 = y;
	wgPipe->U16 = z;
}

static inline void GX_Position3s16(s16 x,s16 y,s16 z)
{
	wgPipe->S16 = x;
	wgPipe->S16 = y;
	wgPipe->S16 = z;
}

static inline void GX_Position3u8(u8 x,u8 y,u8 z)
{
	wgPipe->U8 = x;
	wgPipe->U8 = y;
	wgPipe->U8 = z;
}

static inline void GX_Position3s8(s8 x,s8 y,s8 z)
{
	wgPipe->S8 = x;
	wgPipe->S8 = y;
	wgPipe->S8 = z;
}

static inline void GX_Position2f32(f32 x,f32 y)
{
	wgPipe->F32 = x;
	wgPipe->F32 = y;
}

static inline void GX_Position2u16(u16 x,u16 y)
{
	wgPipe->U16 = x;
	wgPipe->U16 = y;
}

static inline void GX_Position2s16(s16 x,s16 y)
{
	wgPipe->S16 = x;
	wgPipe->S16 = y;
}

static inline void GX_Position2u8(u8 x,u8 y)
{
	wgPipe->U8 = x;
	wgPipe->U8 = y;
}

static inline void GX_Position2s8(s8 x,s8 y)
{
	wgPipe->S8 = x;
	wgPipe->S8 = y;
}

static inline void GX_Position1x8(u8 index)
{
	wgPipe->U8 = index;
}

static inline void GX_Position1x16(u16 index)
{
	wgPipe->U16 = index;
}

static inline void GX_Normal3f32(f32 nx,f32 ny,f32 nz)
{
	wgPipe->F32 = nx;
	wgPipe->F32 = ny;
	wgPipe->F32 = nz;
}

static inline void GX_Normal3s16(s16 nx,s16 ny,s16 nz)
{
	wgPipe->S16 = nx;
	wgPipe->S16 = ny;
	wgPipe->S16 = nz;
}

static inline void GX_Normal3s8(s8 nx,s8 ny,s8 nz)
{
	wgPipe->S8 = nx;
	wgPipe->S8 = ny;
	wgPipe->S8 = nz;
}

static inline void GX_Normal1x8(u8 index)
{
	wgPipe->U8 = index;
}

static inline void GX_Normal1x16(u16 index)
{
	wgPipe->U16 = index;
}

static inline void GX_Color4u8(u8 r,u8 g,u8 b,u8 a)
{
	wgPipe->U8 = r;
	wgPipe->U8 = g;
	wgPipe->U8 = b;
	wgPipe->U8 = a;
}

static inline void GX_Color3u8(u8 r,u8 g,u8 b)
{
	wgPipe->U8 = r;
	wgPipe->U8 = g;
	wgPipe->U8 = b;
}

static inline void GX_Color3f32(f32 r, f32 g, f32 b)
{
	wgPipe->U8 = (u8)(r * 255.0);
	wgPipe->U8 = (u8)(g * 255.0);
	wgPipe->U8 = (u8)(b * 255.0);
}

static inline void GX_Color1u32(u32 clr)
{
	wgPipe->U32 = clr;
}

static inline void GX_Color1u16(u16 clr)
{
	wgPipe->U16 = clr;
}

static inline void GX_Color1x8(u8 index)
{
	wgPipe->U8 = index;
}

static inline void GX_Color1x16(u16 index)
{
	wgPipe->U16 = index;
}

static inline void GX_TexCoord2f32(f32 s,f32 t)
{
	wgPipe->F32 = s;
	wgPipe->F32 = t;
}

static inline void GX_TexCoord2u16(u16 s,u16 t)
{
	wgPipe->U16 = s;
	wgPipe->U16 = t;
}

static inline void GX_TexCoord2s16(s16 s,s16 t)
{
	wgPipe->S16 = s;
	wgPipe->S16 = t;
}

static inline void GX_TexCoord2u8(u8 s,u8 t)
{
	wgPipe->U8 = s;
	wgPipe->U8 = t;
}

static inline void GX_TexCoord2s8(s8 s,s8 t)
{
	wgPipe->S8 = s;
	wgPipe->S8 = t;
}

static inline void GX_TexCoord1f32(f32 s)
{
	wgPipe->F32 = s;
}

static inline void GX_TexCoord1u16(u16 s)
{
	wgPipe->U16 = s;
}

static inline void GX_TexCoord1s16(s16 s)
{
	wgPipe->S16 = s;
}

static inline void GX_TexCoord1u8(u8 s)
{
	wgPipe->U8 = s;
}

static inline void GX_TexCoord1s8(s8 s)
{
	wgPipe->S8 = s;
}

static inline void GX_TexCoord1x8(u8 index)
{
	wgPipe->U8 = index;
}

static inline void GX_TexCoord1x16(u16 index)
{
	wgPipe->U16 = index;
}

static inline void GX_MatrixIndex1x8(u8 index)
{
	wgPipe->U8 = index;
}

/*!
 * \fn void GX_AdjustForOverscan(GXRModeObj *rmin,GXRModeObj *rmout,u16 hor,u16 ver)
 * \brief Takes a given render mode and returns a version that is reduced in size to account for overscan.
 *
 * \details The number of pixels specified by \a hor is subtracted from each side of the screen, and the number of pixels specified
 * by \a ver is subtracted from both the top and the bottom of the screen. The active screen area is centered within what the render
 * mode specified before the adjustment.
 *
 * \note Due to the wide possibilities of how a render mode may be configured, this function may not work in all cases. For instance,
 * if you use Y-scaling to create a size difference between the EFB and XFB, this function may not do the right thing. In such cases,
 * you should configure the desired render mode manually (or else call this function and then fix up the results).
 *
 * \param[in] rmin rmode that is being copied
 * \param[in] rmout rmode to hold the adjusted version. Needs to be allocated but can be uninitialized.
 * \param[in] hor pixels to trim from each side of the screen
 * \param[in] ver pixels to tim from top and bottom of the screen
 *
 * \returns none
 */
void GX_AdjustForOverscan(GXRModeObj *rmin,GXRModeObj *rmout,u16 hor,u16 ver);

/*!
 * \fn void GX_LoadPosMtxImm(Mtx mt,u32 pnidx)
 * \brief Used to load a 3x4 modelview matrix \a mt into matrix memory at location \a pnidx.
 *
 * \details This matrix can be used to transform positions in model space to view space, either by making the matrix the current one (see
 * GX_SetCurrentMtx()), or by setting a matrix \a pnidx for each vertex. The parameter \a mt is a pointer to a 3x4 (row x column) matrix. The
 * parameter \a pnidx is used to refer to the matrix location (see \ref pnmtx) in matrix memory.
 *
 * You can also load a normal matrix (GX_LoadNrmMtxImm() or GX_LoadNrmMtxIdx3x3()) to the same \a pnidx. Generally, the normal matrix
 * will be the inverse transpose of the position matrix. The normal matrix is used during vertex lighting. In cases where the modelview
 * and inverse transpose of the modelview (excluding translation) are the same, you can load the <i>same</i> matrix for both position and normal
 * transforms.
 *
 * \note The matrix data is copied from DRAM through the CPU cache into the Graphics FIFO, so matrices loaded using this function are always
 * coherent with the CPU cache.
 *
 * \param[in] mt the matrix to load
 * \param[in] pnidx \ref pnmtx to load into
 *
 * \return none
 */
void GX_LoadPosMtxImm(Mtx mt,u32 pnidx);

/*!
 * \fn void GX_LoadPosMtxIdx(u16 mtxidx,u32 pnidx)
 * \brief Loads a 3x4 modelview matrix at index \a mtxidx from the array in main memory.
 *
 * \details The array is set by GX_SetArray(), and the matrix is loaded into matrix memory at index \a pnidx (see \ref pnmtx). This
 * modelview matrix is used to transform positions in model space to view space, either by making the matrix the current one (see
 * GX_SetCurrentMtx()) or by setting a matrix \a pnidx for each vertex (see GX_SetVtxDesc()). The matrix will be loaded through the vertex cache.
 *
 * You can also load a normal matrix (GX_LoadNrmMtxImm() or GX_LoadNrmMtxIdx3x3()) to the same \a pnidx. Generally, the normal matrix
 * will be the inverse transpose of the position matrix. The normal matrix is used during vertex lighting. In cases where the modelview
 * and inverse transpose of the modelview (excluding translation) are the same, you can load the <i>same</i> matrix for both position and normal
 * transforms. Since indexed matrix loads are through the vertex cache, you will only incur the main memory bandwidth load of one matrix load.
 *
 * \note The matrix is loaded directly from main memory into the matrix memory thrugh the vertex cache, so it is incoherent with the CPU's cache.
 * It is the application's responsibility to flush any matrix data from the CPU cache (see DCStoreRange()) before calling this function.
 *
 * \param[in] mtxidx index to the matrix array to load
 * \param[in] pnidx \ref pnmtx to load into
 *
 * \return none
 */
void GX_LoadPosMtxIdx(u16 mtxidx,u32 pnidx);

/*!
 * \fn void GX_LoadNrmMtxImm(Mtx mt,u32 pnidx)
 * \brief Used to load a normal transform matrix into matrix memory at location \a pnidx from the 4x3 matrix \a mt.
 *
 * \details This matrix is used to transform normals in model space to view space, either by making it the current matrix (see GX_SetCurrentMtx()),
 * or by setting a matrix pnidx for each vertex. The parameter \a mt is a pointer to a 3x4 (row x column) matrix. The translation terms
 * in the 3x4 matrix are not needed for normal rotation and are ignored during the load. The parameter \a pnidx is used to refer to the
 * matrix location (see \ref pnmtx) in matrix memory.
 *
 * \note You can also load a position matrix (GX_LoadPosMtxImm()) to the same \a pnidx. Normally, the normal matrix will be the inverse transpose of
 * the position (modelview) matrix and is used during vertex lighting. In cases where the modelview and the inverse transpose of the modelview
 * matrix (excluding translation) are the same, the same matrix can be loaded for both normal and position matrices.<br><br>
 *
 * \note The matrix data is copied from main memory or the CPU cache into the Graphics FIFO, so matrices loaded by this function are always coherent
 * with the CPU cache.<br><br>
 *
 * \param[in] mt the matrix to load
 * \param[in] pnidx \ref pnmtx to load into
 *
 * \return none
 */
void GX_LoadNrmMtxImm(Mtx mt,u32 pnidx);

/*!
 * \fn void GX_LoadNrmMtxIdx3x3(u16 mtxidx,u32 pnidx)
 * \brief Loads a 3x3 normal matrix into matrix memory at location \a pnidx from a 3x3 matrix located at index \a mtxidx
 * from the array in main memory.
 *
 * \details The array is set by set by GX_SetArray(), and the matrix is loaded into matrix memory at index \a pnidx. This matrix can be used to
 * transform normals in model space to view space, either by making the matrix the current one (see GX_SetCurrentMtx()), or by setting a
 * matrix \a pnidx for each vertex (see GX_SetVtxDesc()). The matrix will be loaded through the vertex cache. You can also load a position
 * matrix (GX_LoadPosMtxImm() or GX_LoadPosMtxIdx()) to the same \a pnidx.
 *
 * \note You cannot use an indexed load to load a 3x3 matrix from an indexed 3x4 matrix in main memory; you must use GX_LoadNrmMtxImm() for
 * this case.<br><br>
 *
 * \note The matrix is loaded directly from main memory into the matrix memory through the vertex cache, therefore it is incoherent with the
 * CPU's cache. It is the application's responsibility to flush any matrix data from the CPU cache (see DCStoreRange()) before calling this
 * function.
 *
 * \param[in] mtxidx index to the matrix array to load
 * \param[in] pnidx \ref pnmtx to load into
 *
 * \return none
 */
void GX_LoadNrmMtxIdx3x3(u16 mtxidx,u32 pnidx);

/*!
 * \fn void GX_LoadTexMtxImm(Mtx mt,u32 texidx,u8 type)
 * \brief Loads a texture matrix \a mt into the matrix memory at location \a texidx.
 *
 * \details The matrix loaded can be either a 2x4 or 3x4 matrix as indicated by \a type. You can use the loaded matrix to
 * transform texture coordinates, or to generate texture coordinates from position or normal vectors. Such generated texture
 * coordinates are used for projected textures, reflection mapping, etc. See GX_SetTexCoordGen() for more details.
 *
 * Texture matrices can be either 2x4 or 3x4. <tt>GX_MTX_2x4</tt> matrices can be used for simple translations and/or rotations of texture
 * coordinates. <tt>GX_MTX_3x4</tt> matrices are used when projection is required.
 *
 * \note The default matrix memory configuration allows for ten (3x4 or 2x4) texture matrices, and a 3x4 identity matrix. The <tt>GX_IDENTITY</tt>
 * matrix is preloaded by GX_Init().<br><br>
 *
 * \note This function allows one to load post-transform texture matrices as well. Specifying a texidx in the range of \ref dttmtx will load a
 * post-transform texture matrix instead of a regular, first-pass texture matrix. Note that post-transform matrices are always 3x4. Refer to
 * GX_SetTexCoordGen2() for information about how to use post-transform texture matrices.
 *
 * \param[in] mt the matrix to load
 * \param[in] texidx \ref texmtx
 * \param[in] type \ref mtxtype
 *
 * \return none
 */
void GX_LoadTexMtxImm(Mtx mt,u32 texidx,u8 type);

/*!
 * \fn void GX_LoadTexMtxIdx(u16 mtxidx,u32 texidx,u8 type)
 * \brief Loads a texture matrix at index \a mtxidx from the array in main memory
 *
 * \details The array is set by GX_SetArray(), and the matrix is loaded into matrix memory at location \a texid. The loaded matrix can be
 * either 2x4 or 3x4 as indicated by \a type. This matrix can be used to generate texture coordinates from positions, normals, and input
 * texture coordinates (see GX_SetTexCoordGen()). The matrix is loaded through the vertex cache. The size of the matrix to load is indicated by its
 * \a type. Texture matrices can be either 2x4 or 3x4. <tt>GX_MTX_2x4</tt> matrices can be used for simple translations and/or rotations of texture
 * coordinates; <tt>GX_MTX_3x4</tt> matrices are used when projection is required.
 *
 * \note The matrix is loaded directly from main memory into the matrix memory through the vertex cache, so it is incoherent with the CPU's
 * cache. It is the application's responsibility to flush any matrix data from the CPU cache (see DCStoreRange()) before calling this function.<br><br>
 *
 * \note This function allows one to load post-transform texture matrices as well. Specifying a \a texidx in the range of \ref dttmtx will load a
 * post-transform texture matrix instead of a regular, first-pass texture matrix. Note that post-transform matrices are always 3x4. Refer to
 * GX_SetTexCoordGen2() for information about how to use post-transform texture matrices.
 *
 * \param[in] mtxidx index to the matrix array to load
 * \param[in] texidx \ref texmtx
 * \param[in] type \ref mtxtype
 *
 * \return none
 */
void GX_LoadTexMtxIdx(u16 mtxidx,u32 texidx,u8 type);

/*!
 * \fn void GX_SetCurrentMtx(u32 mtx)
 * \brief Selects a specific matrix to use for transformations.
 *
 * \details The matrix \a mtx specified will be used to select the current modelview transform matrix and normal transform matrix,
 * as long as a matrix index is not present in the vertex data (see GX_SetVtxDesc()). If the current vertex descriptor enables <tt>GX_VA_PTNMTXIDX</tt>,
 * the matrix \a mtx specified by this function will be overwritten when the vertices are drawn.
 *
 * \param[in] mtx \ref pnmtx
 *
 * \return none
 */
void GX_SetCurrentMtx(u32 mtx);

/*!
 * \fn void GX_SetTevOp(u8 tevstage,u8 mode)
 * \brief Simplified function to set various TEV parameters for this \a tevstage based on a predefined combiner \a mode.
 *
 * \details This is a convenience function designed to make initial programming of the Texture Environment unit (TEV) easier. This function calls
 * GX_SetTevColorIn(), GX_SetTevColorOp(), GX_SetTevAlphaIn() and GX_SetTevAlphaOp() with predefined arguments to implement familiar texture
 * combining functions.
 *
 * \note To enable a consecutive set of TEV stages, you must call GX_SetNumTevStages().
 *
 * \param[in] tevstage \ref tevstage.
 * \param[in] mode \ref tevdefmode
 *
 * \return none
 */
void GX_SetTevOp(u8 tevstage,u8 mode);

/*!
 * \fn void GX_SetTevColor(u8 tev_regid,GXColor color)
 * \brief Used to set one of the primary color registers in the TEV unit.
 *
 * \details These registers are available to all TEV stages. At least one of these registers is used to pass the output of one TEV stage to
 * the next in a multi-texture configuration. The application is responsible for allocating these registers so that no collisions in usage occur.
 *
 * \note This function can only set unsigned 8-bit colors. To set signed, 10-bit colors use GX_SetTevColorS10().
 *
 * \param[in] tev_regid \ref tevcoloutreg.
 * \param[in] color Constant color value.
 *
 * \return none
 */
void GX_SetTevColor(u8 tev_regid,GXColor color);

/*!
 * \fn void GX_SetTevColorS10(u8 tev_regid,GXColorS10 color)
 * \brief Used to set one of the constant color registers in the TEV unit.
 *
 * \details These registers are available to all TEV stages. At least one of these registers is used to pass the output of one TEV stage to the
 * next in a multi-texture configuration. The application is responsible for allocating these registers so that no collisions in usage occur.
 *
 * \note This function enables the color components to be signed 10-bit numbers. To set 8-bit unsigned colors (the common case), use GX_SetTevColor().
 *
 * \param[in] tev_regid \ref tevcoloutreg.
 * \param[in] color Constant color value in S10 format.
 *
 * \return none
 */
void GX_SetTevColorS10(u8 tev_regid,GXColorS10 color);

/*!
 * \fn void GX_SetTevColorIn(u8 tevstage,u8 a,u8 b,u8 c,u8 d)
 * \brief Sets the color input sources for one \a tevstage of the Texture Environment (TEV) color combiner.
 *
 * \details This includes constant (register) colors and alphas, texture color/alpha, rasterized color/alpha (the result of per-vertex lighting),
 * and a few useful constants.
 *
 * \note The input controls are independent for each TEV stage.
 *
 * \param[in] tevstage \ref tevstage
 * \param[in] a \ref tevcolorarg
 * \param[in] b \ref tevcolorarg
 * \param[in] c \ref tevcolorarg
 * \param[in] d \ref tevcolorarg
 *
 * \return none
 */
void GX_SetTevColorIn(u8 tevstage,u8 a,u8 b,u8 c,u8 d);

/*!
 * \fn void GX_SetTevAlphaIn(u8 tevstage,u8 a,u8 b,u8 c,u8 d)
 * \brief Sets the alpha input sources for one \a tevstage of the Texture Environment (TEV) alpha combiner.
 *
 * \details There are fewer alpha inputs than color inputs, and there are no color channels available in the alpha combiner.
 *
 * \note The input controls are independent for each TEV stage.
 *
 * \param[in] tevstage \ref tevstage
 * \param[in] a \ref tevalphaarg
 * \param[in] b \ref tevalphaarg
 * \param[in] c \ref tevalphaarg
 * \param[in] d \ref tevalphaarg
 *
 * \return none
 */
void GX_SetTevAlphaIn(u8 tevstage,u8 a,u8 b,u8 c,u8 d);

/*!
 * \fn void GX_SetTevColorOp(u8 tevstage,u8 tevop,u8 tevbias,u8 tevscale,u8 clamp,u8 tevregid)
 * \brief Sets the \a tevop, \a tevbias, \a tevscale and \a clamp-mode operation for the color combiner
 *        for this \a tevstage of the TEV unit.
 *
 * \details This function also specifies the register, \a tevregid, that will contain the result of the color combiner function. The color
 * combiner function is:<br><br>
 *
 *		  &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;\a tevregid = (d (\a tevop) ((1.0 - c)*a + c*b) + \a tevbias) * \a tevscale;<br><br>
 *
 *
 * The input sources a,b,c and d are set using GX_SetTevColorIn().
 *
 * \param[in] tevstage \ref tevstage.
 * \param[in] tevop \ref tevop
 * \param[in] tevbias \ref tevbias.
 * \param[in] tevscale \ref tevscale.
 * \param[in] clamp Clamp results when <tt>GX_TRUE</tt>.
 * \param[in] tevregid \ref tevcoloutreg
 *
 * \return none
 */
void GX_SetTevColorOp(u8 tevstage,u8 tevop,u8 tevbias,u8 tevscale,u8 clamp,u8 tevregid);

/*!
 * \fn void GX_SetTevAlphaOp(u8 tevstage,u8 tevop,u8 tevbias,u8 tevscale,u8 clamp,u8 tevregid)
 * \brief Sets the \a tevop, \a tevbias, \a tevscale and \a clamp-mode operation for the alpha combiner
 *        for this \a tevstage of the TEV unit.
 *
 * \details This function also specifies the register, \a tevregid, that will contain the result of the alpha combiner function. The alpha
 * combiner function is:<br><br>
 *
 *        &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;\a tevregid = (d (\a tevop) ((1.0 - c)*a + c*b) + \a tevbias) * \a tevscale;<br><br>
 *
 * The input sources a,b,c and d are set using GX_SetTevAlphaIn().
 *
 * \param[in] tevstage \ref tevstage.
 * \param[in] tevop \ref tevop
 * \param[in] tevbias \ref tevbias.
 * \param[in] tevscale \ref tevscale.
 * \param[in] clamp Clamp results when <tt>GX_TRUE</tt>.
 * \param[in] tevregid \ref tevcoloutreg
 *
 * \return none
 */
void GX_SetTevAlphaOp(u8 tevstage,u8 tevop,u8 tevbias,u8 tevscale,u8 clamp,u8 tevregid);

/*!
 * \fn void GX_SetNumTexGens(u32 nr)
 * \brief Sets the number of texture coordinates that are generated and available for use in the Texture Environment TEV stages.
 *
 * \details Texture coordinates are generated from input data as described by GX_SetTexCoordGen(). The generated texture coordinates are linked to
 * specific textures and specific TEV stages using GX_SetTevOrder().
 *
 * \note A consecutive number of texture coordinates may be generated, starting at <tt>GX_TEXCOORD0</tt>. A maximum of 8 texture coordinates may be generated.
 * If \a nr is set to 0, no texture coordinates will be generated. In this case, at least one color channel must be output (see GX_SetNumChans()).
 *
 * \param[in] nr number of tex coords to generate, between 0 and 8 inclusive
 *
 * \return none
 */
void GX_SetNumTexGens(u32 nr);

/*!
 * \fn void GX_SetTexCoordGen(u16 texcoord,u32 tgen_typ,u32 tgen_src,u32 mtxsrc)
 * \brief Specifies how texture coordinates are generated.
 *
 * \details Output texture coordinates are usually the result of some transform of an input attribute; either position, normal, or texture coordinate.
 * You can also generate texture coordinates from the output color channel of the per-vertex lighting calculations. In C-language syntax the texture
 * coordinate generation function would look like this:<br><br>
 *
 * &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;\a texcoord = \a tgen_typ(\a tgen_src, \a mtxsrc);<br><br>
 *
 * The current vertex descriptor as set by GX_SetVtxDesc() only describes the data input to the graphics processor. Using this function, you can create
 * new output texture coordinates from the input data. The texcoord parameter is used to give the output texture coordinate a name. This texture
 * coordinate can be bound to a texture using GX_SetTevOrder(). GX_SetNumTexGens() specifies a consecutive number of texture coordinates, starting at
 * <tt>GX_TEXCOORD0</tt>, that are available to GX_SetTevOrder().
 *
 * \ref texmtx defines a default set of texture matrix names that can be supplied as mtxsrc. The matrix names are actually row addresses (4 floats per
 * row) in the matrix memory that indicate the first row of a loaded matrix. The user may define another memory map of matrix memory to suit their
 * needs. Keep in mind, however, that modelview matrices (see GX_LoadPosMtxImm() and \ref pnmtx) and texture matrices share matrix memory.
 *
 * \note Input texture coordinates must always go through the texture coordinate generation hardware. GX_Init() initializes the hardware (by calling
 * this function) so that all texture coordinates are transformed by the <tt>GX_IDENTITY</tt> matrix in order to appear as if input coordinates are passed
 * unchanged through to the texture hardware.
 *
 * There are 8 output texture coordinates that can be referenced in any of the 16 TEV stages. There are a maximum of 8 input texture coordinates.
 *
 * \param[in] texcoord \ref texcoordid
 * \param[in] tgen_typ \ref texgentyp
 * \param[in] tgen_src \ref texgensrc
 * \param[in] mtxsrc \ref texmtx
 *
 * \return none
 */
void GX_SetTexCoordGen(u16 texcoord,u32 tgen_typ,u32 tgen_src,u32 mtxsrc);

/*!
 * \fn void GX_SetTexCoordGen2(u16 texcoord,u32 tgen_typ,u32 tgen_src,u32 mtxsrc,u32 normalize,u32 postmtx)
 * \brief An extension of GX_SetTexCoordGen(). It allows one to specify additional texgen options.
 *
 * \details The first four arguments are identical to those for GX_SetTexCoordGen() and function in the same way. All requirements for the first
 * four arguments are the same as they are for that function as well. The new options only apply for "ordinary" texgens, where the texgen type is
 * <tt>GX_TG_MTX2x4</tt> or <tt>GX_TG_MTX3x4</tt>. They do not work for light-based texgens or emboss texgens.
 *
 * The \a normalize argument allows the computed texcoord to be normalized after the multiplication by \a mtxsrc (the first-pass transformation).
 * After the optional normalization step, the texcoord is then multiplied by the 3x4 matrix \a postmtx. This matrix is refered to as the
 * post-transform matrix.
 *
 * The result of this step is the texture coordinate that is used to look up the texture.
 *
 * \note The post-transform matrices are separate from the first pass matrices. They are stored in a separate memory area in the same format as the
 * first pass matrices, except that all matrices have three rows.<br><br>
 *
 * \note When a vertex contains only position and one texture coordinate and the texgen type is <tt>GX_TG_MTX2x4</tt>, there are certain limitations. In
 * this special performance case, normalization is not performed (even if specified).
 *
 * \param[in] texcoord \ref texcoordid
 * \param[in] tgen_typ \ref texgentyp
 * \param[in] tgen_src \ref texgensrc
 * \param[in] mtxsrc \ref texmtx
 * \param[in] normalize if <tt>GX_TRUE</tt>, normalize tex coord after first-pass transform. Only used with <tt>GX_TG_MTX*</tt>.
 * \param[in] postmtx \ref dttmtx
 *
 * \return none
 */
void GX_SetTexCoordGen2(u16 texcoord,u32 tgen_typ,u32 tgen_src,u32 mtxsrc,u32 normalize,u32 postmtx);

/*!
 * \fn void GX_SetZTexture(u8 op,u8 fmt,u32 bias)
 * \brief Controls Z texture operations.
 *
 * \details Z textures can be used to implement image-based rendering algorithms. A composite image consisting of color and depth image planes can
 * be merged into the Embedded Frame Buffer (EFB).
 *
 * Normally, the Z for a quad (2x2) of pixels is computed as a reference Z and two slopes. Once Z texturing is enabled, the Z is computed by adding
 * a Z texel to the reference Z (\a op = <tt>GX_ZT_ADD</tt>) or by replacing the reference Z with the Z texel value (\a op = <tt>GX_ZT_REPLACE</tt>).
 *
 * Z textures are always the output from the last active Texture Environment (TEV) stage (see GX_SetNumTevStages()) when enabled. When Z texturing is
 * enabled, the texture color of the last TEV stage is not available, but all other color inputs and operations are available. The pixel color is
 * always output from the last active TEV stage. You cannot use the TEV to operate on the Z texture, it is fed directly into the Z texture logic.
 *
 * Z texel formats can be unsigned 8-bit (<tt>GX_TF_Z8</tt>), 16-bit (<tt>GX_TF_Z16</tt>), or 24-bit (<tt>GX_TF_Z24X8</tt> (32-bit texture)) are used. The Graphics Processor
 * converts the Z-textures to 24-bit values by placing the texel value in the least-significant bits and inserting zero's in the remaining
 * most-significant bits. The 24-bit constant \a bias is added to the Z texture. If the pixel format is GX_PF_RGB565_Z16 the 24-bit result is converted
 * to the current 16-bit Z format before comparing with the EFB's Z.
 *
 * \note The Z-texture calculation is done before the fog range calculation.<br><br>
 *
 * \note GX_Init() disables Z texturing.
 *
 * \param[in] op \ref ztexop to perform
 * \param[in] fmt \ref ztexfmt to use
 * \param[in] bias Bias value. Format is 24bit unsigned.
 *
 * \return none
 */
void GX_SetZTexture(u8 op,u8 fmt,u32 bias);

/*!
 * \fn void GX_SetZMode(u8 enable,u8 func,u8 update_enable)
 * \brief Sets the Z-buffer compare mode.
 *
 * \details The result of the Z compare is used to conditionally write color values to the Embedded Frame Buffer (EFB).
 *
 * When \a enable is set to <tt>GX_DISABLE</tt>, Z buffering is disabled and the Z buffer is not updated.
 *
 * The \a func parameter determines the comparison that is performed. In the comparison function, the newly rasterized Z value is on the left
 * while the Z value from the Z buffer is on the right. If the result of the comparison is false, the newly rasterized pixel is discarded.
 *
 * The parameter \a update_enable determines whether or not the Z buffer is updated with the new Z value after a comparison is performed. This
 * parameter also affects whether the Z buffer is cleared during copy operations (see GX_CopyDisp() and GX_CopyTex()).
 *
 * \param[in] enable Enables comparisons with source and destination Z values if <tt>GX_TRUE</tt>; disables compares otherwise.
 * \param[in] func \ref compare
 * \param[in] update_enable Enables Z-buffer updates when <tt>GX_TRUE</tt>; otherwise, Z-buffer updates are disabled, but compares may still be enabled.
 *
 * \return none
 */
void GX_SetZMode(u8 enable,u8 func,u8 update_enable);

/*!
 * \fn void GX_SetZCompLoc(u8 before_tex)
 * \brief Sets whether Z buffering happens before or after texturing.
 *
 * \details Normally, Z buffering should happen before texturing, as this enables better performance by not texturing pixels that are not
 * visible; however, when alpha compare is used, Z buffering must be done after texturing (see GX_SetAlphaCompare()).
 *
 * \param[in] before_tex Enables Z-buffering before texturing when set to <tt>GX_TRUE</tt>; otherwise, Z-buffering takes place after texturing.
 *
 * \return none
 */
void GX_SetZCompLoc(u8 before_tex);

/*!
 * \fn void GX_SetLineWidth(u8 width,u8 fmt)
 * \brief Sets the width of line primitives.
 *
 * The parameter \a fmt is added to the texture coordinate to obtain texture coordinates at the other corners of a wide line. The \a fmt
 * values are added after the texture coordinate generation operation; see GX_SetTexCoordGen().
 *
 * \param[in] width width of the line in 1/16th pixel increments; maximum width is 42.5 px
 * \param[in] fmt \ref texoff
 *
 * \return none
 */
void GX_SetLineWidth(u8 width,u8 fmt);

/*!
 * \fn void GX_SetPointSize(u8 width,u8 fmt)
 * \brief Sets the size of point primitives.
 *
 * \details The parameter \a fmt is added to the texture coordinate(s), if any, to obtain texture coordinates at the other corners of a point. The
 * \a fmts are added after the texture coordinate generation operation; see GX_SetTexCoordGen().
 *
 * \param[in] width width of the point in 1/16th pixel increments; maximum width is 42.5 px
 * \param[in] fmt \ref texoff
 *
 * \return none
 */
void GX_SetPointSize(u8 width,u8 fmt);

/*!
 * \fn void GX_SetBlendMode(u8 type,u8 src_fact,u8 dst_fact,u8 op)
 * \brief Determines how the source image, generated by the graphics processor, is blended with the Embedded Frame Buffer (EFB).
 *
 * \details When \a type is set to <tt>GX_BM_NONE</tt>, the source data is written directly to the EFB. When \a type is set to <tt>GX_BM_BLEND</tt>, the source color and EFB
 * pixels are blended using the following equation:
 *
 * &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<i>dst_pix_clr</i> = <i>src_pix_clr</i> * \a src_fact + <i>dst_pix_clr</i> * \a dst_fact
 *
 * The <tt>GX_BL_DSTALPHA</tt> / <tt>GX_BL_INVDSTALPHA</tt> can be used only when the EFB has <tt>GX_PF_RGBA6_Z24</tt> as the pixel format (see GX_SetPixelFmt()). If the pixel
 * format is <tt>GX_PF_RGBA6_Z24</tt> then the \a src_fact and \a dst_fact are also applied to the alpha channel. To write the alpha channel to the EFB you must
 * call GX_SetAlphaUpdate().
 *
 * When type is set to <tt>GX_BM_LOGIC</tt>, the source and EFB pixels are blended using logical bitwise operations. When type is set to <tt>GX_BM_SUBTRACT</tt>, the destination
 * pixel is computed as follows:
 *
 * &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<i>dst_pix_clr</i> = <i>dst_pix_clr</i> - <i>src_pix_clr [clamped to zero]</i>
 *
 * Note that \a src_fact and \a dst_fact are not part of this equation.
 *
 * \note Color updates should be enabled by calling GX_SetColorUpdate().
 *
 * \param[in] type \ref blendmode
 * \param[in] src_fact \ref blendfactor
 * \param[in] dst_fact \ref blendfactor
 * \param[in] op \ref logicop
 *
 * \return none
 */
void GX_SetBlendMode(u8 type,u8 src_fact,u8 dst_fact,u8 op);

/*!
 * \fn void GX_SetCullMode(u8 mode)
 * \brief Enables or disables culling of geometry based on its orientation to the viewer.
 *
 * \details Primitives in which the vertex order is clockwise to the viewer are considered front-facing.
 *
 * \note GX_Init() sets this to <tt>GX_CULL_BACK</tt>.
 *
 * \param[in] mode \ref cullmode
 *
 * \return none
 */
void GX_SetCullMode(u8 mode);

/*!
 * \fn void GX_SetCoPlanar(u8 enable)
 * \brief Enables or disables coplanar triangle processing.
 *
 * \details While coplanar mode is enabled, all subsequent triangles (called decal triangles) will have the same Z coefficients as the reference
 * plane. Coplanar mode should be enabled immediately after a reference triangle is drawn.
 *
 * \note The reference triangle can be culled using <tt>GX_SetCullMode(GX_CULL_ALL)</tt> to create an invisible reference plane; however, the reference
 * triangle must not be completely out of view, i.e. trivially rejected by clipping.<br><br>
 *
 * \note GX_Init() disables coplanar mode.
 *
 * \param[in] enable when <tt>GX_ENABLE</tt>, coplanar mode is enabled; <tt>GX_DISABLE</tt> disables this mode
 *
 * \return none
 */
void GX_SetCoPlanar(u8 enable);

/*!
 * \fn void GX_EnableTexOffsets(u8 coord,u8 line_enable,u8 point_enable)
 * \brief Enables a special texture offset feature for points and lines.
 *
 * \details When a point's size is defined using GX_SetPointSize() or a line's width is described using GX_SetLineWidth(), you can also specify a second
 * parameter. The parameter \a fmt is added to the texture coordinate(s), if any, to obtain texture coordinates at the other corners of a
 * point or line. The \a fmts are added after the texture coordinate generation operation; see GX_SetTexCoordGen(). This function enables this
 * operation for a particular texture coordinate. Offset operations for points and lines are enabled separately. If the enables are false, the same
 * texture coordinate is used for every vertex of the line or point.
 *
 * \param[in] coord \ref texcoordid
 * \param[in] line_enable enable or disable tex offset calculation for lines
 * \param[in] point_enable enable or disable tex offset calculation for points
 *
 * \return none
 */
void GX_EnableTexOffsets(u8 coord,u8 line_enable,u8 point_enable);

/*!
 * \fn void GX_SetClipMode(u8 mode)
 * \brief Enables or disables clipping of geometry.
 *
 * \details This may help performance issues that occur when objects are far-clipped; however, any near-clipped objects will be rendered incorrectly.
 *
 * \note GX_Init() sets this to <tt>GX_CLIP_ENABLE</tt>.
 *
 * \param[in] mode \ref clipmode
 *
 * \return none
 */
void GX_SetClipMode(u8 mode);

/*!
 * \fn void GX_SetScissor(u32 xOrigin,u32 yOrigin,u32 wd,u32 ht)
 * \brief Sets the scissor rectangle.
 *
 * \details The scissor rectangle specifies an area of the screen outside of which all primitives are culled. This function sets the scissor rectangle in
 * screen coordinates. The screen origin (\a xOrigin=0, \a yOrigin=0) is at the top left corner of the display.
 *
 * The values may be within the range of 0 - 2047 inclusive. Using values that extend beyond the EFB size is allowable since the scissor box may be
 * repositioned within the EFB using GX_SetScissorBoxOffset().
 *
 * \note By default, the scissor rectangle is set to match the viewport rectangle. GX_Init() initializes the scissor rectangle to match the viewport
 * given the current render mode.
 *
 * \param[in] xOrigin left-most coord in screen coordinates
 * \param[in] yOrigin top-most coord in screen coordinates
 * \param[in] wd width of the scissorbox in screen coordinates
 * \param[in] ht height of the scissorbox in screen coordinates
 *
 * \return none
 */
void GX_SetScissor(u32 xOrigin,u32 yOrigin,u32 wd,u32 ht);

/*!
 * \fn void GX_SetScissorBoxOffset(s32 xoffset,s32 yoffset)
 * \brief Repositions the scissorbox rectangle within the Embedded Frame Buffer (EFB) memory space.
 *
 * \details The offsets are subtracted from the screen coordinates to determine the actual EFB coordinates where the pixels are stored. Thus with
 * positive offsets, the scissor box may be shifted left and/or up; and with negative offsets, the scissor box may be shifted right and/or down.
 *
 * The intended use for this command is to make it easy to do two-pass antialiased rendering. To draw the top half of the screen, the scissor box is set to
 * the top and the offset set to zero. To draw the bottom half, the scissor box is set to the bottom, and the offset is set to shift the scissor box back up
 * to the top.
 *
 * Another use for the offset is to displace how an image is rendered with respect to the dither matrix. Since the dither matrix is 4x4, a \a yoffset of -2
 * shifts the image down by 2 lines with respect to the matrix. This can be useful for field-rendering mode.
 *
 * \note Achieving an offset of an odd number of lines is possible, but more difficult than just changing the scissor box: one must render and copy 2
 * additional lines, then skip one by adjusting the argument of VIDEO_SetNextFrameBuffer().<br><br>
 *
 * \note GX_Init() initializes the scissor box offset to zero. Since the GP works on 2x2 regions of pixels, only even offsets are allowed.
 *
 * \param[in] xoffset number of pixels to shift the scissorbox left, between -342 - 382 inclusive; must be even
 * \param[in] yoffset number of pixels to shift the scissorbox up, between -342 - 494 inclusive; must be even
 *
 * \return none
 */
void GX_SetScissorBoxOffset(s32 xoffset,s32 yoffset);

/*!
 * \fn void GX_SetNumChans(u8 num)
 * \brief Sets the number of color channels that are output to the TEV stages.
 *
 * \details Color channels are the mechanism used to compute per-vertex lighting effects. Color channels are controlled using GX_SetChanCtrl().
 * Color channels are linked to specific TEV stages using GX_SetTevOrder().
 *
 * This function basically defines the number of per-vertex colors that get rasterized. If \a num is set to 0, then at least one texture coordinate
 * must be generated (see GX_SetNumTexGens()). If \a num is set to 1, then channel <tt>GX_COLOR0A0</tt> will be rasterized. If \a num is set to 2 (the maximum
 * value), then <tt>GX_COLOR0A0</tt> and <tt>GX_COLOR1A1</tt> will be rasterized.
 *
 * \param[in] num number of color channels to rasterize; number must be 0, 1 or 2
 *
 * \return none
 */
void GX_SetNumChans(u8 num);

/*!
 * \fn void GX_SetTevOrder(u8 tevstage,u8 texcoord,u32 texmap,u8 color)
 * \brief Specifies the texture and rasterized color that will be available as inputs to this TEV \a tevstage.
 *
 * The texture coordinate \a texcoord is generated from input attributes using the GX_SetTexCoordGen() function and is used to look up the
 * texture map, previously loaded by GX_LoadTexObj(). The \a color to rasterize for this \a tevstage is also specified. The color
 * is the result of per-vertex lighting which is controlled by GX_SetChanCtrl().
 *
 * This function will scale the normalized texture coordinates produced by GX_SetTexCoordGen() according to the size of the texture map in the
 * function call. For this reason, texture coordinates can only be broadcast to multiple texture maps if and only if the maps are the same size. In
 * some cases, you may want to generate a texture coordinate having a certain scale, but disable the texture lookup (this comes up when generating
 * texture coordinates for indirect bump mapping). To accomplish this, use the <tt>GX_TEXMAP_DISABLE</tt> flag:
 *
 * \code GX_SetTevOrder(GX_TEVSTAGE1, GX_TEXCOORD0, GX_TEXMAP3 | GX_TEXMAP_DISABLE, GX_COLORNULL); \endcode
 *
 * \details This will scale <tt>GX_TEXCOORD0</tt> using <tt>GX_TEXMAP3</tt> but disable the lookup of <tt>GX_TEXMAP3</tt>.
 *
 * \note This function does not enable the TEV stage. To enable a consecutive number of TEV stages, starting at stage <tt>GX_TEVSTAGE0</tt>, use GX_SetNumTevStages().<br><br>
 *
 * \note The operation of each TEV stage is independent. The color operations are controlled by GX_SetTevColorIn() and GX_SetTevColorOp(). The alpha
 * operations are controlled by GX_SetTevAlphaIn() and GX_SetTevAlphaOp().<br><br>
 *
 * \note The number of texture coordinates available for all the active TEV stages is set using GX_SetNumTexGens(). The number of color channels
 * available for all the active TEV stages is set using GX_SetNumChans(). Active TEV stages should not reference more texture coordinates or colors
 * than are being generated.<br><br>
 *
 * \note There are some special settings for the \a color argument. If you specify <tt>GX_COLOR_ZERO</tt>, you always get zero as rasterized color. If you specify
 * <tt>GX_ALPHA_BUMP</tt> or <tt>GX_ALPHA_BUMPN</tt>, you can use "Bump alpha" component from indirect texture unit as rasterized color input (see GX_SetTevIndirect()
 * for details about how to configure bump alpha). Since bump alpha contains only 5-bit data, <tt>GX_ALPHA_BUMP</tt> shifts them to higher bits, which makes the
 * value range 0-248. Meanwhile <tt>GX_ALPHA_BUMPN</tt> performs normalization and you can get the value range 0-255.
 *
 * \param[in] tevstage \ref tevstage
 * \param[in] texcoord \ref texcoordid
 * \param[in] texmap \ref texmapid
 * \param[in] color \ref channelid
 *
 * \return none
 */
void GX_SetTevOrder(u8 tevstage,u8 texcoord,u32 texmap,u8 color);

/*!
 * \fn void GX_SetNumTevStages(u8 num)
 * \brief Enables a <i>consecutive</i> number of TEV stages.
 *
 * \details The output pixel color (before fogging and blending) is the result from the last stage. The last TEV stage must write to register <tt>GX_TEVPREV</tt>;
 * see GX_SetTevColorOp() and GX_SetTevAlphaOp(). At least one TEV stage must be enabled. If a Z-texture is enabled, the Z texture must be looked up on
 * the last stage; see GX_SetZTexture().
 *
 * \note The association of lighting colors, texture coordinates, and texture maps with a TEV stage is set using GX)SetTevOrder(). The number of texture
 * coordinates available is set using GX_SetNumTexGens(). The number of color channels available is set using GX_SetNumChans().<br><br>
 *
 * \note GX_Init() will set \a num to 1.
 *
 * \param[in] num number of active TEV stages, between 1 and 16 inclusive
 *
 * \return none
 */
void GX_SetNumTevStages(u8 num);

/*!
 * \fn void GX_SetAlphaCompare(u8 comp0,u8 ref0,u8 aop,u8 comp1,u8 ref1)
 * \brief Sets the parameters for the alpha compare function which uses the alpha output from the last active TEV stage.
 *
 * \details The alpha compare operation is:<br><br>
 *
 * &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<i>alpha_pass</i> = (<i>alpha_src</i> (\a comp0) \a ref0) (\a op) (<i>alpha_src</i> (\a comp1) \a ref1)<br><br>
 *
 * where <i>alpha_src</i> is the alpha from the last active TEV stage. As an example, you can implement these equations:<br><br>
 *
 * &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<i>alpha_pass</i> = (<i>alpha_src</i> \> \a ref0) <b>AND</b> (<i>alpha_src</i> \< \a ref1)
 *
 * or
 *
 * &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<i>alpha_pass</i> = (<i>alpha_src</i> \> \a ref0) <b>OR</b> (<i>alpha_src</i> \< \a ref1)
 *
 * \note The output alpha can be used in the blending equation (see GX_SetBlendMode()) to control how source and destination (frame buffer)
 * pixels are combined.<br><br>
 *
 * \note The Z compare can occur either before or after texturing (see GX_SetZCompLoc()). In the case where Z compare occurs before texturing, the Z is
 * written based only on the Z test. The color is written if both the Z test and alpha test pass. When Z compare occurs <i>after</i> texturing, the color
 * and Z are written if both the Z test and alpha test pass. When using texture to make cutout shapes (like billboard trees) that need to be correctly Z
 * buffered, you should configure the pipeline to Z buffer after texturing.<br><br>
 *
 * \note The number of active TEV stages is specified using GX_SetNumTevStages().
 *
 * \param[in] comp0 \ref compare subfunction 0
 * \param[in] ref0 reference val for subfunction 0
 * \param[in] aop \ref alphaop for combining subfunctions 0 and 1; must not be <tt>GX_MAX_ALPHAOP</tt>
 * \param[in] comp1 \ref compare subfunction 1
 * \param[in] ref1 reference val for subfunction 1
 *
 * \return none
 */
void GX_SetAlphaCompare(u8 comp0,u8 ref0,u8 aop,u8 comp1,u8 ref1);

/*!
 * \fn void GX_SetTevKColor(u8 sel, GXColor col)
 * \brief Sets one of the "konstant" color registers in the TEV unit.
 *
 * \details These registers are available to all TEV stages. They are constant in the sense that they cannot be written to be the TEV itself.
 *
 * \param[in] sel \ref tevkcolorid
 * \param[in] col constant color value
 *
 * \return none
 */
void GX_SetTevKColor(u8 sel, GXColor col);

/*!
 * \fn void GX_SetTevKColorSel(u8 tevstage,u8 sel)
 * \brief Selects a "konstant" color input to be used in a given TEV stage.
 *
 * The constant color input is used only if <tt>GX_CC_KONST</tt> is selected for an input for that TEV stage. Only one constant color selection is
 * available for a given TEV stage, though it may be used for more than one input.
 *
 * \param[in] tevstage \ref tevstage
 * \param[in] sel \ref tevkcolorsel
 *
 * \return none
 */
void GX_SetTevKColorSel(u8 tevstage,u8 sel);

/*!
 * \fn void GX_SetTevKAlphaSel(u8 tevstage,u8 sel)
 * \brief Selects a "konstant" alpha input to be used in a given TEV stage.
 *
 * \details The constant alpha input is used only if <tt>GX_CA_KONST</tt> is selected for an input for that TEV stage. Only one constant alpha selection is
 * available for a given TEV stage, though it may be used for more than one input.
 *
 * \param[in] tevstage \ref tevstage
 * \param[in] sel \ref tevkalphasel
 *
 * \return none
 */
void GX_SetTevKAlphaSel(u8 tevstage,u8 sel);

/*!
 * \fn void GX_SetTevKColorS10(u8 sel, GXColorS10 col)
 * \brief Used to set one of the constant color registers in the Texture Environment (TEV) unit.
 *
 * \details These registers are available to all TEV stages. At least one of these registers is used to pass the output of one TEV stage to the next
 * in a multi-texture configuration.
 *
 * \note The application is responsible for allocating these registers so that no collisions in usage occur.<br><br>
 *
 * \note This function takes 10-bit signed values as color values; use GX_SetTevColor() to give 8-bit values.
 *
 * \param[in] sel \ref tevcoloutreg
 * \param[in] col constant color value
 *
 * \return none
 */
void GX_SetTevKColorS10(u8 sel, GXColorS10 col);

/*!
 * \fn void GX_SetTevSwapMode(u8 tevstage,u8 ras_sel,u8 tex_sel)
 * \brief Selects a set of swap modes for the rasterized color and texture color for a given TEV stage.
 *
 * \details This allows the color components of these inputs to be rearranged or duplicated.
 *
 * \note There are four different swap mode table entries, and each entry in the table specifies how the RGBA inputs map to the RGBA outputs.
 *
 * \param[in] tevstage \ref tevstage
 * \param[in] ras_sel selects a swap mode for the rasterized color input.
 * \param[in] tex_sel selects a swap mode for the texture color input.
 *
 * \return none
 */
void GX_SetTevSwapMode(u8 tevstage,u8 ras_sel,u8 tex_sel);

/*!
 * \fn void GX_SetTevSwapModeTable(u8 swapid,u8 r,u8 g,u8 b,u8 a)
 * \brief Sets up the TEV color swap table.
 *
 * \details The swap table allows the rasterized color and texture color to be swapped component-wise. An entry in the table specifies how the
 * input color components map to the output color components.
 *
 * \param[in] swapid \ref tevswapsel
 * \param[in] r input color component that should be mapped to the red output component.
 * \param[in] g input color component that should be mapped to the green output component.
 * \param[in] b input color component that should be mapped to the blue output component.
 * \param[in] a input color component that should be mapped to the alpha output component.
 *
 * \return none
 */
void GX_SetTevSwapModeTable(u8 swapid,u8 r,u8 g,u8 b,u8 a);

/*!
 * \fn void GX_SetTevIndirect(u8 tevstage,u8 indtexid,u8 format,u8 bias,u8 mtxid,u8 wrap_s,u8 wrap_t,u8 addprev,u8 utclod,u8 a)
 * \brief Controls how the results from an indirect lookup will be used to modify a given regular TEV stage lookup.
 *
 * \param[in] tevstage \ref tevstage being affected
 * \param[in] indtexid \ref indtexstage results to use with this TEV stage
 * \param[in] format \ref indtexformat, i.e. how many bits to extract from the indirect result color to use in indirect offsets and the indirect "bump" alpha
 * \param[in] bias \ref indtexbias to be applied to each component of the indirect offset
 * \param[in] mtxid which \ref indtexmtx and scale value to multiply the offsets with
 * \param[in] wrap_s \ref indtexwrap to use with the S component of the regular texture coordinate
 * \param[in] wrap_t \ref indtexwrap to use with the T component of the regular texture coordinate
 * \param[in] addprev whether the tex coords results from the previous TEV stage should be added in
 * \param[in] utclod whether to the unmodified (<tt>GX_TRUE</tt>) or modified (<tt>GX_FALSE</tt>) tex coords for mipmap LOD computation
 * \param[in] a which offset component will supply the \ref indtexalphasel, if any
 *
 * \return none
 */
void GX_SetTevIndirect(u8 tevstage,u8 indtexid,u8 format,u8 bias,u8 mtxid,u8 wrap_s,u8 wrap_t,u8 addprev,u8 utclod,u8 a);

/*!
 * \fn void GX_SetTevDirect(u8 tevstage)
 * \brief Used to turn off all indirect texture processing for the specified regular TEV stage.
 *
 * \param[in] tevstage the \ref tevstage to change
 *
 * \return none
 */
void GX_SetTevDirect(u8 tevstage);

/*!
 * \fn void GX_SetNumIndStages(u8 nstages)
 * \brief Used to set how many indirect lookups will take place.
 *
 * \details The results from these indirect lookups may then be used to alter the lookups for any number of regular TEV stages.
 *
 * \param[in] nstages number of indirect lookup stages
 *
 * \return none
 */
void GX_SetNumIndStages(u8 nstages);

/*!
 * \fn void GX_SetIndTexOrder(u8 indtexstage,u8 texcoord,u8 texmap)
 * \brief Used to specify the \a texcoord and \a texmap to used with a given indirect lookup.
 *
 * \param[in] indtexstage \ref indtexstage being affected
 * \param[in] texcoord \ref texcoordid to be used for this stage
 * \param[in] texmap \ref texmapid to be used for this stage
 *
 * \return none
 */
void GX_SetIndTexOrder(u8 indtexstage,u8 texcoord,u8 texmap);

/*!
 * \fn void GX_SetIndTexCoordScale(u8 indtexid,u8 scale_s,u8 scale_t)
 * \brief Allows the sharing of a texcoord between an indirect stage and a regular TEV stage.
 *
 * It allows the texture coordinates to be scaled down for use with an indirect map that is smaller than the corresponding regular map.
 *
 * \param[in] indtexid \ref indtexstage being affected
 * \param[in] scale_s \ref indtexscale factor for the S coord
 * \param[in] scale_t \ref indtexscale factor for the T coord
 *
 * \return none
 */
void GX_SetIndTexCoordScale(u8 indtexid,u8 scale_s,u8 scale_t);

/*!
 * \fn void GX_SetFog(u8 type,f32 startz,f32 endz,f32 nearz,f32 farz,GXColor col)
 * \brief Enables fog.
 *
 * \details Using \a type, the programmer may select one of several functions to control the fog density as a function of range to a quad (2x2 pixels).
 * Range is cosine corrected Z in the x-z plane (eye coordinates), but is not corrected in the y direction (see GX_SetFogRangeAdj()). The parameters \a startz and
 * \a endz allow further control over the fog behavior. The parameters \a nearz and \a farz should be set consistent with the projection matrix parameters. Note that these
 * parameters are defined in eye-space. The fog color, in RGBX format (i.e. the alpha component is ignored), is set using the \a col parameter. This will be the
 * color of the pixel when fully fogged.
 *
 * \note GX_Init() turns fog off by default.
 *
 * \param[in] type \ref fogtype to use
 * \param[in] startz minimum Z value at which the fog function is active
 * \param[in] endz maximum Z value at which the fog function is active
 * \param[in] nearz near plane (which should match the projection matrix parameters)
 * \param[in] farz far plane (which should match the projection matrix parameters)
 * \param[in] col fog color; alpha component is ignored
 *
 * \return none
 */
void GX_SetFog(u8 type,f32 startz,f32 endz,f32 nearz,f32 farz,GXColor col);

/*!
 * \fn void GX_SetFogRangeAdj(u8 enable,u16 center,GXFogAdjTbl *table)
 * \brief Enables or disables horizontal fog-range adjustment.
 *
 * \details This adjustment is a factor that is multiplied by the eye-space Z used for fog computation; it is based upon the X position of the pixels being
 * rendered. The Y direction is not compensated. This effectively increases the fog density at the edges of the screen, making for a more realistic fog
 * effect. The adjustment is computed per quad (2x2 pixels), not per-pixel. The center of the viewport is specified using \a center. The range adjustment
 * table is specified using \a table. The range adjust function is mirrored horizontally about the \a center.
 *
 * \note GX_Init() disables range adjustment.
 *
 * \sa GX_InitFogAdjTable()
 *
 * \param[in] enable enables adjustment when <tt>GX_ENABLE</tt> is passed; disabled with <tt>GX_DISABLE</tt>
 * \param[in] center centers the range adjust function; normally corresponds with the center of the viewport
 * \param[in] table range adjustment parameter table
 *
 * \return none
 */
void GX_SetFogRangeAdj(u8 enable,u16 center,GXFogAdjTbl *table);

/*!
 * \fn GX_SetFogColor(GXColor color)
 * \brief Sets the fog color.
 *
 * \details \a color is the color that a pixel will be if fully fogged. Alpha channel is ignored.
 *
 * \param[in] color color to set fog to
 */
void GX_SetFogColor(GXColor color);

/*!
 * \fn void GX_InitFogAdjTable(GXFogAdjTbl *table,u16 width,f32 projmtx[4][4])
 * \brief Generates the standard range adjustment table and puts the results into \a table.
 *
 * \details This table can be used by GX_SetFogRangeAdj() to adjust the eye-space Z used for fog based upon the X position of the pixels being rendered.
 * The Y direction is not compensated. This effectively increases the fog density at the edges of the screen, making for a more realistic fog effect. The
 * width of the viewport is specified using \a width. The \a projmtx parameter is the projection matrix that is used to render into the viewport. It must
 * be specified so that the function can compute the X extent of the viewing frustum in eye space.
 *
 * \note You must allocate \a table yourself.
 *
 * \param[in] table range adjustment parameter table
 * \param[in] width width of the viewport
 * \param[in] projmtx projection matrix used to render into the viewport
 */
void GX_InitFogAdjTable(GXFogAdjTbl *table,u16 width,f32 projmtx[4][4]);

/*!
 * \fn void GX_SetIndTexMatrix(u8 indtexmtx,f32 offset_mtx[2][3],s8 scale_exp)
 * \brief Sets one of the three static indirect matrices and the associated scale factor.
 *
 * \details The indirect matrix and scale is used to process the results of an indirect lookup in order to produce offsets to use during a regular lookup.
 * The matrix is multiplied by the [S T U] offsets that have been extracted (and optionally biased) from the indirect lookup color. In this matrix-vector
 * multiply, the matrix is on the left and the [S T U] column vector is on the right.
 *
 * \note The matrix values are stored in the hardware as a sign and 10 fractional bits (two's complement); thus the smallest number that can be stored is
 * -1 and the largest is (1 - 1/1024) or approximately 0.999. Since +1 cannot be stored, you may consider dividing all the matrix values by 2 (thus +1
 * becomes +0.5) and adding one to the scale value in order to compensate.
 *
 * \param[in] indtexmtx \ref indtexmtx that is being affected
 * \param[in] offset_mtx values to assign to the indirect matrix
 * \param[in] scale_exp exponent to use for the associated scale factor
 *
 * \return none
 */
void GX_SetIndTexMatrix(u8 indtexmtx,f32 offset_mtx[2][3],s8 scale_exp);

/*!
 * \fn void GX_SetTevIndBumpST(u8 tevstage,u8 indstage,u8 mtx_sel)
 * \brief Sets up an environment-mapped bump-mapped indirect lookup.
 *
 * \details The indirect map specifies offsets in (S,T) space. This kind of lookup requires 3 TEV stages to compute. As a result of all this work, a simple
 * 2D bump map is properly oriented to the surface to which it is applied. It is used to alter a normal-based texgen which then looks up an environment map.
 * The environment map may be a simple light map, or else it may be a reflection map of the surrounding scenery.
 *
 * \note When using this function, texture lookup should be disabled for the first two TEV stages. The third stage is where the texture lookup is actually performed.
 * The associated geometry must supply normal/binormal/tangent coordinates at each vertex. Appropriate texgens must supply each of these to the proper stages
 * (binormal to the first, tangent to the second, and normal to the third). Although a static indirect matrix is not used, one must choose a matrix slot and set up
 * the associated scale value to be used with this lookup.
 *
 * \param[in] tevstage \ref tevstage that is being affected
 * \param[in] indstage \ref indtexstage results to use with this TEV stage
 * \param[in] mtx_sel which \ref indtexmtx to multiply the offsets with
 *
 * \return none
 */
void GX_SetTevIndBumpST(u8 tevstage,u8 indstage,u8 mtx_sel);

/*!
 * \fn void GX_SetTevIndBumpXYZ(u8 tevstage,u8 indstage,u8 mtx_sel)
 * \brief Sets up an environment-mapped bump-mapped indirect lookup.
 *
 * \details The indirect map specifies offsets in object (X, Y, Z) space. This kind of lookup requires only one TEV stage to compute; however, the bump map (indirect
 * map) used is geometry-specific. Thus there is a space/computation tradeoff between using this function and using GX_SetTevIndBumpST().
 *
 * \note The indirect matrix must be loaded with a transformation for normals from object space to texture space (similar to eye space, but possibly with an inverted
 * Y axis). The surface geometry need only provide regular normals at each vertex. A normal-based texgen must be set up for the regular texture coordinate.
 *
 * \param[in] tevstage \ref tevstage that is being affected
 * \param[in] indstage \ref indtexstage results to use with this TEV stage
 * \param[in] mtx_sel which \ref indtexmtx to multiply the offsets with
 *
 * \return none
 */
void GX_SetTevIndBumpXYZ(u8 tevstage,u8 indstage,u8 mtx_sel);

/*!
 * \fn void GX_SetTevIndTile(u8 tevstage,u8 indtexid,u16 tilesize_x,u16 tilesize_y,u16 tilespacing_x,u16 tilespacing_y,u8 indtexfmt,u8 indtexmtx,u8 bias_sel,u8 alpha_sel)
 * \brief Used to implement tiled texturing using indirect textures.
 *
 * \details It will set up the correct values in the given indirect matrix; you only need to specify which matrix slot to use.
 *
 * \note The regular texture map contains only the tile definitions. The actual texture size to be applied to the polygon being drawn is the product of the base tile
 * size and the size of the indirect map. In order to set the proper texture coordinate scale, one must call GX_SetTexCoordScaleManually(). One can also use
 * GX_SetIndTexCoordScale() in order to use the same texcoord for the indirect stage as the regular TEV stage.
 *
 * \param[in] tevstage \ref tevstage that is being affected
 * \param[in] indtexid \ref indtexstage results to use with this TEV stage
 * \param[in] tilesize_x size of the tile in the X dimension
 * \param[in] tilesize_y size of the tile in the Y dimension
 * \param[in] tilespacing_x spacing of the tiles (in the tile-definition map) in the X dimension
 * \param[in] tilespacing_y spacing of the tiles (in the tile-definition map) in the Y dimension
 * \param[in] indtexfmt \ref indtexformat to use
 * \param[in] indtexmtx \ref indtexmtx to multiply the offsets with
 * \param[in] bias_sel \ref indtexbias to indicate tile stacking direction for pseudo-3D textures
 * \param[in] alpha_sel which \ref indtexalphasel will supply the indirect "bump" alpha, if any (for pseudo-3D textures).
 *
 * \return none
 */
void GX_SetTevIndTile(u8 tevstage,u8 indtexid,u16 tilesize_x,u16 tilesize_y,u16 tilespacing_x,u16 tilespacing_y,u8 indtexfmt,u8 indtexmtx,u8 bias_sel,u8 alpha_sel);

/*!
 * \fn void GX_SetTevIndRepeat(u8 tevstage)
 * \brief Set a given TEV stage to use the same texture coordinates as were computed in the previous stage.
 *
 * \note This is only useful when the previous stage texture coordinates took more than one stage to compute, as is the case for GX_SetTevIndBumpST().
 *
 * \param[in] tevstage \ref tevstage to modify
 *
 * \return none
 */
void GX_SetTevIndRepeat(u8 tevstage);

/*!
 * \fn void GX_SetColorUpdate(u8 enable)
 * \brief Enables or disables color-buffer updates when rendering into the Embedded Frame Buffer (EFB).
 *
 * \note This function also affects whether the color buffer is cleared during copies; see GX_CopyDisp() and GX_CopyTex().
 *
 * \param[in] enable enables color-buffer updates with <tt>GX_TRUE</tt>
 *
 * \return none
 */
void GX_SetColorUpdate(u8 enable);

/*!
 * \fn void GX_SetAlphaUpdate(u8 enable)
 * \brief Enables or disables alpha-buffer updates of the Embedded Frame Buffer (EFB).
 *
 * \note This function also affects whether the alpha buffer is cleared during copy operations; see GX_CopyDisp() and GX_CopyTex().<br><br>
 *
 * \note The only EFB pixel format supporting an alpha buffer is <tt>GX_PF_RGBA6_Z24</tt>; see GX_SetPixelFmt(). The alpha \a enable is ignored for non-alpha
 * pixel formats.
 *
 * \param[in] enable enables alpha-buffer updates with <tt>GX_TRUE</tt>
 *
 * \return none
 */
void GX_SetAlphaUpdate(u8 enable);

/*!
 * \fn void GX_SetPixelFmt(u8 pix_fmt,u8 z_fmt)
 * \brief Sets the format of pixels in the Embedded Frame Buffer (EFB).
 *
 * \details There are two non-antialiased \a pix_fmts: <tt>GX_PF_RGB8_Z24</tt> and <tt>GX_PF_RGBA6_Z24</tt>. The stride of the EFB is fixed at 640 pixels. The
 * non-antialiased EFB has 528 lines available.
 *
 * When \a pix_fmt is set to <tt>GX_PF_RGB565_Z16</tt>, multi-sample antialiasing is enabled. In order to get proper results, one must also call GX_SetCopyFilter().
 * The position of the subsamples and the antialiasing filter coefficients are set using GX_SetCopyFilter(). When antialiasing, three 16b color/Z
 * samples are computed for each pixel, and the total available number of pixels in the EFB is reduced by half (640 pixels x 264 lines). This function also sets the
 * compression type for 16-bit Z formats, which allows trading off Z precision for range. The following guidelines apply:<br><br>
 *
 * &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;a) far/near ratio <= 2^16, use <tt>GX_ZC_LINEAR</tt><br>
 * &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;b) far/near ratio <= 2^18, use <tt>GX_ZC_NEAR</tt><br>
 * &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;c) far/near ratio <= 2^20, use <tt>GX_ZC_MID</tt><br>
 * &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;d) far/near ratio <= 2^24, use <tt>GX_ZC_FAR</tt><br><br>
 *
 * It is always best to use as little compression as possible (choice "a" is least compressed, choice "d" is most compressed). You get less precision with higher compression.
 * The "far" in the above list does not necessarily refer to the far clipping plane. You should think of it as the farthest object you want correct occlusion for.
 *
 * \note This function also controls antialiasing (AA) mode.<br><br>
 *
 * \note Since changing pixel format requires the pixel pipeline to be synchronized, the use of this function causes stall of the graphics processor as a result. Therefore,
 * you should avoid redundant calls of this function.
 *
 * \param[in] pix_fmt <tt>GX_PF_RGB8_Z24</tt> or <tt>GX_PF_RGBA6_Z24</tt> for non-AA, <tt>GX_PF_RGB565_Z16</tt> for AA
 * \param[in] z_fmt \ref zfmt to use
 *
 * \return none
 */
void GX_SetPixelFmt(u8 pix_fmt,u8 z_fmt);

/*!
 * \fn void GX_SetDither(u8 dither)
 * \brief Enables or disables dithering.
 *
 * \details A 4x4 Bayer matrix is used for dithering.
 *
 * \note Only valid when the pixel format (see GX_SetPixelFmt()) is either <tt>GX_PF_RGBA6_Z24</tt> or <tt>GX_PF_RGB565_Z16</tt>.<br><br>
 *
 * \note Dithering should probably be turned off if you are planning on using the result of rendering for comparisons (e.g. outline rendering
 * algorithm that writes IDs to the alpha channel, copies the alpha channel to a texture, and later compares the texture in the TEV).
 *
 * \param[in] dither enables dithering if <tt>GX_TRUE</tt> is given and pixel format is one of the two above, otherwise disabled
 *
 * \return none
 */
void GX_SetDither(u8 dither);

/*!
 * \fn void GX_SetDstAlpha(u8 enable,u8 a)
 * \brief Sets a constant alpha value for writing to the Embedded Frame Buffer (EFB).
 *
 * \note To be effective, the EFB pixel type must have an alpha channel (see GX_SetPixelFmt()). The alpha compare operation (see
 * GX_SetAlphaCompare()) and blending operations (see GX_SetBlendMode()) still use source alpha (output from the last TEV stage) but when
 * writing the pixel color, the constant alpha will replace the pixel alpha in the EFB.
 *
 * \param[in] enable \a a will be written to the framebuffer if <tt>GX_ENABLE</tt> is here and frame buffer pixel format supports destination alpha
 * \param[in] a constant alpha value
 *
 * \return none
 */
void GX_SetDstAlpha(u8 enable,u8 a);

/*!
 * \fn void GX_SetFieldMask(u8 even_mask,u8 odd_mask)
 * \brief selectively enables and disables interlacing of the frame buffer image.
 *
 * \details This function is used when rendering fields to an interlaced Embedded Frame Buffer (EFB).
 *
 * \note When the mask is <tt>GX_FALSE</tt>, that field will not be written to the EFB, but the other field will be computed. In other words, you pay the
 * fill rate price of a frame to produce a field.
 *
 * \param[in] even_mask whether to write pixels with even Y coordinate
 * \param[in] odd_mask whether to write pixels with odd Y coordinate
 *
 * \return none
 */
void GX_SetFieldMask(u8 even_mask,u8 odd_mask);

/*!
 * \fn void GX_SetFieldMode(u8 field_mode,u8 half_aspect_ratio)
 * \brief Controls various rasterization and texturing parameters that relate to field-mode and double-strike rendering.
 *
 * \details In field-mode rendering, one must adjust the vertical part of the texture LOD computation to account for the fact that pixels cover only half of
 * the space from one rendered scan line to the next (with the other half of the space filled by a pixel from the other field).  In both field-mode and
 * double-strike rendering, one must adjust the aspect ratio for points and lines to account for the fact that pixels will be double-height when displayed
 * (the pixel aspect ratio is 1/2).
 *
 * \note The values set here usually come directly from the render mode. The \a field_rendering flags goes straight into \a field_mode. The \a half_aspect_ratio
 * parameter is true if the \a xfbHeight is half of the \a viHeight, false otherwise.<br><br>
 *
 * \note GX_Init() sets both fields according to the default render mode.<br><br>
 *
 * \note On production hardware (i.e. a retail GameCube), only line aspect-ratio adjustment is implemented. Points are not adjusted.
 *
 * \param[in] field_mode adjusts texture LOD computation as described above if true, otherwise does not
 * \param[in] half_aspect_ratio adjusts line aspect ratio accordingly, otherwise does not
 *
 * \return none
 */
void GX_SetFieldMode(u8 field_mode,u8 half_aspect_ratio);

/*!
 * \fn f32 GX_GetYScaleFactor(u16 efbHeight,u16 xfbHeight)
 * \brief Calculates an appropriate Y scale factor value for GX_SetDispCopyYScale() based on the height of the EFB and
 *        the height of the XFB.
 *
 * \param[in] efbHeight Height of embedded framebuffer. Range from 2 to 528. Should be a multiple of 2.
 * \param[in] xfbHeight Height of external framebuffer. Range from 2 to 1024. Should be equal or greater than \a efbHeight.
 *
 * \return Y scale factor which can be used as argument of GX_SetDispCopyYScale().
 */
f32 GX_GetYScaleFactor(u16 efbHeight,u16 xfbHeight);

/*!
 * \fn u32 GX_SetDispCopyYScale(f32 yscale)
 * \brief Sets the vertical scale factor for the EFB to XFB copy operation.
 *
 * \details The number of actual lines copied is returned, based on the current EFB height. You can use this number to allocate the proper XFB size. You
 * have to call GX_SetDispCopySrc() prior to this function call if you want to get the number of lines by using this function.
 *
 * \param[in] yscale Vertical scale value. Range from 1.0 to 256.0.
 *
 * \return Number of lines that will be copied.
 */
u32 GX_SetDispCopyYScale(f32 yscale);

/*!
 * \fn void GX_SetDispCopySrc(u16 left,u16 top,u16 wd,u16 ht)
 * \brief Sets the source parameters for the EFB to XFB copy operation.
 *
 * \param[in] left left most source pixel to copy. Must be a multiple of 2 pixels.
 * \param[in] top top most source line to copy. Must be a multiple of 2 lines.
 * \param[in] wd width in pixels to copy. Must be a multiple of 2 pixels.
 * \param[in] ht height in lines to copy. Must be a multiple of 2 lines.
 *
 * \return none
 */
void GX_SetDispCopySrc(u16 left,u16 top,u16 wd,u16 ht);

/*!
 * \fn void GX_SetDispCopyDst(u16 wd,u16 ht)
 * \brief Sets the witdth and height of the display buffer in pixels.
 *
 * \details The application typical renders an image into the EFB(source) and then copies it into the XFB(destination) in main memory. \a wd
 * specifies the number of pixels between adjacent lines in the destination buffer and can be different than the width of the EFB.
 *
 * \param[in] wd Distance between successive lines in the XFB, in pixels. Must be a multiple of 16.
 * \param[in] ht Height of the XFB in lines.
 *
 * \return none
 */
void GX_SetDispCopyDst(u16 wd,u16 ht);

/*!
 * \fn void GX_SetCopyClamp(u8 clamp)
 * \brief Sets the vertical clamping mode to use during the EFB to XFB or texture copy.
 *
 * \param[in] clamp bit-wise OR of desired \ref xfbclamp. Use <tt>GX_CLAMP_NONE</tt> for no clamping.
 *
 * \return none
 */
void GX_SetCopyClamp(u8 clamp);

/*!
 * \fn void GX_SetDispCopyGamma(u8 gamma)
 * \brief Sets the gamma correction applied to pixels during EFB to XFB copy operation.
 *
 * \param[in] gamma \ref gammamode
 *
 * \return none
 */
void GX_SetDispCopyGamma(u8 gamma);

/*!
 * \fn void GX_SetCopyFilter(u8 aa,u8 sample_pattern[12][2],u8 vf,u8 vfilter[7])
 * \brief Sets the subpixel sample patterns and vertical filter coefficients used to filter subpixels into pixels.
 *
 * \details This function normally uses the \a aa, \a sample_pattern and \a vfilter provided by the render mode struct:<br><br>
 *
 * \code GXRModeObj* rmode = VIDEO_GetPreferredMode(NULL);
 * GX_SetCopyFilter(rmode->aa,rmode->sample_pattern,GX_TRUE,rmode->vfilter); \endcode
 *
 * \note In order to make use of the \a sample_pattern, antialiasing must be enabled by setting the Embedded Frame Buffer (EFB) format to
 * <tt>GX_PF_RGB565_Z16</tt>; see GX_SetPixelFmt().
 *
 * \param[in] aa utilizes \a sample_pattern if <tt>GX_TRUE</tt>, otherwise all sample points are centered
 * \param[in] sample_pattern array of coordinates for sample points; valid range is 1 - 11 inclusive
 * \param[in] vf use \a vfilter if <tt>GX_TRUE</tt>, otherwise use default 1-line filter
 * \param[in] vfilter vertical filter coefficients; valid coefficient range is 0 - 63 inclusive; sum should equal 64
 *
 * \return none
 */
void GX_SetCopyFilter(u8 aa,u8 sample_pattern[12][2],u8 vf,u8 vfilter[7]);

/*!
 * \fn void GX_SetDispCopyFrame2Field(u8 mode)
 * \brief Determines which lines are read from the Embedded Frame Buffer (EFB) when using GX_CopyDisp().
 *
 * \details Specifically, it determines whether all lines, only even lines, or only odd lines are read.
 *
 * \note The opposite function, which determines whether all lines, only even lines or only odd lines are <i>written</i> to the EFB, is GX_SetFieldMask().<br><br>
 *
 * \note Only applies to display copies, GX_CopyTex() always uses the <tt>GX_COPY_PROGRESSIVE</tt> mode.
 *
 * \param[in] mode \ref copymode to determine which field to copy (or both)
 *
 * \return none
 */
void GX_SetDispCopyFrame2Field(u8 mode);

/*!
 * \fn void GX_SetCopyClear(GXColor color,u32 zvalue)
 * \brief Sets color and Z value to clear the EFB to during copy operations.
 *
 * \details These values are used during both display copies and texture copies.
 *
 * \param[in] color RGBA color (8-bit/component) to use during clear operation.
 * \param[in] zvalue 24-bit Z value to use during clear operation. Use the constant <tt>GX_MAX_Z24</tt> to specify the maximum depth value.
 *
 * \return none
 */
void GX_SetCopyClear(GXColor color,u32 zvalue);

/*!
 * \fn void GX_CopyDisp(void *dest,u8 clear)
 * \brief Copies the embedded framebuffer (EFB) to the external framebuffer(XFB) in main memory.
 *
 * \note The stride of the XFB is set using GX_SetDispCopyDst(). The source image in the EFB is described using GX_SetDispCopySrc().<br><br>
 *
 * \note The graphics processor will stall all graphics commands util the copy is complete.<br><br>
 *
 * \note If the \a clear flag is true, the color and Z buffers will be cleared during the copy. They will be cleared to the constant
 * values set using GX_SetCopyClear().
 *
 * \param[in] dest pointer to the external framebuffer. \a dest should be 32B aligned.
 * \param[in] clear flag that indicates framebuffer should be cleared if <tt>GX_TRUE</tt>.
 *
 * \return none
 */
void GX_CopyDisp(void *dest,u8 clear);

/*!
 * \fn void GX_SetTexCopySrc(u16 left,u16 top,u16 wd,u16 ht)
 * \brief Sets the source parameters for the Embedded Frame Buffer (EFB) to texture image copy.
 *
 * \details The GP will copy textures into the tiled texture format specified in GX_CopyTex(). The GP always copies tiles (32B) so image widths and
 * heights that are not a multiple of the tile width will be padded with undefined data in the copied image
 *
 * \param[in] left left-most source pixel to copy, multiple of two
 * \param[in] top top-most source line to copy, multiple of two
 * \param[in] wd width to copy in pixels, multiple of two
 * \param[in] ht height to copy in pixels, multiple of two
 *
 * \return none
 */
void GX_SetTexCopySrc(u16 left,u16 top,u16 wd,u16 ht);

/*!
 * \fn void GX_SetTexCopyDst(u16 wd,u16 ht,u32 fmt,u8 mipmap)
 * \brief This function sets the width and height of the destination texture buffer in texels.
 *
 * \details This function sets the width (\a wd) and height (\a ht) of the destination texture buffer in texels. The application may render an image into
 * the EFB and then copy it into a texture buffer in main memory. \a wd specifies the number of texels between adjacent lines in the texture buffer and can
 * be different than the width of the source image. This function also sets the texture format (\a fmt) to be created during the copy operation. An
 * optional box filter can be enabled using \a mipmap. This flag will scale the source image by 1/2.
 *
 * Normally, the width of the EFB and destination \a wd are the same. When rendering smaller images that get copied and composited into a larger texture
 * buffer, however, the EFB width and texture buffer \a wd are not necessarily the same.
 *
 * The Z buffer can be copied to a Z texture format by setting \a fmt to <tt>GX_TF_Z24X8</tt>. This operation is only valid when the EFB format is
 * <tt>GX_PF_RGB8_Z24</tt> or <tt>GX_PF_RGBA6_Z24</tt>.
 *
 * The alpha channel can be copied from an EFB with format <tt>GX_PF_RGBA6_Z24</tt> by setting \a fmt to <tt>GX_TF_A8</tt>.
 *
 * \param[in] wd distance between successive lines in the texture buffer, in texels; must be a multiple of the texture tile width, which depends on \a fmt.
 * \param[in] ht height of the texture buffer
 * \param[in] fmt \ref texfmt
 * \param[in] mipmap flag that indicates framebuffer should be cleared if <tt>GX_TRUE</tt>.
 *
 * \return none
 */
void GX_SetTexCopyDst(u16 wd,u16 ht,u32 fmt,u8 mipmap);

/*!
 * \fn void GX_CopyTex(void *dest,u8 clear)
 * \brief Copies the embedded framebuffer (EFB) to the texture image buffer \a dest in main memory.
 *
 * \details This is useful when creating textures using the Graphics Processor (GP). If the \a clear flag is set to <tt>GX_TRUE</tt>, the EFB will be cleared
 * to the current color(see GX_SetCopyClear()) during the copy operation.
 *
 * \param[in] dest pointer to the image buffer in main memory. \a dest should be 32B aligned.
 * \param[in] clear flag that indicates framebuffer should be cleared if <tt>GX_TRUE</tt>.
 *
 * \return none
 */
void GX_CopyTex(void *dest,u8 clear);

/*!
 * \fn void GX_PixModeSync()
 * \brief Causes the GPU to wait for the pipe to flush.
 *
 * \details This function inserts a synchronization command into the graphics FIFO. When the GPU sees this command it will allow the rest of the pipe to
 * flush before continuing. This command is useful in certain situation such as after using GX_CopyTex() and before a primitive that uses the copied texture.
 *
 * \note The command is actually implemented by writing the control register that determines the format of the embedded frame buffer (EFB). As a result, care
 * should be used if this command is placed within a display list.
 *
 * \return none
 */
void GX_PixModeSync();

/*!
 * \fn void GX_ClearBoundingBox()
 * \brief Clears the bounding box values before a new image is drawn.
 *
 * \details The graphics hardware keeps track of a bounding box of pixel coordinates that are drawn in the Embedded Frame Buffer (EFB).
 *
 * \return none
 */
void GX_ClearBoundingBox();

/*!
 * \fn GX_PokeAlphaMode(u8 func,u8 threshold)
 * \brief Sets a threshold which is compared to the alpha of pixels written to the Embedded Frame Buffer (EFB) using the GX_Poke*() functions.
 *
 * \details The compare function order is:<br><br>
 *
 * &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;src_alpha \a func \a threshold
 *
 * \note The alpha compare function can be used to conditionally write pixels to the EFB using the source alpha channel as a template. If the compare function is
 * true, the source color will be written to the EFB based on the result of the Z compare (see GX_PokeZMode()). If the alpha compare function is false, the source
 * color is not written to the EFB.<br><br>
 *
 * \note The alpha compare test happens before the Z compare and before blending (see GX_PokeBlendMode()).
 *
 * \param[in] func \ref compare to use
 * \param[in] threshold to which the source alpha will be compared to
 *
 * \return none
 */
void GX_PokeAlphaMode(u8 func,u8 threshold);

/*!
 * \fn void GX_PokeAlphaUpdate(u8 update_enable)
 * \brief Enables or disables alpha-buffer updates for GX_Poke*() functions.
 *
 * \details The normal rendering state (set by GX_SetAlphaUpdate()) is not affected.
 *
 * \param[in] update_enable enables alpha-buffer updates with <tt>GX_TRUE</tt>, otherwise does not
 *
 * \return none
 */
void GX_PokeAlphaUpdate(u8 update_enable);

/*!
 * \fn void GX_PokeColorUpdate(u8 update_enable)
 * \brief Enables or disables color-buffer updates when writing the Embedded Frame Buffer (EFB) using the GX_Poke*() functions.
 *
 * \param[in] update_enable enables color-buffer updates with <tt>GX_TRUE</tt>, otherwise does not
 *
 * \return none
 */
void GX_PokeColorUpdate(u8 update_enable);

/*!
 * \fn void GX_PokeDither(u8 dither)
 * \brief Enables dithering when writing the Embedded Frame Buffer (EFB) using GX_Poke*() functions.
 *
 * \note The \a dither enable is only valid when the pixel format (see GX_SetPixelFmt()) is either <tt>GX_PF_RGBA6_Z24</tt> or <tt>GX_PF_RGB565_Z16</tt>.<br><br>
 *
 * \note A 4x4 Bayer matrix is used for dithering.
 *
 * \param[in] dither if set to <tt>GX_TRUE</tt> and pixel format is one of the above, dithering is enabled; otherwise disabled
 *
 * \return none
 */
void GX_PokeDither(u8 dither);

/*!
 * \fn void GX_PokeBlendMode(u8 type,u8 src_fact,u8 dst_fact,u8 op)
 * \brief Determines how the source image, is blended with the current Embedded Frame Buffer (EFB).
 *
 * \details When type is set to <tt>GX_BM_NONE</tt>, no color data is written to the EFB. When type is set to <tt>GX_BM_BLEND</tt>, the source and EFB pixels
 * are blended using the following equation:<br><br>
 *
 * &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<i>dst_pix_clr</i> = <i>src_pix_clr</i> * \a src_fact + <i>dst_pix_clr</i> * \a dst_fact<br><br>
 *
 * When type is set to <tt>GX_BM_SUBTRACT</tt>, the destination pixel is computed as follows:<br><br>
 *
 * &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<i>dst_pix_clr</i> = <i>dst_pix_clr</i> - <i>src_pix_clr [clamped to zero]</i><br><br>
 *
 * Note that \a src_fact and \a dst_fact are not part of the equation.
 *
 * \note \a dst_fact can be used only when the frame buffer has <tt>GX_PF_RGBA6_Z24</tt> as the pixel format (see GX_SetPixelFmt()).<br><br>
 *
 * \note When type is set to <tt>GX_BM_LOGIC</tt>, the source and EFB pixels are blended using logical bitwise operations.<br><br>
 *
 * \note This function does not effect the normal rendering state; see GX_SetBlendMode().
 *
 * \param[in] type \ref blendmode
 * \param[in] src_fact source \ref blendfactor; the pixel color produced by the graphics processor is multiplied by this factor
 * \param[in] dst_fact destination \ref blendfactor; the current frame buffer pixel color is multiplied by this factor
 * \param[in] op \ref logicop to use
 */
void GX_PokeBlendMode(u8 type,u8 src_fact,u8 dst_fact,u8 op);

/*!
 * \fn void GX_PokeAlphaRead(u8 mode)
 * \brief Determines what value of alpha will be read from the Embedded Frame Buffer (EFB).
 *
 * \details The mode only applies to GX_Peek*() functions.
 *
 * \note This feature works no matter what pixel type (see GX_SetPixelFmt()) you are using. If you are using the EFB with alpha plane, it is
 * recommended that you use <tt>GX_READ_NONE</tt> so that you can read correct alpha value from the EFB. If you are using the EFB with no alpha, you should
 * set either of <tt>GX_READ_00</tt> or <tt>GX_READ_FF</tt> in order to get a certain value.<br><br>
 *
 * \param[in] mode \ref alphareadmode that determines value of alpha read from a frame buffer with no alpha channel.
 *
 * \return none
 */
void GX_PokeAlphaRead(u8 mode);

/*!
 * \fn void GX_PokeDstAlpha(u8 enable,u8 a)
 * \brief Sets a constant alpha value for writing to the Embedded Frame Buffer (EFB).
 *
 * \details The EFB pixel type must have an alpha channel for this function to be effective (see GX_SetPixelFmt()). The blending operations (see
 * GX_PokeBlendMode()) still use source alpha but when writing the pixel color, the constant \a a will replace the pixel alpha in the EFB.
 *
 * \param[in] enable if set to <tt>GX_ENABLE</tt> and pixel format supports dest alpha, \a a will be written to the framebuffer
 * \param[in] a constant alpha value
 *
 * \return none
 */
void GX_PokeDstAlpha(u8 enable,u8 a);

/*!
 * \fn void GX_PokeARGB(u16 x,u16 y,GXColor color)
 * \brief Allows the CPU to write \a color directly to the Embedded Frame Buffer (EFB) at position \a x,\a y.
 *
 * \details The alpha value in \a color can be compared with the current alpha threshold (see GX_PokeAlphaMode()). The color will be blended
 * into the EFB using the blend mode set by GX_PokeBlendMode().
 *
 * \note For an antialiased frame buffer, all 3 subsamples of a pixel are affected by the poke.
 *
 * \param[in] x coordinate, in pixels; must be 0 - 639 inclusive
 * \param[in] y coordinate, in lines; must be 0 - 527 inclusive
 * \param[in] color color to write at the location
 *
 * \return none
 */
void GX_PokeARGB(u16 x,u16 y,GXColor color);

/*!
 * \fn void GX_PeekARGB(u16 x,u16 y,GXColor *color)
 * \brief Allows the CPU to read a color value directly from the Embedded Frame Buffer (EFB) at position \a x,\a y.
 *
 * \note For an antialiased frame buffer, only subsample 0 of a pixel is read.
 *
 * \param[in] x coordinate, in pixels; must be 0 - 639 inclusive
 * \param[in] y coordinate, in lines; must be 0 - 527 inclusive
 * \param[out] color struct to store color in
 *
 * \return none
 */
void GX_PeekARGB(u16 x,u16 y,GXColor *color);

/*!
 * \fn void GX_PokeZ(u16 x,u16 y,u32 z)
 * \brief Allows the CPU to write a z value directly to the Embedded Frame Buffer (EFB) at position \a x,\a y.
 *
 * \details The \a z value can be compared with the current contents of the EFB. The Z compare fuction is set using GX_PokeZMode().
 *
 * \note The \a z value should be in the range of 0x00000000 <= \a z < 0x00FFFFFF in the case of non-antialiased frame buffer. For an antialiased
 * frame buffer, the \a z value should be in the compressed 16-bit format (0x00000000 <= \a z <= 0x0000FFFF), and the poke will affect all 3
 * subsamples of a pixel.
 *
 * \param[in] x coordinate, in pixels; must be 0 - 639 inclusive
 * \param[in] y coordinate, in lines; must be 0 - 527 inclusive
 * \param[in] z value to write at position \a x,\a y in the EFB
 *
 * \return none
 */
void GX_PokeZ(u16 x,u16 y,u32 z);

/*!
 * \fn void GX_PeekZ(u16 x,u16 y,u32 *z)
 * \brief Allows the CPU to read a z value directly from the Embedded Frame Buffer (EFB) at position x,y.
 *
 * \details The z value is raw integer value from the Z buffer.
 *
 * \note The value range is 24-bit when reading from non-antialiased frame buffer. When reading from an antialiased frame buffer, subsample
 * 0 is read and returned. The value will be compressed 16-bit form in this case.
 *
 * \param[in] x coordinate, in pixels; must be 0 - 639 inclusive
 * \param[in] y coordinate, in lines; must be 0 - 527 inclusive
 * \param[out] z pointer to a returned Z value
 *
 * \return none
 */
void GX_PeekZ(u16 x,u16 y,u32 *z);

/*!
 * \fn void GX_PokeZMode(u8 comp_enable,u8 func,u8 update_enable)
 * \brief Sets the Z-buffer compare mode when writing the Embedded Frame Buffer (EFB).
 *
 * \details The result of the Z compare is used to conditionally write color values to the EFB. The Z value will be updated according to the
 * result of the compare if Z update is enabled.
 *
 * When \a comp_enable is set to <tt>GX_DISABLE</tt>, poke Z buffering is disabled and the Z buffer is not updated. The \a func parameter determines the
 * comparison that is performed. In the comparison function, the poked Z value is on the left while the Z value from the Z buffer is on the
 * right. If the result of the comparison is false, the poked Z value is discarded. The parameter \a update_enable determines whether or not the
 * Z buffer is updated with the new Z value after a comparison is performed.
 *
 * \note The normal rendering Z mode (set by GX_SetZMode()) is not affected by this function.<br><br>
 *
 * \note Even if update_enable is <tt>GX_FALSE</tt>, compares may still be enabled.
 *
 * \param[in] comp_enable enables comparisons with source and destination Z values if <tt>GX_TRUE</tt>
 * \param[in] func \ref compare function to use
 * \param[in] update_enable enables Z-buffer updates when <tt>GX_TRUE</tt>
 *
 * \return none
 */
void GX_PokeZMode(u8 comp_enable,u8 func,u8 update_enable);

/*!
 * \fn u32 GX_GetTexObjFmt(GXTexObj *obj)
 * \brief Returns the texture format described by texture object \a obj.
 *
 * \note Use GX_InitTexObj() or GX_InitTexObjCI() to initialize the texture format.
 *
 * \param[in] obj ptr to a texture object
 *
 * \return texture format of the given texture object
 */
u32 GX_GetTexObjFmt(GXTexObj *obj);

/*!
 * \fn u32 GX_GetTexObjMipMap(GXTexObj *obj)
 * \brief Returns the texture mipmap enable described by texture object \a obj.
 *
 * \note Use GX_InitTexObj() or GX_InitTexObjCI() to initialize the texture mipmap enable.
 *
 * \param[in] obj ptr to a texture object
 *
 * \return mipmap enable flag
 */
u32 GX_GetTexObjMipMap(GXTexObj *obj);

/*!
 * \fn void* GX_GetTexObjUserData(GXTexObj *obj)
 * \brief Used to get a pointer to user data from the \ref GXTexObj structure.
 *
 * \details You can use this function to retrieve private data structures from the texture object. This pointer is set using GX_InitTexObjUserData().
 *
 * \param[in] obj ptr to object to read data from
 *
 * \return Pointer to user data.
 */
void* GX_GetTexObjUserData(GXTexObj *obj);

/*!
 * \fn void* GX_GetTexObjData(GXTexObj *obj)
 * \brief Used to get a pointer to texture data from the \ref GXTexObj structure.
 *
 * \note The returned pointer is a physical address.
 *
 * \param[in] obj ptr to a texture object
 *
 * \return Physical pointer to texture data.
 */
void* GX_GetTexObjData(GXTexObj *obj);

/*!
 * \fn u8 GX_GetTexObjWrapS(GXTexObj* obj)
 * \brief Returns the texture wrap s mode described by texture object \a obj.
 *
 * \note Use GX_InitTexObj() or GX_InitTexObjCI() to initialize the texture wrap s mode.
 *
 * \param[in] obj ptr to a texture object
 *
 * \return wrap s mode
 */
u8 GX_GetTexObjWrapS(GXTexObj* obj);

/*!
 * \fn u8 GX_GetTexObjWrapT(GXTexObj* obj)
 * \brief Returns the texture wrap t mode described by texture object \a obj.
 *
 * \note Use GX_InitTexObj() or GX_InitTexObjCI() to initialize the texture wrap t mode.
 *
 * \param[in] obj ptr to a texture object
 *
 * \return wrap t mode
 */
u8 GX_GetTexObjWrapT(GXTexObj* obj);

/*!
 * \fn u16 GX_GetTexObjHeight(GXTexObj* obj)
 * \brief Returns the texture height described by texture object \a obj.
 *
 * \note Use GX_InitTexObj() or GX_InitTexObjCI() to initialize the texture height.
 *
 * \param[in] obj ptr to a texture object
 *
 * \return texture height
 */
u16 GX_GetTexObjHeight(GXTexObj* obj);

/*!
 * \fn u16 GX_GetTexObjWidth(GXTexObj* obj)
 * \brief Returns the texture width described by texture object \a obj.
 *
 * \note Use GX_InitTexObj() or GX_InitTexObjCI() to initialize the texture width.
 *
 * \param[in] obj ptr to a texture object
 *
 * \return texture width
 */
u16 GX_GetTexObjWidth(GXTexObj* obj);

/*!
 * \fn void GX_GetTexObjAll(GXTexObj* obj, void** image_ptr, u16* width, u16* height, u8* format, u8* wrap_s, u8* wrap_t, u8* mipmap);
 * \brief Returns the parameters described by a texture object. Texture objects are used to describe all the parameters associated with a texture, including size, format, wrap modes, filter modes, etc. Texture objects are initialized using either GX_InitTexObj() or, for color index format textures, GX_InitTexObjCI().
 *
 * \param[in] obj ptr to a texture object
 * \param[out] image_ptr Returns a physical pointer to the image data for a texture.
 * \param[out] width Returns the width of the texture or LOD 0 for mipmaps
 * \param[out] height Returns the height of the texture or LOD 0 for mipmaps
 * \param[out] format Returns the texel format
 * \param[out] mipmap Returns the mipmap enable flag.
 *
 * \return none
 */
void GX_GetTexObjAll(GXTexObj* obj, void** image_ptr, u16* width, u16* height, u8* format, u8* wrap_s, u8* wrap_t, u8* mipmap);

/*!
 * \fn u32 GX_GetTexBufferSize(u16 wd,u16 ht,u32 fmt,u8 mipmap,u8 maxlod)
 * \brief Returns the amount of memory in bytes needed to store a texture of the given size and \a fmt.
 *
 * \details If the \a mipmap flag is <tt>GX_TRUE</tt>, then the size of buffer needed for the mipmap pyramid up to \a maxlod will be returned.
 * \a maxlod will be clamped to the number of LODs possible given the map \a wd and \a ht. For mipmaps, \a wd and \a ht must be a power of two.
 *
 * \note This function takes into account the tiling and padding requirements of the GameCube's native texture format. The resulting size can be used
 * along with memalign() to allocate texture buffers (see GX_CopyTex()).
 *
 * \param[in] wd width of the texture in texels
 * \param[in] ht height of the texture in texels
 * \param[in] fmt format of the texture; use GX_TexFmt() or GX_CITexFmt() to get it
 * \param[in] mipmap flag indicating whether or not the texture is a mipmap
 * \param[in] maxlod if \a mipmap is \a GX_TRUE, texture size will include mipmap pyramid up to this value
 *
 * \return number of bytes needed for the texture, including tile padding
 */
u32 GX_GetTexBufferSize(u16 wd,u16 ht,u32 fmt,u8 mipmap,u8 maxlod);

/*!
 * \fn void GX_InvalidateTexAll()
 * \brief Invalidates the current caches of the Texture Memory (TMEM).
 *
 * \details It takes about 512 GP clocks to invalidate all the texture caches.
 *
 * \note Preloaded textures (see GX_PreloadEntireTexture()) are not affected.
 *
 * \return none
 */
void GX_InvalidateTexAll();

/*!
 * \fn void GX_InvalidateTexRegion(GXTexRegion *region)
 * \brief Invalidates the texture cache in Texture Memory (TMEM) described by \a region.
 *
 * \details This function should be called when the CPU is used to modify a texture in main memory, or a new texture is loaded into main memory that
 * is possibly cached in the texture region.
 *
 * \note In reality, this function invalidates the cache tags, forcing the texture cache to load new data. Preloaded textures (see
 * GX_PreloadEntireTexture()) do not use the tags.<br><br>
 *
 * \note The texture hardware can invalidate 4 tags each GP clock. Each tag represents a superline or 512B of TMEM. Therefore, it takes 16
 * GP clocks to invalidate a 32KB texture region.
 *
 * \param[in] region ptr to GXTexRegion object
 *
 * \return none
 */
void GX_InvalidateTexRegion(GXTexRegion *region);

/*!
 * \fn void GX_InitTexCacheRegion(GXTexRegion *region,u8 is32bmipmap,u32 tmem_even,u8 size_even,u32 tmem_odd,u8 size_odd)
 * \brief Initializes a texture memory (TMEM) region object for cache.
 *
 * \details The region is allocated by the application and can be used as a cache. An application can create many region objects and some of them can
 * overlap; however, no two overlapping regions can be active at the same time.
 *
 * The possible sizes of a TMEM cache region are 32K, 128K or 512K.
 *
 * \note For pre-loaded textures, the region must be defined by using GX_InitTexPreloadRegion().<br><br>
 *
 * \note GX_Init() creates default texture regions, so it is not necessary for the application to use this function unless a different Texture Memory
 * configuration is desired. In that case, the application should also define a region allocator using GX_SetTexRegionCallback().<br><br>
 *
 * \note The function GX_InvalidateTexRegion() can be used to force the texture in main memory associated with this region to be reloaded. This will be
 * necessary whenever the texture data in main memory changes. You may invalidate all cached regions at once using GX_InvalidateTexAll().
 *
 * \param[in] region ptr to a GXTexRegion struct
 * \param[in] is32bmipmap should be set to <tt>GX_TRUE</tt> to interpret parameters according to the 32b mipmap meaning.
 * \param[in] tmem_even base ptr in TMEM for even LODs; must be multiple of 2KB
 * \param[in] size_even even \ref texcachesize other than <tt>GX_TEXCACHE_NONE</tt>
 * \param[in] tmem_odd base ptr in TMEM for odd LODs; must be multiple of 2KB
 * \param[in] size_odd odd \ref texcachesize other than <tt>GX_TEXCACHE_NONE</tt>
 *
 * \return none
 */
void GX_InitTexCacheRegion(GXTexRegion *region,u8 is32bmipmap,u32 tmem_even,u8 size_even,u32 tmem_odd,u8 size_odd);

/*!
 * \fn void GX_InitTexPreloadRegion(GXTexRegion *region,u32 tmem_even,u32 size_even,u32 tmem_odd,u32 size_odd)
 * \brief Initializes a Texture Memory (TMEM) region object for preloading.
 *
 * \details The region is allocated in TMEM by the application and can be used only as a pre-loaded buffer. Cache regions must be allocated
 * by using GX_InitTexCacheRegion(). For pre-loaded textures, the size of the region must match the size of the texture. An application can
 * create many region objects and some of them can overlap; however, no two overlapping regions can be active at the same time.
 *
 * \note The maximum size of a region is 512K.
 *
 * \warning GX_Init() creates no region for preloading, so the application should allocate appropriate regions if preloading is necessary. It
 * is also required to create cache regions and its allocator by using GX_InitTexCacheRegion() and GX_SetTexRegionCallback(), otherwise new
 * cache regions may overwrite the preloaded areas. (Alternatively, if you do not use any color-index textures, you may preload textures into
 * the portion of texture memory normally allocated to color-index usage by the default allocator.)
 *
 * \param[in] region ptr to a GXTexRegion struct
 * \param[in] tmem_even base ptr in TMEM for even LODs; must be 32B aligned
 * \param[in] size_even size of the even cache, in bytes; should be multiple of 32B
 * \param[in] tmem_odd base ptr in TMEM for odd LODs; must be 32B aligned
 * \param[in] size_odd size of the odd cache, in bytes; should be multiple of 32B
 *
 * \return none
 */
void GX_InitTexPreloadRegion(GXTexRegion *region,u32 tmem_even,u32 size_even,u32 tmem_odd,u32 size_odd);

/*!
 * \fn void GX_InitTexObj(GXTexObj *obj,void *img_ptr,u16 wd,u16 ht,u8 fmt,u8 wrap_s,u8 wrap_t,u8 mipmap)
 * \brief Used to initialize or change a texture object for non-color index textures.
 *
 * \details Texture objects are used to describe all the parameters associated with a texture, including size, format, wrap modes, filter modes,
 * etc. It is the application's responsibility to provide memory for a texture object. Once initialized, a texture object can be associated with
 * one of eight active texture IDs using GX_LoadTexObj().
 *
 * \note To initialize a texture object for color index format textures, use GX_InitTexObjCI().<br><br>
 *
 * \note If the mipmap flag is <tt>GX_TRUE</tt>, then the texture is a mipmap and the texture will be trilerped. If the mipmap flag is <tt>GX_FALSE</tt>, the texture
 * is not a mipmap and the texture will be bilerped. To override the filter modes and other mipmap controls, see GX_InitTexObjLOD().
 *
 * \param[out] obj ptr to a texture object
 * \param[in] img_ptr ptr to the image data for a texture, aligned to 32B
 * \param[in] wd width of the texture, or LOD level 0 for mipmaps; max value is 1024; mipmaps must be a power of two
 * \param[in] ht height of the texture, or LOD level 0 for mipmaps; max value is 1024; mipmaps must be a power of two
 * \param[in] fmt \ref texfmt
 * \param[in] wrap_s texture coordinate wrapping strategy in the S direction; use <tt>GX_CLAMP</tt>, <tt>GX_REPEAT</tt> or <tt>GX_MIRROR</tt>
 * \param[in] wrap_t texture coordinate wrapping strategy in the T direction; use <tt>GX_CLAMP</tt>, <tt>GX_REPEAT</tt> or <tt>GX_MIRROR</tt>
 * \param[in] mipmap trilinear filtering will be used if <tt>GX_TRUE</tt>, otherwise bilinear is used
 *
 * \return none
 */
void GX_InitTexObj(GXTexObj *obj,void *img_ptr,u16 wd,u16 ht,u8 fmt,u8 wrap_s,u8 wrap_t,u8 mipmap);

/*!
 * \fn void GX_InitTexObjCI(GXTexObj *obj,void *img_ptr,u16 wd,u16 ht,u8 fmt,u8 wrap_s,u8 wrap_t,u8 mipmap,u32 tlut_name)
 * \brief Used to initialize or change a texture object when the texture is color index format.
 *
 * \details Texture objects are used to describe all the parameters associated with a texture, including size, format, wrap modes, filter modes,
 * etc. It is the application's responsibility to provide memory for a texture object. Once initialized, a texture object can be associated with
 * one of eight active texture IDs using GX_LoadTexObj().
 *
 * \note If the \a mipmap flag is <tt>GX_TRUE</tt>, then the texture is a mipmap and the texture will be filtered using the <tt>GX_LIN_MIP_NEAR</tt> filter mode
 * (color index mipmaps cannot use the <tt>GX_LIN_MIP_LIN</tt> or <tt>GX_NEAR_MIP_LIN</tt> mode). If the \a mipmap flag is <tt>GX_FALSE</tt>, the texture is not a mipmap
 * and the texture will be bilerped. To override the filter modes and other mipmap controls, use GX_InitTexObjLOD(). Mipmap textures should
 * set the width and height to a power of two, but mipmaps do not need to be square.<br><br>
 *
 * \note Non-mipmap (planar) textures do not have to be a power of two. However, to use the <tt>GX_REPEAT</tt> or <tt>GX_MIRROR</tt> modes for \a wrap_s and \a wrap_t
 * the width and height, respectively, must be a power of two.<br><br>
 *
 * \note The \a tlut_name is used to indicate which texture lookup table (TLUT) to use for the index to color conversion. To load the TLUT into
 * texture memory, use GX_LoadTlut().
 *
 * \param[in] obj ptr to a texture object
 * \param[in] img_ptr ptr to the image data for a texture, aligned to 32B
 * \param[in] wd width of the texture, or LOD level 0 for mipmaps; max value is 1024; mipmaps must be a power of two
 * \param[in] ht height of the texture, or LOD level 0 for mipmaps; max value is 1024; mipmaps must be a power of two
 * \param[in] fmt \ref texfmt
 * \param[in] wrap_s texture coordinate wrapping strategy in the S direction; use <tt>GX_CLAMP</tt>, <tt>GX_REPEAT</tt> or <tt>GX_MIRROR</tt>
 * \param[in] wrap_t texture coordinate wrapping strategy in the T direction; use <tt>GX_CLAMP</tt>, <tt>GX_REPEAT</tt> or <tt>GX_MIRROR</tt>
 * \param[in] mipmap if <tt>GX_TRUE</tt>, it is a mipmap texture, else it is a planar texture
 * \param[in] tlut_name TLUT name to use for this texture; default texture configuration recognizes \ref tlutname
 *
 * \return none
 */
void GX_InitTexObjCI(GXTexObj *obj,void *img_ptr,u16 wd,u16 ht,u8 fmt,u8 wrap_s,u8 wrap_t,u8 mipmap,u32 tlut_name);

/*!
 * \fn void GX_InitTexObjTlut(GXTexObj *obj,u32 tlut_name)
 * \brief Allows one to modify the TLUT that is associated with an existing texture object.
 *
 * \param[in] obj ptr to a texture object
 * \param[in] tlut_name TLUT name to use for this texture; default texture configuration recognizes \ref tlutname
 *
 * \return none
 */
void GX_InitTexObjTlut(GXTexObj *obj,u32 tlut_name);

/*!
 * \fn void GX_InitTexObjData(GXTexObj *obj,void *img_ptr)
 * \brief Allows one to modify the image data pointer for an existing texture object.
 *
 * \note The image format and size for the new data must agree with what they were when the texture object was first initialized using
 * GX_InitTexObj() or GX_InitTexObjCI().
 *
 * \param[in] obj ptr to a texture object
 * \param[in] img_ptr ptr to the texture data in main memory
 *
 * \return none
 */
void GX_InitTexObjData(GXTexObj *obj,void *img_ptr);

/*!
 * \fn void GX_InitTexObjWrapMode(GXTexObj *obj,u8 wrap_s,u8 wrap_t)
 * \brief Allows one to modify the texture coordinate wrap modes for an existing texture object.
 *
 * \param[in] obj ptr to a texture object
 * \param[in] wrap_s texture coordinate wrapping strategy in the S direction; use <tt>GX_CLAMP</tt>, <tt>GX_REPEAT</tt> or <tt>GX_MIRROR</tt>
 * \param[in] wrap_t texture coordinate wrapping strategy in the T direction; use <tt>GX_CLAMP</tt>, <tt>GX_REPEAT</tt> or <tt>GX_MIRROR</tt>
 *
 * \return none
 */
void GX_InitTexObjWrapMode(GXTexObj *obj,u8 wrap_s,u8 wrap_t);

/*!
 * \fn void GX_InitTexObjFilterMode(GXTexObj *obj,u8 minfilt,u8 magfilt)
 * \brief Sets the filter mode for a texture.
 *
 * \details When the ratio of texels for this texture to pixels is not 1:1, the filter type for \a minfilt or \a magfilt is used.
 *
 * \param[in] obj texture object to set the filters for
 * \param[in] minfilt filter mode to use when the texel/pixel ratio is >= 1.0; needs to be one of \ref texfilter.
 * \param[in] magfilt filter mode to use when the texel/pixel ratio is < 1.0; needs to be \a GX_NEAR or \a GX_LINEAR
 */
void GX_InitTexObjFilterMode(GXTexObj *obj,u8 minfilt,u8 magfilt);

/*!
 * \fn void GX_InitTexObjMinLOD(GXTexObj *obj,f32 minlod)
 * \brief Sets the minimum LOD for a given texture.
 *
 * \param[in] obj texture to set the minimum LOD for
 * \param[in] minlod minimum LOD value; the hardware will use MAX(min_lod, lod); range is 0.0 to 10.0.
 */
void GX_InitTexObjMinLOD(GXTexObj *obj,f32 minlod);

/*!
 * void GX_InitTexObjMaxLOD(GXTexObj *obj,f32 maxlod)
 * \brief Sets the maximum LOD for a given texture.
 *
 * \param[in] obj texture to set the maximum LOD for
 * \param[in] maxlod maximum LOD value; the hardware will use MIN(max_lod, lod); range is 0.0 to 10.0.
 */
void GX_InitTexObjMaxLOD(GXTexObj *obj,f32 maxlod);

/*!
 * \fn void GX_InitTexObjLODBias(GXTexObj *obj,f32 lodbias)
 * \brief Sets the LOD bias for a given texture.
 *
 * \details The LOD computed by the graphics hardware can be biased using this function. The \a lodbias is added to the computed lod and the
 * result is clamped between the values given to GX_InitTexObjMinLOD() and GX_InitTexObjMaxLOD(). If \a GX_ENABLE is given to
 * GX_InitTexObjBiasClamp(), the effect of \a lodbias will diminish as the polygon becomes more perpendicular to the view direction.
 *
 * \param[in] obj texture to set the LOD bias for
 * \param[in] lodbias bias to add to computed LOD value
 */
void GX_InitTexObjLODBias(GXTexObj *obj,f32 lodbias);

/*!
 * \fn void GX_InitTexObjBiasClamp(GXTexObj *obj,u8 biasclamp)
 * \brief Enables bias clamping for texture LOD.
 *
 * \details If \a biasclamp is \a GX_ENABLE, the sum of LOD and \a lodbias (given in GX_InitTexObjLODBias()) is clamped so that it is never
 * less than the minimum extent of the pixel projected in texture space. This prevents over-biasing the LOD when the polygon is perpendicular
 * to the view direction.
 *
 * \param[in] obj texture to set the bias clamp value for
 * \param[in] biasclamp whether or not to enable the bias clamp
 */
void GX_InitTexObjBiasClamp(GXTexObj *obj,u8 biasclamp);

/*!
 * \fn void GX_InitTexObjEdgeLOD(GXTexObj *obj,u8 edgelod)
 * \brief Changes LOD computing mode.
 *
 * \details When set to \a GX_ENABLE, the LOD is computed using adjacent texels; when \a GX_DISABLE, diagonal texels are used instead. This
 * should be set to \a GX_ENABLE if you use bias clamping (see GX_InitTexObjBiasClamp()) or anisotropic filtering (GX_ANISO_2 or GX_ANISO_4
 * for GX_InitTexObjMaxAniso() argument).
 *
 * \param[in] obj texture to set the edge LOD for
 * \param[in] edgelod mode to set LOD computation to
 */
void GX_InitTexObjEdgeLOD(GXTexObj *obj,u8 edgelod);

/*!
 * \fn void GX_InitTexObjMaxAniso(GXTexObj *obj,u8 maxaniso)
 * \brief Sets the maximum anisotropic filter to use for a texture.
 *
 * \details Anisotropic filtering is accomplished by iterating the square filter along the direction of anisotropy to better approximate the
 * quadralateral. This type of filtering results in sharper textures at the expense of multiple cycles per quad. The hardware will only use
 * multiple cycles when necessary, and the maximum number of cycles is clamped by the \a maxaniso parameter, so setting \a maxaniso to
 * \a GX_ANISO_2 will use at most 2 filter cycles per texture.
 *
 * \note These filter cycles are internal to the texture filter hardware and do not affect the available number of TEV stages. When setting
 * \a maxaniso to \a GX_ANISO_2 or \a GX_ANISO_4, the \a minfilt parameter given to GX_InitTexObjFilterMode() should be set to
 * \a GX_LIN_MIP_LIN.
 *
 * \param[in] obj texture to set the max anisotropy value to
 * \param[in] maxaniso the maximum anistropic filter to use; must be one of \ref anisotropy
 */
void GX_InitTexObjMaxAniso(GXTexObj *obj,u8 maxaniso);

/*!
 * \fn GX_InitTexObjUserData(GXTexObj *obj,void *userdata)
 * \brief Used to set a pointer to user data in the \ref GXTexObj structure.
 *
 * \details You can use this function to attach private data structures to the texture object. This pointer can be retrieved using GX_GetTexObjUserData().
 *
 * \param[in] obj ptr to a texture object
 * \param[in] userdata pointer to your data to attach to this texture
 */
void GX_InitTexObjUserData(GXTexObj *obj,void *userdata);

/*!
 * \fn void GX_LoadTexObj(GXTexObj *obj,u8 mapid)
 * \brief Loads the state describing a texture into one of eight hardware register sets.
 *
 * \details Before this happens, the texture object \a obj should be initialized using GX_InitTexObj() or GX_InitTexObjCI(). The \a id parameter refers to
 * the texture state register set. Once loaded, the texture can be used in any Texture Environment (TEV) stage using GX_SetTevOrder().
 *
 * \note This function will call the functions set by GX_SetTexRegionCallback() (and GX_SetTlutRegionCallback() if the texture is color-index
 * format) to obtain the texture regions associated with this texture object. These callbacks are set to default functions by GX_Init().
 *
 * \warning If the texture is a color-index texture, you <b>must</b> load the associated TLUT (using GX_LoadTlut()) before calling GX_LoadTexObj().
 *
 * \param[in] obj ptr to a texture object
 * \param[in] mapid \ref texmapid, <tt>GX_TEXMAP0</tt> to <tt>GX_TEXMAP7</tt> only
 *
 * \return none
 */
void GX_LoadTexObj(GXTexObj *obj,u8 mapid);

/*!
 * \fn void GX_LoadTlut(GXTlutObj *obj,u32 tlut_name)
 * \brief Copies a Texture Look-Up Table (TLUT) from main memory to Texture Memory (TMEM).
 *
 * \details The \a tlut_name parameter is the name of a pre-allocated area of TMEM. The callback function set by GX_SetTlutRegionCallback() converts
 * the \a tlut_name into a \ref GXTlutRegion pointer. The TLUT is loaded in the TMEM region described by this pointer. The TLUT object \a obj describes the
 * location of the TLUT in main memory, the TLUT format, and the TLUT size. \a obj should have been previously initialized using GX_InitTlutObj().
 *
 * \note GX_Init() sets a default callback to convert \a tlut_names from \ref tlutname to \ref GXTlutRegion pointers. The default configuration of
 * TMEM has 20 TLUTs, 16 each 256 entries by 16 bits, and 4 each 1k entries by 16 bits. This configuration can be overriden by calling
 * GX_InitTlutRegion() and GX_InitTexCacheRegion() to allocate TMEM. Then you can define your own region allocation scheme using GX_SetTlutRegionCallback()
 * and GX_SetTexRegionCallback().
 *
 * \param[in] obj ptr to a TLUT object; application must allocate this
 * \param[in] tlut_name \ref tlutname
 *
 * \return none
 */
void GX_LoadTlut(GXTlutObj *obj,u32 tlut_name);

/*!
 * \fn void GX_LoadTexObjPreloaded(GXTexObj *obj,GXTexRegion *region,u8 mapid)
 * \brief Loads the state describing a preloaded texture into one of eight hardware register sets.
 *
 * \details Before this happens, the texture object \a obj should be initialized using GX_InitTexObj() or GX_InitTexObjCI(). The \a mapid parameter refers to
 * the texture state register set. The texture should be loaded beforehand using GX_PreloadEntireTexture(). Once loaded, the texture can be used in any Texture Environment
 * (TEV) stage using GX_SetTevOrder().
 *
 * \note GX_Init() initially calls GX_SetTevOrder() to make a simple texture pipeline that associates <tt>GX_TEXMAP0</tt> with <tt>GX_TEVSTAGE0</tt>,
 * <tt>GX_TEXMAP1</tt> with <tt>GX_TEVSTAGE1</tt>, etc.<br><br>
 *
 * \note GX_LoadTexObjPreloaded() will not call the functions set by GX_SetTexRegionCallback() (and GX_SetTlutRegionCallback() if the texture is color
 * index format) because the region is set explicitly; however, these callback functions must be aware of all regions that are preloaded. The default
 * callbacks set by GX_Init() assume there are no preloaded regions.
 *
 * \param[in] obj ptr to a texture object
 * \param[in] region ptr to a region object that describes an area of texture memory
 * \param[in] mapid \ref texmapid for reference in a TEV stage
 *
 * \return none
 */
void GX_LoadTexObjPreloaded(GXTexObj *obj,GXTexRegion *region,u8 mapid);

/*!
 * \fn void GX_PreloadEntireTexture(GXTexObj *obj,GXTexRegion *region)
 * \brief Loads a given texture from DRAM into the texture memory.
 *
 * \details Accesses to this texture will bypass the texture cache tag look-up and instead read the texels directly from texture memory. The
 * texture region must be the same size as the texture (see GX_InitTexPreloadRegion()).
 *
 * \note This function loads the texture into texture memory, but to use it as a source for the Texture Environment (TEV) unit, you must first
 * call GX_LoadTexObjPreloaded(). The default configuration (as set by GX_Init()) of texture memory has no preloaded regions, so you must install
 * your own region allocator callbacks using GX_SetTexRegionCallback() and GX_SetTlutRegionCallback().
 *
 * \param[in] obj ptr to object describing the texture to laod
 * \param[in] region TMEM texture region to load the texture into
 *
 * \return none
 */
void GX_PreloadEntireTexture(GXTexObj *obj,GXTexRegion *region);

/*!
 * \fn void GX_InitTlutObj(GXTlutObj *obj,void *lut,u8 fmt,u16 entries)
 * \brief Initializes a Texture Look-Up Table (TLUT) object.
 *
 * \details The TLUT object describes the location of the TLUT in main memory, its format and the number of entries. The TLUT in main
 * memory described by this object can be loaded into a TLUT allocated in the texture memory using the GX_LoadTlut() function.
 *
 * \param[in] obj ptr to a TLUT object
 * \param[in] lut ptr to look-up table data; must be 32B aligned
 * \param[in] fmt format of the entries in the TLUt; <tt>GX_TL_IA8</tt>, <tt>GX_TL_RGB565</tt> or <tt>GX_TL_RGB5A3</tt>
 * \param[in] entries number of entries in this table; maximum is 16,384
 *
 * \return none
 */
void GX_InitTlutObj(GXTlutObj *obj,void *lut,u8 fmt,u16 entries);

/*!
 * \fn void GX_InitTlutRegion(GXTlutRegion *region,u32 tmem_addr,u8 tlut_sz)
 * \brief Initializes a Texture Look-Up Table (TLUT) region object.
 *
 * \note GX_Init() creates default TLUT regions, so the application does not need to call this function unless a new configuration
 * of Texture Memory is desired. In that case, the application should also set a new TLUT region allocator using GX_SetTlutRegionCallback().
 *
 * \param[in] region obj ptr to a TLUT region struct; application must allocate this
 * \param[in] tmem_addr location of the TLU in TMEM; ptr must be aligned to table size
 * \param[in] tlut_sz size of the table
 *
 * \return none
 */
void GX_InitTlutRegion(GXTlutRegion *region,u32 tmem_addr,u8 tlut_sz);

/*!
 * \fn void GX_InitTexObjLOD(GXTexObj *obj,u8 minfilt,u8 magfilt,f32 minlod,f32 maxlod,f32 lodbias,u8 biasclamp,u8 edgelod,u8 maxaniso)
 * \brief Sets texture Level Of Detail (LOD) controls explicitly for a texture object.
 *
 * \details It is the application's responsibility to provide memory for a texture object. When initializing a texture object using GX_InitTexObj()
 * or GX_InitTexObjCI(), this information is set to default values based on the mipmap flag. This function allows the programmer to override those
 * defaults.
 *
 * \note This function should be called after GX_InitTexObj() or GX_InitTexObjCI() for a particular texture object.<br><br>
 *
 * \note Setting \a biasclamp prevents over-biasing the LOD when the polygon is perpendicular to the view direction.<br><br>
 *
 * \note \a edgelod should be set if \a biasclamp is set or \a maxaniso is set to <tt>GX_ANISO_2</tt> or <tt>GX_ANISO_4</tt>.<br><br>
 *
 * \note Theoretically, there is no performance difference amongst various magnification/minification filter settings except <tt>GX_LIN_MIP_LIN</tt> filter with
 * <tt>GX_TF_RGBA8</tt> texture format which takes twice as much as other formats. However, this argument is assuming an environment where texture cache always
 * hits. On real environments, you will see some performance differences by changing filter modes (especially minification filter) because cache-hit ratio
 * changes according to which filter mode is being used.
 *
 * \param[in] obj ptr to a texture object
 * \param[in] minfilt \ref texfilter to use when the texel/pixel ratio is >= 1.0
 * \param[in] magfilt \ref texfilter to use when the texel/pixel ratio is < 1.0; use only <tt>GX_NEAR</tt> or <tt>GX_LINEAR</tt>
 * \param[in] minlod minimum LOD value from 0.0 - 10.0 inclusive
 * \param[in] maxlod maximum LOD value from 0.0 - 10.0 inclusive
 * \param[in] lodbias bias to add to computed LOD value
 * \param[in] biasclamp if <tt>GX_ENABLE</tt>, clamp (LOD+lodbias) so that it is never less than the minimum extent of the pixel projected in texture space
 * \param[in] edgelod if <tt>GX_ENABLE</tt>, compute LOD using adjacent texels
 * \param[in] maxaniso \ref anisotropy to use
 *
 * \return none
 */
void GX_InitTexObjLOD(GXTexObj *obj,u8 minfilt,u8 magfilt,f32 minlod,f32 maxlod,f32 lodbias,u8 biasclamp,u8 edgelod,u8 maxaniso);

/*!
 * \fn void GX_SetTexCoordScaleManually(u8 texcoord,u8 enable,u16 ss,u16 ts)
 * \brief Overrides the automatic texture coordinate scaling (based upon the associated map size) and lets one manually assign the scale values that
 * are used for a given \a texcoord.
 *
 * \details Setting the \a enable parameter to <tt>GX_TRUE</tt> gives this behavior. The given \a texcoord retains these manual scale values until this function is
 * called again. This function is also used to return a given texture coordinate back to normal, automatic scaling (by setting \a enable to <tt>GX_FALSE</tt>).
 *
 * \note A texture coordinate is scaled after being computed by the relevant texgen and before the actual texture lookup  Normally, the scale value is set
 * according to the texture map that is associated with the texcoord by GX_SetTevOrder(). However, there are certain cases where a different scale value is
 * desirable. One such case is when using indirect tiled textures (see GX_SetTevIndTile()).
 *
 * \param[in] texcoord the \ref texcoordid being changed
 * \param[in] enable if <tt>GX_TRUE</tt>, scale will be set manually, otherwise set automatically and \a ss and \a ts ignored
 * \param[in] ss manual scale value for the S component of the coordinate
 * \param[in] ts manual scale value for the T component of the coordinate
 *
 * \return none
 */
void GX_SetTexCoordScaleManually(u8 texcoord,u8 enable,u16 ss,u16 ts);

/*!
 * \fn void GX_SetTexCoordBias(u8 texcoord,u8 s_enable,u8 t_enable)
 * \brief Sets the texture coordinate bias of a particular texture.
 *
 * \details Range bias is used with texture coordinates applied in <tt>GX_REPEAT</tt> wrap mode in order to increase the precision of texture coordinates
 * that spread out over a large range. The texture coordinate values for a primitive are biased (by an equal integer) towards zero early in the
 * graphics pipeline, thus preserving bits for calculation later in the pipe.  Since the coordinates are repeated, this bias by an integer should
 * have no effect upon the actual appearance of the texture.
 *
 * \note Texture coordinate range bias is something that is normally set automatically by the GX API (during GX_Begin()); however, when a texture
 * coordinate is being scaled manually (by using GX_SetTexCoordScaleManually()), the associated bias is no longer modified by GX. Thus,
 * GX_SetTexCoordBias() allows the bias to be changed while a texture coordinate is being manually controlled.
 *
 * \param[in] texcoord \ref texcoordid being changed
 * \param[in] s_enable enable or disable range bias in the S direction with <tt>GX_ENABLE</tt>/<tt>GX_DISABLE</tt>
 * \param[in] t_enable enable or disable range bias in the T direction with <tt>GX_ENABLE</tt>/<tt>GX_DISABLE</tt>
 *
 * \return none
 */
void GX_SetTexCoordBias(u8 texcoord,u8 s_enable,u8 t_enable);

/*!
 * \fn GXTexRegionCallback GX_SetTexRegionCallback(GXTexRegionCallback cb)
 * \brief Sets the callback function called by GX_LoadTexObj() to obtain an available texture region.
 *
 * \details GX_Init() calls this function to set a default region-assignment policy. A programmer can override this default region assignment
 * by implementing his own callback function. A pointer to the texture object and the texture map ID that are passed
 * to GX_LoadTexObj() are provided to the callback function.
 *
 * \param[in] cb ptr to a function that takes a pointer to a GXTexObj and a \ref texmapid as a parameter and returns a pointer to a \ref GXTexRegion.
 *
 * \return pointer to the previously set callback
 */
GXTexRegionCallback GX_SetTexRegionCallback(GXTexRegionCallback cb);

/*!
 * \fn GXTlutRegionCallback GX_SetTlutRegionCallback(GXTlutRegionCallback cb)
 * \brief Sets the callback function called by GX_LoadTlut() to find the region into which to load the TLUT.
 *
 * \details GX_LoadTexObj() will also call \a cb to obtain the Texture Look-up Table (TLUT) region when the texture forma
 * is color-index.
 *
 * GX_Init() calls GX_SetTlutRegionCallback() to set a default TLUT index-to-region mapping. The name for the TLUT from the texture
 * object is provided as an argument to the callback. The callback should return a pointer to the \ref GXTlutRegion for this TLUT index.
 *
 * \note For a given \a tlut_name (in the \ref GXTlutRegionCallback struct), \a cb must always return the same \ref GXTlutRegion; this is because
 * GX_LoadTlut() will initialize data into the \ref GXTlutRegion which GX_LoadTexObj() will subsequently use.
 *
 * \param[in] cb ptr to a function that takes a u32 TLUT name as a parameter and returns a pointer to a \ref GXTlutRegion.
 *
 * \return pointer to the previously set callback
 */
GXTlutRegionCallback GX_SetTlutRegionCallback(GXTlutRegionCallback cb);

/*!
 * \fn void GX_InitLightPos(GXLightObj *lit_obj,f32 x,f32 y,f32 z)
 * \brief Sets the position of the light in the light object.
 *
 * \details The GameCube graphics hardware supports local diffuse lights. The position of the light should be in the same space as a transformed
 * vertex position (i.e., view space).
 *
 * \note Although the hardware doesn't support parallel directional diffuse lights, it is possible to get "almost parallel" lights by setting
 * sufficient large values to position parameters (x, y and z) which makes the light position very far away from objects to be lit and all rays
 * considered almost parallel.<br><br>
 *
 * \note The memory for the light object must be allocated by the application; this function does not load any hardware registers directly. To
 * load a light object into a hardware light, use GX_LoadLightObj() or GX_LoadLightObjIdx().
 *
 * \param[in] lit_obj ptr to the light object
 * \param[in] x X coordinate to place the light at
 * \param[in] y Y coordinate to place the light at
 * \param[in] z Z coordinate to place the light at
 *
 * \return none
 */
void GX_InitLightPos(GXLightObj *lit_obj,f32 x,f32 y,f32 z);

/*!
 * \fn void GX_InitLightColor(GXLightObj *lit_obj,GXColor col)
 * \brief Sets the color of the light in the light object.
 *
 * \note The memory for the light object should be allocated by the application; this function does not load any hardware register directly.  To
 * load a light object into a hardware light, use GX_LoadLightObj() or GX_LoadLightObjIdx().
 *
 * \param[in] lit_obj ptr to the light object
 * \param[in] col color to set the light to
 *
 * \return none
 */
void GX_InitLightColor(GXLightObj *lit_obj,GXColor col);

/*!
 * \fn void GX_InitLightDir(GXLightObj *lit_obj,f32 nx,f32 ny,f32 nz)
 * \brief Sets the direction of a light in the light object.
 *
 * \details This direction is used when the light object is used as spotlight or a specular light (see the <i>attn_fn</i> parameter of GX_SetChanCtrl()).
 *
 * \note The coordinate space of the light normal should be consistent with a vertex normal transformed by a normal matrix; i.e., it should be
 * transformed to view space.<br><br>
 *
 * \note This function does not set the direction of parallel directional diffuse lights. If you want parallel diffuse lights, you may put the light
 * position very far from every objects to be lit. (See GX_InitLightPos() and GX_SetChanCtrl())<br><br>
 *
 * \note The memory for the light object must be allocated by the application; this function does not load any hardware registers.  To load a light
 * object into a hardware light, use GX_LoadLightObj() or GX_LoadLightObjIdx().
 *
 * \param[in] lit_obj ptr to the light object
 * \param[in] nx X coordinate of the light normal
 * \param[in] ny Y coordinate of the light normal
 * \param[in] nz Z coordinate of the light normal
 *
 * \return none
 */
void GX_InitLightDir(GXLightObj *lit_obj,f32 nx,f32 ny,f32 nz);

/*!
 * \fn void GX_LoadLightObj(GXLightObj *lit_obj,u8 lit_id)
 * \brief Loads a light object into a set of hardware registers associated with a \ref lightid.
 *
 * \details This function copies the light object data into the graphics FIFO through the CPU write-gather buffer mechanism. This guarantees that
 * the light object is coherent with the CPU cache.
 *
 * \note The light object must have been initialized first using the necessary GX_InitLight*() functions.<br><br>
 *
 * \note Another way to load a light object is with GX_LoadLightObjIdx().
 *
 * \param[in] lit_obj ptr to the light object to load
 * \param[in] lit_id \ref lightid to load this light into
 *
 * \return none
 */
void GX_LoadLightObj(GXLightObj *lit_obj,u8 lit_id);

/*!
 * \fn void GX_LoadLightObjIdx(u32 litobjidx,u8 litid)
 * \brief Instructs the GP to fetch the light object at \a ltobjindx from an array.
 *
 * \details The light object is retrieved from the array to which <tt>GX_SetArray(GX_VA_LIGHTARRAY, ...)</tt> points. Then it loads the object into
 * the hardware register associated with \ref lightid.
 *
 * \note Data flows directly from the array in DRAM to the GP; therefore, the light object data may not be coherent with the CPU's cache. The
 * application is responsible for storing the light object data from the CPU cache (using DCStoreRange()) before calling GX_LoadLightObjIdx().
 *
 * \param[in] litobjidx index to a light object
 * \param[in] litid \ref lightid to load this light into
 *
 * \return none
 */
void GX_LoadLightObjIdx(u32 litobjidx,u8 litid);

/*!
 * \fn void GX_InitLightDistAttn(GXLightObj *lit_obj,f32 ref_dist,f32 ref_brite,u8 dist_fn)
 * \brief Sets coefficients for distance attenuation in a light object.
 *
 * \details This function uses three easy-to-control parameters instead of <i>k0</i>, <i>k1</i>, and <i>k2</i> in GX_InitLightAttn().
 *
 * In this function, you can specify the brightness on an assumed reference point. The parameter \a ref_distance is distance between the light
 * and the reference point. The parameter \a ref_brite specifies ratio of the brightness on the reference point. The value for \a ref_dist should
 * be greater than 0 and that for \a ref_brite should be within 0 < \a ref_brite < 1, otherwise distance attenuation feature is turned off. The
 * parameter \a dist_fn defines type of the brightness decreasing curve by distance; <tt>GX_DA_OFF</tt> turns distance attenuation feature off.
 *
 * \note If you want more flexible control, it is better to use GX_InitLightAttn() and calculate appropriate coefficients.<br><br>
 *
 * \note This function sets parameters only for distance attenuation. Parameters for angular attenuation should be set by using
 * GX_InitLightSpot() or GX_InitLightAttnA().<br><br>
 *
 * \note This function does not load any hardware registers directly. To load a light object into a hardware light, use GX_LoadLightObj() or
 * GX_LoadLightObjIdx().
 *
 * \param[in] lit_obj ptr to a light object
 * \param[in] ref_dist distance between the light and reference point
 * \param[in] ref_brite brightness of the reference point
 * \param[in] dist_fn \ref distattnfn to use
 *
 * \return none
 */
void GX_InitLightDistAttn(GXLightObj *lit_obj,f32 ref_dist,f32 ref_brite,u8 dist_fn);

/*!
 * \fn void GX_InitLightAttn(GXLightObj *lit_obj,f32 a0,f32 a1,f32 a2,f32 k0,f32 k1,f32 k2)
 * \brief Sts coefficients used in the lighting attenuation calculation in a given light object.
 *
 * \details The parameters \a a0, \a a1, and \a a2 are used for angular (spotlight) attenuation. The coefficients \a k0, \a k1, and \a k2 are used for
 * distance attenuation. The attenuation function is:
 *
 * &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<i>atten</i> = <i>clamp0</i>(\a a2^2 * <i>aattn</i>^2 + \a a1 * <i>aattn</i> + \a a0) / (\a k2 * <i>d</i>^2 + \a k1  * <i>d</i> + \a k0)
 *
 * where <i>aattn</i> is the cosine of the angle between the light direction and the vector from the light position to the vertex, and <i>d</i> is
 * the distance from the light position to the vertex when the channel attenuation function is <tt>GX_AF_SPOT</tt>. The light color will be
 * multiplied by the <i>atten</i> factor when the attenuation function for the color channel referencing this light is set to <tt>GX_AF_SPOT</tt>
 * (see GX_SetChanCtrl()).
 *
 * \note The convenience function GX_InitLightSpot() can be used to set the angle attenuation coefficents based on several spot light
 * types. The convenience function GX_InitLightDistAttn() can be used to set the distance attenuation coefficients using one of several
 * common attenuation functions.<br><br>
 *
 * \note The convenience macro GX_InitLightShininess() can be used to set the attenuation parameters for specular lights.<br><br>
 *
 * \note When the channel attenuation function is set to <tt>GX_AF_SPEC</tt>, the <i>aattn</i> and <i>d</i> parameter are equal to the dot product of the
 * eye-space vertex normal and the half-angle vector set by GX_InitSpecularDir().<br><br>
 *
 * \note This function does not load any hardware registers directly. To load a light object into a hardware light, use GX_LoadLightObj()
 * or GX_LoadLightObjIdx().
 *
 * \param[in] lit_obj ptr to a light object
 * \param[in] a0 angle attenuation coefficient
 * \param[in] a1 angle attenuation coefficient
 * \param[in] a2 angle attenuation coefficient
 * \param[in] k0 distance attenuation coefficient
 * \param[in] k1 distance attenuation coefficient
 * \param[in] k2 distance attenuation coefficient
 *
 * \return none
 */
void GX_InitLightAttn(GXLightObj *lit_obj,f32 a0,f32 a1,f32 a2,f32 k0,f32 k1,f32 k2);

/*!
 * \fn void GX_InitLightAttnA(GXLightObj *lit_obj,f32 a0,f32 a1,f32 a2)
 * \brief Sets coefficients used in the lighting angle attenuation calculation in a given light object.
 *
 * \details The parameters \a a0, \a a1, and \a a2 are used for angular (spotlight) attenuation. The attenuation
 * function is:
 *
 * &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<i>atten</i> = <i>clamp0</i>(\a a2^2 * <i>cos(theta)</i>^2 + \a a1 * <i>cos(theta)</i> + \a a0) / (\a k2 * <i>d</i>^2 + \a k1  * <i>d</i> + \a k0)
 *
 * where <i>cos(theta)</i> is the cosine of the angle between the light normal and the vector from the light position to the vertex, and <i>d</i> is the distance
 * from the light position to the vertex. The \a k0-\a 2 coefficients can be set using GX_InitLightAttnK(). You can set both the \a a0-\a 2 and \a k0-\a 2 coefficients
 * can be set using GX_InitLightAttn(). The light color will be multiplied by the <i>atten</i> factor when the attenuation function for the color channel
 * referencing this light is set to <tt>GX_AF_SPOT</tt> (see GX_SetChanCtrl()).
 *
 * \note The convenience function GX_InitLightSpot() can be used to set the angle attenuation coefficents based on several spot light types. The
 * convenience function GX_InitLightDistAttn() can be used to set the distance attenuation coefficients using one of several common attenuation functions.<br><br>
 *
 * \note This function does not load any hardware registers directly. To load a light object into a hardware light, use GX_LoadLightObj() or GX_LoadLightObjIdx().
 *
 * \param[in] lit_obj ptr to a light object
 * \param[in] a0 angle attenuation coefficient
 * \param[in] a1 angle attenuation coefficient
 * \param[in] a2 angle attenuation coefficient
 *
 * \return none
 */
void GX_InitLightAttnA(GXLightObj *lit_obj,f32 a0,f32 a1,f32 a2);

/*!
 * \fn void GX_InitLightAttnK(GXLightObj *lit_obj,f32 k0,f32 k1,f32 k2)
 * \brief Sets coefficients used in the lighting distance attenuation calculation in a given light object.
 *
 * \details The coefficients \a k0, \a k1, and \a k2 are used for distance attenuation. The attenuation function is:
 *
 * &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<i>atten</i> = <i>clamp0</i>(\a a2^2 * <i>cos(theta)</i>^2 + \a a1 * <i>cos(theta)</i> + \a a0) / (\a k2 * <i>d</i>^2 + \a k1  * <i>d</i> + \a k0)
 *
 * where <i>cos(theta)</i> is the cosine of the angle between the light normal and the vector from the light position to the vertex, and <i>d</i> is the distance
 * from the light position to the vertex. The \a a0-\a 2 coefficients can be set using GX_InitLightAttnA(). You can set both the \a a0-\a 2 and \a k0-\a 2 coefficients
 * can be set using GX_InitLightAttn(). The light color will be multiplied by the <i>atten</i> factor when the attenuation function for the color channel
 * referencing this light is set to <tt>GX_AF_SPOT</tt> (see GX_SetChanCtrl()).
 *
 * \note The convenience function GX_InitLightSpot() can be used to set the angle attenuation coefficents based on several spot light types. The convenience
 * function GX_InitLightDistAttn() can be used to set the distance attenuation coefficients using one of several common attenuation functions.<br><br>
 *
 * \note Note that this function does not load any hardware registers directly. To load a light object into a hardware light, use GX_LoadLightObj() or
 * GX_LoadLightObjIdx().
 *
 * \param[in] lit_obj ptr to a light object
 * \param[in] k0 distance attenuation coefficient
 * \param[in] k1 distance attenuation coefficient
 * \param[in] k2 distance attenuation coefficient
 *
 * \return none
 */
void GX_InitLightAttnK(GXLightObj *lit_obj,f32 k0,f32 k1,f32 k2);

/*!
 * \fn void GX_InitSpecularDirHA(GXLightObj *lit_obj,f32 nx,f32 ny,f32 nz,f32 hx,f32 hy,f32 hz)
 * \brief Sets the direction and half-angle vector of a specular light in the light object.
 *
 * \details These vectors are used when the light object is used only as specular light. In contrast to GX_InitSpecularDir(),
 * which caclulates half-angle vector automatically by assuming the view vector as (0, 0, 1), this function allows users to
 * specify half-angle vector directly as input arguments. It is useful to do detailed control for orientation of highlights.
 *
 * \note This function does not load any hardware registers. To load a light object into a hardware light, use GX_LoadLightObj()
 * or GX_LoadLightObjIdx().<br><br>
 *
 * \note Other notes are similar to those described in GX_InitSpecularDir().
 *
 * \param[in] lit_obj ptr to a light object
 * \param[in] nx X coordinate of the light normal
 * \param[in] ny Y coordinate of the light normal
 * \param[in] nz Z coordinate of the light normal
 * \param[in] hx X coordinate of half-angle
 * \param[in] hy Y coordinate of half-angle
 * \param[in] hz Z coordinate of half-angle
 *
 * \return none
 */
void GX_InitSpecularDirHA(GXLightObj *lit_obj,f32 nx,f32 ny,f32 nz,f32 hx,f32 hy,f32 hz);

/*!
 * \fn void GX_InitSpecularDir(GXLightObj *lit_obj,f32 nx,f32 ny,f32 nz)
 * \brief Sets the direction of a specular light in the light object.
 *
 * \details This direction is used when the light object is used only as specular light. The coordinate space of the light normal
 * should be consistent with a vertex normal transformed by a normal matrix; i.e., it should be transformed to view space.
 *
 * \note This function should be used if and only if the light object is used as specular light. One specifies a specular light in
 * GX_SetChanCtrl() by setting the \ref attenfunc to <tt>GX_AF_SPEC</tt>. Furthermore, one must not use GX_InitLightDir() or
 * GX_InitLightPos() to set up a light object which will be used as a specular light since these functions will destroy the information
 * set by GX_InitSpecularDir(). In contrast to diffuse lights (including spotlights) that are considered local lights, a specular light
 * is a parallel light (i.e. the specular light is infinitely far away such that all the rays of the light are parallel), and thus one
 * can only specify directional information.
 *
 * \note This function does not load any hardware registers. To load a light object into a hardware light, use GX_LoadLightObj()
 * or GX_LoadLightObjIdx().
 *
 * \param[in] lit_obj ptr to a light object
 * \param[in] nx X coordinate of the light normal
 * \param[in] ny Y coordinate of the light normal
 * \param[in] nz Z coordinate of the light normal
 *
 * \return none
 */
void GX_InitSpecularDir(GXLightObj *lit_obj,f32 nx,f32 ny,f32 nz);

/*!
 * \fn void GX_InitLightSpot(GXLightObj *lit_obj,f32 cut_off,u8 spotfn)
 * \brief Sets coefficients for angular (spotlight) attenuation in light object.
 *
 * \details This function uses two easy-to-control parameters instead of \a a0, \a a1, and \a a2 on GX_InitLightAttn().
 *
 * \details The parameter \a cut_off specifies cutoff angle of the spotlight by degree. The spotlight works while the angle between the ray for a vertex and
 * the light direction given by GX_InitLightDir() is smaller than this cutoff angle. The value for \a cut_off should be within 0 < \a cut_off <= 90.0, otherwise
 * given light object doesn't become a spotlight.
 *
 * The parameter \a spotfn defines type of the illumination distribution within cutoff angle. The value <tt>GX_SP_OFF</tt> turns spotlight feature off even if
 * color channel setting is using <tt>GX_AF_SPOT</tt> (see GX_SetChanCtrl()).
 *
 * \note This function can generate only some kind of simple spotlights. If you want more flexible control, it is better to use GX_InitLightAttn() and calculate
 * appropriate coefficients.<br><br>
 *
 * \note This function sets parameters only for angular attenuation. Parameters for distance attenuation should be set by using GX_InitLightDistAttn() or
 * GX_InitLightAttnK().<br><br>
 *
 * \note This function does not load any hardware registers directly. To load a light object into a hardware light, use GX_LoadLightObj() or GX_LoadLightObjIdx().
 *
 * \param[in] lit_obj ptr to a light object
 * \param[in] cut_off cutoff angle of the spotlight, in degrees
 * \param[in] spotfn \ref spotfn to use for this light
 *
 * \return none
 */
void GX_InitLightSpot(GXLightObj *lit_obj,f32 cut_off,u8 spotfn);

u32 GX_ReadClksPerVtx();
u32 GX_GetOverflowCount();
u32 GX_ResetOverflowCount();

/*!
 * \fn lwp_t GX_GetCurrentGXThread()
 * \brief Returns the current GX thread.
 *
 * \details The current GX thread should be the thread that is currently responsible for generating graphics data. By default,
 * the GX thread is the thread that invoked GX_Init(); however, it may be changed by calling GX_SetCurrentGXThread().
 *
 * \note When graphics data is being generated in immediate mode (that is, the CPU FIFO = GP FIFO, and the GP is actively consuming
 * data), the high watermark may be triggered. When this happens, the high watermark interrupt handler will suspend the GX thread, thus
 * preventing any further graphics data from being generated. The low watermark interrupt handler will resume the thread.
 *
 * \return the current GX thread
 */
lwp_t GX_GetCurrentGXThread();

/*!
 * \fn lwp_t GX_SetCurrentGXThread()
 * \brief Sets the current GX thread to the calling thread.
 *
 * \details The new thread should be the thread that will be responsible for generating graphics data. By default, the GX thread is
 * the thread that invoked GX_Init(); however, it may be changed by calling this function.
 *
 * \note It is a programming error to change GX thread while the current GX thread is suspended by a high water mark interrupt. This
 * indicates that you have two threads about to generate GX data.<br><br>
 *
 * \note When graphics data is being generated in immediate mode (that is, the CPU FIFO = GP FIFO, and the GP is actively consuming
 * data), the high watermark may be triggered. When this happens, the high watermark interrupt handler will suspend the GX thread, thus
 * preventing any further graphics data from being generated. The low watermark interrupt handler will resume the thread.
 *
 * \return the previous GX thread ID
 */
lwp_t GX_SetCurrentGXThread();

/*!
 * \fn void GX_RestoreWriteGatherPipe()
 * \brief Restores the write-gather pipe.
 *
 * \details The CPU fifo that was attached at the time GX_RedirectWriteGatherPipe() was called will be re-attached. If there is data pending
 * in the write gather pipe (e.g. if the amount of data written was not a multiple of 32 bytes), the data will be padded with zeroes and
 * flushed out.
 *
 * \warning This function must be called between successive calls to GX_RedirectWriteGatherPipe().
 *
 * \return none
 */
void GX_RestoreWriteGatherPipe();

/*!
 * \fn void GX_SetGPMetric(u32 perf0,u32 perf1)
 * \brief Sets two performance metrics to measure in the GP.
 *
 * \details perf0 and perf1 are set to measure. The initial metrics measured are <tt>GX_PERF0_NONE</tt> and <tt>GX_PERF1_NONE</tt>, which return counts of zero
 * for the first call to GX_ReadGPMetric().
 *
 * Each performance counter has a unique set of events or ratios that it can count. In some cases the same metric can be counted using both
 * counters, for example <tt>GX_PERF0_VERTICES</tt> and <tt>GX_PERF1_VERTICES</tt>. Ratios (the metric name ends in <tt>_RATIO</tt>) are multiplied by
 * 1000 (1000 = all misses/clips, etc., 0 = no misses/clips, etc.).
 *
 * \note GX_ReadGPMetric() and GX_ClearGPMetric() can be used in the callback associated with the draw sync interrupt (see GX_SetDrawSyncCallback()).
 * This function should not be used in the draw sync callback because it will insert tokens in the GP command stream at random times.
 *
 * \warning This function reads results from CPU-accessible registers in the GP, therefore, this command <i>must not</i> be used in a display list. In
 * addition, the performance counters in some cases are triggered by sending tokens through the Graphics FIFO to the GP.  This implies that
 * the function should only be used in immediate mode (when the Graphics FIFO is connected to the CPU and the GP at the same time).  It may
 * also be necessary to send a draw sync token using GX_SetDrawSync() or call GX_SetDrawDone() after GX_ReadGPMetric() to ensure that the
 * state has actually been processed by the GP.
 *
 * \param[in] perf0 \ref perf0metrics to measure
 * \param[in] perf1 \ref perf1metrics to measure
 *
 * \returns none
 */
void GX_SetGPMetric(u32 perf0,u32 perf1);

/*!
 * \fn void GX_ClearGPMetric()
 * \brief Clears the two virtual GP performance counters to zero.
 *
 * \note The counter's function is set using GX_SetGPMetric(); the counter's value is read using GX_ReadGPMetric(). Consult these for more details.
 *
 * \warning This function resets CPU accessible counters, so it should <b>not</b> be used in a display list.
 *
 * \return none
 */
void GX_ClearGPMetric();

/*!
 * \fn void GX_InitXfRasMetric()
 * \brief Initialize the transformation unit (XF) rasterizer unit (RAS) to take performance measurements.
 *
 * \warning This function should be avoided; use the GP performance metric functions instead.
 *
 * \return none
 */
void GX_InitXfRasMetric();

/*!
 * \fn void GX_ReadXfRasMetric(u32 *xfwaitin,u32 *xfwaitout,u32 *rasbusy,u32 *clks)
 * \brief Read performance metric values from the XF and RAS units.
 *
 * \warning This function should be avoided; use the GP performance metric functions instead.<br><br>
 *
 * \warning The parameters for this function are a best guess based on names and existing code.
 *
 * \param[out] xfwaitin Number of clocks the XF has waited for data to arrive?
 * \param[out] xfwaitout Number of clocks the XF has waited to push finished data down?
 * \param[out] rasbusy Number of clocks the RAS has spent being busy?
 * \param[out] clks Clocks that have passed since last count reset?
 *
 * \return none
 */
void GX_ReadXfRasMetric(u32 *xfwaitin,u32 *xfwaitout,u32 *rasbusy,u32 *clks);

/*!
 * \fn void GX_ClearVCacheMetric()
 * \brief Clears the Vertex Cache performance counter.
 *
 * \details This function clears the performance counter by sending a special clear token via the Graphics FIFO.
 *
 * \note To set the metric for the counter, call GX_SetVCacheMetric(); to read the counter value, call GX_ReadVCacheMetric().
 *
 * \return none
 */
void GX_ClearVCacheMetric();

/*!
 * \fn void GX_ReadVCacheMetric(u32 *check,u32 *miss,u32 *stall)
 * \brief Returns Vertex Cache performance counters.
 *
 * \details Each call to this function resets the counter to zero. GX_SetVCacheMetric() sets the metric to be measured by
 * the Vertex Cache performance counter.
 *
 * \warning This function reads CPU-accessible registers in the GP and so should not be called in a display list.
 *
 * \param[out] check total number of accesses to the vertex cache
 * \param[out] miss total number of cache misses to the vertex cache
 * \param[out] stall number of GP clocks that the vertex cache was stalled
 *
 * \return none
 */
void GX_ReadVCacheMetric(u32 *check,u32 *miss,u32 *stall);

/*!
 * \fn void GX_SetVCacheMetric(u32 attr)
 * \brief Sets the metric the Vertex Cache performance counter will measure.
 *
 * \details It is possible to monitor a particular attribute or all attributes using \a attr.
 *
 * \note To clear the counter, call GX_ClearVCacheMetric(); to read the counter value, call GX_ReadVCacheMetric().
 *
 * \param[in] attr \ref vcachemetrics to measure
 *
 * \return none
 */
void GX_SetVCacheMetric(u32 attr);

/*!
 * \fn void GX_GetGPStatus(u8 *overhi,u8 *underlow,u8 *readIdle,u8 *cmdIdle,u8 *brkpt)
 * \brief Reads the current status of the GP.
 *
 * \details \a overhi and \a underlow will indicate whether or not the watermarks have been reached. If the CPU and GP FIFOs
 * are the same, then \a overhi will indicate whether or not the current GX thread is suspended. The value of \a brkpt can be
 * used to determine if a breakpoint is in progress (i.e. GP reads are suspended; they are resumed by a call to
 * GX_DisableBreakPt()). A callback can also be used to notify your application that the break point has been reached. (see
 * GX_SetBreakPtCallback())
 *
 * \param[out] overhi <tt>GX_TRUE</tt> if high watermark has been passed
 * \param[out] underlow <tt>GX_TRUE</tt> if low watermark has been passed
 * \param[out] readIdle <tt>GX_TRUE</tt> if the GP read unit is idle
 * \param[out] cmdIdle <tt>GX_TRUE</tt> if all commands have been flushed to XF
 * \param[out] brkpt <tt>GX_TRUE</tt> if FIFO has reached a breakpoint and GP reads have been stopped
 *
 * \return none
 */
void GX_GetGPStatus(u8 *overhi,u8 *underlow,u8 *readIdle,u8 *cmdIdle,u8 *brkpt);

/*!
 * \fn void GX_ReadGPMetric(u32 *cnt0,u32 *cnt1)
 * \brief Returns the count of the previously set performance metrics.
 *
 * \note The performance metrics can be set using GX_SetGPMetric(); the counters can be cleared using GX_ClearGPMetric().<br><br>
 *
 * \note GX_ReadGPMetric() and GX_ClearGPMetric() can be used in the callback associated with the draw sync interrupt (see GX_SetDrawSyncCallback()).
 * The function GX_SetGPMetric() should <b>not</b> be used in the draw sync callback because it will insert tokens in the GP command stream at random times.<br><br>
 *
 * \warning This function reads results from CPU-accessible registers in the GP, therefore, this command <i>must not</i> be used in a display list. It
 * may also be necessary to send a draw sync token using GX_SetDrawSync() or GX_SetDrawDone() before GX_ReadGPMetric() is called to ensure that the
 * state has actually been processed by the GP.
 *
 * \param[out] cnt0 current value of GP counter 0
 * \param[out] cnt1 current value of GP counter 1
 *
 * \return none
 */
void GX_ReadGPMetric(u32 *cnt0,u32 *cnt1);

/*!
 * \fn void GX_ReadBoundingBox(u16 *top,u16 *bottom,u16 *left,u16 *right)
 * \brief Returns the bounding box of pixel coordinates that are drawn in the Embedded Framebuffer (EFB).
 *
 * \details This function reads the bounding box values. GX_ClearBoundingBox() can be used reset the values of the bounding box.
 *
 * \note Since the hardware can only test the bounding box in quads (2x2 pixel blocks), the result of this function may contain error
 * of plus or minus 1 pixel. Also because of this, <b>left</b> and <b>top</b> are always even-numbered and <b>right</b> and <b>bottom</b>
 * are always odd-numbered.
 *
 * \param[out] top uppermost line in the bounding box
 * \param[out] bottom lowest line in the bounding box
 * \param[out] left leftmost pixel in the bounding box
 * \param[out] right rightmost pixel in the bounding box
 *
 * \return none
 */
void GX_ReadBoundingBox(u16 *top,u16 *bottom,u16 *left,u16 *right);

/*!
 * \fn volatile void* GX_RedirectWriteGatherPipe(void *ptr)
 * \brief Temporarily points the CPU's write-gather pipe at a new location.
 *
 * \details After calling this function, subsequent writes to the address returned by this function (or the WGPipe union)
 * will be gathered and sent to a destination buffer. The write pointer is automatically incremented by the GP.  The write
 * gather pipe can be restored by calling GX_RestoreWriteGatherPipe(). This function cannot be called between a
 * GX_Begin()/GX_End() pair.
 *
 * \note The destination buffer, referred to by \a ptr, must be 32 byte aligned. The amount of data written should
 * also be 32-byte aligned. If it is not, zeroes will be added to pad the destination buffer to 32 bytes. No part of the
 * destination buffer should be modified inside the CPU caches - this may introduce cache incoherency problems.<br><br>
 *
 * \note The write gather pipe is one of the fastest ways to move data out of the CPU (the other being the locked cache DMA).
 * In general, you are compute-bound when sending data from the CPU.<br><br>
 *
 * \note This function is cheaper than trying to create a fake CPU fifo around a destination buffer, which requires calls to
 * GX_SetCPUFifo(), GX_InitFifoBase(), etc. This function performs very light weight state saves by assuming that the CPU and
 * GP FIFOs never change.
 *
 * \warning <b>No GX commands can be called until the write gather pipe is restored. You MUST call
 * GX_RestoreWriteGatherPipe() before calling this function again, or else the final call to restore the pipe will fail.</b>
 *
 * \param[in] ptr to destination buffer, 32-byte aligned
 *
 * \return real address of the write-gather "port". All writes to this address will be gathered by the CPU write gather pipe.
 * You may also use the WGPipe union. If you do not use the WGPipe union, ensure that your local variable is volatile.
 */
volatile void* GX_RedirectWriteGatherPipe(void *ptr);

/*!
 * \def GX_InitLightPosv(lo,vec)
 * \brief Sets the position of the light in the light object using a vector structure.
 *
 * \note The GameCube graphics hardware supports local diffuse lights. The position of the light should be in the same space as a
 * transformed vertex position (i.e. view space).<br><br>
 *
 * \note The memory for the light object must be allocated by the application; this function does not load any hardware registers directly. To
 * load a light object into a hardware light, use GX_LoadLightObj() or GX_LoadLightObjIdx().
 *
 * \param[in] lo ptr to the light object
 * \param[in] vec struct or array of three values for the position
 *
 * \return none
 */
#define GX_InitLightPosv(lo,vec) \
    (GX_InitLightPos((lo), *(f32*)(vec), *((f32*)(vec)+1), *((f32*)(vec)+2)))

/*!
 * \def GX_InitLightDirv(lo,vec)
 * \brief Sets the direction of a light in the light object using a vector structure.
 *
 * \details This direction is used when the light object is used as a spotlight or a specular light, see the \a attn_fn parameter of
 * GX_SetChanCtrl().
 *
 * \note The memory for the light object must be allocated by the application; this function does not load any hardware registers. To load a
 * light object into a hardware light, use GX_LoadLightObj() or GX_LoadLightObjIdx().<br><br>
 *
 * \note The coordinate space of the light normal should be consistent with a vertex normal transformed by a normal matrix; i.e., it should be
 * transformed to view space.<br><br>
 *
 * \note This function does not set the direction of parallel lights.
 *
 * \param[in] lo ptr to the light object
 * \param[in] vec struct or array of three values for the direction
 *
 * \return none
 */
#define GX_InitLightDirv(lo,vec) \
    (GX_InitLightDir((lo), *(f32*)(vec), *((f32*)(vec)+1), *((f32*)(vec)+2)))

/*!
 * \def GX_InitSpecularDirv(lo,vec)
 * \brief Sets the direction of a specular light in the light object using a vector.
 *
 * \details This direction is used when the light object is used only as specular light.
 *
 * \note The memory for the light object must be allocated by the application; this function does not load any hardware registers. To load a
 * light object into a hardware light, use GX_LoadLightObj() or GX_LoadLightObjIdx().<br><br>
 *
 * \note The coordinate space of the light normal should be consistent with a vertex normal transformed by a normal matrix; i.e., it should
 * be transformed to view space.
 *
 * \warning This function should be used if and only if the light object is used as specular light. One specifies a specular light in
 * GX_SetChanCtrl() by setting \a attn_fn to <tt>GX_AF_SPEC</tt>. Furthermore, one must not use GX_InitLightDir() or GX_InitLightPos() to
 * set up a light object which will be used as a specular light since these functions will destroy the information set by GX_InitSpecularDir().
 * In contrast to diffuse lights (including spotlights) that are considered <i>local</i> lights, a specular light is a <i>parallel</i> light (i.e. the
 * specular light is infinitely far away such that all the rays of the light are parallel), and thus one can only specify directional
 * information.
 *
 * \param[in] lo ptr to the light object
 * \param[in] vec struct or array of three values for the direction
 *
 * \return none
 */
#define GX_InitSpecularDirv(lo,vec) \
    (GX_InitSpecularDir((lo), *(f32*)(vec), *((f32*)(vec)+1), *((f32*)(vec)+2)))

/*!
 * \def GX_InitSpecularDirHAv(lo,vec0,vec1)
 * \brief Sets the direction and half-angle vector of a specular light in the light object using a vector.
 *
 * \details These vectors are used when the light object is used only as specular light.
 *
 * \note The memory for the light object must be allocated by the application; this function does not load any hardware registers. To load a
 * light object into a hardware light, use GX_LoadLightObj() or GX_LoadLightObjIdx().<br><br>
 *
 * \note In contrast to GX_InitSpecularDirv(), which caclulates half-angle vector automatically by assuming the view vector as (0,0,1), this
 * function allows users to specify half-angle vector directly as input arguments. It is useful to do detailed control for orientation of
 * highlights.<br><br>
 *
 * \note Other notes are similar to that described in GX_InitSpecularDirv().
 *
 * \param[in] lo ptr to the light object
 * \param[in] vec0 struct or array of three values for the direction
 * \param[in] vec1 struct or array of three values for the half-angle
 *
 * \return none
 */
#define GX_InitSpecularDirHAv(lo,vec0,vec1) \
    (GX_InitSpecularDirHA((lo), \
    *(f32*)(vec0), *((f32*)(vec0)+1), *((f32*)(vec0)+2), \
    *(f32*)(vec1), *((f32*)(vec1)+1), *((f32*)(vec1)+2)))

/*!
 * \def GX_InitLightShininess(lobj, shininess)
 * \brief Sets \a shininess of a per-vertex specular light.
 *
 * \details In reality, shininess is a property of the <i>material</i> being lit, not the light. However, in the Graphics Processor, the specular
 * calculation is implemented by reusing the diffuse angle/distance attenuation function, so shininess is determined by the light attenuation
 * parameters (see GX_InitLightAttn()). Note that the equation is attempting to approximate the function (N*H)^shininess. Since the attenuation
 * equation is only a ratio of quadratics, a true exponential function is not possible. To enable the specular calculation, you must set the
 * attenuation parameter of the lighting channel to <tt>GX_AF_SPEC</tt> using GX_SetChanCtrl().
 *
 * \param[in] lobj ptr to the light object
 * \param[in] shininess shininess parameter
 *
 * \return none
 */
#define GX_InitLightShininess(lobj, shininess) \
    (GX_InitLightAttn(lobj, 0.0F, 0.0F, 1.0F,  \
                    (shininess)/2.0F, 0.0F,   \
                    1.0F-(shininess)/2.0F ))

#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif
