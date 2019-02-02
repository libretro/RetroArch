/*
Copyright (c) 2012, Broadcom Europe Ltd
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <math.h>
#include <stdio.h>
#include <stdint.h>

#include "GLES/gl.h"
#include <GLES/glext.h>
#include "EGL/egl.h"
#include "EGL/eglext.h"
#include "models.h"

#define VMCS_RESOURCE(a,b) (b)

/******************************************************************************
Private typedefs, macros and constants
******************************************************************************/

enum {VBO_VERTEX, VBO_NORMAL, VBO_TEXTURE, VBO_MAX};
#define MAX_MATERIALS 4
#define MAX_MATERIAL_NAME 32

typedef struct wavefront_material_s {
   GLuint vbo[VBO_MAX];
   int numverts;
   char name[MAX_MATERIAL_NAME];
   GLuint texture;
} WAVEFRONT_MATERIAL_T;

typedef struct wavefront_model_s {
   WAVEFRONT_MATERIAL_T material[MAX_MATERIALS];
   int num_materials;
   GLuint texture;
} WAVEFRONT_MODEL_T;


/******************************************************************************
Static Data
******************************************************************************/

/******************************************************************************
Static Function Declarations
******************************************************************************/

/******************************************************************************
Static Function Definitions
******************************************************************************/

static void create_vbo(GLenum type, GLuint *vbo, int size, void *data)
{
   glGenBuffers(1, vbo);
   vc_assert(*vbo);
   glBindBuffer(type, *vbo);
   glBufferData(type, size, data, GL_STATIC_DRAW);
   glBindBuffer(type, 0);     
}


static void destroy_vbo(GLuint *vbo)
{
   glDeleteBuffers(1, vbo);
   *vbo = 0;
}

#define MAX_VERTICES 100000
static void *allocbuffer(int size)
{
   return malloc(size);
}

static void freebuffer(void *p)
{
   free (p);
}

static void centre_and_rescale(float *verts, int numvertices)
{
   float cx=0.0f, cy=0.0f, cz=0.0f, scale=0.0f;
   float minx=0.0f, miny=0.0f, minz=0.0f;
   float maxx=0.0f, maxy=0.0f, maxz=0.0f;
   int i;
   float *v = verts;
   minx = maxx = verts[0];
   miny = maxy = verts[1];
   minz = maxz = verts[2];
   for (i=0; i<numvertices; i++) {
      float x = *v++;
      float y = *v++;
      float z = *v++;
      minx = vcos_min(minx, x);
      miny = vcos_min(miny, y);
      minz = vcos_min(minz, z);
      maxx = vcos_max(maxx, x);
      maxy = vcos_max(maxy, y);
      maxz = vcos_max(maxz, z);
      cx += x;
      cy += y;
      cz += z;
   }
   cx /= (float)numvertices;
   cy /= (float)numvertices;
   cz /= (float)numvertices;
   scale = 3.0f / (maxx-minx + maxy-miny + maxz-minz);
   v = verts;
   for (i=0; i<numvertices; i++) {
      *v = (*v-cx) * scale; v++;
      *v = (*v-cy) * scale; v++;
      *v = (*v-cz) * scale; v++;
   }
}

static void renormalise(float *verts, int numvertices)
{
   int i;
   float *v = verts;
   for (i=0;i<numvertices; i++) {
      float x = v[0];
      float y = v[1];
      float z = v[2];
      float scale = 1.0f/sqrtf(x*x + y*y + z*z);
      *v++ = x * scale;
      *v++ = y * scale;
      *v++ = z * scale;
   }
}

static void deindex(float *dst, const float *src, const unsigned short *indexes, GLsizei size, GLsizei count)
{
   int i;
   for (i=0; i<count; i++) {
      int ind = size * (indexes[0]-1);
      *dst++ = src[ind + 0];
      *dst++ = src[ind + 1];
      // todo: optimise - move out of loop
      if (size >= 3) *dst++ = src[ind + 2];
      indexes += 3;
   }
}

int draw_wavefront(MODEL_T m, GLuint texture)
{
   int i;
   WAVEFRONT_MODEL_T *model = (WAVEFRONT_MODEL_T *)m;

   for (i=0; i<model->num_materials; i++) {
      WAVEFRONT_MATERIAL_T *mat = model->material + i;
      if (mat->texture == -1) continue;

      if (mat->texture)
         glBindTexture(GL_TEXTURE_2D, mat->texture);
      else
         glBindTexture(GL_TEXTURE_EXTERNAL_OES, texture);

      if (mat->vbo[VBO_VERTEX]) {
         glBindBuffer(GL_ARRAY_BUFFER, mat->vbo[VBO_VERTEX]);
         glVertexPointer(3, GL_FLOAT, 0, NULL);
      }
      if (mat->vbo[VBO_NORMAL]) {   
         glEnableClientState(GL_NORMAL_ARRAY);
         glBindBuffer(GL_ARRAY_BUFFER, mat->vbo[VBO_NORMAL]);
         glNormalPointer(GL_FLOAT, 0, NULL);
      } else {
         glDisableClientState(GL_NORMAL_ARRAY);
      }
      if (mat->vbo[VBO_TEXTURE]) {   
         glEnableClientState(GL_TEXTURE_COORD_ARRAY);
         glBindBuffer(GL_ARRAY_BUFFER, mat->vbo[VBO_TEXTURE]);
         glTexCoordPointer(2, GL_FLOAT, 0, NULL);
      } else {
         glDisableClientState(GL_TEXTURE_COORD_ARRAY);
      }
      glDrawArrays(GL_TRIANGLES, 0, mat->numverts);
   }
   glBindBuffer(GL_ARRAY_BUFFER, 0);
   return 0;
}

struct wavefront_model_loading_s {
   unsigned short material_index[MAX_MATERIALS];
   int num_materials;
   int numv, numt, numn, numf;
   unsigned int data[0];
};

static int load_wavefront_obj(const char *modelname, WAVEFRONT_MODEL_T *model, struct wavefront_model_loading_s *m)
{
   char line[256+1];
   unsigned short pp[54+1];
   FILE *fp;
   int i, valid;
   float *qv = (float *)m->data;
   float *qt = (float *)m->data + 3 * MAX_VERTICES;
   float *qn = (float *)m->data + (3+2) * MAX_VERTICES;
   unsigned short *qf = (unsigned short *)((float *)m->data + (3+2+3) * MAX_VERTICES);
   float *pv = qv, *pt = qt, *pn = qn;
   unsigned short *pf = qf;
   fp = fopen(modelname, "r");
   if (!fp) return -1;

   m->num_materials = 0;
   m->material_index[0] = 0;

   valid = fread(line, 1, sizeof(line)-1, fp);

   while (valid > 0) {
      char *s, *end = line;
      
      while((end-line < valid) && *end != '\n' && *end != '\r')
         end++;
      *end++ = 0;

      if((end-line < valid) && *end != '\n' && *end != '\r')
         *end++ = 0;

      s = line;

      if (s[strlen(s)-1] == 10) s[strlen(s)-1]=0;
      switch (s[0]) {
      case '#': break; // comment
      case '\r': case '\n': case '\0': break; // blank line
      case 'm': vc_assert(strncmp(s, "mtllib", sizeof "mtllib"-1)==0); break;
      case 'o': break;
      case 'u': 
         if (sscanf(s, "usemtl %s", /*MAX_MATERIAL_NAME-1, */model->material[m->num_materials].name) == 1) {
            if (m->num_materials < MAX_MATERIALS) {
               if (m->num_materials > 0 && ((pf-qf)/3 == m->material_index[m->num_materials-1] || strcmp(model->material[m->num_materials-1].name, model->material[m->num_materials].name)==0)) {
                  strcpy(model->material[m->num_materials-1].name, model->material[m->num_materials].name);
                  m->num_materials--;
               } else
               m->material_index[m->num_materials] = (pf-qf)/3;
               m->num_materials++;
            }
         } else { printf("%s", s); vc_assert(0); }
         break;
      case 'g': vc_assert(strncmp(s, "g ", sizeof "g "-1)==0); break;
      case 's': vc_assert(strncmp(s, "s ", sizeof "s "-1)==0); break;
      case 'v': case 'f':
         if (sscanf(s, "v %f %f %f", pv+0, pv+1, pv+2) == 3) {
            pv += 3;
         } else if (sscanf(s, "vt %f %f", pt+0, pt+1) == 2) {
            pt += 2;
         } else if (sscanf(s, "vn %f %f %f", pn+0, pn+1, pn+2) == 3) {
            pn += 3;
         } else if (i = sscanf(s, "f"" %hu//%hu %hu//%hu %hu//%hu %hu//%hu %hu//%hu %hu//%hu"
                                     " %hu//%hu %hu//%hu %hu//%hu %hu//%hu %hu//%hu %hu//%hu"
                                     " %hu//%hu %hu//%hu %hu//%hu %hu//%hu %hu//%hu %hu//%hu %hu",
               pp+ 0, pp+ 1, pp+ 2, pp+ 3, pp+ 4, pp+ 5, pp+ 6, pp+ 7, pp+ 8, pp+ 9, pp+10, pp+11, 
               pp+12, pp+13, pp+14, pp+15, pp+16, pp+17, pp+18, pp+19, pp+20, pp+21, pp+22, pp+23, 
               pp+24, pp+25, pp+26, pp+27, pp+28, pp+29, pp+30, pp+32, pp+32, pp+33, pp+34, pp+35, pp+36), i >= 6) {
            int poly = i/2;
            //vc_assert(i < countof(pp)); // may need to increment poly count and pp array
            for (i=1; i<poly-1; i++) {
               *pf++ = pp[0]; *pf++ = 0; *pf++ = pp[1];
               *pf++ = pp[2*i+0]; *pf++ = 0; *pf++ = pp[2*i+1];
               *pf++ = pp[2*(i+1)+0]; *pf++ = 0; *pf++ = pp[2*(i+1)+1];
            }
         } else if (i = sscanf(s, "f"" %hu/%hu %hu/%hu %hu/%hu %hu/%hu %hu/%hu %hu/%hu"
                                     " %hu/%hu %hu/%hu %hu/%hu %hu/%hu %hu/%hu %hu/%hu"
                                     " %hu/%hu %hu/%hu %hu/%hu %hu/%hu %hu/%hu %hu/%hu %hu",
               pp+ 0, pp+ 1, pp+ 2, pp+ 3, pp+ 4, pp+ 5, pp+ 6, pp+ 7, pp+ 8, pp+ 9, pp+10, pp+11, 
               pp+12, pp+13, pp+14, pp+15, pp+16, pp+17, pp+18, pp+19, pp+20, pp+21, pp+22, pp+23, 
               pp+24, pp+25, pp+26, pp+27, pp+28, pp+29, pp+30, pp+32, pp+32, pp+33, pp+34, pp+35, pp+36), i >= 6) {
            int poly = i/2;
            //vc_assert(i < countof(pp); // may need to increment poly count and pp array
            for (i=1; i<poly-1; i++) {
               *pf++ = pp[0]; *pf++ = pp[1]; *pf++ = 0;
               *pf++ = pp[2*i+0]; *pf++ = pp[2*i+1]; *pf++ = 0;
               *pf++ = pp[2*(i+1)+0]; *pf++ = pp[2*(i+1)+1]; *pf++ = 0;
            }
         } else if (i = sscanf(s, "f"" %hu/%hu/%hu %hu/%hu/%hu %hu/%hu/%hu %hu/%hu/%hu %hu/%hu/%hu %hu/%hu/%hu"
                                     " %hu/%hu/%hu %hu/%hu/%hu %hu/%hu/%hu %hu/%hu/%hu %hu/%hu/%hu %hu/%hu/%hu"
                                     " %hu/%hu/%hu %hu/%hu/%hu %hu/%hu/%hu %hu/%hu/%hu %hu/%hu/%hu %hu/%hu/%hu %hu",
               pp+ 0, pp+ 1, pp+ 2, pp+ 3, pp+ 4, pp+ 5, pp+ 6, pp+ 7, pp+ 8, pp+ 9, pp+10, pp+11, pp+12, pp+13, pp+14, pp+15, pp+16, pp+17, 
               pp+18, pp+19, pp+20, pp+21, pp+22, pp+23, pp+24, pp+25, pp+26, pp+27, pp+28, pp+29, pp+30, pp+32, pp+32, pp+33, pp+34, pp+35, 
               pp+36, pp+37, pp+38, pp+39, pp+40, pp+41, pp+42, pp+43, pp+44, pp+45, pp+46, pp+47, pp+48, pp+49, pp+50, pp+51, pp+52, pp+53, pp+54), i >= 9) {
            int poly = i/3;
            //vc_assert(i < countof(pp); // may need to increment poly count and pp array
            for (i=1; i<poly-1; i++) {
               *pf++ = pp[0]; *pf++ = pp[1]; *pf++ = pp[2];
               *pf++ = pp[3*i+0]; *pf++ = pp[3*i+1]; *pf++ = pp[3*i+2];
               *pf++ = pp[3*(i+1)+0]; *pf++ = pp[3*(i+1)+1]; *pf++ = pp[3*(i+1)+2];
            }
         } else { printf("%s", s); vc_assert(0); }
         break;
      default: 
         printf("%02x %02x %s", s[0], s[1], s); vc_assert(0); break;
      }

      // shift down read characters and read some more into the end
      // if we didn't find a newline, then end is one off the end of our
      // line, so end-line will be valid+1
      i = end-line > valid ? valid : end-line;
      memmove(line, end, valid - i);
      valid -= i;
      valid += fread(line+valid, 1, sizeof(line)-1-valid, fp);
   }
   fclose(fp);

   if (m->num_materials==0) m->material_index[m->num_materials++] = 0;

   centre_and_rescale(qv, (pv-qv)/3);
   renormalise(qn, (pn-qn)/3);
   //centre_and_rescale2(qt, (pt-qt)/2);

   m->numv = pv-qv;
   m->numt = pt-qt;
   m->numn = pn-qn;
   m->numf = pf-qf;

   // compress array
   //memcpy((float *)m->data, (float *)m->data, m->numv * sizeof *qv); - nop
   memcpy((float *)m->data + m->numv, (float *)m->data + 3 * MAX_VERTICES, m->numt * sizeof *qt);
   memcpy((float *)m->data + m->numv + m->numt,(float *) m->data + (3 + 2) * MAX_VERTICES, m->numn * sizeof *qn);
   memcpy((float *)m->data + m->numv + m->numt + m->numn, (float *)m->data + (3 + 2 + 3) * MAX_VERTICES, m->numf * sizeof *qf);

   return 0;
}

static int load_wavefront_dat(const char *modelname, WAVEFRONT_MODEL_T *model, struct wavefront_model_loading_s *m)
{
   FILE *fp;
   int s;
   const int size = sizeof *m + 
      sizeof(float)*(3+2+3)*MAX_VERTICES +   // 3 vertices + 2 textures + 3 normals
      sizeof(unsigned short)*3*MAX_VERTICES; //each face has 9 vertices

   fp = fopen(modelname, "r");
   if (!fp) return -1;
   s = fread(m, 1, size, fp);
   if (s < 0) return -1;
   fclose(fp);
   return 0;
}

MODEL_T load_wavefront(const char *modelname, const char *texturename)
{
   WAVEFRONT_MODEL_T *model;
   float *temp, *qv, *qt, *qn;
   unsigned short *qf;
   int i;
   int numverts = 0, offset = 0;
   struct wavefront_model_loading_s *m;
   int s=-1;
   char modelname_obj[128];
   model = malloc(sizeof *model);
   if (!model || !modelname) return NULL;
   memset (model, 0, sizeof *model);
   model->texture = 0; //load_texture(texturename);
   m = allocbuffer(sizeof *m + 
      sizeof(float)*(3+2+3)*MAX_VERTICES +    // 3 vertices + 2 textures + 3 normals
      sizeof(unsigned short)*3*MAX_VERTICES); //each face has 9 vertices
   if (!m) return 0;

   if (strlen(modelname) + 5 <= sizeof modelname_obj) {
      strcpy(modelname_obj, modelname);
      strcat(modelname_obj, ".dat");
      s = load_wavefront_dat(modelname_obj, model, m);
   }
   if (s==0) {}
   else if (strncmp(modelname + strlen(modelname) - 4, ".obj", 4) == 0) {
      #ifdef DUMP_OBJ_DAT
      int size;
      FILE *fp;
      #endif
      s = load_wavefront_obj(modelname, model, m);
      #ifdef DUMP_OBJ_DAT
      strcpy(modelname_obj, modelname);
      strcat(modelname_obj, ".dat");
      size = sizeof *m + 
         sizeof(float)*(3*m->numv+2*m->numt+3*m->numn) +  // 3 vertices + 2 textures + 3 normals
         sizeof(unsigned short)*3*m->numf;                //each face has 9 vertices
      fp = host_file_open(modelname_obj, "w");
      fwrite(m, 1, size, fp);
      fclose(fp);
      #endif
   } else if (strncmp(modelname + strlen(modelname) - 4, ".dat", 4) == 0) {
      s = load_wavefront_dat(modelname, model, m);
   }
   if (s != 0) return 0;

   qv = (float *)(m->data);
   qt = (float *)(m->data + m->numv);
   qn = (float *)(m->data + m->numv + m->numt);
   qf = (unsigned short *)(m->data + m->numv + m->numt + m->numn);

   numverts = m->numf/3;
   vc_assert(numverts <= MAX_VERTICES);

   temp = allocbuffer(3*numverts*sizeof *temp);
   for (i=0; i<m->num_materials; i++) {
      WAVEFRONT_MATERIAL_T *mat = model->material + i;
      mat->numverts = i < m->num_materials-1 ? m->material_index[i+1]-m->material_index[i] : numverts - m->material_index[i];
      // vertex, texture, normal
      deindex(temp, qv, qf+3*offset+0, 3, mat->numverts);
      create_vbo(GL_ARRAY_BUFFER, mat->vbo+VBO_VERTEX, 3 * mat->numverts * sizeof *qv, temp); // 3
   
      deindex(temp, qt, qf+3*offset+1, 2, mat->numverts);
      create_vbo(GL_ARRAY_BUFFER, mat->vbo+VBO_TEXTURE, 2 * mat->numverts * sizeof *qt, temp); // 2
   
      deindex(temp, qn, qf+3*offset+2, 3, mat->numverts);
      create_vbo(GL_ARRAY_BUFFER, mat->vbo+VBO_NORMAL, 3 * mat->numverts * sizeof *qn, temp); // 3
      offset += mat->numverts;
      mat->texture = model->texture;
   }
   model->num_materials = m->num_materials;
   vc_assert(offset == numverts);
   freebuffer(temp);
   freebuffer(m);
   return (MODEL_T)model;
}

void unload_wavefront(MODEL_T m)
{
   WAVEFRONT_MODEL_T *model = (WAVEFRONT_MODEL_T *)m;
   int i;
   for (i=0; i<model->num_materials; i++) {
      WAVEFRONT_MATERIAL_T *mat = model->material + i;
      if (mat->vbo[VBO_VERTEX])
         destroy_vbo(mat->vbo+VBO_VERTEX);
      if (mat->vbo[VBO_TEXTURE])
         destroy_vbo(mat->vbo+VBO_TEXTURE);
      if (mat->vbo[VBO_NORMAL])
         destroy_vbo(mat->vbo+VBO_NORMAL);
   }
}

// create a cube model that looks like a wavefront model, 
MODEL_T cube_wavefront(void)
{
   static const float qv[] = {
    -0.5f, -0.5f,  0.5f,
    -0.5f, -0.5f, -0.5f,
     0.5f, -0.5f, -0.5f,
     0.5f, -0.5f,  0.5f,
    -0.5f,  0.5f,  0.5f,
     0.5f,  0.5f,  0.5f,
     0.5f,  0.5f, -0.5f,
    -0.5f,  0.5f, -0.5f,
   };
   
   static const float qn[] = {
     0.0f, -1.0f, -0.0f,
     0.0f,  1.0f, -0.0f,
     0.0f,  0.0f,  1.0f,
     1.0f,  0.0f, -0.0f,
     0.0f,  0.0f, -1.0f,
    -1.0f,  0.0f, -0.0f,
   };
   
   static const float qt[] = {
    1.0f, 0.0f,
    1.0f, 1.0f,
    0.0f, 1.0f,
    0.0f, 0.0f,
   };
   
   static const unsigned short qf[] = {
    1,1,1, 2,2,1, 3,3,1,
    3,3,1, 4,4,1, 1,1,1,
    5,4,2, 6,1,2, 7,2,2,
    7,2,2, 8,3,2, 5,4,2,
    1,4,3, 4,1,3, 6,2,3,
    6,2,3, 5,3,3, 1,4,3,
    4,4,4, 3,1,4, 7,2,4,
    7,2,4, 6,3,4, 4,4,4,
    3,4,5, 2,1,5, 8,2,5,
    8,2,5, 7,3,5, 3,4,5,
    2,4,6, 1,1,6, 5,2,6,
    5,2,6, 8,3,6, 2,4,6,
   };
   WAVEFRONT_MODEL_T *model = malloc(sizeof *model);
   if (model) {
      WAVEFRONT_MATERIAL_T *mat = model->material;
      float *temp;
      const int offset = 0;
      memset(model, 0, sizeof *model);

      temp = allocbuffer(3*MAX_VERTICES*sizeof *temp);
      mat->numverts = countof(qf)/3;
      // vertex, texture, normal
      deindex(temp, qv, qf+3*offset+0, 3, mat->numverts);
      create_vbo(GL_ARRAY_BUFFER, mat->vbo+VBO_VERTEX, 3 * mat->numverts * sizeof *qv, temp); // 3

      deindex(temp, qt, qf+3*offset+1, 2, mat->numverts);
      create_vbo(GL_ARRAY_BUFFER, mat->vbo+VBO_TEXTURE, 2 * mat->numverts * sizeof *qt, temp); // 2

      deindex(temp, qn, qf+3*offset+2, 3, mat->numverts);
      create_vbo(GL_ARRAY_BUFFER, mat->vbo+VBO_NORMAL, 3 * mat->numverts * sizeof *qn, temp); // 3

      freebuffer(temp);
      model->num_materials = 1;
   }
   return (MODEL_T)model;
}


