/* Real menus/frontend/codecs: null mode uses a memory texture sink;
 * explicit native mode presents and captures the fullscreen movie. */
#if defined(TEST_MATERIALUI)
#include "../../../menu/drivers/materialui.c"
typedef materialui_handle_t test_viewer_t;
#define TEST_IDENT "glui"
static bool test_showing(test_viewer_t *v) { return (v->flags&MUI_FLAG_SHOW_FULLSCREEN_THUMBNAILS)!=0; }
static bool test_fade_ready(test_viewer_t *v) { return v->fullscreen_thumbnail_alpha>=0.9999f; }
static gfx_thumbnail_t *test_thumbnail(test_viewer_t *v) { return &v->browser_thumbnail; }
static void test_prepare(test_viewer_t *v) { v->flags&=~(MUI_FLAG_IS_PLAYLIST|MUI_FLAG_IS_EXPLORE_LIST);v->flags|=MUI_FLAG_IS_FILE_LIST;v->list_view_type=MUI_LIST_VIEW_DEFAULT; }
static enum menu_action test_action(test_viewer_t *v,enum menu_action a) { return materialui_parse_menu_entry_action(v,a); }
static int test_release(test_viewer_t *v,unsigned x,unsigned y) { return materialui_pointer_up(v,x,y,0,MENU_INPUT_GESTURE_TAP,NULL,NULL,0); }
#elif defined(TEST_XMB)
#include "../../../menu/drivers/xmb.c"
typedef xmb_handle_t test_viewer_t;
#define TEST_IDENT "xmb"
static bool test_showing(test_viewer_t *v) { return v->show_fullscreen_thumbnails; }
static bool test_fade_ready(test_viewer_t *v) { return v->fullscreen_thumbnail_alpha>=0.9999f; }
static gfx_thumbnail_t *test_thumbnail(test_viewer_t *v) { return v->thumbnails.left.anim_sess?&v->thumbnails.left:&v->thumbnails.right; }
static void test_prepare(test_viewer_t *v) { v->is_playlist=v->is_explore_list=false;v->is_file_list=true;v->depth=2; }
static enum menu_action test_action(test_viewer_t *v,enum menu_action a) { return xmb_parse_menu_entry_action(v,a); }
static int test_release(test_viewer_t *v,unsigned x,unsigned y) { return xmb_pointer_up(v,x,y,0,MENU_INPUT_GESTURE_TAP,NULL,NULL,0); }
#else
#include "../../../menu/drivers/ozone.c"
typedef ozone_handle_t test_viewer_t;
#define TEST_IDENT "ozone"
static bool test_showing(test_viewer_t *v) { return (v->flags2&OZONE_FLAG2_SHOW_FULLSCREEN_THUMBNAILS)!=0; }
static bool test_fade_ready(test_viewer_t *v) { return v->animations.fullscreen_thumbnail_alpha>=0.9999f; }
static gfx_thumbnail_t *test_thumbnail(test_viewer_t *v) { return v->thumbnails.left.anim_sess?&v->thumbnails.left:&v->thumbnails.right; }
static void test_prepare(test_viewer_t *v) { v->flags&=~(OZONE_FLAG_IS_PLAYLIST|OZONE_FLAG_IS_EXPLORE_LIST|OZONE_FLAG_CURSOR_IN_SIDEBAR);v->flags|=OZONE_FLAG_IS_FILE_LIST;v->depth=2; }
static enum menu_action test_action(test_viewer_t *v,enum menu_action a) { return ozone_parse_menu_entry_action(v,true,true,a); }
static int test_release(test_viewer_t *v,unsigned x,unsigned y) { return ozone_pointer_up(v,x,y,0,MENU_INPUT_GESTURE_TAP,NULL,NULL,0); }
#endif
#include "../../../retroarch.h"
#include "../../../frontend/frontend.h"
#include <time/rtime.h>
#include <retro_timers.h>
#include "../../../gfx/gfx_surface_player.h"
#include "../../../gfx/gfx_anim_preview.h"

static unsigned test_hdr_transfer(void)
{
   const char *transfer=getenv("MEDIA_TEST_HDR_TRANSFER");
   return transfer?(!strcmp(transfer,"18")?18:0):16;
}

static bool test_hdr_import_matches(test_viewer_t *viewer, unsigned cll, unsigned fall)
{
   gfx_surface_t *surface=(gfx_surface_t*)test_thumbnail(viewer)->anim_surface;
   return surface&&surface->imported
      &&(surface->import.pixfmt==IMAGE_PIXFMT_P010||surface->import.pixfmt==IMAGE_PIXFMT_YUV10_FP16)
      &&surface->import.color.max_cll==cll&&surface->import.color.max_fall==fall
      &&surface->import.color.transfer==test_hdr_transfer()&&surface->import.color.primaries==9
      &&surface->import.color.matrix==9;
}

static unsigned failures, uploads;
static uint32_t uploaded_pixels;
static bool native_gpu;
static uint32_t native_background[320 * 240];
#define CHECK(message, cond) do { bool passed=(cond)!=0; \
   printf("%s: %s\n",passed?"ok":"FAIL",message); if(!passed)failures++; } while(0)

static uintptr_t memory_texture(void *video, void *data, bool threaded,
      enum texture_filter_type filter)
{
   const struct texture_image *image = (const struct texture_image*)data;
   size_t i;
   (void)video; (void)threaded; (void)filter;
   if (!image || !image->pixels || !image->width || !image->height) return 0;
   for (i=0;i<(size_t)image->width*image->height;i++) uploaded_pixels|=image->pixels[i];
   return ++uploads;
}
static void memory_unload(void *data, bool threaded, uintptr_t id)
{ (void)data; (void)threaded; (void)id; }

static void render(struct menu_state *menu_st)
{
   unsigned dims=native_gpu?video_driver_get_ui_dims():VIDEO_SCALE_PACK(640,480);
   if(native_gpu)menu_st->current_time_us=cpu_features_get_time_usec();
   else menu_st->current_time_us+=1000;
   if(native_gpu)gfx_animation_update(menu_st->current_time_us,false,1.0f,dims);
   menu_st->driver_ctx->render(menu_st->userdata,dims,false);
   if(native_gpu)
   {
      video_driver_state_t *video=video_state_get_ptr();
      if(menu_st->driver_ctx->set_texture)
         menu_st->driver_ctx->set_texture(menu_st->userdata);
      if(video->poke&&video->poke->set_texture_enable)
         video->poke->set_texture_enable(video->data,true,false);
      video_driver_frame(native_background,320,240,320*sizeof(uint32_t));
   }
}

#include "native_media_capture.h"

int main(int argc,char **argv)
{
   struct menu_state *menu_st;
   test_viewer_t *viewer;
   video_driver_state_t *video;
   const video_poke_interface_t *old_poke;
   static video_poke_interface_t poke;
   file_list_t *list,*stack;
   char *rarch_argv[4];
   static char directory[PATH_MAX_LENGTH];
   gfx_media_playback_state_t state;
   int tick;
   unsigned x,y,selection;
   bool seeking=true;
   if(argc!=3)return 2;
   native_gpu=getenv("MEDIA_TEST_GPU")!=NULL;
   config_file_set_io_default(config_file_io_filestream());rtime_init();retroarch_config_init();
   retroarch_ctl(RARCH_CTL_STATE_FREE,NULL);frontend_driver_init_first(NULL);
   rarch_argv[0]=(char*)"retroarch";rarch_argv[1]=(char*)"--menu";
   rarch_argv[2]=(char*)"--config";rarch_argv[3]=argv[2];
   if(!retroarch_main_init(4,rarch_argv))return 1;
   retroarch_menu_running();menu_st=menu_state_get_ptr();
   CHECK("actual texture menu initialized",menu_st->driver_ctx&&!strcmp(menu_st->driver_ctx->ident,TEST_IDENT));
   if(failures){main_exit(NULL);return 1;}
   viewer=(test_viewer_t*)menu_st->userdata;
   video=video_state_get_ptr();old_poke=video->poke;
   if(!native_gpu)
   { poke.load_texture=memory_texture;poke.unload_texture=memory_unload;video->poke=&poke; }
   config_get_ptr()->bools.menu_thumbnail_preview_audio=false;
   list=MENU_LIST_GET_SELECTION(menu_st->entries.list,0);stack=MENU_LIST_GET(menu_st->entries.list,0);
   file_list_clear(list);file_list_clear(stack);fill_pathname_basedir(directory,argv[1],sizeof(directory));
   CHECK("file-browser directory installed",file_list_append(stack,directory,"",FILE_TYPE_PLAIN,0,0));
   CHECK("movie entry installed",file_list_append(list,path_basename(argv[1]),"",FILE_TYPE_MOVIE,0,0));
   menu_st->driver_ctx->list_insert(viewer,list,path_basename(argv[1]),directory,"",0,FILE_TYPE_MOVIE);
   menu_st->selection_ptr=0;menu_st->current_time_us=native_gpu?cpu_features_get_time_usec():1000000;
   test_prepare(viewer);
   config_get_ptr()->uints.gfx_thumbnails=0;
   config_get_ptr()->uints.menu_left_thumbnails=0;
#if defined(TEST_MATERIALUI)
   materialui_browser_update(viewer,menu_st);
   CHECK("browser thumbnail resolves in default list view",*viewer->browser_path);
#endif
   CHECK("closed browser does not upload hidden video",uploads==0);
   CHECK("movie activation is consumed before core launch",
         test_action(viewer,MENU_ACTION_OK)==MENU_ACTION_NOOP
         &&test_showing(viewer));
   for(tick=0;tick<2000;tick++)
   {
      render(menu_st);
      if((native_gpu?test_thumbnail(viewer)->texture!=0:uploads!=0)
            &&viewer->media.controls.target.user)break;
      retro_sleep(1);
   }
   if(native_gpu)
   {
      gfx_surface_player_t *player=(gfx_surface_player_t*)test_thumbnail(viewer)->anim_player;
      CHECK("real movie reaches the GPU texture",test_thumbnail(viewer)->texture!=0);
      CHECK("preview uses the decoding worker",player&&player->threaded);
      if(getenv("MEDIA_TEST_HDR_AV1"))
      {
         gfx_surface_t *surface=(gfx_surface_t*)test_thumbnail(viewer)->anim_surface;
         CHECK("HDR AV1 preserves 10-bit samples at the importer",surface&&surface->imported
               &&(surface->import.pixfmt==IMAGE_PIXFMT_P010
                  ||surface->import.pixfmt==IMAGE_PIXFMT_YUV10_FP16));
         CHECK("coded HDR metadata reaches the importer",surface
               &&surface->import.color.max_cll==1000&&surface->import.color.max_fall==400
               &&surface->import.color.transfer==test_hdr_transfer());
      }
      CHECK("preview uses the windowed file feeder",test_thumbnail(viewer)->anim_sess
            &&gfx_anim_preview_windowed((gfx_anim_preview_t*)test_thumbnail(viewer)->anim_sess));
   }
   else CHECK("real movie pixels reach texture boundary",uploads&&uploaded_pixels);
   CHECK("browser media controls are reachable",viewer->media.controls.target.user==test_thumbnail(viewer));
   CHECK("confirm pauses through menu adapter",
         test_action(viewer,MENU_ACTION_OK)==MENU_ACTION_NOOP
         &&gfx_thumbnail_get_playback_state(test_thumbnail(viewer),menu_st->current_time_us,&state)&&state.paused);
   if(native_gpu)
   {
      gfx_media_target_t target=viewer->media.controls.target;
      uint32_t first=0,middle=0,again=0;
      unsigned burst;
      for(tick=0;tick<2000&&(test_thumbnail(viewer)->alpha<0.9999f||!test_fade_ready(viewer));tick++)
      { render(menu_st);retro_sleep(1); }
      CHECK("thumbnail fades complete before pixel comparison",test_thumbnail(viewer)->alpha>=0.9999f&&test_fade_ready(viewer));
      CHECK("paused start picture reaches renderer",media_native_seek_capture(menu_st,render,&target,0,0,0,0,&first));
      if(getenv("MEDIA_TEST_HDR_TIMELINE"))CHECK("start picture has initial HDR metadata",test_hdr_import_matches(viewer,1000,400));
      CHECK("seek changes the rendered movie picture",media_native_seek_capture(menu_st,render,&target,375000,first,1,1,&middle));
      if(getenv("MEDIA_TEST_HDR_TIMELINE"))CHECK("forward seek imports changed HDR metadata",test_hdr_import_matches(viewer,2000,600));
      CHECK("backward seek restores the rendered picture",media_native_seek_capture(menu_st,render,&target,0,first,-1,2,&again));
      if(getenv("MEDIA_TEST_HDR_TIMELINE"))CHECK("backward seek restores initial HDR metadata",test_hdr_import_matches(viewer,1000,400));
      for(burst=0;burst<16;burst++)
      {
         CHECK("burst request accepted",target.seek(target.user,burst&1?375000:0,cpu_features_get_time_usec()));
         render(menu_st);retro_sleep(1);
      }
      CHECK("latest burst target reaches renderer while paused",media_native_seek_capture(menu_st,render,&target,375000,first,1,3,&middle));
      if(getenv("MEDIA_TEST_HDR_TIMELINE"))CHECK("latest burst imports changed HDR metadata",test_hdr_import_matches(viewer,2000,600));
      printf("rendered media hashes: %08x %08x %08x\n",first,middle,again);
   }
   render(menu_st);
   selection=(unsigned)menu_st->selection_ptr;
   x=(unsigned)(viewer->media.controls.layout.timeline.x+viewer->media.controls.layout.timeline.w/2);
   y=(unsigned)(viewer->media.controls.layout.timeline.y+1);
   menu_st->input_state.pointer.type=MENU_POINTER_MOUSE;
   menu_st->input_state.pointer.pos=VIDEO_POS_PACK(0,0);menu_st->input_state.pointer.flags=0;render(menu_st);
   menu_st->input_state.pointer.pos=VIDEO_POS_PACK(x,y);menu_st->input_state.pointer.flags=MENU_INP_PTR_FLG_PRESSED;
   render(menu_st);CHECK("timeline press captured",viewer->media.controls.dragging);
   test_release(viewer,x,y);
   CHECK("release keeps fullscreen and browser selection",
         test_showing(viewer)&&menu_st->selection_ptr==selection);
   menu_st->input_state.pointer.flags=0;
   for(tick=0;tick<2000&&seeking;tick++)
   {
      render(menu_st);gfx_thumbnail_get_playback_state(test_thumbnail(viewer),menu_st->current_time_us,&state);
      seeking=state.seeking;retro_sleep(1);
   }
   printf("texture seek: pending=%d paused=%d position_us=%ld ticks=%d\n",
         (int)seeking,(int)state.paused,(long)state.position_us,tick);
   CHECK("seek lands while user pause is preserved",!seeking&&state.paused&&state.position_us>0);
   CHECK("Stop is consumed by the transport",test_action(viewer,MENU_ACTION_START)==MENU_ACTION_NOOP);
   seeking=true;
   for(tick=0;tick<2000&&seeking;tick++)
   {
      render(menu_st);
      gfx_thumbnail_get_playback_state(test_thumbnail(viewer),menu_st->current_time_us,&state);
      seeking=state.seeking;retro_sleep(1);
   }
   CHECK("Stop lands at the start and stays paused",!seeking&&state.paused&&state.position_us==0);

   test_action(viewer,MENU_ACTION_CANCEL);render(menu_st);
   CHECK("Back closes the browser movie viewer",!test_showing(viewer));
#if defined(TEST_MATERIALUI)
   CHECK("closed browser movie is suspended",viewer->browser_suspended);
   test_action(viewer,MENU_ACTION_OK);render(menu_st);
   CHECK("reopening retains user pause",gfx_thumbnail_get_playback_state(test_thumbnail(viewer),
         menu_st->current_time_us,&state)&&state.paused);
#else
   CHECK("disabled thumbnails do not keep a hidden movie running",
         !viewer->thumbnails.left.anim_player&&!viewer->thumbnails.right.anim_player);
   gfx_thumbnail_request_file(menu_st->thumbnail_path_data->content_path,&viewer->thumbnails.right,0);
   gfx_thumbnail_set_paused(&viewer->thumbnails.right,true,menu_st->current_time_us);
   test_action(viewer,MENU_ACTION_OK);render(menu_st);
   CHECK("activation reuses an existing matching paused preview",
         gfx_thumbnail_get_playback_state(test_thumbnail(viewer),menu_st->current_time_us,&state)&&state.paused);
#endif
#if defined(TEST_MATERIALUI)
   CHECK("ordinary file installed",file_list_append(list,"notes.txt","",FILE_TYPE_PLAIN,0,0));
   menu_st->driver_ctx->list_insert(viewer,list,"notes.txt",directory,"",1,FILE_TYPE_PLAIN);
   menu_st->selection_ptr=1;render(menu_st);
   CHECK("ordinary file retires browser movie and controls",!*viewer->browser_path&&!viewer->media.controls.target.user);
   viewer->first_onscreen_entry=viewer->last_onscreen_entry=1;
   CHECK("ordinary file retains existing activation action",test_action(viewer,MENU_ACTION_OK)==MENU_ACTION_OK&&!test_showing(viewer));
   gfx_thumbnail_reset(test_thumbnail(viewer));
#else
   gfx_thumbnail_reset(&viewer->thumbnails.left);gfx_thumbnail_reset(&viewer->thumbnails.right);
#endif
   gfx_thumbnail_anim_worker_deinit();video->poke=old_poke;
   main_exit(NULL);printf("texture menu media: %u failure(s)\n",failures);return failures?1:0;
}
