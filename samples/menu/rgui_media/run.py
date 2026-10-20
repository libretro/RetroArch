from pathlib import Path
import argparse, os, subprocess, sys, tempfile
p=argparse.ArgumentParser();p.add_argument('fixture');p.add_argument('--objdir',default='obj-unix/release');p.add_argument('--c89',action='store_true');p.add_argument('--shell',default='bash');p.add_argument('--driver',choices=['rgui','materialui','xmb','ozone'],default='rgui');a=p.parse_args()
here=Path(__file__).resolve().parent;r=here.parents[2]
test_dir=r/'samples/menu'/((a.driver+'_media') if a.driver!='rgui' else 'rgui_media')
exe=test_dir/((a.driver+'_media_test')+('.exe' if os.name=='nt' else ''))
cmd=[sys.executable,str(here/'build.py'),'--objdir',a.objdir,'--shell',a.shell,'--driver',a.driver,'--test-source',str(test_dir/(a.driver+'_media_test.c')),'--output',str(exe)]
if a.c89:cmd.append('--c89')
subprocess.run(cmd,cwd=r,check=True)
with tempfile.TemporaryDirectory(prefix='rgui-media-') as temp:
 cfg=Path(temp)/'retroarch.cfg'
 values={'video_driver':'null','audio_driver':'null','input_driver':'null','input_joypad_driver':'null','menu_driver':('glui' if a.driver=='materialui' else a.driver),'audio_enable':'false','menu_ignore_missing_assets':'true','video_threaded':'false','menu_show_start_screen':'false','config_save_on_exit':'false','menu_mouse_enable':'true','history_list_enable':'false','savefile_directory':temp,'savestate_directory':temp,'screenshot_directory':temp}
 cfg.write_text(''.join(k+' = "'+v.replace('\\','/')+'"\n' for k,v in values.items()),encoding='utf-8')
 cmd=[str(exe),str(Path(a.fixture).resolve()),str(cfg)]
 if os.name=='nt':cmd=['gdb','--batch','-return-child-result','-ex','run','-ex','thread apply all bt','--args']+cmd
 subprocess.run(cmd,cwd=r,check=True)
