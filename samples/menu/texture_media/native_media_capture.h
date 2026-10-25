#ifndef NATIVE_MEDIA_CAPTURE_H
#define NATIVE_MEDIA_CAPTURE_H
static bool media_native_video_driver_ready(void)
{
   const char *expected = getenv("MEDIA_TEST_VIDEO_DRIVER");
   const char *actual = video_driver_get_ident();
   printf("native video driver: requested=%s actual=%s\n",
         expected ? expected : "(unspecified)", actual ? actual : "(none)");
   return !expected || (actual && !strcmp(expected, actual));
}
/* Native acceptance only: readback never enters a production frame path. */
static bool media_native_capture_region(uint32_t *hash,int ordinal, bool movie_only)
{
   video_driver_state_t *video=video_state_get_ptr();
   struct video_viewport vp;
   uint8_t *pixels;
   unsigned w,h,x,y,ch,bpp;
   bool ok,bottom_up=true,preview_ok=true;
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
   if (getenv("MEDIA_TEST_MODAL_UNIFORM"))
   {
      unsigned total[3] = {0, 0, 0};
      for (y = 0; ok && y < 8; y++)
      {
         for (x = 0; x < 8; x++)
         {
            unsigned sx = w / 2 - w / 16 + (w / 8) * x / 8;
            unsigned sy = h / 2 - h / 16 + (h / 8) * y / 8;
            for (ch = 0; ch < 3; ch++)
               total[ch] += pixels[((size_t)sy * w + sx) * bpp + ch];
         }
      }
      *hash = (total[0] / 64) | ((total[1] / 64) << 8)
            | ((total[2] / 64) << 16);
   }
   else
   for(y=h/4;ok&&y<(movie_only?h/2:h*3/4);y++)for(x=w/4;x<w*3/4;x++)for(ch=0;ch<3;ch++)
      *hash=(*hash^pixels[((size_t)y*w+x)*bpp+ch])*UINT32_C(16777619);
   if (ok && ordinal == 50 && getenv("MEDIA_TEST_SETTINGS_ORANGE"))
   {
      unsigned matching = 0, sampled = 0;
      for (y = h / 8; y < h * 7 / 8; y++)
         for (x = w * 3 / 4; x < w; x++)
         {
            size_t at = ((size_t)y * w + x) * bpp;
            if (pixels[at + 2] > 180 && pixels[at + 1] > 80
                  && pixels[at + 1] < 220 && pixels[at] < 60)
               matching++;
            sampled++;
         }
      printf("settings preview pixels: orange=%u sampled=%u\n", matching, sampled);
      preview_ok = matching > sampled / 20;
   }
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
   free(pixels);return ok && preview_ok;
}

static bool media_startup_failure_check(struct menu_state *menu_st,
      void (*draw_frame)(struct menu_state*))
{
   unsigned i;
   if (!menu_driver_media_startup_token()) return false;
   for (i = 0; i < 4000; i++)
   {
      task_queue_check();
      menu_driver_iterate(menu_st, disp_get_ptr(), anim_get_ptr(),
            config_get_ptr(), MENU_ACTION_NOOP, cpu_features_get_time_usec());
      draw_frame(menu_st);
      if (!menu_driver_media_startup_token()) return true;
      retro_sleep(1);
   }
   return false;
}

static bool media_startup_cancellation_check(struct menu_state *menu_st,
      const char *path, int previous)
{
   int token;
   bool ok;
   if (!menu_driver_media_open(path)) return false;
   token = menu_driver_media_startup_token();
   ok = token != 0 && !(token & 1);
   menu_driver_media_startup_ready(previous | 1);
   menu_driver_media_startup_ready(token);
   ok = menu_driver_media_startup_token() == token && ok;
   menu_driver_iterate(menu_st, disp_get_ptr(), anim_get_ptr(),
         config_get_ptr(), MENU_ACTION_CANCEL, cpu_features_get_time_usec());
   ok = !menu_driver_media_startup_token() && ok;
   menu_driver_media_startup_ready(token | 1);
   ok = !menu_driver_media_startup_token() && ok;
   if (!menu_driver_media_open(path)) return false;
   previous = menu_driver_media_startup_token();
   menu_driver_media_startup_ready(token | 1);
   ok = previous != token && menu_driver_media_startup_token() == previous && ok;
   menu_driver_media_open(NULL);
   return ok && !menu_driver_media_startup_token();
}

static bool media_native_startup_black(unsigned *nonblack)
{
   video_driver_state_t *video = video_state_get_ptr();
   struct video_viewport vp;
   uint8_t *pixels;
   size_t count, i;
   unsigned bpp;
   bool ok, bottom_up = true;
   unsigned dims = video_driver_get_output_dims();
   bool full_window = getenv("MEDIA_TEST_FULL_WINDOW_CAPTURE") != NULL;
   if (full_window && video->current_video->set_viewport)
      video->current_video->set_viewport(video->data, dims, true, false);
   memset(&vp, 0, sizeof(vp));
   video_driver_get_viewport_info(&vp);
   count = (size_t)VIDEO_SCALE_W(vp.dims) * VIDEO_SCALE_H(vp.dims);
   bpp = video->current_video->read_viewport_bgrx ? 4 : 3;
   if (!count || count > (size_t)-1 / bpp) goto failed;
   pixels = (uint8_t*)malloc(count * bpp);
   if (!pixels) goto failed;
   ok = bpp == 4
      ? video->current_video->read_viewport_bgrx(video->data, pixels, false, &bottom_up)
      : video->current_video->read_viewport
         && video->current_video->read_viewport(video->data, pixels, false);
   if (full_window && video->current_video->set_viewport)
      video->current_video->set_viewport(video->data, dims, false, true);
   if (ok)
      for (i = 0; i < count; i++)
         if (pixels[i * bpp] > 2 || pixels[i * bpp + 1] > 2
               || pixels[i * bpp + 2] > 2)
         {
            if (!*nonblack)
               fprintf(stderr, "startup nonblack pixel %lu: %u,%u,%u\n",
                     (unsigned long)i, pixels[i * bpp], pixels[i * bpp + 1],
                     pixels[i * bpp + 2]);
            *nonblack = 1;
            break;
         }
   free(pixels);
   if (ok && *nonblack && getenv("MEDIA_TEST_CAPTURE_DIR"))
   {
      uint32_t hash;
      static unsigned capture;
      media_native_capture_region(&hash, 1000 + capture++, false);
   }
   return ok;
failed:
   if (full_window && video->current_video->set_viewport)
      video->current_video->set_viewport(video->data, dims, false, true);
   return false;
}

static bool media_native_capture(uint32_t *hash, int ordinal)
{
   return media_native_capture_region(hash, ordinal, false);
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

static bool media_native_window_toggle(struct menu_state *menu_st,
      void (*draw_frame)(struct menu_state *),
      bool (*get_target)(struct menu_state *, gfx_media_target_t *))
{
   unsigned cycle;
   for (cycle = 0; cycle < 4; cycle++)
   {
      gfx_media_target_t target;
      gfx_media_playback_state_t state;
      int64_t until;
      uint32_t first = 0, second = 0, playing = 0;
      if (!command_event(CMD_EVENT_FULLSCREEN_TOGGLE, NULL))
         return false;
      /* Exercise rapid reinitializations before a seek can hide a
       * stalled preview by restarting its decoder jobs. */
      {
         unsigned burst;
         for (burst = 0; burst < 6; burst++)
         {
            if (!command_event(CMD_EVENT_FULLSCREEN_TOGGLE, NULL))
               return false;
            draw_frame(menu_st);
         }
      }
      until = cpu_features_get_time_usec() + 150000;
      do
      {
         draw_frame(menu_st);
         retro_sleep(1);
      } while (cpu_features_get_time_usec() < until);
      {
         uint32_t before = 0, after = 0;
         int64_t deadline = cpu_features_get_time_usec() + 3000000;
         bool changed = false;
         if (!media_native_capture_region(&before, -1, true)) return false;
         do
         {
            draw_frame(menu_st);
            retro_sleep(1);
            if (media_native_capture_region(&after, -1, true) && after != before)
            {
               changed = true;
               break;
            }
         } while (cpu_features_get_time_usec() < deadline);
         if (!changed)
         {
            fprintf(stderr, "window toggle %u: playback froze before recovery seek\n", cycle);
            return false;
         }
      }
      if (!get_target(menu_st, &target) || !target.pause
            || !target.pause(target.user, true, cpu_features_get_time_usec())
            || !media_native_seek_capture(menu_st, draw_frame, &target,
               125000, 0, 0, -1, &first)
            || !media_native_seek_capture(menu_st, draw_frame, &target,
               625000, first, 1, -1, &second)
            || !media_native_capture_region(&second, -1, true)
            || !target.pause(target.user, false, cpu_features_get_time_usec()))
      {
         fprintf(stderr, "window toggle %u: transport or seek failed\n", cycle);
         return false;
      }
      until = cpu_features_get_time_usec() + 250000;
      do
      {
         draw_frame(menu_st);
         retro_sleep(1);
      } while (cpu_features_get_time_usec() < until);
      if (!target.state(target.user, cpu_features_get_time_usec(), &state))
         return false;
      if (state.paused || state.seeking
            || !media_native_capture_region(&playing, -1, true) || playing == second)
      {
         fprintf(stderr, "window toggle %u: paused=%u seeking=%u pixels=%08x,%08x\n",
               cycle, state.paused, state.seeking, second, playing);
         return false;
      }
   }
   return true;
}

#endif
