/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: MIT */
#include "gpu_gl_context.h"

int main(int argc, char **argv)
{
   rpy_gl_test_t test;
   rpy_gpu_vector_t *vector = NULL;
   size_t i;
   double maximum = 0.0;
   int success = 0;
   memset(&test, 0, sizeof(test));
   if (argc != 4)
   {
      fprintf(stderr, "Usage: gpu_gl shader.comp frame.pyro reference.u16\n");
      return EXIT_FAILURE;
   }
   vector = rpy_gpu_vector_load(argv[2], argv[3]);
   if (!vector || !rpy_gl_setup(&test, vector, argv[1]))
      goto end;
   if (!rpy_gl_execute(&test, vector, 1))
      goto end;
   test.api.BindBuffer(RPY_GL_STORAGE, test.buffers[0]);
   test.api.MemoryBarrier(0x200);
   test.api.GetBufferSubData(RPY_GL_STORAGE, 0, (ptrdiff_t)vector->bytes, vector->readback);
   if (!rpy_gpu_vector_compare(vector, vector->readback))
      goto end;
   memcpy(vector->atlas, vector->readback, vector->bytes);
   if (!rpy_gl_execute(&test, vector, 0) || !rpy_gl_execute(&test, vector, 1))
      goto end;
   test.api.BindBuffer(RPY_GL_STORAGE, test.buffers[0]);
   test.api.MemoryBarrier(0x200);
   test.api.GetBufferSubData(RPY_GL_STORAGE, 0, (ptrdiff_t)vector->bytes, vector->readback);
   for (i = 0; i < vector->bytes / sizeof(float); i++)
   {
      double error = fabs((double)vector->atlas[i] - vector->readback[i]);
      if (!(error <= 0.0001))
         goto end;
      if (error > maximum)
         maximum = error;
   }
   if (glGetError() != GL_NO_ERROR)
      goto end;
   printf("GPU forward/inverse round trip: maximum error %.9f\n", maximum);
   success = 1;
end:
   rpy_gl_free(&test);
   rpy_gpu_vector_free(vector);
   puts(success ? "OpenGL compute DWT: PASS" : "OpenGL compute DWT: FAIL");
   return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
