from pathlib import Path
import argparse, os, subprocess, sys, tempfile
p=argparse.ArgumentParser();p.add_argument('fixture');p.add_argument('--objdir',default='obj-unix/release');p.add_argument('--c89',action='store_true');p.add_argument('--shell',default='bash');p.add_argument('--driver',choices=['rgui','materialui','xmb','ozone'],default='rgui');p.add_argument('--video-driver',default='null');p.add_argument('--threaded',action='store_true');p.add_argument('--assets');p.add_argument('--gpu-surface',action='store_true',help='require RGUI GPU surface playback');p.add_argument('--hardware-decode',action='store_true',help='require hardware decoding as well as GPU presentation');p.add_argument('--first-video-us',type=int,default=0,help='first visible video timestamp used for capture comparisons');p.add_argument("--external",action="store_true",help="open the fixture as OS/command-line content");p.add_argument("--browser-image",action="store_true",help="activate an image through the real file browser");p.add_argument("--window-toggle",action="store_true",help="verify pixels and seeking across application fullscreen transitions");p.add_argument("--dxgi",action="store_true",help="use the Windows DXGI presentation bridge");a=p.parse_args()
if a.window_toggle and (a.video_driver=="null" or a.external or a.browser_image):p.error("--window-toggle needs standalone GPU playback")
if a.dxgi and (os.name!="nt" or a.video_driver!="vulkan"):p.error("--dxgi needs Windows Vulkan")
if a.external and a.browser_image:p.error("choose external or browser-image mode")
if a.gpu_surface and (a.driver!='rgui' or a.video_driver=='null'):p.error('--gpu-surface requires RGUI and a GPU video driver')
if a.hardware_decode and not a.gpu_surface:p.error('--hardware-decode requires --gpu-surface')
if not 0<=a.first_video_us<=2147483647 or (a.first_video_us and a.driver!='rgui'):p.error('--first-video-us requires RGUI and a timestamp from 0 through 2147483647')
here=Path(__file__).resolve().parent;r=here.parents[2]
test_dir=r/'samples/menu'/((a.driver+'_media') if a.driver!='rgui' else 'rgui_media')
exe=test_dir/((a.driver+'_media_test')+('.exe' if os.name=='nt' else ''))
cmd=[sys.executable,str(here/'build.py'),'--objdir',a.objdir,'--shell',a.shell,'--driver',a.driver,'--test-source',str(test_dir/(a.driver+'_media_test.c')),'--output',str(exe)]
if a.c89:cmd.append('--c89')
subprocess.run(cmd,cwd=r,check=True)
with tempfile.TemporaryDirectory(prefix='rgui-media-') as temp:
 cfg=Path(temp)/'retroarch.cfg'
 values={'video_driver':'null','audio_driver':'null','input_driver':'null','input_joypad_driver':'null','menu_driver':('glui' if a.driver=='materialui' else a.driver),'audio_enable':'false','menu_ignore_missing_assets':'true','video_threaded':'false','menu_show_start_screen':'false','config_save_on_exit':'false','menu_mouse_enable':'true','history_list_enable':'false','savefile_directory':temp,'savestate_directory':temp,'screenshot_directory':temp}
 values.update(video_driver=a.video_driver,video_threaded='true' if a.threaded else 'false',video_vsync='false',pause_nonactive='false',video_hw_decode='1')
 if a.dxgi:values.update(video_dxgi_present='true',log_verbosity='true')
 if a.assets:values['assets_directory']=str(Path(a.assets).resolve())
 cfg.write_text(''.join(k+' = "'+v.replace('\\','/')+'"\n' for k,v in values.items()),encoding='utf-8')
 cmd=[str(exe),str(Path(a.fixture).resolve()),str(cfg)]
 if os.name=='nt':cmd=['gdb','--batch','-return-child-result','-ex','run','-ex','thread apply all bt','--args']+cmd
 env=os.environ.copy()
 if a.window_toggle:env['MEDIA_TEST_WINDOW_TOGGLE']='1'
 if a.external:env["MEDIA_TEST_EXTERNAL"]="1"
 if a.browser_image:env["MEDIA_TEST_BROWSER_IMAGE"]="1"
 if a.video_driver!='null':env['MEDIA_TEST_GPU']='1'
 if a.gpu_surface:env['MEDIA_TEST_GPU_SURFACE']='1'
 if a.hardware_decode:env['MEDIA_TEST_HW_DECODE']='1'
 env['MEDIA_TEST_FIRST_VIDEO_US']=str(a.first_video_us)
 if a.dxgi:
  result=subprocess.run(cmd,cwd=r,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,encoding='utf-8',errors='replace')
  print(result.stdout,flush=True)
  result.check_returncode()
  if 'DXGI presentation active.' not in result.stdout or 'DXGI presentation unavailable' in result.stdout:raise RuntimeError('DXGI presentation was not retained')
 else:subprocess.run(cmd,cwd=r,env=env,check=True)
