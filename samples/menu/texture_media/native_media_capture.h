#ifndef NATIVE_MEDIA_CAPTURE_H
#define NATIVE_MEDIA_CAPTURE_H
/* Native acceptance only: readback never enters a production frame path. */
static bool media_native_capture(uint32_t *hash,int ordinal)
{
   video_driver_state_t *video=video_state_get_ptr();
   struct video_viewport vp;
   uint8_t *pixels;
   unsigned w,h,x,y,ch,bpp;
   bool ok,bottom_up=true;
   const char *directory=getenv("MEDIA_TEST_CAPTURE_DIR");
   memset(&vp,0,sizeof(vp));video_driver_get_viewport_info(&vp);
   w=VIDEO_SCALE_W(vp.dims);h=VIDEO_SCALE_H(vp.dims);
   if(!w||!h)return false;
   bpp=video->current_video->read_viewport_bgrx?4:3;
   pixels=(uint8_t*)malloc((size_t)w*h*bpp);
   if(!pixels)return false;
   ok=bpp==4?video->current_video->read_viewport_bgrx(video->data,pixels,false,&bottom_up)
      :video->current_video->read_viewport
         &&video->current_video->read_viewport(video->data,pixels,false);
   *hash=UINT32_C(2166136261);
   for(y=h/4;ok&&y<h*3/4;y++)for(x=w/4;x<w*3/4;x++)for(ch=0;ch<3;ch++)
      *hash=(*hash^pixels[((size_t)y*w+x)*bpp+ch])*UINT32_C(16777619);
   if(ok&&directory&&ordinal>=0)
   {
      char path[512];
      FILE *file;
      snprintf(path,sizeof(path),"%s/frame-%d.ppm",directory,ordinal);
      file=fopen(path,"wb");
      if(!file)ok=false;
      else
      {
         fprintf(file,"P6\n%u %u\n255\n",w,h);
         for(y=0;y<h;y++)for(x=0;x<w;x++)
         {
            size_t at=((size_t)(bottom_up?h-1-y:y)*w+x)*bpp;
            uint8_t rgb[3];rgb[0]=pixels[at+2];rgb[1]=pixels[at+1];rgb[2]=pixels[at];
            if(fwrite(rgb,1,3,file)!=3)ok=false;
         }
         if(fclose(file))ok=false;
      }
   }
   free(pixels);return ok;
}

static bool media_native_seek_capture(struct menu_state *menu_st,
      void (*draw_frame)(struct menu_state*),const gfx_media_target_t *target,
      int64_t position,uint32_t expected,int comparison,int ordinal,uint32_t *hash)
{
   int64_t deadline=cpu_features_get_time_usec()+10000000,settled=0;
   gfx_media_playback_state_t state;
   if(!target->seek||!target->seek(target->user,position,cpu_features_get_time_usec()))return false;
   while(cpu_features_get_time_usec()<deadline)
   {
      int64_t now;
      draw_frame(menu_st);now=cpu_features_get_time_usec();
      if(target->state(target->user,now,&state)&&!state.seeking&&state.paused)
      {
         if(!settled)settled=now;
         if(now-settled>=50000&&media_native_capture(hash,ordinal)
               &&(!comparison||(comparison>0?*hash!=expected:*hash==expected)))
            return state.position_us>=position-50000&&state.position_us<=position+50000;
      }
      retro_sleep(1);
   }
   return false;
}

#endif
