#include <gu.h>
#include <math.h>

extern void __ps_guMtxRotAxisRadInternal(register Mtx mt,const register guVector *axis,register f32 sT,register f32 cT);

void guFrustum(Mtx44 mt,f32 t,f32 b,f32 l,f32 r,f32 n,f32 f)
{
	f32 tmp;

	tmp = 1.0f/(r-l);
	mt[0][0] = (2*n)*tmp;
	mt[0][1] = 0.0f;
	mt[0][2] = (r+l)*tmp;
	mt[0][3] = 0.0f;

	tmp = 1.0f/(t-b);
	mt[1][0] = 0.0f;
	mt[1][1] = (2*n)*tmp;
	mt[1][2] = (t+b)*tmp;
	mt[1][3] = 0.0f;

	tmp = 1.0f/(f-n);
	mt[2][0] = 0.0f;
	mt[2][1] = 0.0f;
	mt[2][2] = -(n)*tmp;
	mt[2][3] = -(f*n)*tmp;

	mt[3][0] = 0.0f;
	mt[3][1] = 0.0f;
	mt[3][2] = -1.0f;
	mt[3][3] = 0.0f;
}

void guPerspective(Mtx44 mt,f32 fovy,f32 aspect,f32 n,f32 f)
{
	f32 cot,angle,tmp;

	angle = fovy*0.5f;
	angle = DegToRad(angle);

	cot = 1.0f/tanf(angle);

	mt[0][0] = cot/aspect;
	mt[0][1] = 0.0f;
	mt[0][2] = 0.0f;
	mt[0][3] = 0.0f;

	mt[1][0] = 0.0f;
	mt[1][1] = cot;
	mt[1][2] = 0.0f;
	mt[1][3] = 0.0f;

	tmp = 1.0f/(f-n);
	mt[2][0] = 0.0f;
	mt[2][1] = 0.0f;
	mt[2][2] = -(n)*tmp;
	mt[2][3] = -(f*n)*tmp;

	mt[3][0] = 0.0f;
	mt[3][1] = 0.0f;
	mt[3][2] = -1.0f;
	mt[3][3] = 0.0f;
}

void guOrtho(Mtx44 mt,f32 t,f32 b,f32 l,f32 r,f32 n,f32 f)
{
	f32 tmp;

	tmp = 1.0f/(r-l);
	mt[0][0] = 2.0f*tmp;
	mt[0][1] = 0.0f;
	mt[0][2] = 0.0f;
	mt[0][3] = -(r+l)*tmp;

	tmp = 1.0f/(t-b);
	mt[1][0] = 0.0f;
	mt[1][1] = 2.0f*tmp;
	mt[1][2] = 0.0f;
	mt[1][3] = -(t+b)*tmp;

	tmp = 1.0f/(f-n);
	mt[2][0] = 0.0f;
	mt[2][1] = 0.0f;
	mt[2][2] = -(1.0f)*tmp;
	mt[2][3] = -(f)*tmp;

	mt[3][0] = 0.0f;
	mt[3][1] = 0.0f;
	mt[3][2] = 0.0f;
	mt[3][3] = 1.0f;
}

void guLightPerspective(Mtx mt,f32 fovY,f32 aspect,f32 scaleS,f32 scaleT,f32 transS,f32 transT)
{
	f32 angle;
	f32 cot;

	angle = fovY*0.5f;
	angle = DegToRad(angle);

	cot = 1.0f/tanf(angle);

    mt[0][0] =    (cot / aspect) * scaleS;
    mt[0][1] =    0.0f;
    mt[0][2] =    -transS;
    mt[0][3] =    0.0f;

    mt[1][0] =    0.0f;
    mt[1][1] =    cot * scaleT;
    mt[1][2] =    -transT;
    mt[1][3] =    0.0f;

    mt[2][0] =    0.0f;
    mt[2][1] =    0.0f;
    mt[2][2] =   -1.0f;
    mt[2][3] =    0.0f;
}

void guLightOrtho(Mtx mt,f32 t,f32 b,f32 l,f32 r,f32 scaleS,f32 scaleT,f32 transS,f32 transT)
{
	f32 tmp;

    tmp     =  1.0f / (r - l);
    mt[0][0] =  (2.0f * tmp * scaleS);
    mt[0][1] =  0.0f;
    mt[0][2] =  0.0f;
    mt[0][3] =  ((-(r + l) * tmp) * scaleS) + transS;

    tmp     =  1.0f / (t - b);
    mt[1][0] =  0.0f;
    mt[1][1] =  (2.0f * tmp) * scaleT;
    mt[1][2] =  0.0f;
    mt[1][3] =  ((-(t + b) * tmp)* scaleT) + transT;

    mt[2][0] =  0.0f;
    mt[2][1] =  0.0f;
    mt[2][2] =  0.0f;
    mt[2][3] =  1.0f;
}

void guLightFrustum(Mtx mt,f32 t,f32 b,f32 l,f32 r,f32 n,f32 scaleS,f32 scaleT,f32 transS,f32 transT)
{
    f32 tmp;

    tmp     =  1.0f / (r - l);
    mt[0][0] =  ((2*n) * tmp) * scaleS;
    mt[0][1] =  0.0f;
    mt[0][2] =  (((r + l) * tmp) * scaleS) - transS;
    mt[0][3] =  0.0f;

    tmp     =  1.0f / (t - b);
    mt[1][0] =  0.0f;
    mt[1][1] =  ((2*n) * tmp) * scaleT;
    mt[1][2] =  (((t + b) * tmp) * scaleT) - transT;
    mt[1][3] =  0.0f;

    mt[2][0] =  0.0f;
    mt[2][1] =  0.0f;
    mt[2][2] = -1.0f;
    mt[2][3] =  0.0f;
}

void guLookAt(Mtx mt,guVector *camPos,guVector *camUp,guVector *target)
{
	guVector vLook,vRight,vUp;

	vLook.x = camPos->x - target->x;
	vLook.y = camPos->y - target->y;
	vLook.z = camPos->z - target->z;
	guVecNormalize(&vLook);

	guVecCross(camUp,&vLook,&vRight);
	guVecNormalize(&vRight);

	guVecCross(&vLook,&vRight,&vUp);

    mt[0][0] = vRight.x;
    mt[0][1] = vRight.y;
    mt[0][2] = vRight.z;
    mt[0][3] = -( camPos->x * vRight.x + camPos->y * vRight.y + camPos->z * vRight.z );

    mt[1][0] = vUp.x;
    mt[1][1] = vUp.y;
    mt[1][2] = vUp.z;
    mt[1][3] = -( camPos->x * vUp.x + camPos->y * vUp.y + camPos->z * vUp.z );

    mt[2][0] = vLook.x;
    mt[2][1] = vLook.y;
    mt[2][2] = vLook.z;
    mt[2][3] = -( camPos->x * vLook.x + camPos->y * vLook.y + camPos->z * vLook.z );
}

void c_guMtxIdentity(Mtx mt)
{
	s32 i,j;

	for(i=0;i<3;i++) {
		for(j=0;j<4;j++) {
			if(i==j) mt[i][j] = 1.0;
			else mt[i][j] = 0.0;
		}
	}
}

void c_guMtxRotRad(Mtx mt,const char axis,f32 rad)
{
	f32 sinA,cosA;

	sinA = sinf(rad);
	cosA = cosf(rad);

	c_guMtxRotTrig(mt,axis,sinA,cosA);
}

#ifdef GEKKO
void ps_guMtxRotRad(register Mtx mt,const register char axis,register f32 rad)
{
	register f32 sinA = sinf(rad);
	register f32 cosA = cosf(rad);

	ps_guMtxRotTrig(mt,axis,sinA,cosA);
}

void ps_guMtxRotAxisRad(Mtx mt,guVector *axis,f32 rad)
{
	f32 sinT = sinf(rad);
	f32 cosT = cosf(rad);

	__ps_guMtxRotAxisRadInternal(mt,axis,sinT,cosT);
}

#endif

void c_guMtxRotTrig(Mtx mt,const char axis,f32 sinA,f32 cosA)
{
	switch(axis) {
		case 'x':
		case 'X':
			mt[0][0] =  1.0f;  mt[0][1] =  0.0f;    mt[0][2] =  0.0f;  mt[0][3] = 0.0f;
			mt[1][0] =  0.0f;  mt[1][1] =  cosA;    mt[1][2] = -sinA;  mt[1][3] = 0.0f;
			mt[2][0] =  0.0f;  mt[2][1] =  sinA;    mt[2][2] =  cosA;  mt[2][3] = 0.0f;
			break;
		case 'y':
		case 'Y':
			mt[0][0] =  cosA;  mt[0][1] =  0.0f;    mt[0][2] =  sinA;  mt[0][3] = 0.0f;
			mt[1][0] =  0.0f;  mt[1][1] =  1.0f;    mt[1][2] =  0.0f;  mt[1][3] = 0.0f;
			mt[2][0] = -sinA;  mt[2][1] =  0.0f;    mt[2][2] =  cosA;  mt[2][3] = 0.0f;
			break;
		case 'z':
		case 'Z':
			mt[0][0] =  cosA;  mt[0][1] = -sinA;    mt[0][2] =  0.0f;  mt[0][3] = 0.0f;
			mt[1][0] =  sinA;  mt[1][1] =  cosA;    mt[1][2] =  0.0f;  mt[1][3] = 0.0f;
			mt[2][0] =  0.0f;  mt[2][1] =  0.0f;    mt[2][2] =  1.0f;  mt[2][3] = 0.0f;
			break;
		default:
			break;
	}
}

void c_guMtxRotAxisRad(Mtx mt,guVector *axis,f32 rad)
{
	f32 s,c;
	f32 t;
	f32 x,y,z;
	f32 xSq,ySq,zSq;

	s = sinf(rad);
	c = cosf(rad);
	t = 1.0f-c;

	c_guVecNormalize(axis);

	x = axis->x;
	y = axis->y;
	z = axis->z;

	xSq = x*x;
	ySq = y*y;
	zSq = z*z;

    mt[0][0] = ( t * xSq )   + ( c );
    mt[0][1] = ( t * x * y ) - ( s * z );
    mt[0][2] = ( t * x * z ) + ( s * y );
    mt[0][3] =    0.0f;

    mt[1][0] = ( t * x * y ) + ( s * z );
    mt[1][1] = ( t * ySq )   + ( c );
    mt[1][2] = ( t * y * z ) - ( s * x );
    mt[1][3] =    0.0f;

    mt[2][0] = ( t * x * z ) - ( s * y );
    mt[2][1] = ( t * y * z ) + ( s * x );
    mt[2][2] = ( t * zSq )   + ( c );
    mt[2][3] =    0.0f;

}

void c_guMtxCopy(Mtx src,Mtx dst)
{
	if(src==dst) return;

    dst[0][0] = src[0][0];    dst[0][1] = src[0][1];    dst[0][2] = src[0][2];    dst[0][3] = src[0][3];
    dst[1][0] = src[1][0];    dst[1][1] = src[1][1];    dst[1][2] = src[1][2];    dst[1][3] = src[1][3];
    dst[2][0] = src[2][0];    dst[2][1] = src[2][1];    dst[2][2] = src[2][2];    dst[2][3] = src[2][3];
}

void c_guMtxConcat(Mtx a,Mtx b,Mtx ab)
{
	Mtx tmp;
	MtxP m;

	if(ab==b || ab==a)
		m = tmp;
	else
		m = ab;

    m[0][0] = a[0][0]*b[0][0] + a[0][1]*b[1][0] + a[0][2]*b[2][0];
    m[0][1] = a[0][0]*b[0][1] + a[0][1]*b[1][1] + a[0][2]*b[2][1];
    m[0][2] = a[0][0]*b[0][2] + a[0][1]*b[1][2] + a[0][2]*b[2][2];
    m[0][3] = a[0][0]*b[0][3] + a[0][1]*b[1][3] + a[0][2]*b[2][3] + a[0][3];

    m[1][0] = a[1][0]*b[0][0] + a[1][1]*b[1][0] + a[1][2]*b[2][0];
    m[1][1] = a[1][0]*b[0][1] + a[1][1]*b[1][1] + a[1][2]*b[2][1];
    m[1][2] = a[1][0]*b[0][2] + a[1][1]*b[1][2] + a[1][2]*b[2][2];
    m[1][3] = a[1][0]*b[0][3] + a[1][1]*b[1][3] + a[1][2]*b[2][3] + a[1][3];

    m[2][0] = a[2][0]*b[0][0] + a[2][1]*b[1][0] + a[2][2]*b[2][0];
    m[2][1] = a[2][0]*b[0][1] + a[2][1]*b[1][1] + a[2][2]*b[2][1];
    m[2][2] = a[2][0]*b[0][2] + a[2][1]*b[1][2] + a[2][2]*b[2][2];
    m[2][3] = a[2][0]*b[0][3] + a[2][1]*b[1][3] + a[2][2]*b[2][3] + a[2][3];

	if(m==tmp)
		c_guMtxCopy(tmp,ab);
}

void c_guMtxScale(Mtx mt,f32 xS,f32 yS,f32 zS)
{
    mt[0][0] = xS;    mt[0][1] = 0.0f;  mt[0][2] = 0.0f;  mt[0][3] = 0.0f;
    mt[1][0] = 0.0f;  mt[1][1] = yS;    mt[1][2] = 0.0f;  mt[1][3] = 0.0f;
    mt[2][0] = 0.0f;  mt[2][1] = 0.0f;  mt[2][2] = zS;    mt[2][3] = 0.0f;
}

void c_guMtxScaleApply(Mtx src,Mtx dst,f32 xS,f32 yS,f32 zS)
{
	dst[0][0] = src[0][0] * xS;     dst[0][1] = src[0][1] * xS;
	dst[0][2] = src[0][2] * xS;     dst[0][3] = src[0][3] * xS;

	dst[1][0] = src[1][0] * yS;     dst[1][1] = src[1][1] * yS;
	dst[1][2] = src[1][2] * yS;     dst[1][3] = src[1][3] * yS;

	dst[2][0] = src[2][0] * zS;     dst[2][1] = src[2][1] * zS;
	dst[2][2] = src[2][2] * zS;     dst[2][3] = src[2][3] * zS;
}

void c_guMtxApplyScale(Mtx src,Mtx dst,f32 xS,f32 yS,f32 zS)
{
	dst[0][0] = src[0][0] * xS;     dst[0][1] = src[0][1] * yS;
	dst[0][2] = src[0][2] * zS;     dst[0][3] = src[0][3];

	dst[1][0] = src[1][0] * xS;     dst[1][1] = src[1][1] * yS;
	dst[1][2] = src[1][2] * zS;     dst[1][3] = src[1][3];

	dst[2][0] = src[2][0] * xS;     dst[2][1] = src[2][1] * yS;
	dst[2][2] = src[2][2] * zS;     dst[2][3] = src[2][3];
}

void c_guMtxTrans(Mtx mt,f32 xT,f32 yT,f32 zT)
{
    mt[0][0] = 1.0f;  mt[0][1] = 0.0f;  mt[0][2] = 0.0f;  mt[0][3] =  xT;
    mt[1][0] = 0.0f;  mt[1][1] = 1.0f;  mt[1][2] = 0.0f;  mt[1][3] =  yT;
    mt[2][0] = 0.0f;  mt[2][1] = 0.0f;  mt[2][2] = 1.0f;  mt[2][3] =  zT;
}

void c_guMtxTransApply(Mtx src,Mtx dst,f32 xT,f32 yT,f32 zT)
{
	if ( src != dst )
	{
		dst[0][0] = src[0][0];    dst[0][1] = src[0][1];    dst[0][2] = src[0][2];
		dst[1][0] = src[1][0];    dst[1][1] = src[1][1];    dst[1][2] = src[1][2];
		dst[2][0] = src[2][0];    dst[2][1] = src[2][1];    dst[2][2] = src[2][2];
	}

	dst[0][3] = src[0][3] + xT;
	dst[1][3] = src[1][3] + yT;
	dst[2][3] = src[2][3] + zT;
}

void c_guMtxApplyTrans(Mtx src,Mtx dst,f32 xT,f32 yT,f32 zT)
{
	if ( src != dst )
	{
		dst[0][0] = src[0][0];    dst[0][1] = src[0][1];    dst[0][2] = src[0][2];
		dst[1][0] = src[1][0];    dst[1][1] = src[1][1];    dst[1][2] = src[1][2];
		dst[2][0] = src[2][0];    dst[2][1] = src[2][1];    dst[2][2] = src[2][2];
	}

	dst[0][3] = src[0][0]*xT + src[0][1]*yT + src[0][2]*zT + src[0][3];
	dst[1][3] = src[1][0]*xT + src[1][1]*yT + src[1][2]*zT + src[1][3];
	dst[2][3] = src[2][0]*xT + src[2][1]*yT + src[2][2]*zT + src[2][3];
}

u32 c_guMtxInverse(Mtx src,Mtx inv)
{
    Mtx mTmp;
    MtxP m;
    f32 det;

    if(src==inv)
        m = mTmp;
	else
        m = inv;

    // compute the determinant of the upper 3x3 submatrix
    det =   src[0][0]*src[1][1]*src[2][2] + src[0][1]*src[1][2]*src[2][0] + src[0][2]*src[1][0]*src[2][1]
          - src[2][0]*src[1][1]*src[0][2] - src[1][0]*src[0][1]*src[2][2] - src[0][0]*src[2][1]*src[1][2];

    // check if matrix is singular
    if(det==0.0f)return 0;

    // compute the inverse of the upper submatrix:

    // find the transposed matrix of cofactors of the upper submatrix
    // and multiply by (1/det)

    det = 1.0f / det;

    m[0][0] =  (src[1][1]*src[2][2] - src[2][1]*src[1][2]) * det;
    m[0][1] = -(src[0][1]*src[2][2] - src[2][1]*src[0][2]) * det;
    m[0][2] =  (src[0][1]*src[1][2] - src[1][1]*src[0][2]) * det;

    m[1][0] = -(src[1][0]*src[2][2] - src[2][0]*src[1][2]) * det;
    m[1][1] =  (src[0][0]*src[2][2] - src[2][0]*src[0][2]) * det;
    m[1][2] = -(src[0][0]*src[1][2] - src[1][0]*src[0][2]) * det;

    m[2][0] =  (src[1][0]*src[2][1] - src[2][0]*src[1][1]) * det;
    m[2][1] = -(src[0][0]*src[2][1] - src[2][0]*src[0][1]) * det;
    m[2][2] =  (src[0][0]*src[1][1] - src[1][0]*src[0][1]) * det;

    // compute (invA)*(-C)
    m[0][3] = -m[0][0]*src[0][3] - m[0][1]*src[1][3] - m[0][2]*src[2][3];
    m[1][3] = -m[1][0]*src[0][3] - m[1][1]*src[1][3] - m[1][2]*src[2][3];
    m[2][3] = -m[2][0]*src[0][3] - m[2][1]*src[1][3] - m[2][2]*src[2][3];

    // copy back if needed
    if( m == mTmp )
        c_guMtxCopy(mTmp,inv);

    return 1;
}

void c_guMtxTranspose(Mtx src,Mtx xPose)
{
    Mtx mTmp;
    MtxP m;

    if(src==xPose)
        m = mTmp;
    else
        m = xPose;

    m[0][0] = src[0][0];   m[0][1] = src[1][0];      m[0][2] = src[2][0];     m[0][3] = 0.0f;
    m[1][0] = src[0][1];   m[1][1] = src[1][1];      m[1][2] = src[2][1];     m[1][3] = 0.0f;
    m[2][0] = src[0][2];   m[2][1] = src[1][2];      m[2][2] = src[2][2];     m[2][3] = 0.0f;

    // copy back if needed
    if(m==mTmp)
        c_guMtxCopy(mTmp,xPose);
}

u32 c_guMtxInvXpose(Mtx src, Mtx xPose)
{
    Mtx mTmp;
    MtxP m;
    f32 det;

    if(src == xPose)
        m = mTmp;
    else
        m = xPose;

    // Compute the determinant of the upper 3x3 submatrix
    det =   src[0][0]*src[1][1]*src[2][2] + src[0][1]*src[1][2]*src[2][0] + src[0][2]*src[1][0]*src[2][1]
          - src[2][0]*src[1][1]*src[0][2] - src[1][0]*src[0][1]*src[2][2] - src[0][0]*src[2][1]*src[1][2];

    // Check if matrix is singular
    if(det == 0.0f) return 0;

    // Compute the inverse of the upper submatrix:

    // Find the transposed matrix of cofactors of the upper submatrix
    // and multiply by (1/det)

    det = 1.0f / det;

    m[0][0] =  (src[1][1]*src[2][2] - src[2][1]*src[1][2]) * det;
    m[0][1] = -(src[1][0]*src[2][2] - src[2][0]*src[1][2]) * det;
    m[0][2] =  (src[1][0]*src[2][1] - src[2][0]*src[1][1]) * det;

    m[1][0] = -(src[0][1]*src[2][2] - src[2][1]*src[0][2]) * det;
    m[1][1] =  (src[0][0]*src[2][2] - src[2][0]*src[0][2]) * det;
    m[1][2] = -(src[0][0]*src[2][1] - src[2][0]*src[0][1]) * det;

    m[2][0] =  (src[0][1]*src[1][2] - src[1][1]*src[0][2]) * det;
    m[2][1] = -(src[0][0]*src[1][2] - src[1][0]*src[0][2]) * det;
    m[2][2] =  (src[0][0]*src[1][1] - src[1][0]*src[0][1]) * det;

    // The 4th columns should be zero
    m[0][3] = 0.0F;
    m[1][3] = 0.0F;
    m[2][3] = 0.0F;

    // Copy back if needed
    if(m == mTmp)
        c_guMtxCopy(mTmp, xPose);

    return 1;
}

void c_guMtxReflect(Mtx m,guVector *p,guVector *n)
{
    f32 vxy, vxz, vyz, pdotn;

    vxy   = -2.0f * n->x * n->y;
    vxz   = -2.0f * n->x * n->z;
    vyz   = -2.0f * n->y * n->z;
    pdotn = 2.0f * c_guVecDotProduct(p,n);

    m[0][0] = 1.0f - 2.0f * n->x * n->x;
    m[0][1] = vxy;
    m[0][2] = vxz;
    m[0][3] = pdotn * n->x;

    m[1][0] = vxy;
    m[1][1] = 1.0f - 2.0f * n->y * n->y;
    m[1][2] = vyz;
    m[1][3] = pdotn * n->y;

    m[2][0] = vxz;
    m[2][1] = vyz;
    m[2][2] = 1.0f - 2.0f * n->z * n->z;
    m[2][3] = pdotn * n->z;
}

void c_guVecAdd(guVector *a,guVector *b,guVector *ab)
{
    ab->x = a->x + b->x;
    ab->y = a->y + b->y;
    ab->z = a->z + b->z;
}

void c_guVecSub(guVector *a,guVector *b,guVector *ab)
{
    ab->x = a->x - b->x;
    ab->y = a->y - b->y;
    ab->z = a->z - b->z;
}

void c_guVecScale(guVector *src,guVector *dst,f32 scale)
{
    dst->x = src->x * scale;
    dst->y = src->y * scale;
    dst->z = src->z * scale;
}

void c_guVecNormalize(guVector *v)
{
	f32 m;

	m = ((v->x)*(v->x)) + ((v->y)*(v->y)) + ((v->z)*(v->z));
	m = 1/sqrtf(m);
	v->x *= m;
	v->y *= m;
	v->z *= m;
}

void c_guVecCross(guVector *a,guVector *b,guVector *axb)
{
	guVector vTmp;

	vTmp.x = (a->y*b->z)-(a->z*b->y);
	vTmp.y = (a->z*b->x)-(a->x*b->z);
	vTmp.z = (a->x*b->y)-(a->y*b->x);

	axb->x = vTmp.x;
	axb->y = vTmp.y;
	axb->z = vTmp.z;
}

void c_guVecMultiply(Mtx mt,guVector *src,guVector *dst)
{
	guVector tmp;

    tmp.x = mt[0][0]*src->x + mt[0][1]*src->y + mt[0][2]*src->z + mt[0][3];
    tmp.y = mt[1][0]*src->x + mt[1][1]*src->y + mt[1][2]*src->z + mt[1][3];
    tmp.z = mt[2][0]*src->x + mt[2][1]*src->y + mt[2][2]*src->z + mt[2][3];

    dst->x = tmp.x;
    dst->y = tmp.y;
    dst->z = tmp.z;
}

void c_guVecMultiplySR(Mtx mt,guVector *src,guVector *dst)
{
	guVector tmp;

    tmp.x = mt[0][0]*src->x + mt[0][1]*src->y + mt[0][2]*src->z;
    tmp.y = mt[1][0]*src->x + mt[1][1]*src->y + mt[1][2]*src->z;
    tmp.z = mt[2][0]*src->x + mt[2][1]*src->y + mt[2][2]*src->z;

    // copy back
    dst->x = tmp.x;
    dst->y = tmp.y;
    dst->z = tmp.z;
}

f32 c_guVecDotProduct(guVector *a,guVector *b)
{
    f32 dot;

	dot = (a->x * b->x) + (a->y * b->y) + (a->z * b->z);

    return dot;
}

void c_guQuatAdd(guQuaternion *a,guQuaternion *b,guQuaternion *ab)
{
	ab->x = a->x + b->x;
	ab->y = a->x + b->y;
	ab->z = a->x + b->z;
	ab->w = a->x + b->w;
}

#ifdef GEKKO
void ps_guQuatAdd(register guQuaternion *a,register guQuaternion *b,register guQuaternion *ab)
{
	register f32 tmp0,tmp1;

	__asm__ __volatile__ (
		"psq_l		%0,0(%2),0,0\n"		// [ax][ay]
		"psq_l		%1,0(%3),0,0\n"		// [bx][by]
		"ps_add		%1,%0,%1\n"			// [ax+bx][ay+by]
		"psq_st		%1,0(%4),0,0\n"		// X = [ax+bx], Y = [ay+by]
		"psq_l		%0,8(%2),0,0\n"		// [az][aw]
		"psq_l		%1,8(%3),0,0\n"		// [bz][bw]
		"ps_add		%1,%0,%1\n"			// [az+bz][aw+bw]
		"psq_st		%1,8(%4),0,0"		// Z = [az+bz], W = [aw+bw]
		: "=&f"(tmp0),"=&f"(tmp1)
		: "b"(a),"b"(b),"b"(ab)
		: "memory"
	);
}
#endif

void c_guQuatSub(guQuaternion *a,guQuaternion *b,guQuaternion *ab)
{
	ab->x = a->x - b->x;
	ab->y = a->x - b->y;
	ab->z = a->x - b->z;
	ab->w = a->x - b->w;
}

#ifdef GEKKO
void ps_guQuatSub(register guQuaternion *a,register guQuaternion *b,register guQuaternion *ab)
{
	register f32 tmp0,tmp1;

	__asm__ __volatile__ (
		"psq_l		%0,0(%2),0,0\n"			// [ax][ay]
		"psq_l		%1,0(%3),0,0\n"			// [bx][by]
		"ps_sub		%1,%0,%1\n"				// [ax-bx][ay-by]
		"psq_st		%1,0(%4),0,0\n"			// X = [ax-bx], Y = [ay-by]
		"psq_l		%0,8(%2),0,0\n"			// [az][aw]
		"psq_l		%1,8(%3),0,0\n"			// [bz][bw]
		"ps_sub		%1,%0,%1\n"				// [az-bz][aw-bw]
		"psq_st		%1,8(%4),0,0"			// Z = [az-bz], W = [aw-bw]
		: "=&f"(tmp0),"=&f"(tmp1)
		: "b"(a),"b"(b),"b"(ab)
		: "memory"
	);
}
#endif

void c_guQuatMultiply(guQuaternion *a,guQuaternion *b,guQuaternion *ab)
{
	guQuaternion *r;
	guQuaternion ab_tmp;

	if(a==ab || b==ab) r = &ab_tmp;
	else r = ab;

	r->w = a->w*b->w - a->x*b->x - a->y*b->y - a->z*b->z;
	r->x = a->w*b->x + a->x*b->w + a->y*b->z - a->z*b->y;
	r->y = a->w*b->y + a->y*b->w + a->z*b->x - a->x*b->z;
	r->z = a->w*b->z + a->z*b->w + a->x*b->y - a->y*b->x;

	if(r==&ab_tmp) *ab = ab_tmp;
}

#ifdef GEKKO
void ps_guQuatMultiply(register guQuaternion *a,register guQuaternion *b,register guQuaternion *ab)
{
	register f32 aXY,aZW,bXY,bZW;
	register f32 tmp0,tmp1,tmp2,tmp3,tmp4,tmp5,tmp6,tmp7;

	__asm__ __volatile__ (
		"psq_l		%0,0(%12),0,0\n"		// [px][py]
		"psq_l		%1,8(%12),0,0\n"		// [pz][pw]
		"psq_l		%2,0(%13),0,0\n"		// [qx][qy]
		"ps_neg		%4,%0\n"				// [-px][-py]
		"psq_l		%3,8(%13),0,0\n"		// [qz][qw]
		"ps_neg		%5,%1\n"				// [-pz][-pw]
		"ps_merge01	%6,%4,%0\n"				// [-px][py]
		"ps_muls0	%8,%1,%2\n"				// [pz*qx][pw*qx]
		"ps_muls0	%9,%4,%2\n"				// [-px*qx][-py*qx]
		"ps_merge01	%7,%5,%1\n"				// [-pz][pw]
		"ps_muls1	%11,%6,%2\n"			// [-px*qy][py*qy]
		"ps_madds0	%8,%6,%3,%8\n"			// [-px*qz+pz*qx][py*qz+pw*qx]
		"ps_muls1	%10,%7,%2\n"			// [-pz*qy][pw*qy]
		"ps_madds0	%9,%7,%3,%9\n"			// [-pz*qz+-px*qx][pw*qz+-py*qx]
		"ps_madds1	%11,%5,%3,%11\n"		// [-pz*qw+-px*qy][-pw*qw+py*qy]
		"ps_merge10	%8,%8,%8\n"				// [py*qz+pw*qx][-px*qz+pz*qx]
		"ps_madds1	%10,%0,%3,%10\n"		// [px*qw+-pz*qy][py*qw+pw*qy]
		"ps_merge10	%9,%9,%9\n"				// [pw*qz+-py*qx][-pz*qz+-px*qx]
		"ps_add		%8,%8,%10\n"			// [py*qz+pw*qx+px*qw+-pz*qy][-px*qz+pz*qx+py*qw+pw*qy]
		"psq_st		%8,0(%14),0,0\n"		// X = [py*qz+pw*qx+px*qw+-pz*qy], Y = [-px*qz+pz*qx+py*qw+pw*qy]
		"ps_sub		%9,%9,%11\n"			// [pw*qz+-py*qx--pz*qw+-px*qy][-pz*qz+-px*qx--pw*qw+py*qy]
		"psq_st		%9,8(%14),0,0"			// Z = [pw*qz+-py*qx--pz*qw+-px*qy], W = [-pz*qz+-px*qx--pw*qw+py*qy]
		: "=&f"(aXY),"=&f"(aZW),"=&f"(bXY),"=&f"(bZW),"=&f"(tmp0),"=&f"(tmp1),"=&f"(tmp2),"=&f"(tmp3),"=&f"(tmp4),"=&f"(tmp5),"=&f"(tmp6),"=&f"(tmp7)
		: "b"(a),"b"(b),"b"(ab)
		: "memory"
	);
}
#endif

void c_guQuatNormalize(guQuaternion *a,guQuaternion *d)
{
	f32 dot,scale;

	dot = (a->x*a->x) + (a->y*a->y) + (a->z*a->z) + (a->w*a->w);
	if(dot==0.0f) d->x = d->y = d->z = d->w = 0.0f;
	else {
		scale = 1.0f/sqrtf(dot);
		d->x = a->x*scale;
		d->y = a->y*scale;
		d->z = a->z*scale;
		d->w = a->w*scale;
	}
}

#ifdef GEKKO
void ps_guQuatNormalize(register guQuaternion *a,register guQuaternion *d)
{
	register f32 c_zero = 0.0f;
	register f32 c_half = 0.5f;
	register f32 c_three = 3.0f;
	register f32 axy,azw,tmp0,tmp1,tmp2,tmp3;

	__asm__ __volatile__ (
		"psq_l		%0,0(%6),0,0\n"		// [ax][ay]
		"ps_mul		%2,%0,%0\n"			// [ax*ax][ay*ay]
		"psq_l		%1,8(%6),0,0\n"		// [az][aw]
		"ps_madd	%2,%1,%1,%2\n"		// [az*az+ax*ax][aw*aw+ay*ay]
		"ps_sum0	%2,%2,%2,%2\n"		// [az*az+ax*ax+aw*aw+ay*ay][aw*aw+ay*ay]
		"frsqrte	%3,%2\n"			// reciprocal sqrt estimated
		//Newton-Raphson refinement 1 step: (E/2)*(3 - x*E*E)
		"fmul		%4,%3,%3\n"			// E*E
		"fmul		%5,%3,%8\n"			// E*0.5 = E/2
		"fnmsub		%4,%4,%2,%9\n"		// -(E*E*x - 3) = (3 - x*E*E)
		"fmul		%3,%4,%5\n"			// (E/2)*(3 - x*E*E)
		"ps_sel		%3,%2,%3,%10\n"		// NaN check: if(mag==0.0f)
		"ps_muls0	%0,%0,%3\n"			// [ax*rsqmag][ay*rsqmag]
		"ps_muls0	%1,%1,%3\n"			// [az*rsqmag][aw*rsqmag]
		"psq_st		%0,0(%7),0,0\n"		// X = [az*rsqmag], Y = [aw*rsqmag]
		"psq_st		%1,8(%7),0,0\n"		// Z = [az*rsqmag], W = [aw*rsqmag]
		: "=&f"(axy),"=&f"(azw),"=&f"(tmp0),"=&f"(tmp1),"=&f"(tmp2),"=&f"(tmp3)
		: "b"(a),"b"(d),"f"(c_half),"f"(c_three),"f"(c_zero)
		: "memory"
	);
}
#endif

void c_guQuatInverse(guQuaternion *a,guQuaternion *d)
{
	f32 mag,nrminv;

	mag = (a->x*a->x) + (a->y*a->y) + (a->z*a->z) + (a->w*a->w);
	if(mag==0.0f) mag = 1.0f;

	nrminv = 1.0f/mag;
	d->x = -a->x*nrminv;
	d->y = -a->y*nrminv;
	d->z = -a->z*nrminv;
	d->w =  a->w*nrminv;
}

#ifdef GEKKO
void ps_guQuatInverse(register guQuaternion *a,register guQuaternion *d)
{
	register f32 c_one = 1.0f;
	register f32 axy,azw,tmp0,tmp1,tmp2,tmp3,tmp4,tmp5;

	__asm__ __volatile__ (
		"psq_l		%0,0(%8),0,0\n"		// [ax][ay]
		"ps_mul		%2,%0,%0\n"			// [ax*ax][ay*ay]
		"ps_sub		%3,%10,%10\n"		// [1 - 1][1 - 1]
		"psq_l		%1,8(%8),0,0\n"		// [az][aw]
		"ps_madd	%2,%1,%1,%2\n"		// [az*az+ax*ax][aw*aw+ay*ay]
		"ps_add		%7,%0,%10\n"		// [1 + 1][1 + 1]
		"ps_sum0	%2,%2,%2,%2\n"		// [az*az+ax*ax+aw*aw+ay*ay][aw*aw+ay*ay]
		"fcmpu		cr0,%2,%3\n"		// [az*az+ax*ax+aw*aw+ay*ay] == 0.0f
		"beq-		1f\n"
		"fres		%4,%2\n"			// 1.0f/mag
		"ps_neg		%5,%2\n"			// -mag
		// Newton-Rapson refinement (x1) : E' = 2E-X*E*E
		"ps_nmsub	%6,%2,%4,%7\n"		//
		"ps_mul		%4,%4,%6\n"			//
		"b			2f\n"
		"1:\n"
		"fmr		%4,%10\n"
		"2:\n"
		"ps_neg		%7,%4\n"
		"ps_muls1	%5,%4,%1\n"
		"ps_muls0	%0,%0,%7\n"
		"psq_st		%5,12(%9),1,0\n"
		"ps_muls0	%6,%1,%7\n"
		"psq_st		%0,0(%9),0,0\n"
		"psq_st		%6,8(%9),1,0\n"
		: "=&f"(axy),"=&f"(azw),"=&f"(tmp0),"=&f"(tmp1),"=&f"(tmp2),"=&f"(tmp3),"=&f"(tmp4),"=&f"(tmp5)
		: "b"(a),"b"(d),"f"(c_one)
	);
}
#endif

void c_guQuatMtx(guQuaternion *a,Mtx m)
{
	const f32 diag = guMtxRowCol(m,0,0) + guMtxRowCol(m,1,1) + guMtxRowCol(m,2,2) + 1;

	if(diag>0.0f) {
		const f32 scale = sqrtf(diag)*2.0f;

		a->x = (guMtxRowCol(m,2,1) - guMtxRowCol(m,1,2))/scale;
		a->y = (guMtxRowCol(m,0,2) - guMtxRowCol(m,2,0))/scale;
		a->z = (guMtxRowCol(m,1,0) - guMtxRowCol(m,0,1))/scale;
		a->w = 0.25f*scale;
	} else {
		if(guMtxRowCol(m,0,0)>guMtxRowCol(m,1,1) && guMtxRowCol(m,0,0)>guMtxRowCol(m,2,2)) {
			const f32 scale = sqrtf(1.0f + guMtxRowCol(m,0,0) + guMtxRowCol(m,1,1) + guMtxRowCol(m,2,2))*2.0f;

			a->x = 0.25f*scale;
			a->y = (guMtxRowCol(m,0,1) + guMtxRowCol(m,1,0))/scale;
			a->z = (guMtxRowCol(m,2,0) + guMtxRowCol(m,0,2))/scale;
			a->w = (guMtxRowCol(m,2,1) - guMtxRowCol(m,1,2))/scale;
		} else if(guMtxRowCol(m,1,1)>guMtxRowCol(m,2,2)) {
			const f32 scale = sqrtf(1.0f + guMtxRowCol(m,0,0) + guMtxRowCol(m,1,1) + guMtxRowCol(m,2,2))*2.0f;

			a->x = (guMtxRowCol(m,0,1) + guMtxRowCol(m,1,0))/scale;
			a->y = 0.25f*scale;
			a->z = (guMtxRowCol(m,1,2) + guMtxRowCol(m,2,1))/scale;
			a->w = (guMtxRowCol(m,0,2) - guMtxRowCol(m,2,0))/scale;
		} else {
			const f32 scale = sqrtf(1.0f + guMtxRowCol(m,0,0) + guMtxRowCol(m,1,1) + guMtxRowCol(m,2,2))*2.0f;

			a->x = (guMtxRowCol(m,0,2) + guMtxRowCol(m,2,0))/scale;
			a->y = (guMtxRowCol(m,1,2) + guMtxRowCol(m,2,1))/scale;
			a->z = 0.25f*scale;
			a->w = (guMtxRowCol(m,1,0) - guMtxRowCol(m,0,1))/scale;
		}
	}
	c_guQuatNormalize(a,a);
}

void c_guMtxQuat(Mtx m,guQuaternion *a)
{
	guMtxRowCol(m,0,0) = 1.0f - 2.0f*a->y*a->y - 2.0f*a->z*a->z;
	guMtxRowCol(m,1,0) = 2.0f*a->x*a->y - 2.0f*a->z*a->w;
	guMtxRowCol(m,2,0) = 2.0f*a->x*a->z + 2.0f*a->y*a->w;

	guMtxRowCol(m,0,1) = 2.0f*a->x*a->y + 2.0f*a->z*a->w;
	guMtxRowCol(m,1,1) = 1.0f - 2.0f*a->x*a->x - 2.0f*a->z*a->z;
	guMtxRowCol(m,2,1) = 2.0f*a->z*a->y - 2.0f*a->x*a->w;

	guMtxRowCol(m,0,2) = 2.0f*a->x*a->z - 2.0f*a->y*a->w;
	guMtxRowCol(m,1,2) = 2.0f*a->z*a->y + 2.0f*a->x*a->w;
	guMtxRowCol(m,2,2) = 1.0f - 2.0f*a->x*a->x - 2.0f*a->y*a->y;
}

void guVecHalfAngle(guVector *a,guVector *b,guVector *half)
{
	guVector tmp1,tmp2,tmp3;

	tmp1.x = -a->x;
	tmp1.y = -a->y;
	tmp1.z = -a->z;

	tmp2.x = -b->x;
	tmp2.y = -b->y;
	tmp2.z = -b->z;

	guVecNormalize(&tmp1);
	guVecNormalize(&tmp2);

	guVecAdd(&tmp1,&tmp2,&tmp3);
	if(guVecDotProduct(&tmp3,&tmp3)>0.0f) guVecNormalize(&tmp3);

	*half = tmp3;
}
