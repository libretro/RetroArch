#ifndef __GU_H__
#define __GU_H__

/*!
 * \file gu.h
 * \brief GU/Matrix subsystem
 *
 * \details The GU/Matrix subsystem is used for many matrix- , vector- and quaternion-related operations.
 *
 * The matrix functions are coupled tightly with GX (GX will take Mtx for many of its own matrix-related functions, for example).
 * This library supports 3x3, 3x4, 4x3 and 4x4 matrices.
 *
 * This library has functions for manipulating vectors as well; some of its functions are for transforming matrices using vectors.
 *
 * It also includes functions for using and converting between quaternions. Although quaternions are not used natively in libogc,
 * they work well with rotation functions as they aren't susceptible to "gimbal lock". You can use the appropriate functions to
 * freely convert between quaternions and matrices.
 *
 * \note Many of the functions come in two flavors: C and "paired single". Both perform the same exact operations, but with a key
 * difference: the C versions are written in pure C and are slightly more accurate, while the PS versions are hand-written
 * assembly routines utilizing the Gekko's "paired-single" extension, which is much faster for almost every operation but slightly
 * less accurate. When building for the GameCube or Wii (which is probably always), the library is configured to automatically use
 * the paired-single tuned versions, as the speed difference is worth the accuracy hit. If you want to use the C routine and take
 * the performance hit instead, prefix the function with "c_". You are not limited to using only one or the other collection; you
 * can use both in your code if you wish.
 *
 * \warning Some functions (notably guFrustum() and related) take a 4x4 matrix, while the rest work only on 4x3 matrices. Make sure
 * you are passing the correct matrix type to each function, as passing the wrong one can create subtle bugs.
 */

#include <gctypes.h>

#ifdef GEKKO
#define MTX_USE_PS
#undef MTX_USE_C
#endif

#ifndef GEKKO
#define MTX_USE_C
#undef MTX_USE_PS
#endif

#ifndef MAX
#define MAX(a,b) (((a)>(b))?(a):(b))
#endif
#ifndef MIN
#define MIN(a,b) (((a)<(b))?(a):(b))
#endif

#define M_PI		3.14159265358979323846
#define M_DTOR		(3.14159265358979323846/180.0)

#define	FTOFIX32(x)	(s32)((x) * (f32)0x00010000)
#define	FIX32TOF(x)	((f32)(x) * (1.0f / (f32)0x00010000))
#define	FTOFRAC8(x)	((s32) MIN(((x) * (128.0f)), 127.0f) & 0xff)

#define DegToRad(a)   ( (a) *  0.01745329252f )
#define RadToDeg(a)   ( (a) * 57.29577951f )

/*!
 * \def guMtxRowCol(mt,row,col)
 * \brief Provides storage-safe access to elements of Mtx and Mtx44.
 *
 * \details This macro provides storage-safe access to elements of Mtx and Mtx44. Matrix storage format is transparent to the
 * programmer as long as matrices are initialized and manipulated exclusively with the matrix API. Do not initialize matrices
 * when they are first declared and do not set values by hand. To insulate code from changes to matrix storage format, you should
 * use this macro instead of directly accessing individual matrix elements.
 *
 * \note When using this function, think of the matrix in row-major format.
 *
 * \param[in] mt Matrix to be accessed.
 * \param[in] r Row index of element to access.
 * \param[in] c Column index of element to access.
 *
 * \return none
 */
#define guMtxRowCol(mt,row,col)		(mt[row][col])

#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */

/*! \struct guVector
 * \brief 3-element vector with x, y and z components.
 *
 * \details When used in 3D transformations, it is treated as a column vector with an implied fourth 'w' coordinate of 1.
 * For example, to multiply a vector <i>vOld</i> by a matrix <i>m</i>: <i>vNew</i> = <i>m</i> x <i>vOld</i>. In code:
 *
 * \code guVecMultiply( m, &vOld, &vNew ); \endcode
 *
 * \note This is a generic structure which can be used in any situation or function that accepts an array or struct with
 * three f32 values.
 */
typedef struct _vecf {
	f32 x,y,z;
} guVector;

/*! \struct guQuaternion
 * \brief Quaternion type consisting of an (x,y,z) vector component and a (w) scalar component.
 *
 * \details This struct is used by gu library function such as guQuatMtx(), which generates a rotation matrix from a
 * quaternion.
 *
 * \note This is a generic structure which can be used in any situation or function that accepts an array or struct with
 * four f32 values.
 */
typedef struct _qrtn {
	f32 x,y,z,w;
} guQuaternion;

/*! \typedef f32 Mtx[3][4]
 * \brief Standard 3x4 matrix.
 * \warning Some functions take the 4x4 matrix type rather than this one, so make sure you don't mix up the two.
 */
typedef f32	Mtx[3][4];
typedef f32 (*MtxP)[4];

/*! \typedef f32 ROMtx[4][3]
 * \brief Column-major representation of the standard Mtx structure.
 *
 * \details It is not a true transpose, as it is a 4x3 matrix. These structures are only accepted by functions that explicitly
 * require reordered matrices.
 */
typedef f32 ROMtx[4][3];
typedef f32 (*ROMtxP)[3];

/*! \typedef f32 Mtx33[3][3]
 * \brief 3x3 matrix.
 */
typedef f32 Mtx33[3][3];
typedef f32 (*Mtx33P)[3];

/*! \typedef f32 Mtx44[4][4]
 * \brief 4x4 matrix.
 * \warning Some functions take this instead of the 3x4 matrix, so make sure you don't mix up the two.
 */
typedef f32 Mtx44[4][4];
typedef f32 (*Mtx44P)[4];

/*!
 * \fn void guFrustum(Mtx44 mt,f32 t,f32 b,f32 l,f32 r,f32 n,f32 f)
 * \brief Sets a 4x4 perspective projection matrix from viewing volume dimensions.
 *
 * \details This matrix is used by the GX API to transform points to screen space.
 *
 * For normal perspective projection, the axis of projection is the -z axis, so \a t = positive, \a b = -\a t, \a r  =
 * positive, \a l = -\a r. \a n and \a f must both be given as positive distances.
 *
 * \note \a m negates a point's 'z' values, so pre-transformed points should have negative 'z' values in eye space in
 * order to be visible after projection.
 *
 * \param[out] mt New projection matrix.
 * \param[in] t Top edge of view volume at the near clipping plane.
 * \param[in] b Bottom edge of view volume at the near clipping plane.
 * \param[in] l Left edge of view volume at the near clipping plane.
 * \param[in] r Right edge of view volume at the near clipping plane.
 * \param[in] n Positive distance to the near clipping plane.
 * \param[in] f Positive distance to the far clipping plane.
 *
 * \return none
 */
void guFrustum(Mtx44 mt,f32 t,f32 b,f32 l,f32 r,f32 n,f32 f);

/*!
 * \fn void guPerspective(Mtx44 mt,f32 fovy,f32 aspect,f32 n,f32 f)
 * \brief Sets a 4x4 perspective projection matrix from field of view and aspect ratio parameters.
 *
 * \details This matrix is used by the GX API to transform points to screen space.
 *
 * This function generates a projection matrix equivalent to that created by guFrustum() with the axis of projection
 * centered around Z. It is included to provide an alternative method of specifying view volume dimensions.
 *
 * The field of view (\a fovy) is the total field of view in degrees in the Y-Z plane. \a aspect is the ratio
 * (width/height) of the view window in screen space. \a n and \a f must both be given as positive distances.
 *
 * \note \a m negates a point's 'z' values, so pre-transformed points should have negative 'z' values in eye space in order to
 * be visible after projection.
 *
 * \param[out] mt New perspective projection matrix.
 * \param[in] fovy Total field of view in the Y-Z plane measured in degrees.
 * \param[in] aspect View window aspect ratio (width/height)
 * \param[in] n Positive distance to near clipping plane.
 * \param[in] f Positive distance to far clipping plane.
 *
 * \return none
 */
void guPerspective(Mtx44 mt,f32 fovy,f32 aspect,f32 n,f32 f);

/*!
 * \fn void guOrtho(Mtx44 mt,f32 t,f32 b,f32 l,f32 r,f32 n,f32 f)
 * \brief Sets a 4x4 matrix for orthographic projection.
 *
 * \details This matrix is used by the GX API to transform points from eye space to screen space.
 *
 * For normal parallel projections, the axis of projection is the -z axis, so \a t = positive, \a b = -\a t, \a r =
 * positive, \a l = -\a r. \a n and \a f must both be given as positive distances.
 *
 * \note \a m negates \a a point's 'z' values, so pre-transformed points should have negative 'z' values in eye space in order
 * to be visible after projection.
 *
 * \param[out] mt New parallel projection matrix.
 * \param[in] t Top edge of view volume.
 * \param[in] b Bottom edge of view volume.
 * \param[in] l Left edge of view volume.
 * \param[in] r Right edge of view volume.
 * \param[in] n Positive distance to the near clipping plane.
 * \param[in] f Positive distance to the far clipping plane.
 *
 * \return none
 */
void guOrtho(Mtx44 mt,f32 t,f32 b,f32 l,f32 r,f32 n,f32 f);

/*!
 * \fn void guLightPerspective(Mtx mt,f32 fovY,f32 aspect,f32 scaleS,f32 scaleT,f32 transS,f32 transT)
 * \brief Sets a 3x4 perspective projection matrix from field of view and aspect ratio parameters, two scale values, and two
 * translation values.
 *
 * \details This matrix is used to project points into texture space and yield texture coordinates.
 *
 * This function generates a projection matrix, equivalent to that created by guLightFrustum(), with the axis of projection
 * centered around Z. This function is included to provide an alternative method of specifying texture projection volume
 * dimensions.
 *
 * The field of view (\a fovy) is the total field of view in degrees in the YZ plane. \a aspect is the ratio (width / height)
 * of the view window in screen space.
 *
 * Standard projection yields values ranging from -1.0 to 1.0 in both dimensions of the front clipping plane. Since texture
 * coordinates should usually be within the range of 0.0 to 1.0, we have added a scale and translation value for both S and T.
 * The most common way to use these values is to set all of them to 0.5 (so that points in the range of -1.0 to 1.0 are first
 * scaled by 0.5) to be in the range of -0.5 to 0.5. Then they are translated by 0.5 to be in the range of 0.0 to 1.0. Other
 * values can be used for translation and scale to yield different effects.
 *
 * \param[out] mt New projection matrix.
 * \param[in] fovy Total field of view in the YZ plane measured in degrees.
 * \param[in] aspect View window aspect ratio (width / height)
 * \param[in] scaleS Scale in the S direction for projected coordinates (usually 0.5).
 * \param[in] scaleT Scale in the T direction for projected coordinates (usually 0.5).
 * \param[in] transS Translate in the S direction for projected coordinates (usually 0.5).
 * \param[in] transT Translate in the T direction for projected coordinates (usually 0.5).
 *
 * \return none
 */
void guLightPerspective(Mtx mt,f32 fovY,f32 aspect,f32 scaleS,f32 scaleT,f32 transS,f32 transT);

/*!
 * \fn void guLightOrtho(Mtx mt,f32 t,f32 b,f32 l,f32 r,f32 scaleS,f32 scaleT,f32 transS,f32 transT)
 * \brief Sets a 3x4 matrix for orthographic projection.
 *
 * \details Use this matrix to project points into texture space and yield texture coordinates.
 *
 * For normal parallel projections, the axis of projection is the -z axis, so \a t = positive, \a b = -\a t, \a r = positive,
 * \a l = -\a r.
 *
 * Standard projection yields values ranging from -1.0 to 1.0 in both dimensions of the front clipping plane. Since texture
 * coordinates should usually be within the range of 0.0 to 1.0, we have added a scale and translation value for both S and T.
 * The most common way to use these values is to set all of them to 0.5 so that points in the range of -1.0 to 1.0 are first
 * scaled by 0.5 (to be in the range of -0.5 to 0.5). Then they are translated by 0.5 to be in the range of 0.0 to 1.0. Other
 * values can be used for translation and scale to yield different effects.
 *
 * \param[out] mt New parallel projection matrix.
 * \param[in] t Top edge of view volume.
 * \param[in] b Bottom edge of view volume.
 * \param[in] l Left edge of view volume.
 * \param[in] r Right edge of view volume.
 * \param[in] scaleS Scale in the S direction for projected coordinates (usually 0.5).
 * \param[in] scaleT Scale in the T direction for projected coordinates (usually 0.5).
 * \param[in] transS Translate in the S direction for projected coordinates (usually 0.5).
 * \param[in] transT Translate in the T direction for projected coordinates (usually 0.5).
 *
 * \return none
 */
void guLightOrtho(Mtx mt,f32 t,f32 b,f32 l,f32 r,f32 scaleS,f32 scaleT,f32 transS,f32 transT);

/*!
 * \fn void guLightFrustum(Mtx mt,f32 t,f32 b,f32 l,f32 r,f32 n,f32 scaleS,f32 scaleT,f32 transS,f32 transT)
 * \brief Sets a 3x4 perspective projection matrix from viewing volume dimensions, two scale values, and two translation values.
 *
 * \details This matrix is used to project points into texture space and yield texture coordinates.
 *
 * For normal perspective projection, the axis of projection is the -z axis, so \a t = positive, \a b = -\a t, \a r = positive,
 * \a l = -\a r. \a n must be given as a positive distance.
 *
 * Standard projection yields values ranging from -1.0 to 1.0 in both dimensions of the front clipping plane.  Since texture
 * coordinates usually should be within the range of 0.0 to 1.0, we have added a scale and translation value for both S and T.
 * The most common usage of these values is to set all of them to 0.5 so that points in the range of -1.0 to 1.0 are first
 * scaled by 0.5 to be in the range of -0.5 to 0.5, and are then translated by 0.5 to be in the range of 0.0 to 1.0.  Other
 * values can be used for translation and scale to yield different effects.
 *
 * \param[out] mt New projection matrix.
 * \param[in] t Top edge of view volume at the near clipping plane.
 * \param[in] b Bottom edge of view volume at the near clipping plane.
 * \param[in] l Left edge of view volume at the near clipping plane.
 * \param[in] r Right edge of view volume at the near clipping plane.
 * \param[in] n Positive distance to the near clipping plane.
 * \param[in] scaleS Scale in the S direction for projected coordinates (usually 0.5).
 * \param[in] scaleT Scale in the T direction for projected coordinates (usually 0.5).
 * \param[in] transS Translate in the S direction for projected coordinates (usually 0.5).
 * \param[in] transT Translate in the T direction for projected coordinates (usually 0.5).
 *
 * \return none
 */
void guLightFrustum(Mtx mt,f32 t,f32 b,f32 l,f32 r,f32 n,f32 scaleS,f32 scaleT,f32 transS,f32 transT);

/*!
 * \fn void guLookAt(Mtx mt,guVector *camPos,guVector *camUp,guVector *target)
 * \brief Sets a world-space to camera-space transformation matrix.
 *
 * \details Create the matrix \a m by specifying a camera position (\a camPos), a camera "up" direction (\a camUp), and a target
 * position (\a target).
 *
 * The camera's reference viewing direction is the -z axis. The camera's reference 'up' direction is the +y axis.
 *
 * This function is especially convenient for creating a tethered camera, aiming at an object, panning, or specifying an
 * arbitrary view.
 *
 * \param[out] mt New viewing matrix.
 * \param[in] camPos Vector giving 3D camera position in world space.
 * \param[in] camUp Vector containing camera "up" vector; does not have to be a unit vector.
 * \param[in] target Vector giving 3D target position in world space.
 *
 * \return none
 */
void guLookAt(Mtx mt,guVector *camPos,guVector *camUp,guVector *target);

/*!
 * \fn void guVecHalfAngle(guVector *a,guVector *b,guVector *half)
 * \brief Computes a vector that lies halfway between \a a and \a b.
 *
 * \details The halfway vector is useful in specular reflection calculations. It is interpreted as pointing from the reflecting
 * surface to the general viewing direction.
 *
 * \a a and \a b do not have to be unit vectors. Both of these vectors are assumed to be pointing towards the surface from the
 * light or viewer, respectively. Local copies of these vectors are negated, normalized and added head to tail.
 *
 * \a half is computed as a unit vector that points from the surface to halfway between the light and the viewing direction.
 *
 * \param[in] a Pointer to incident vector. Must point from the light source to the surface.
 * \param[in] b Pointer to viewing vector. Must point from the viewer to the surface.
 * \param[out] half Pointer to resultant half-angle unit vector; points from the surface to halfway between the light and the viewing direction.
 *
 * \return none
 */
void guVecHalfAngle(guVector *a,guVector *b,guVector *half);

void c_guVecAdd(guVector *a,guVector *b,guVector *ab);
void c_guVecSub(guVector *a,guVector *b,guVector *ab);
void c_guVecScale(guVector *src,guVector *dst,f32 scale);
void c_guVecNormalize(guVector *v);
void c_guVecMultiply(Mtx mt,guVector *src,guVector *dst);
void c_guVecCross(guVector *a,guVector *b,guVector *axb);
void c_guVecMultiplySR(Mtx mt,guVector *src,guVector *dst);
f32 c_guVecDotProduct(guVector *a,guVector *b);

#ifdef GEKKO
void ps_guVecAdd(register guVector *a,register guVector *b,register guVector *ab);
void ps_guVecSub(register guVector *a,register guVector *b,register guVector *ab);
void ps_guVecScale(register guVector *src,register guVector *dst,f32 scale);
void ps_guVecNormalize(register guVector *v);
void ps_guVecCross(register guVector *a,register guVector *b,register guVector *axb);
void ps_guVecMultiply(register Mtx mt,register guVector *src,register guVector *dst);
void ps_guVecMultiplySR(register Mtx mt,register guVector *src,register guVector *dst);
f32 ps_guVecDotProduct(register guVector *a,register guVector *b);
#endif	//GEKKO

void c_guQuatAdd(guQuaternion *a,guQuaternion *b,guQuaternion *ab);
void c_guQuatSub(guQuaternion *a,guQuaternion *b,guQuaternion *ab);
void c_guQuatMultiply(guQuaternion *a,guQuaternion *b,guQuaternion *ab);
void c_guQuatNormalize(guQuaternion *a,guQuaternion *d);
void c_guQuatInverse(guQuaternion *a,guQuaternion *d);
void c_guQuatMtx(guQuaternion *a,Mtx m);

#ifdef GEKKO
void ps_guQuatAdd(register guQuaternion *a,register guQuaternion *b,register guQuaternion *ab);
void ps_guQuatSub(register guQuaternion *a,register guQuaternion *b,register guQuaternion *ab);
void ps_guQuatMultiply(register guQuaternion *a,register guQuaternion *b,register guQuaternion *ab);
void ps_guQuatNormalize(register guQuaternion *a,register guQuaternion *d);
void ps_guQuatInverse(register guQuaternion *a,register guQuaternion *d);
#endif

void c_guMtxIdentity(Mtx mt);
void c_guMtxCopy(Mtx src,Mtx dst);
void c_guMtxConcat(Mtx a,Mtx b,Mtx ab);
void c_guMtxScale(Mtx mt,f32 xS,f32 yS,f32 zS);
void c_guMtxScaleApply(Mtx src,Mtx dst,f32 xS,f32 yS,f32 zS);
void c_guMtxApplyScale(Mtx src,Mtx dst,f32 xS,f32 yS,f32 zS);
void c_guMtxTrans(Mtx mt,f32 xT,f32 yT,f32 zT);
void c_guMtxTransApply(Mtx src,Mtx dst,f32 xT,f32 yT,f32 zT);
void c_guMtxApplyTrans(Mtx src,Mtx dst,f32 xT,f32 yT,f32 zT);
u32 c_guMtxInverse(Mtx src,Mtx inv);
u32 c_guMtxInvXpose(Mtx src,Mtx xPose);
void c_guMtxTranspose(Mtx src,Mtx xPose);
void c_guMtxRotRad(Mtx mt,const char axis,f32 rad);
void c_guMtxRotTrig(Mtx mt,const char axis,f32 sinA,f32 cosA);
void c_guMtxRotAxisRad(Mtx mt,guVector *axis,f32 rad);
void c_guMtxReflect(Mtx m,guVector *p,guVector *n);
void c_guMtxQuat(Mtx m,guQuaternion *a);

#ifdef GEKKO
void ps_guMtxIdentity(register Mtx mt);
void ps_guMtxCopy(register Mtx src,register Mtx dst);
void ps_guMtxConcat(register Mtx a,register Mtx b,register Mtx ab);
void ps_guMtxTranspose(register Mtx src,register Mtx xPose);
u32 ps_guMtxInverse(register Mtx src,register Mtx inv);
u32 ps_guMtxInvXpose(register Mtx src,register Mtx xPose);
void ps_guMtxScale(register Mtx mt,register f32 xS,register f32 yS,register f32 zS);
void ps_guMtxScaleApply(register Mtx src,register Mtx dst,register f32 xS,register f32 yS,register f32 zS);
void ps_guMtxApplyScale(register Mtx src,register Mtx dst,register f32 xS,register f32 yS,register f32 zS);
void ps_guMtxTrans(register Mtx mt,register f32 xT,register f32 yT,register f32 zT);
void ps_guMtxTransApply(register Mtx src,register Mtx dst,register f32 xT,register f32 yT,register f32 zT);
void ps_guMtxApplyTrans(register Mtx src,register Mtx dst,register f32 xT,register f32 yT,register f32 zT);
void ps_guMtxRotRad(register Mtx mt,register const char axis,register f32 rad);
void ps_guMtxRotTrig(register Mtx mt,register const char axis,register f32 sinA,register f32 cosA);
void ps_guMtxRotAxisRad(register Mtx mt,register guVector *axis,register f32 tmp0);
void ps_guMtxReflect(register Mtx m,register guVector *p,register guVector *n);
#endif	//GEKKO

#ifdef MTX_USE_C

#define guVecAdd				c_guVecAdd
#define guVecSub				c_guVecSub
#define guVecScale				c_guVecScale
#define guVecNormalize			c_guVecNormalize
#define guVecMultiply			c_guVecMultiply
#define guVecCross				c_guVecCross
#define guVecMultiplySR			c_guVecMultiplySR
#define guVecDotProduct			c_guVecDotProduct

#define guQuatAdd				c_guQuatAdd
#define guQuatSub				c_guQuatSub
#define guQuatMultiply			c_guQuatMultiply
#define guQuatNoramlize			c_guQuatNormalize
#define guQuatInverse			c_guQuatInverse
#define guQuatMtx				c_guQuatMtx

#define guMtxIdentity			c_guMtxIdentity
#define guMtxCopy				c_guMtxCopy
#define guMtxConcat				c_guMtxConcat
#define guMtxScale				c_guMtxScale
#define guMtxScaleApply			c_guMtxScaleApply
#define guMtxApplyScale			c_guMtxApplyScale
#define guMtxTrans				c_guMtxTrans
#define guMtxTransApply			c_guMtxTransApply
#define guMtxApplyTrans			c_guMtxApplyTrans
#define guMtxInverse			c_guMtxInverse
#define guMtxTranspose			c_guMtxTranspose
#define guMtxInvXpose			c_guMtxInvXpose
#define guMtxRotRad				c_guMtxRotRad
#define guMtxRotTrig			c_guMtxRotTrig
#define guMtxRotAxisRad			c_guMtxRotAxisRad
#define guMtxReflect			c_guMtxReflect
#define guMtxQuat				c_guMtxQuat

#else //MTX_USE_C

#define guVecAdd				ps_guVecAdd
#define guVecSub				ps_guVecSub
#define guVecScale				ps_guVecScale
#define guVecNormalize			ps_guVecNormalize
#define guVecMultiply			ps_guVecMultiply
#define guVecCross				ps_guVecCross
#define guVecMultiplySR			ps_guVecMultiplySR
#define guVecDotProduct			ps_guVecDotProduct

#define guQuatAdd				ps_guQuatAdd
#define guQuatSub				ps_guQuatSub
#define guQuatMultiply			ps_guQuatMultiply
#define guQuatNormalize			ps_guQuatNormalize
#define guQuatInverse			ps_guQuatInverse

#define guMtxIdentity			ps_guMtxIdentity
#define guMtxCopy				ps_guMtxCopy
#define guMtxConcat				ps_guMtxConcat
#define guMtxScale				ps_guMtxScale
#define guMtxScaleApply			ps_guMtxScaleApply
#define guMtxApplyScale			ps_guMtxApplyScale
#define guMtxTrans				ps_guMtxTrans
#define guMtxTransApply			ps_guMtxTransApply
#define guMtxApplyTrans			ps_guMtxApplyTrans
#define guMtxInverse			ps_guMtxInverse
#define guMtxTranspose			ps_guMtxTranspose
#define guMtxInvXpose			ps_guMtxInvXpose
#define guMtxRotRad				ps_guMtxRotRad
#define guMtxRotTrig			ps_guMtxRotTrig
#define guMtxRotAxisRad			ps_guMtxRotAxisRad
#define guMtxReflect			ps_guMtxReflect

#endif //MTX_USE_PS

#define guMtxRotDeg(mt,axis,deg)		guMtxRotRad(mt,axis,DegToRad(deg))
#define guMtxRotAxisDeg(mt,axis,deg)	guMtxRotAxisRad(mt,axis,DegToRad(deg))

#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif
