"""Relink a built non-Qt frontend with a real menu translation unit."""
from pathlib import Path
import argparse, os, shlex, shutil, subprocess, sys
p=argparse.ArgumentParser();p.add_argument('--objdir',default='obj-unix/release');p.add_argument('--c89',action='store_true');p.add_argument('--shell',default='bash');p.add_argument('--driver',default='rgui');p.add_argument('--test-source');p.add_argument('--output');a=p.parse_args()
if (os.environ.get('MEDIA_TEST_IMPORT_REFUSAL') or os.environ.get('MEDIA_TEST_COLD_CANCEL') or os.environ.get('MEDIA_TEST_COLD_RESET')) and a.driver!='rgui':p.error('native import refusal uses the RGUI harness')
r=Path(__file__).resolve().parents[3];here=Path(__file__).resolve().parent
output=Path(a.output).resolve() if a.output else here/('rgui_media_test.exe' if os.name=='nt' else 'rgui_media_test')
objects=output.parent;objects.mkdir(parents=True,exist_ok=True)
driver_source='menu/drivers/'+a.driver+'.c'
test_source=Path(a.test_source).resolve() if a.test_source else here/'rgui_media_test.c'
make=['make','-n','OBJDIR='+a.objdir,'C89_BUILD='+('1' if a.c89 else ''),'-W','retroarch.c','-W',driver_source]
text=subprocess.check_output(make,cwd=r,text=True,encoding='utf-8')
commands=[shlex.split(line) for line in text.splitlines() if line and not line.startswith('make[')]
def compile_line(source):
 found=[c for c in commands if '-c' in c and source in c and '-o' in c]
 if len(found)!=1:raise RuntimeError('Expected one compile command for '+source)
 return found[0]
def run(c):
 if any(x in c for x in ['&&',';','|']):raise RuntimeError('Unexpected shell command')
 print('+',c[0],'- compile' if '-c' in c else '- link',len(c),'arguments',flush=True)
 if os.name=='nt':
  # CreateProcess searches System32 before PATH and may select WSL's bash.
  shell=shutil.which(a.shell)
  if not shell:raise FileNotFoundError('Build shell not found: '+a.shell)
  result=subprocess.run([shell,'--noprofile','--norc','-c',shlex.join(c)],cwd=r,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,creationflags=subprocess.CREATE_NO_WINDOW)
  sys.stdout.buffer.write(result.stdout);sys.stdout.buffer.flush()
  result.check_returncode()
 else:subprocess.run(c,cwd=r,check=True)
cc=compile_line('retroarch.c');retro_object=cc[cc.index('-o')+1]
cc[cc.index('-o')+1]=str(objects/'retroarch_nomain.o');cc.extend(['-Umain','-Dmain=rarch_harness_unused_main']);run(cc)
cc=compile_line(driver_source);rgui_object=cc[cc.index('-o')+1]
cc[cc.index('-o')+1]=str(objects/'harness.o');cc[cc.index(driver_source)]=str(test_source)
if os.environ.get('MEDIA_TEST_IMPORT_REFUSAL'):cc.append('-DMEDIA_TEST_IMPORT_REFUSAL')
if os.environ.get('MEDIA_TEST_COLD_CANCEL') or os.environ.get('MEDIA_TEST_COLD_RESET'):cc.append('-DMEDIA_TEST_COLD_CANCEL')
if os.environ.get('MEDIA_TEST_COLD_RESET'):cc.append('-DMEDIA_TEST_COLD_RESET')
run(cc)
links=[c for c in commands if '-c' not in c and '-o' in c and c[c.index('-o')+1] in {'retroarch','retroarch.exe'}]
if len(links)!=1:raise RuntimeError('Expected one frontend link command')
c=links[0];c[c.index('-o')+1]=str(output)
if os.name=='nt':c.append('-mconsole')
c[c.index(retro_object)]=str(objects/'retroarch_nomain.o');c[c.index(rgui_object)]=str(objects/'harness.o')
if os.environ.get('MEDIA_TEST_IMPORT_REFUSAL'):c.append('-Wl,--wrap=gfx_surface_submit_import')
if os.environ.get('MEDIA_TEST_COLD_CANCEL') or os.environ.get('MEDIA_TEST_COLD_RESET'):c.append('-Wl,--wrap=image_transfer_anim_stream_set_hw')
if os.environ.get('MEDIA_TEST_COLD_RESET'):c.extend(['-Wl,--wrap=gfx_thumbnail_anim_worker_deinit','-Wl,--wrap=video_driver_free_internal'])
response=objects/'link.args'
response.write_text('\n'.join('"'+arg.replace('\\','\\\\').replace('"','\\"')+'"' for arg in c[1:])+'\n',encoding='utf-8')
run([c[0],'@'+str(response)])
print(output)
