from pathlib import Path
import argparse, os, shutil, subprocess, sys, tempfile
p=argparse.ArgumentParser();p.add_argument('fixture');p.add_argument('--objdir',default='obj-unix/release');p.add_argument('--c89',action='store_true');p.add_argument('--shell',default='bash');p.add_argument('--driver',choices=['rgui','materialui','xmb','ozone'],default='rgui');p.add_argument('--video-driver',default='null');p.add_argument('--threaded',action='store_true');p.add_argument('--assets');p.add_argument('--gpu-surface',action='store_true',help='require RGUI GPU surface playback');p.add_argument('--hardware-decode',action='store_true',help='require hardware decoding as well as GPU presentation');p.add_argument('--continuous',action='store_true',help='test natural folder playback using three temporary fixture links/copies');p.add_argument('--first-video-us',type=int,default=0,help='first visible video timestamp used for capture comparisons');p.add_argument('--continuous-error',action='store_true',help='test that an invalid next movie stops folder playback');p.add_argument('--fullscreen-settings',action='store_true',help='test the shared fullscreen settings action');p.add_argument("--external",action="store_true",help="open the fixture as OS/command-line content");p.add_argument("--browser-image",action="store_true",help="activate an image through the real file browser");p.add_argument("--wallpaper-pick",action="store_true",help="pick the image through the menu Background setting browser");p.add_argument("--window-toggle",action="store_true",help="verify pixels and seeking across application fullscreen transitions");p.add_argument("--dxgi",action="store_true",help="use the Windows DXGI presentation bridge");p.add_argument("--first-frame",action="store_true",help="require black loading frames before the external viewer is ready");p.add_argument("--startup-failure",action="store_true",help="require startup recovery from an undecodable media fixture");p.add_argument('--subtitles',action='store_true',help='verify the generated UTF-8 sidecar through the real menu adapter');p.add_argument('--modal',choices=['move','resize']);p.add_argument('--audio-driver',default='wasapi');p.add_argument('--cadence-hz',type=float,help='require measured movie cadence in Hz');p.add_argument('--require-cadence',action='store_true',help='require fullscreen display source in the folder fixture');p.add_argument('--desktop-refresh',action='store_true',help='exercise fullscreen refresh source and settings lifecycle');p.add_argument('--refresh-pixels',action='store_true',help='require changing movie pixels during fullscreen refresh matching')
p.add_argument('--first-import-refusal',action='store_true',help='require recovery after a real asynchronous native import is refused')
p.add_argument('--cold-cancel',action='store_true',help='close and reopen while native decoder admission is held on the worker')
p.add_argument('--cold-reset',action='store_true',help='reinitialize video while native decoder admission is held on the worker')
p.add_argument('--audio-latency',type=int)
p.add_argument('--audio-sync',choices=['true','false'])
a=p.parse_args()
if a.audio_latency is not None and a.audio_latency<0:p.error("--audio-latency must be nonnegative")
if a.cadence_hz is not None and not 5<=a.cadence_hz<=240:p.error('--cadence-hz needs a rate from 5 through 240')
if a.refresh_pixels:a.desktop_refresh=True
if a.refresh_pixels and a.video_driver=='null':p.error('--refresh-pixels needs a native video driver')
if a.desktop_refresh:a.require_cadence=True;a.fullscreen_settings=True
if a.continuous_error or a.fullscreen_settings or a.require_cadence:a.continuous=True
if a.modal and a.continuous:p.error("--modal and --continuous are separate acceptance modes")
if a.window_toggle and (a.video_driver=='null' or a.external or a.browser_image or a.continuous):p.error('--window-toggle needs standalone GPU playback')
if a.dxgi and (os.name!='nt' or a.video_driver!='vulkan'):p.error('--dxgi needs Windows Vulkan')
if a.subtitles and a.continuous:p.error("--subtitles and --continuous are separate acceptance modes")
if (a.external or a.browser_image) and a.continuous:p.error("external/image launch checks cannot use continuous mode")
if a.modal and os.name!='nt':p.error('--modal needs native Windows')
if a.first_frame and (not a.external or a.video_driver=="null"):p.error("--first-frame requires external GPU presentation")
if a.startup_failure and not a.external:p.error("--startup-failure requires an external media fixture")
if a.external and a.browser_image:p.error("choose external or browser-image mode")
if a.wallpaper_pick and (a.external or a.browser_image or a.continuous or a.subtitles or a.modal):p.error("--wallpaper-pick is its own acceptance mode")
if a.wallpaper_pick and a.driver not in ('xmb','materialui'):p.error("--wallpaper-pick needs a menu that offers the Background setting (xmb, materialui)")
if a.gpu_surface and (a.driver!='rgui' or a.video_driver=='null'):p.error('--gpu-surface requires RGUI and a GPU video driver')
if a.hardware_decode and not a.gpu_surface:p.error('--hardware-decode requires --gpu-surface')
if a.first_import_refusal and (not a.gpu_surface or not a.threaded or a.hardware_decode):p.error('--first-import-refusal requires a threaded RGUI GPU surface and software recovery')
if a.cold_cancel and (not a.gpu_surface or not a.threaded or a.external or a.browser_image):p.error('--cold-cancel requires standalone threaded RGUI GPU playback')
if a.cold_reset and (not a.gpu_surface or not a.threaded or a.external or a.browser_image or a.cold_cancel):p.error('--cold-reset requires standalone threaded RGUI GPU playback, separate from --cold-cancel')
if not 0<=a.first_video_us<=2147483647:p.error('--first-video-us requires a timestamp from 0 through 2147483647')
here=Path(__file__).resolve().parent;r=here.parents[2]
test_dir=r/'samples/menu'/((a.driver+'_media') if a.driver!='rgui' else 'rgui_media')
exe=test_dir/((a.driver+'_media_test')+('.exe' if os.name=='nt' else ''))
cmd=[sys.executable,str(here/'build.py'),'--objdir',a.objdir,'--shell',a.shell,'--driver',a.driver,'--test-source',str(test_dir/(a.driver+'_media_test.c')),'--output',str(exe)]
if a.c89:cmd.append('--c89')
build_env=os.environ.copy()
if a.first_import_refusal:build_env['MEDIA_TEST_IMPORT_REFUSAL']='1'
if a.cold_cancel:build_env['MEDIA_TEST_COLD_CANCEL']='1'
if a.cold_reset:build_env['MEDIA_TEST_COLD_RESET']='1'
subprocess.run(cmd,cwd=r,env=build_env,check=True)
with tempfile.TemporaryDirectory(prefix='rgui-media-') as temp:
 fixture=Path(a.fixture).resolve()
 if a.continuous:
  if fixture.suffix.lower() not in ('.mp4','.webm','.mkv'):p.error('--continuous needs an MP4, WebM or Matroska fixture')
  for episode in ((1,2,10,11) if a.continuous_error else (1,2,10)):
   target=Path(temp)/('episode'+str(episode)+fixture.suffix)
   try:os.link(fixture,target)
   except OSError:shutil.copyfile(fixture,target)
  if a.continuous_error:
   broken=Path(temp)/('episode10'+fixture.suffix)
   broken.unlink()
   broken.write_bytes(b'invalid movie')
  fixture=Path(temp)/('episode2'+fixture.suffix)
 cfg=Path(temp)/'retroarch.cfg'
 values={'video_driver':'null','audio_driver':'null','input_driver':'null','input_joypad_driver':'null','menu_driver':('glui' if a.driver=='materialui' else a.driver),'audio_enable':'false','menu_ignore_missing_assets':'true','video_threaded':'false','menu_show_start_screen':'false','config_save_on_exit':'false','menu_mouse_enable':'true','history_list_enable':'false','savefile_directory':temp,'savestate_directory':temp,'screenshot_directory':temp}
 values.update(video_driver=a.video_driver,video_threaded='true' if a.threaded else 'false',video_vsync='false',pause_nonactive='false',video_hw_decode='1')
 if a.continuous:values['media_player_continuous']='true'
 if a.require_cadence:values.update(media_player_crt_switch='true',crt_switch_resolution='1' if os.environ.get('MEDIA_TEST_CRT_ENGINE') else '0',crt_switch_resolution_super='0')
 if a.desktop_refresh:values.update(media_player_crt_switch='false',media_player_refresh_rate='true',crt_switch_resolution='0')
 if a.refresh_pixels:values['video_monitor_index']='1'
 if a.modal:values.update(audio_enable='true',audio_driver=a.audio_driver,menu_thumbnail_preview_audio='true')
 if a.audio_latency is not None:values['audio_latency']=str(a.audio_latency)
 if a.audio_sync is not None:values['audio_sync']=a.audio_sync
 if a.modal and a.audio_driver=='wasapi':values.update(audio_wasapi_exclusive_mode='false',audio_format_negotiation='1',audio_output_layout='0',audio_out_rate='48000',log_verbosity='true')
 if a.dxgi:values.update(video_dxgi_present='true',log_verbosity='true')
 # Loading suppression must also cover controller/OSD widgets.
 if a.first_frame:values.update(menu_enable_widgets='true',video_window_custom_size_enable='true',video_window_width='1280',video_window_height='720')
 if a.assets:values['assets_directory']=str(Path(a.assets).resolve())
 cfg.write_text(''.join(k+' = "'+v.replace('\\','/')+'"\n' for k,v in values.items()),encoding='utf-8')
 cmd=[str(exe),str(fixture),str(cfg)]
 native_cmd=cmd
 if os.name=='nt' and os.environ.get('GDB')=='1':cmd=['gdb','--batch','-return-child-result','-ex','run','-ex','thread apply all bt','--args']+cmd
 env=os.environ.copy();env['MEDIA_TEST_VIDEO_DRIVER']=a.video_driver
 if a.cadence_hz is not None:env['MEDIA_TEST_CADENCE_HZ']=str(a.cadence_hz)
 if a.modal:env['MEDIA_TEST_MODAL_MODE']=a.modal
 if a.subtitles:env['MEDIA_TEST_SUBTITLES']='1'
 if a.window_toggle:env['MEDIA_TEST_WINDOW_TOGGLE']='1'
 if a.external:env["MEDIA_TEST_EXTERNAL"]="1"
 if a.first_frame:env["MEDIA_TEST_FIRST_FRAME"]="1"
 if a.first_frame and a.dxgi:env["MEDIA_TEST_FULL_WINDOW_CAPTURE"]="1"
 if a.startup_failure:env["MEDIA_TEST_STARTUP_FAILURE"]="1"
 if a.browser_image:env["MEDIA_TEST_BROWSER_IMAGE"]="1"
 if a.wallpaper_pick:env["MEDIA_TEST_WALLPAPER_PICK"]="1"
 if a.continuous:env['MEDIA_TEST_CONTINUOUS']='1'
 if a.require_cadence:env['MEDIA_TEST_CADENCE']='1'
 if a.desktop_refresh:env['MEDIA_TEST_DESKTOP_REFRESH']='1'
 if a.refresh_pixels:env['MEDIA_TEST_REFRESH_PIXELS']='1'
 if a.continuous_error:env['MEDIA_TEST_CONTINUOUS_ERROR']='1'
 if a.fullscreen_settings:env['MEDIA_TEST_SETTINGS']='1'
 if a.video_driver!='null':env['MEDIA_TEST_GPU']='1'
 if a.gpu_surface:env['MEDIA_TEST_GPU_SURFACE']='1'
 if a.hardware_decode:env['MEDIA_TEST_HW_DECODE']='1'
 env['MEDIA_TEST_FIRST_VIDEO_US']=str(a.first_video_us)
 if a.dxgi:
  result=subprocess.run(cmd,cwd=r,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,encoding='utf-8',errors='replace')
  print(result.stdout,flush=True)
 else:result=subprocess.run(cmd,cwd=r,env=env)
 if result.returncode and os.name=='nt' and os.environ.get('GDB')!='1':
  try:
   debug_env=env.copy()
   if env.get('MEDIA_TEST_CAPTURE_DIR'):
    capture=Path(env['MEDIA_TEST_CAPTURE_DIR'])/'debugger'
    capture.mkdir(parents=True,exist_ok=True)
    debug_env['MEDIA_TEST_CAPTURE_DIR']=str(capture)
   subprocess.run(['gdb','--batch','-ex','run','-ex','thread apply all bt','--args']+native_cmd,cwd=r,env=debug_env)
  except OSError as error:print('Debugger replay unavailable: '+str(error),file=sys.stderr)
 result.check_returncode()
 if a.dxgi and ('DXGI presentation active.' not in result.stdout or 'DXGI presentation unavailable' in result.stdout):raise RuntimeError('DXGI presentation was not retained')
