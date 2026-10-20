#!/usr/bin/env python3
from pathlib import Path
import argparse,os,shlex,subprocess,tempfile
here=Path(__file__).resolve().parent
root=here.parents[2]
p=argparse.ArgumentParser()
p.add_argument("--source",type=Path,default=root/"gfx/drivers/d3d11.c")
p.add_argument("--kind",choices=("all","sdr","hdr"),default="all")
a=p.parse_args()
def function(text,name):
 start=text.index(name);brace=text.index('{',start);depth=1;end=brace+1
 while depth:
  depth+=(text[end]=='{')-(text[end]=='}');end+=1
 return text[start:end]
with tempfile.TemporaryDirectory(prefix='d3d11-readback-') as tmp:
 tmp=Path(tmp)
 body=function(a.source.read_text(),'static bool d3d11_gfx_read_viewport(void*')
 clamp=function((root/'gfx/common/dxgi_common.h').read_text(),'static INLINE void dxgi_readback_clamp_window(')
 (tmp/'readback.inc').write_text(clamp+'\n'+body)
 hdr=function(a.source.read_text(),'static bool d3d11_gfx_read_viewport_hdr(')
 prefix=hdr[hdr.index('   if (!d3d11)'):hdr.index('   BackBuffer->lpVtbl->GetDesc')]
 (tmp/'hdr_prefix.inc').write_text(prefix)
 for kind,filename in [('sdr','test.c'),('hdr','hdr_test.c')]:
  if a.kind not in ('all',kind):continue
  exe=tmp/(kind+('.exe' if os.name=='nt' else ''))
  cmd=shlex.split(os.environ.get('CC','cc'))+['-std=c89','-pedantic','-Wall','-Wextra','-Werror','-Wframe-larger-than=4096','-I'+str(root/'libretro-common/include'),'-I'+str(tmp),str(here/filename),'-o',str(exe)]
  subprocess.run(cmd,stdin=subprocess.DEVNULL,check=True)
  runner=shlex.split(os.environ.get('RUNNER',''))
  if os.name=='nt' and not runner:runner=['gdb','--batch','-return-child-result','-ex','run','-ex','thread apply all bt','--args']
  subprocess.run(runner+[str(exe)],stdin=subprocess.DEVNULL,check=True)
