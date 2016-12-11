#version 310 es
precision highp float;

layout(std140, set = 0, binding = 0) uniform UBO
{
   float time;
} constants;

layout(location = 0) out vec4 FragColor;

	vec2 res = vec2(1920*3, 1080*3);

	// High quality = larger patterns
	// quality * 84 is the number of seconds in a loop
	float quality = 8.0;
	
	float Cellular2D(vec2 P) {
		//  https://github.com/BrianSharpe/Wombat/blob/master/Cellular2D.glsl
		vec2 Pi = floor(P);
		vec2 Pf = P - Pi;
		vec2 Pt = vec2( Pi.x, Pi.y + 1.0 );
		Pt = Pt - floor(Pt * ( 1.0 / 71.0 )) * 71.0;
		Pt += vec2( 26.0, 161.0 ).xy;
		Pt *= Pt;
		Pt = Pt.xy * Pt.yx;
		float hash_x = fract( Pt * ( 1.0 / 951.135664 ) ).x * 2.0 - 1.0;
		float hash_y = fract( Pt * ( 1.0 / 642.949883 ) ).y * 2.0 - 1.0;
		const float JITTER_WINDOW = 0.25;
		hash_x = ( ( hash_x * hash_x * hash_x ) - sign( hash_x ) ) * JITTER_WINDOW;
		hash_y = ( ( hash_y * hash_y * hash_y ) - sign( hash_y ) ) * JITTER_WINDOW;
		vec2 dd = vec2(Pf.x - hash_x, Pf.y - hash_y);
		float d = dd.x * dd.x + dd.y * dd.y;
		return d * ( 1.0 / 1.125 );
	}
	
	float snow(vec2 pos, float time, float scale) {
		// add wobble
		pos.x += cos(pos.y*4.0 + time*3.14159*2.0 + 1.0/scale)/(8.0/scale);
		// add gravity
		pos += time*scale*vec2(-0.5, 1.0);
		return max(
			1.0 - Cellular2D(mod(pos/scale, scale*quality)*16.0)*1024.0,
			0.0
		) * (scale*0.5 + 0.5);
	}

void main()
{
		float winscale = max(res.x, res.y);
		float tim = mod(constants.time/800.0, 84.0*quality);
		vec2 pos = gl_FragCoord.xy/winscale;
		float a = 0.0;
		// Each of these is a layer of snow
		// Remove some for better performance
		// Changing the scale (3rd value) will mess with the looping
		a += snow(pos, tim, 1.0);
		a += snow(pos, tim, 0.7);
		a += snow(pos, tim, 0.6);
		a += snow(pos, tim, 0.5);
		a += snow(pos, tim, 0.4);
		a += snow(pos, tim, 0.3);
		a += snow(pos, tim, 0.25);
		a += snow(pos, tim, 0.125);
		a = a * min(pos.y*4.0, 1.0);
		FragColor = vec4(1.0, 1.0, 1.0, a);
}
