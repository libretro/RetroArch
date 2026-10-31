/* Run the same guest continuation checks with a real frontend's GPU context. */
#define main snapshot_headless_main
#include "snapshot_smoke.c"
#undef main

static retro_environment_t frontend_environment;
static retro_video_refresh_t frontend_video;
static void *proxy_library, *proxy_anchor, *proxy_empty;
static bool proxy_present = true, proxy_rollback, proxy_complete;
static bool proxy_reset_checked;
static unsigned proxy_frames, proxy_hardware_frames;
static const char *proxy_output;

static bool RETRO_CALLCONV proxy_environment(unsigned command, void *data)
{
   if (command == RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY)
      return frontend_environment(command,data);
   if (command == RETRO_ENVIRONMENT_SET_PIXEL_FORMAT)
   {
      pixel_format=*(unsigned*)data;
      return frontend_environment(command,data);
   }
   if (command == RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO ||
       command == RETRO_ENVIRONMENT_SET_GEOMETRY || command == RETRO_ENVIRONMENT_SET_MEMORY_MAPS)
      return frontend_environment(command,data);
   if (environment(command,data)) return true;
   return frontend_environment(command,data);
}

static void RETRO_CALLCONV proxy_video(const void *pixels,unsigned width,
      unsigned height,size_t pitch)
{
   if (phase == RETRO_SNAPSHOT_SPECULATIVE) return;
   if (pixels == RETRO_HW_FRAME_BUFFER_VALID)
   {
      REQUIRE(width && height); proxy_hardware_frames++;
      video.width=width; video.height=height; video_calls++;
   }
   else video_refresh(pixels,width,height,pitch);
   if (proxy_present) frontend_video(pixels,width,height,pitch);
}

static void proxy_open(void)
{
   const char *path;
   if (proxy_library) return;
   path=getenv("SNAPSHOT_TEST_CORE"); REQUIRE(path);
   proxy_output=getenv("SNAPSHOT_TEST_OUTPUT"); REQUIRE(proxy_output);
   REQUIRE(strlen(proxy_output)<sizeof(directory)); strcpy(directory,proxy_output);
   path=getenv("SNAPSHOT_TEST_PROFILE"); REQUIRE(path); load_profile(path);
   conformance=true;
   /* Keep rollback active through the runner's final screenshot at frame 80. */
   retained_steps=64;
   path=getenv("SNAPSHOT_TEST_METHOD");
   REQUIRE(path && (!strcmp(path,"reference") || !strcmp(path,"snapshot")));
   proxy_rollback=!strcmp(path,"snapshot");
   proxy_library=snapshot_core_open(&core,getenv("SNAPSHOT_TEST_CORE")); REQUIRE(proxy_library);
}

RETRO_API unsigned retro_api_version(void) { proxy_open(); return core.retro_api_version(); }
RETRO_API void retro_set_environment(retro_environment_t callback)
{ proxy_open(); frontend_environment=callback; core.retro_set_environment(proxy_environment); }
RETRO_API void retro_set_video_refresh(retro_video_refresh_t callback)
{ frontend_video=callback; core.retro_set_video_refresh(proxy_video); }
RETRO_API void retro_set_audio_sample(retro_audio_sample_t callback)
{ core.retro_set_audio_sample(callback); }
RETRO_API void retro_set_audio_sample_batch(retro_audio_sample_batch_t callback)
{ core.retro_set_audio_sample_batch(callback); }
RETRO_API void retro_set_input_poll(retro_input_poll_t callback)
{ core.retro_set_input_poll(callback); }
RETRO_API void retro_set_input_state(retro_input_state_t callback)
{ (void)callback; core.retro_set_input_state(input_state); }
RETRO_API void retro_init(void) { core.retro_init(); REQUIRE(snapshots.version); }
RETRO_API void retro_get_system_info(struct retro_system_info *info)
{ proxy_open(); core.retro_get_system_info(info); }
RETRO_API void retro_get_system_av_info(struct retro_system_av_info *info)
{ core.retro_get_system_av_info(info); }
RETRO_API bool retro_load_game(const struct retro_game_info *info)
{ return core.retro_load_game(info); }
RETRO_API bool retro_load_game_special(unsigned type,const struct retro_game_info *info,size_t count)
{ (void)type;(void)info;(void)count;return false; }
RETRO_API void retro_set_controller_port_device(unsigned port,unsigned device)
{ core.retro_set_controller_port_device(port,device); }
RETRO_API unsigned retro_get_region(void) { return core.retro_get_region(); }
RETRO_API void retro_reset(void) { core.retro_reset(); }
RETRO_API size_t retro_serialize_size(void) { return core.retro_serialize_size(); }
RETRO_API bool retro_serialize(void *data,size_t size) { return core.retro_serialize(data,size); }
RETRO_API bool retro_unserialize(const void *data,size_t size) { return core.retro_unserialize(data,size); }
RETRO_API void *retro_get_memory_data(unsigned id) { return core.retro_get_memory_data(id); }
RETRO_API size_t retro_get_memory_size(unsigned id) { return core.retro_get_memory_size(id); }
RETRO_API void retro_cheat_reset(void) { core.retro_cheat_reset(); }
RETRO_API void retro_cheat_set(unsigned index,bool enabled,const char *code)
{ core.retro_cheat_set(index,enabled,code); }

static void proxy_finish(void)
{
   FILE *file;
   static char path[4096];
   unsigned i;
   size_t bytes=core.retro_get_memory_size(RETRO_MEMORY_SYSTEM_RAM);
   for (i=0;i<option_count;i++) REQUIRE(option_hits[i]);
   file_path(path,sizeof(path),"final-ram.bin"); file=fopen(path,"wb"); REQUIRE(file);
   REQUIRE(fwrite(core.retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM),1,bytes,file)==bytes);
   REQUIRE(!fclose(file));
   file_path(path,sizeof(path),"checks.txt"); file=fopen(path,"w"); REQUIRE(file);
   fprintf(file,"frames=%u\ncreates=%u\nsaves=%u\nloads=%u\nspeculative=%u\nhardware_frames=%u\nram_bytes=%lu\n",
         proxy_frames,creates,saves,loads,speculative_runs,proxy_hardware_frames,(unsigned long)bytes);
   REQUIRE(!fclose(file)); proxy_complete=true;
}

RETRO_API void retro_run(void)
{
   unsigned frame=proxy_frames++;
   phase=proxy_rollback?RETRO_SNAPSHOT_CANONICAL:RETRO_SNAPSHOT_ORDINARY;
   if (frame<warmup_frames || frame>=warmup_frames+retained_steps)
   {
      input_frame=frame; core.retro_run();
      if (frame==warmup_frames-1) native_oracle();
   }
   else if (!proxy_rollback)
   {
      input_frame=frame; core.retro_run(); native_oracle();
   }
   else
   {
      unsigned i,counter,depth=frame%11==0?12:1+frame%3;
      size_t bytes=core.retro_get_memory_size(RETRO_MEMORY_SYSTEM_RAM);
      uint8_t *before,*expected;
      if (!proxy_anchor)
      {
         REQUIRE(snapshots.supported(2)); proxy_anchor=new_snapshot(); proxy_empty=new_snapshot();
         REQUIRE(!snapshots.valid(proxy_empty));
         REQUIRE(snapshots.load(proxy_empty)==RETRO_SNAPSHOT_UNAVAILABLE);
      }
      before=malloc(bytes); expected=malloc(bytes); REQUIRE(before && expected);
      memcpy(before,core.retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM),bytes);
      capture(proxy_anchor); REQUIRE(!memcmp(before,core.retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM),bytes));
      proxy_present=false; advance(frame); counter=native_oracle();
      memcpy(expected,core.retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM),bytes);
      restore_ram(proxy_anchor,before,bytes); restore_ram(proxy_anchor,before,bytes);
      phase=RETRO_SNAPSHOT_SPECULATIVE;
      for(i=0;i<depth;i++) {input_frame=frame+i+1;core.retro_run();speculative_runs++;}
      phase=RETRO_SNAPSHOT_CANONICAL; restore_ram(proxy_anchor,before,bytes);
      proxy_present=true; advance(frame); REQUIRE(native_oracle()==counter);
      REQUIRE(!memcmp(expected,core.retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM),bytes));
      free(expected); free(before);
   }
   if (proxy_frames==64 && !proxy_complete) proxy_finish();
   if (check_ordinary_load && proxy_frames==80 && proxy_anchor)
   {
      REQUIRE(snapshots.valid(proxy_anchor));
      core.retro_reset();
      REQUIRE(!snapshots.valid(proxy_anchor));
      REQUIRE(core.retro_serialize_size() && core.retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM));
      proxy_reset_checked=true;
   }
}

RETRO_API void retro_unload_game(void)
{
   REQUIRE(proxy_complete);
   if (check_ordinary_load && proxy_anchor)
      REQUIRE(proxy_reset_checked);
   if (proxy_empty) {release(proxy_empty);proxy_empty=NULL;}
   if (proxy_anchor) {release(proxy_anchor);proxy_anchor=NULL;}
   REQUIRE(creates==releases);
   phase=RETRO_SNAPSHOT_ORDINARY; core.retro_unload_game();
}
RETRO_API void retro_deinit(void)
{
   unsigned i;
   core.retro_deinit(); REQUIRE(snapshot_core_close(proxy_library)); proxy_library=NULL;
   free(video.pixels);
   for(i=0;i<option_count;i++) {free((void*)options[i].key);free((void*)options[i].value);}
}
