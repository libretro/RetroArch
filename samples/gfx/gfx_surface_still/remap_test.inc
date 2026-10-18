static uint32_t st_maps[2][2][64];
static unsigned st_generation[2],st_valid,st_errors,st_gets;
static bool st_packed_maps;
static void *st_published[2];
static size_t st_published_pitch[2];

void *video_driver_texture_lend(uintptr_t id,unsigned slot,size_t row,size_t *pitch)
{
   size_t actual;
   if (!st_refresh) return persistent_lend(id,slot,row,pitch);
   if (!id || slot>=2 || st_errors & (1u<<slot) || st_lend_busy & (1u<<slot)) return NULL;
   if (!(st_valid & (1u<<slot)))
   {
      st_generation[slot]++;
      st_valid|=1u<<slot;
   }
   actual=st_packed_maps && st_generation[slot]==1 ? row : (st_generation[slot]&1) ? 32 : 48;
   if (actual<row || (!pitch && actual!=row)) return NULL;
   if (pitch) *pitch=actual;
   st_published[slot]=st_maps[slot][st_generation[slot]&1];
   st_published_pitch[slot]=actual;
   return st_published[slot];
}
bool video_thread_lend_get(int idx,void **mem,size_t *pitch)
{
   st_gets++;
   CHECK(st_refresh && idx>=0 && idx<2,"snapshot record");
   if (st_lend_busy & (1u<<idx)) return false;
   *mem=st_published[idx];*pitch=st_published_pitch[idx];return true;
}
enum video_texture_update video_driver_texture_update(uintptr_t id,void *data)
{
   enum video_texture_update r=persistent_update(id,data);
   const struct texture_image *image=(const struct texture_image*)data;
   unsigned slot;
   if (!st_refresh || r!=VIDEO_TEXTURE_UPDATE_DONE) return r;
   for (slot=0;slot<2;slot++)
      if ((st_valid & (1u<<slot)) && image->pixels==st_published[slot])
      {
         st_valid&=~(1u<<slot);st_lend_busy|=1u<<slot;
         memset(st_published[slot],0xea,sizeof(st_maps[slot][0]));
      }
   return r;
}
static void publish(unsigned slot)
{
   size_t pitch=4*(st_fp16 ? 8 : 4);
   st_lend_busy&=~(1u<<slot);
   st_published[slot]=video_driver_texture_lend(1,slot,pitch,st_packed_maps ? NULL : &pitch);
   st_published_pitch[slot]=pitch;
}
static void fill_slot(gfx_surface_t *s,unsigned slot,uint32_t value,bool finish)
{
   uint32_t *pixels;
   unsigned x,y,words=4*(st_fp16 ? 2 : 1);
   CHECK(gfx_surface_slot_writable(s,slot),"slot writable");
   pixels=gfx_surface_slot_begin(s,slot);
   CHECK(pixels,"slot acquired");
   if (!pixels) return;
   for (y=0;y<4;y++)
      for (x=0;x<words;x++) pixels[y*s->pitch[slot]/4+x]=value;
   if (finish) gfx_surface_slot_end(s,slot);
}
static void own_value(gfx_surface_t *s,unsigned slot,uint32_t value)
{
   unsigned x;
   for (x=0;x<16*(st_fp16 ? 2 : 1);x++)
      CHECK(s->own_slots[slot][x]==value,"own slot retained valid pixels");
}
static void submit(gfx_surface_t *s,unsigned slot)
{
   CHECK(gfx_surface_submit(s,slot,false)==(st_async ? GFX_SURFACE_SUBMIT_QUEUED : GFX_SURFACE_SUBMIT_DONE),"submit");
   if (st_async)
   {
      unsigned gets=st_gets;
      if (s->lend_refresh && (s->lent & (1u<<slot)))
         CHECK(!gfx_surface_slot_writable(s,slot),"posted slot is not a producer's");
      CHECK(st_gets==gets,"posted slot descriptor was read");
      st_flush();
   }
}
static gfx_surface_t *seed(unsigned slots,bool packed_maps)
{
   gfx_surface_t *s;
   unsigned slot;
   memset(st_generation,0,sizeof(st_generation));memset(st_maps,0,sizeof(st_maps));
   memset(st_published,0,sizeof(st_published));memset(st_published_pitch,0,sizeof(st_published_pitch));
   st_valid=st_errors=st_lend_busy=st_gets=0;st_packed_maps=packed_maps;st_refresh=true;
   s=gfx_surface_new(VIDEO_SCALE_PACK(4,4),slots,st_fp16 ? IMAGE_PIXFMT_FP16 : IMAGE_PIXFMT_8888,TEXTURE_FILTER_NEAREST,NULL,NULL);
   CHECK(s,"surface allocation");
   if (!s) return NULL;
   s->padded=!packed_maps;
   for (slot=0;slot<slots;slot++)
   {
      fill_slot(s,slot,7,true);submit(s,slot);
      if (st_async && !(s->lent & (1u<<slot)))
      {
         fill_slot(s,slot,7,true);submit(s,slot);
      }
   }
   CHECK(s->lend_refresh==(slots==1 ? 3 : (1u<<slots)-1),"refresh flag adopted");
   return s;
}
static void one_slot(bool packed_maps)
{
   gfx_surface_t *s=seed(1,packed_maps);
   void *first;
   if (!s) return;
   first=s->slots[0];
   fill_slot(s,0,11,true);submit(s,0);
   CHECK(gfx_surface_slot_writable(s,0) && s->slots[0]!=first,"spare mapping selected");
   fill_slot(s,0,12,true);submit(s,0);
   CHECK(!gfx_surface_slot_writable(s,0),"both GPU-held maps refused");
   own_value(s,0,7);
   publish(0);
   CHECK(gfx_surface_slot_writable(s,0),"fresh map or fallback acquired");
   if (packed_maps)
      CHECK(!s->lent && s->slots[0]==s->own_slots[0],"packed remap revoked safely");
   else
   {
      CHECK(s->slots[0]==st_published[0] && s->slots[0]!=first && s->pitch[0]==48,"fresh address and pitch");
      fill_slot(s,0,13,true);submit(s,0);
      st_errors=1;publish(0);
      CHECK(gfx_surface_slot_writable(s,0) && !s->lent && s->slots[0]==s->own_slots[0],"failed remap revokes to owned slot");
   }
   own_value(s,0,7);
   gfx_surface_free(s);CHECK(!st_live,"one-slot texture cleanup");
}
static void retirement(void)
{
   gfx_surface_t *s=seed(2,false);
   uintptr_t old;
   unsigned slot;
   if (!s) return;
   old=s->handle;
   fill_slot(s,1,22,false);
   fill_slot(s,0,21,true);submit(s,0);
   publish(0);fill_slot(s,0,23,true);
   s->filter=TEXTURE_FILTER_LINEAR;
   submit(s,0);
   CHECK(s->retired_handle==old && st_last_unloaded!=old,"writer retains old mapped texture");
   gfx_surface_slot_end(s,1);
   CHECK(!s->retired_handle && st_last_unloaded==old,"last writer releases old texture");
   own_value(s,1,22);
   for (slot=0;slot<2;slot++)
      CHECK(!s->producer_mem[slot],"producer pointer cleared");
   gfx_surface_free(s);CHECK(!st_live,"retired texture cleanup");
}
int main(void)
{
   unsigned threaded,fp16;
   baseline_main();
   st_lend=true;
   for (threaded=0;threaded<2;threaded++)
      for (fp16=0;fp16<2;fp16++)
      {
         st_async=(int)threaded;st_fp16=fp16!=0;
         one_slot(false);one_slot(true);retirement();
      }
   puts(failures ? "FAIL renewable surface loans" : "PASS renewable surface loans: direct/threaded, fresh rows, busy/revoked maps and deferred unload");
   return failures ? 1 : 0;
}
