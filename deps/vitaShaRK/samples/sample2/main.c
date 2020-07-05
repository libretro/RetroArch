// Simple compiler with file logging

#include <vitashark.h>
#include <stdlib.h>
#include <stdio.h>

const char fragment_shader[] =
	"float4 main(uniform float4 u_clear_color) : COLOR\n"
	"{\n"
	"	return u_clear_color;\n"
	"}"
	;
	
const char vertex_shader[] =
	"void main(\n"
	"float3 aPosition,\n"
	"float3 aColor,\n"
	"uniform float4x4 wvp,\n"
	"float4 out vPosition: POSITION,\n"
	"float4 out vColor: COLOR)\n"
	"{\n"
	"	vPosition = mul(float4(aPosition, 1.f), wvp);\n"
	"	vColor = float4(aColor, 1.f);\n"
	"}"
	;

char curr_compilation[256];

void log_cb(const char *msg, shark_log_level msg_level, int line) {
	FILE *f = fopen("ux0:/data/shark.log", "a+");
	switch (msg_level) {
	case SHARK_LOG_INFO:
		fprintf(f, "%s) INFO: %s at line %d\n", curr_compilation, msg, line);
		break;
	case SHARK_LOG_WARNING:
		fprintf(f, "%s) WARNING: %s at line %d\n", curr_compilation, msg, line);
		break;
	case SHARK_LOG_ERROR:
		fprintf(f, "%s) ERROR: %s at line %d\n", curr_compilation, msg, line);
		break;
	default:
		break;
	}
	fclose(f);
}

void saveGXP(SceGxmProgram *p, uint32_t size, const char *fname) {
	FILE *f = fopen(fname, "wb");
	fwrite(p, 1, size, f);
	fclose(f);
}

int main() {
	// Initializing vitaShaRK
	if (shark_init(NULL) < 0) // NOTE: libshacccg.suprx will need to be placed in ur0:data
		return -1;

	// Setting up logger
	shark_install_log_cb(log_cb);
	shark_set_warnings_level(SHARK_WARN_MAX);
	
	// Compiling fragment shader
	sprintf(curr_compilation, "clear_f.gxp");
	uint32_t size = sizeof(fragment_shader) - 1;
	SceGxmProgram *p = shark_compile_shader(fragment_shader, &size, SHARK_FRAGMENT_SHADER);
	
	// Saving compiled GXP file on SD
	if (p) saveGXP(p, size, "ux0:data/clear_f.gxp");
	
	shark_clear_output();
	
	// Compiling vertex shader
	sprintf(curr_compilation, "rgb_v.gxp");
	size = sizeof(vertex_shader) - 1;
	p = shark_compile_shader(vertex_shader, &size, SHARK_VERTEX_SHADER);
	
	// Saving compiled GXP file on SD
	if (p) saveGXP(p, size, "ux0:data/rgb_v.gxp");

	shark_clear_output();
	
	shark_end();
	
	return 0;
}
