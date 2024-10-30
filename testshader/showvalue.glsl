#if defined(VERTEX)

#if __VERSION__ >= 130
#define COMPAT_VARYING out
#define COMPAT_ATTRIBUTE in
#define COMPAT_TEXTURE texture
#else
#define COMPAT_VARYING varying
#define COMPAT_ATTRIBUTE attribute
#define COMPAT_TEXTURE texture2D
#endif

#ifdef GL_ES
#define COMPAT_PRECISION mediump
#else
#define COMPAT_PRECISION
#endif

COMPAT_ATTRIBUTE vec4 VertexCoord;
COMPAT_ATTRIBUTE vec4 COLOR;
COMPAT_ATTRIBUTE vec4 TexCoord;
COMPAT_VARYING vec4 COL0;
COMPAT_VARYING vec4 TEX0;

uniform mat4 MVPMatrix;
uniform COMPAT_PRECISION int FrameDirection;
uniform COMPAT_PRECISION int FrameCount;
uniform COMPAT_PRECISION vec2 OutputSize;
uniform COMPAT_PRECISION vec2 TextureSize;
uniform COMPAT_PRECISION vec2 InputSize;
uniform COMPAT_PRECISION int  Rotation;
#ifdef _HAS_ORIGINALASPECT_UNIFORMS
uniform COMPAT_PRECISION float OriginalAspect;
uniform COMPAT_PRECISION float OriginalAspectRotated;
#endif
#ifdef _HAS_FRAMETIME_UNIFORMS
uniform COMPAT_PRECISION int FrameTimeDelta;
uniform COMPAT_PRECISION float CoreFPS;
#endif
void main()
{
    gl_Position = VertexCoord.x * MVPMatrix[0] + VertexCoord.y * MVPMatrix[1] + VertexCoord.z * MVPMatrix[2] + VertexCoord.w * MVPMatrix[3];
    TEX0.xy = TexCoord.xy;
}

#elif defined(FRAGMENT)

#if __VERSION__ >= 130
#define COMPAT_VARYING in
#define COMPAT_TEXTURE texture
out vec4 FragColor;
#else
#define COMPAT_VARYING varying
#define FragColor gl_FragColor
#define COMPAT_TEXTURE texture2D
#endif

#ifdef GL_ES
#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif
#define COMPAT_PRECISION mediump
#else
#define COMPAT_PRECISION
#endif

uniform COMPAT_PRECISION int FrameDirection;
uniform COMPAT_PRECISION int FrameCount;
uniform COMPAT_PRECISION vec2 OutputSize;
uniform COMPAT_PRECISION vec2 TextureSize;
uniform COMPAT_PRECISION vec2 InputSize;
uniform COMPAT_PRECISION int  Rotation;
#ifdef _HAS_ORIGINALASPECT_UNIFORMS
uniform COMPAT_PRECISION float OriginalAspect;
uniform COMPAT_PRECISION float OriginalAspectRotated;
#endif
#ifdef _HAS_FRAMETIME_UNIFORMS
uniform COMPAT_PRECISION int FrameTimeDelta;
uniform COMPAT_PRECISION float CoreFPS;
#endif
uniform sampler2D Texture;
COMPAT_VARYING vec4 TEX0;



float DigitBin( const int x )
{
    return x==0?480599.0:x==1?139810.0:x==2?476951.0:x==3?476999.0:x==4?350020.0:x==5?464711.0:x==6?464727.0:x==7?476228.0:x==8?481111.0:x==9?481095.0:0.0;
}

float PrintValue( vec2 vStringCoords, float fValue, float fMaxDigits, float fDecimalPlaces )
{
    if ((vStringCoords.y < 0.0) || (vStringCoords.y >= 1.0)) return 0.0;

    bool bNeg = ( fValue < 0.0 );
	fValue = abs(fValue);

	float fLog10Value = log2(abs(fValue)) / log2(10.0);
	float fBiggestIndex = max(floor(fLog10Value), 0.0);
	float fDigitIndex = fMaxDigits - floor(vStringCoords.x);
	float fCharBin = 0.0;
	if(fDigitIndex > (-fDecimalPlaces - 1.01)) {
		if(fDigitIndex > fBiggestIndex) {
			if((bNeg) && (fDigitIndex < (fBiggestIndex+1.5))) fCharBin = 1792.0;
		} else {
			if(fDigitIndex == -1.0) {
				if(fDecimalPlaces > 0.0) fCharBin = 2.0;
			} else {
                float fReducedRangeValue = fValue;
                if(fDigitIndex < 0.0) { fReducedRangeValue = fract( fValue ); fDigitIndex += 1.0; }
				float fDigitValue = (abs(fReducedRangeValue / (pow(10.0, fDigitIndex))));
                fCharBin = DigitBin(int(floor(mod(fDigitValue, 10.0))));
			}
        }
	}
    return floor(mod((fCharBin / pow(2.0, floor(fract(vStringCoords.x) * 4.0) + (floor(vStringCoords.y * 5.0) * 4.0))), 2.0));
}

vec3 PrintValueVec3( vec2 vStringCoords, vec2 FragCoord,  float fValue, float fMaxDigits, float fDecimalPlaces ) {
	vec3 vColour = vec3(0.0);
	vec2 vFontSize = vec2(8.0, 15.0);
	vec2 vPixelCoord1 = vStringCoords;
	FragCoord.y = (vFontSize.y*2.0) - FragCoord.y;
    float customDigit = PrintValue( (  FragCoord - vPixelCoord1    ) / vFontSize, fValue, fMaxDigits, fDecimalPlaces);
	vColour = mix( vColour, vec3(0.0, 1.0, 1.0), customDigit);
	return vColour;
}


void main()
{
	vec2 FragCoord = TEX0.xy * OutputSize.xy;
	float f0 = float(Rotation);
	float f1 = -1. ;
	float f2 = -1. ;
	int f3 = 999 ;
	float f4 = -1. ;
#ifdef _HAS_ORIGINALASPECT_UNIFORMS
	f1 = OriginalAspect;
	f2 = OriginalAspectRotated;
#endif
#ifdef _HAS_FRAMETIME_UNIFORMS
	f3 = FrameTimeDelta/1000;
	f4 = CoreFPS;
#endif
	vec3 v0 = PrintValueVec3( vec2(50., 0.), FragCoord,  f0, 3., 3. );
	vec3 v1 = PrintValueVec3( vec2(50., -50.), FragCoord, f1, 3., 3. );
	vec3 v2 = PrintValueVec3( vec2(50., -100), FragCoord,  f2, 3., 3. );
	vec3 v3 = PrintValueVec3( vec2(50,  -150), FragCoord,  float(f3), 3., 3.);
	vec3 v4 = PrintValueVec3( vec2(50,  -200), FragCoord,  f4, 3., 3.);
	FragColor.rgb = v0+v1+v2+v3+v4;

}
#endif
