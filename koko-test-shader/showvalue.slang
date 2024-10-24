#version 450

layout(push_constant) uniform Push
{
	vec4 SourceSize;
	vec4 OriginalSize;
	vec4 OutputSize;
	uint FrameCount;
	uint Rotation;
#ifdef _HAS_ORIGINALASPECT_UNIFORMS
	float OriginalAspect;
	float OriginalAspectRot;
#endif



} params;

layout(std140, set = 0, binding = 0) uniform UBO
{
	mat4 MVP;
} global;

#pragma stage vertex
layout(location = 0) in vec4 Position;
layout(location = 1) in vec2 TexCoord;
layout(location = 0) out vec2 vTexCoord;

void main()
{
   gl_Position = global.MVP * Position;
   vTexCoord = TexCoord;
}

#pragma stage fragment
layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 FragColor;

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


void main() {
	vec2 FragCoord = vTexCoord * params.OutputSize.xy;
	float f0 = float(params.Rotation);
	float f1 = -1 ;
	float f2 = -1 ;
#ifdef _HAS_ORIGINALASPECT_UNIFORMS
	f1 = params.OriginalAspect;
	f2 = params.OriginalAspectRot;
#endif
	vec3 v0 = PrintValueVec3( vec2(50, -10) , FragCoord,  f0, 3, 3);
	vec3 v1 = PrintValueVec3( vec2(100, -10), FragCoord,  f1, 3, 3);
	vec3 v2 = PrintValueVec3( vec2(150, -10), FragCoord,  f2, 3, 3);
	FragColor.rgb = v0+v1+v2;
}
