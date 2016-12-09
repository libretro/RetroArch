#include "shaders_common.h"

static const char *stock_fragment_xmb_simple_snow = GLSL(
	uniform float time;
	vec2 res      = vec2(1920*3, 1080*3);
	float quality = 8.0;

	float Cellular2D(vec2 P) {
		vec2 Pi = floor(P);
		vec2 Pf = P - Pi;
		vec2 Pt = vec2( Pi.x, Pi.y + 1.0 );
		Pt = Pt - floor(Pt * ( 1.0 / 71.0 )) * 71.0;
		Pt += vec2( 26.0, 161.0 );
		Pt *= Pt;
		Pt = Pt.xy * Pt.yx;
		float hash_x = fract( Pt.x * ( 1.0 / 951.135664 ) ) * 2.0 - 1.0;
		float hash_y = fract( Pt.y * ( 1.0 / 642.949883 ) ) * 2.0 - 1.0;
		const float JITTER_WINDOW = 0.25;
		hash_x = ( ( hash_x * hash_x * hash_x ) - sign( hash_x ) ) * JITTER_WINDOW;
		hash_y = ( ( hash_y * hash_y * hash_y ) - sign( hash_y ) ) * JITTER_WINDOW;
		vec2 dd = vec2(Pf.x - hash_x, Pf.y - hash_y);
		float d = dd.x * dd.x + dd.y * dd.y;
		return d * ( 1.0 / 1.125 );
	}

	float snow(vec2 pos, float time, float scale) {
		pos.x += cos(pos.y*4.0 + time*3.14159*2.0 + 1.0/scale)/(8.0/scale);
		pos += time*scale*vec2(-0.5, 1.0);
		return max(
			1.0 - Cellular2D(mod(pos/scale, scale*quality)*16.0)*1024.0,
			0.0
		) * (scale*0.5 + 0.5);
	}

	void main( void ) {
		float winscale = max(res.x, res.y);
		float tim = mod(time/8.0, 84.0*quality);
		vec2 pos = gl_FragCoord.xy/winscale;
		float a = 0.0;
		a += snow(pos, tim, 1.0);
		a += snow(pos, tim, 0.7);
		a += snow(pos, tim, 0.6);
		a += snow(pos, tim, 0.5);
		a += snow(pos, tim, 0.4);
		a += snow(pos, tim, 0.3);
		a += snow(pos, tim, 0.25);
		a += snow(pos, tim, 0.125);
		a = a * min(pos.y*4.0, 1.0);
		gl_FragColor = vec4(1.0, 1.0, 1.0, a);
	}

);
