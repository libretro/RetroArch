typedef struct
{
   void **surfaces;
   uint32_t **anim_2frame_pix;
   struct texture_image **images;
} input_overlay_t;
SHIPPING_FUNCTION
int main(void)
{
   input_overlay_t ol;
   struct texture_image image, *images[1];
   void *surfaces[1];
   uint32_t frames[16],saved[16],own[8],*canvases[1];
   gfx_surface_t *s;
   unsigned i;
   int current=1,before;
   if (baseline_main()) return 1;
   memset(&image,0,sizeof(image));
   for (i=0;i<16;i++) frames[i]=0x10203040+i;
   memcpy(saved,frames,sizeof(saved));
   canvases[0]=frames; images[0]=&image;
   ol.surfaces=surfaces; ol.anim_2frame_pix=canvases; ol.images=images;
   s=gfx_surface_new(VIDEO_SCALE_PACK(4,2),1,IMAGE_PIXFMT_8888,TEXTURE_FILTER_NEAREST,NULL,NULL);
   CHECK(s,"two-frame surface"); if (!s) return 1;
   surfaces[0]=s;
   for (i=0;i<8;i++) s->slots[0][i]=own[i]=0xdeadbeef;
   CHECK(gfx_surface_submit(s,0,false)==GFX_SURFACE_SUBMIT_DONE,"seed");
   for (i=0;i<2;i++)
   {
      CHECK(input_overlay_update_apng_frame(&ol,0,(int)i),"direct frame");
      CHECK(st_last_pixels==frames+i*8 && st_px_first==frames[i*8] && st_px_last==frames[i*8+7],"canvas not uploaded directly");
      CHECK(!memcmp(s->slots[0],own,sizeof(own)),"redundant own-slot copy");
   }
   image.supports_rgba=true;
   CHECK(input_overlay_update_apng_frame(&ol,0,1) && st_seen_rgba,"RGBA order lost");
   image.supports_rgba=false;
   CHECK(input_overlay_update_apng_frame(&ol,0,1) && !st_seen_rgba,"BGRA order lost");
   canvases[0]=NULL;
   CHECK(!input_overlay_update_apng_frame(&ol,0,0),"missing canvas accepted");
   canvases[0]=frames;
   st_drop=1;
   if (input_overlay_update_apng_frame(&ol,0,0)) current=0;
   CHECK(current==1,"dropped state accepted");
   if (input_overlay_update_apng_frame(&ol,0,0)) current=0;
   CHECK(current==0 && st_last_pixels==frames,"drop not retried");
   st_refuse=1;
   CHECK(input_overlay_update_apng_frame(&ol,0,1) && st_last_pixels==frames+8,"replacement lost canvas");
   s->inflight=1; before=st_updates;
   CHECK(!input_overlay_update_apng_frame(&ol,0,0) && st_updates==before,"inflight changed");
   s->inflight=0;
   CHECK(!memcmp(frames,saved,sizeof(saved)),"canvas modified");
   gfx_surface_free(s);
   s=gfx_surface_new(VIDEO_SCALE_PACK(4,2),1,IMAGE_PIXFMT_8888,TEXTURE_FILTER_NEAREST,NULL,NULL);
   CHECK(s,"loan surface"); if (!s) return 1;
   surfaces[0]=s;
   st_lend=true; s->padded=1;
   st_lend_pitch[0]=32; st_lend_pitch[1]=48;
   CHECK(gfx_surface_submit(s,0,false)==GFX_SURFACE_SUBMIT_DONE && s->lent,"loan seed");
   memset(st_lent_pixels,0xa5,sizeof(st_lent_pixels));
   CHECK(input_overlay_update_apng_frame(&ol,0,1),"loan submit");
   CHECK(st_lent_pixels[0][0]==frames[8] && st_lent_pixels[0][3]==frames[11]
         && st_lent_pixels[0][8]==frames[12] && st_lent_pixels[0][11]==frames[15]
         && st_lent_pixels[0][4]==0xa5a5a5a5,"padded rows");
   st_lend_busy=1;
   CHECK(input_overlay_update_apng_frame(&ol,0,0),"spare loan submit");
   CHECK(st_lent_pixels[1][0]==frames[0] && st_lent_pixels[1][12]==frames[4]
         && st_lent_pixels[1][4]==0xa5a5a5a5,"spare loan pitch");
   st_lend_busy=3; before=st_updates;
   CHECK(!input_overlay_update_apng_frame(&ol,0,0) && st_updates==before,"held loan overwritten");
   st_lend_busy=0; gfx_surface_free(s);
   st_lend=false; st_lend_pitch[0]=st_lend_pitch[1]=0;
   s=gfx_surface_new(VIDEO_SCALE_PACK(4,2),1,IMAGE_PIXFMT_8888,TEXTURE_FILTER_NEAREST,NULL,NULL);
   CHECK(s,"queued surface"); if (!s) return 1;
   surfaces[0]=s;
   CHECK(gfx_surface_submit(s,0,false)==GFX_SURFACE_SUBMIT_DONE,"queued seed");
   st_async=1;
   CHECK(input_overlay_update_apng_frame(&ol,0,1) && s->inflight,"not queued");
   CHECK(!input_overlay_update_apng_frame(&ol,0,0),"queued slot overwritten");
   frames[8]=0x55667788;
   st_flush();
   CHECK(st_px_first==saved[8] && st_px_last==saved[15] && st_last_pixels==s->slots[0],"queued canvas not owned");
   st_async=0; gfx_surface_free(s);
   CHECK(st_live==0,"texture leak");
   printf("%s: two-frame canvas, padded loans, retry and queue ownership\n",failures ? "FAILED" : "PASS");
   return failures ? 1 : 0;
}
