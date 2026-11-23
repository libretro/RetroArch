/* Direct planes must match the two-draw RGBA8 path, including clamp-before-
 * filtering, odd chroma edges, cropping, channel order and SDR in HDR storage. */
static void run_planar_direct(bool core, unsigned layout, bool linear, GLint format)
{
   static const float vertex[8]={0,0,1,0,0,1,1,1};
   static const float uv[8]={-0.1f,1.1f,1.1f,1.1f,-0.1f,-0.1f,1.1f,-0.1f};
   static const float transform[16]={2,0,0,0,0,2,0,0,0,0,1,0,-1,-1,0,1};
   static const float white[16]={1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1};
   gl_planar_t planar;
   struct texture_planar planes;
   struct texture_image image;
   uint8_t y[24], u[8], v[8], pair[16];
#ifdef HAVE_OPENGLES
   uint8_t actual[48], expected[48];
#else
   float actual[48], expected[48];
#endif
   float tint[16];
   GLuint handle,reference,target,fbo;
   GLint previous_fbo,prog,binding,active,buffer;
#ifndef HAVE_OPENGLES
   GLint width;
#endif
   unsigned frame,i,before;
   memset(&planar,0,sizeof(planar));memset(&planes,0,sizeof(planes));
   memset(&image,0,sizeof(image));
   for(i=0;i<8;i++){u[i]=(uint8_t)(i*31);v[i]=(uint8_t)(255-i*27);}
   for(i=0;i<8;i++){pair[i*2]=layout==2?v[i]:u[i];pair[i*2+1]=layout==2?u[i]:v[i];}
   planes.planes[0]=y;planes.planes[1]=layout?pair+(layout==2):u;
   planes.planes[2]=layout?pair+(layout==1):v;
   planes.strides[0]=8;planes.strides[1]=planes.strides[2]=layout?8:4;
   planes.chroma_step=layout?2:1;planes.yuv=0;
   image.width=5;image.height=3;image.planar=&planes;
   for(i=0;i<24;i++)y[i]=(uint8_t)(i*11);
   before=draws;
   handle=gl_planar_load_deferred(&planar,&image,linear,core);assert(handle);
   assert(draws==before); /* no conversion pass on load */
   glBindTexture(GL_TEXTURE_2D,handle);
#ifndef HAVE_OPENGLES
   glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_WIDTH,&width);assert(!width);
#endif
   glGetIntegerv(GL_FRAMEBUFFER_BINDING,&previous_fbo);
   target=texture(format,NULL);glGenFramebuffers(1,&fbo);
   glBindFramebuffer(GL_FRAMEBUFFER,fbo);
   glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,target,0);
   assert(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);
   glViewport(0,0,4,3);glDisable(GL_BLEND);
   image.width=4;assert(!gl_planar_update_deferred(&planar,handle,&image));image.width=5;
   for(frame=0;frame<4;frame++)
   {
      for(i=0;i<24;i++)y[i]=(uint8_t)(frame*17+i*11);
      planes.yuv=(frame&1?IMAGE_YUV_FLAG_FULL_RANGE:0)|(frame&2?IMAGE_YUV_FLAG_BT709:0);
      memcpy(tint,white,sizeof(tint));
      if(frame==1)for(i=0;i<4;i++){tint[i*4]=0.75f;tint[i*4+1]=0.5f;tint[i*4+3]=0.5f;}
      before=draws;assert(gl_planar_update_deferred(&planar,handle,&image));assert(draws==before);
      reference=gl_planar_load(&planar,&image,linear,core);assert(reference);
      glUseProgram(program);glUniform1i(glGetUniformLocation(program,"image"),0);
      glUniform1i(glGetUniformLocation(program,"pick"),0);
      glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,reference);
      glBindBuffer(GL_ARRAY_BUFFER,0);
      glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,0,vertex);glEnableVertexAttribArray(0);
      glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,0,uv);glEnableVertexAttribArray(1);
      glDrawArrays(GL_TRIANGLE_STRIP,0,4);
#ifdef HAVE_OPENGLES
      glReadPixels(0,0,4,3,GL_RGBA,GL_UNSIGNED_BYTE,expected);
#else
      glReadPixels(0,0,4,3,GL_RGBA,GL_FLOAT,expected);
#endif
      /* The borrowed CPU frame may be released as soon as upload returns. */
      memset(y,0x11,sizeof(y));
      glGetIntegerv(GL_CURRENT_PROGRAM,&prog);glGetIntegerv(GL_ACTIVE_TEXTURE,&active);
      glGetIntegerv(GL_TEXTURE_BINDING_2D,&binding);glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&buffer);
      before=draws;
      tint[5]+=0.25f;
      assert(!gl_planar_render(&planar,handle,vertex,uv,tint,transform));assert(draws==before);
      tint[5]-=0.25f;
      assert(gl_planar_render(&planar,handle,vertex,uv,tint,transform));assert(draws==before+1);
      {GLint value;glGetIntegerv(GL_CURRENT_PROGRAM,&value);assert(value==prog);
       glGetIntegerv(GL_ACTIVE_TEXTURE,&value);assert(value==active);
       glGetIntegerv(GL_TEXTURE_BINDING_2D,&value);assert(value==binding);
       glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&value);assert(value==buffer);}
#ifdef HAVE_OPENGLES
      glReadPixels(0,0,4,3,GL_RGBA,GL_UNSIGNED_BYTE,actual);
      for(i=0;i<48;i++)assert(abs((int)actual[i]-(int)(expected[i]*tint[i&3]+0.5f))<=2);
#else
      glReadPixels(0,0,4,3,GL_RGBA,GL_FLOAT,actual);
      for(i=0;i<48;i++)assert(fabsf(actual[i]-expected[i]*tint[i&3])<0.005f);
#endif
      glBindTexture(GL_TEXTURE_2D,handle);
#ifndef HAVE_OPENGLES
   glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_WIDTH,&width);assert(!width);
#endif
      gl_planar_forget(&planar,reference);glDeleteTextures(1,&reference);
   }
   before=draws;assert(gl_planar_resolve(&planar,handle)==handle);assert(draws==before+1);
   assert(gl_planar_resolve(&planar,handle)==handle && draws==before+1);
   glBindTexture(GL_TEXTURE_2D,handle);
#ifndef HAVE_OPENGLES
   glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_WIDTH,&width);assert(width==5);
#endif
   before=draws;
   assert(gl_planar_update_deferred(&planar,handle,&image));assert(draws==before);
   assert(gl_planar_pending(&planar,handle,NULL,NULL));
   assert(gl_planar_render(&planar,handle,vertex,uv,tint,transform));assert(draws==before+1);
   assert(gl_planar_resolve(&planar,handle)==handle);assert(draws==before+2);
   assert(!gl_planar_pending(&planar,handle,NULL,NULL));
   assert(gl_planar_update(&planar,handle,&image));
   gl_planar_forget(&planar,handle);glDeleteTextures(1,&handle);gl_planar_free(&planar);
   glBindFramebuffer(GL_FRAMEBUFFER,(GLuint)previous_fbo);
   glDeleteFramebuffers(1,&fbo);glDeleteTextures(1,&target);
   assert(glGetError()==GL_NO_ERROR);
}
