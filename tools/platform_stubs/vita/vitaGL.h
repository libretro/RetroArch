/* Compile-only Vita stub for the matrix's gl1 video lane.
 *
 * gl1.c's Vita branches reach a compiler only inside the Vita
 * toolchain, so a change to them is green everywhere else until that
 * job runs.  vitaGL is a fixed-function GL on top of SceGxm, so the GL
 * types, entry points and enums come from the host's GL headers; this
 * adds the vitaGL-only names the driver uses, in the shapes vitaGL
 * gives them, and nothing else. */
#ifndef STUB_VITAGL_H
#define STUB_VITAGL_H

#include <GL/gl.h>
#include <GL/glext.h>
#include <psp2/gxm.h>

GLboolean vglInitExtended(int legacy_pool_size, int width, int height,
      int ram_threshold, SceGxmMultisampleMode msaa);
void vglUseVram(GLboolean usage);
void glUseProgram(GLuint program);

#endif
