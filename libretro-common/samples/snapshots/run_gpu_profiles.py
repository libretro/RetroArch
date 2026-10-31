"""Compare actual frontend images and guest consumers with/without snapshots."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import zlib


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def png_rgb(path):
    data=path.read_bytes()
    assert data[:8]==b'\x89PNG\r\n\x1a\n'
    cursor=8
    compressed=bytearray()
    while cursor<len(data):
        size,=struct.unpack_from('>I',data,cursor)
        kind=data[cursor+4:cursor+8]
        block=data[cursor+8:cursor+8+size]
        assert len(block)==size
        assert zlib.crc32(kind+block)&0xffffffff==struct.unpack_from('>I',data,cursor+8+size)[0]
        if kind==b'IHDR':width,height,depth,color,compression,filtering,interlace=struct.unpack('>IIBBBBB',block)
        if kind==b'IDAT':compressed.extend(block)
        cursor+=size+12
    assert depth==8 and color in (2,6) and compression==filtering==interlace==0
    assert 0<width<=16384 and 0<height<=16384 and width*height<=64000000
    bpp=3 if color==2 else 4
    stride=width*bpp
    raw=zlib.decompress(compressed)
    assert len(raw)==height*(stride+1)
    previous=bytearray(stride)
    rgb=bytearray()
    for y in range(height):
        start=y*(stride+1)
        mode=raw[start]
        assert mode<=4
        row=bytearray(raw[start+1:start+1+stride])
        for x in range(stride):
            left=row[x-bpp] if x>=bpp else 0
            above=previous[x]
            diagonal=previous[x-bpp] if x>=bpp else 0
            if mode==1:predict=left
            elif mode==2:predict=above
            elif mode==3:predict=(left+above)//2
            elif mode==4:
                p=left+above-diagonal
                a,b,c=abs(p-left),abs(p-above),abs(p-diagonal)
                predict=left if a<=b and a<=c else above if b<=c else diagonal
            else:predict=0
            row[x]=(row[x]+predict)&255
        if bpp==3:rgb.extend(row)
        else:
            for x in range(0,stride,4):rgb.extend(row[x:x+3])
        previous=row
    return (width,height),bytes(rgb)


def config(path,values):
    path.write_text(''.join(key+' = "'+str(value)+'"\n' for key,value in values.items()),encoding='utf-8')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for key in ['frontend','proxy','core','content','suite','output']:
        parser.add_argument('--'+key,required=True,type=Path)
    parser.add_argument('--system',type=Path)
    args=parser.parse_args()
    for key in ['frontend','proxy','core','content','suite']:
        setattr(args,key,getattr(args,key).resolve(strict=True))
    output=args.output.resolve()
    output.mkdir(parents=True,exist_ok=False)
    suite=json.loads(args.suite.read_text(encoding='utf-8'))
    base={}
    for line in (args.suite.parent/suite['profile']).read_text(encoding='utf-8').splitlines():
        if line and not line.startswith('#'):
            key,value=line.split('=',1)
            assert key not in base
            base[key]=value
    records=[]
    provenance={key:digest(getattr(args,key)) for key in ['frontend','proxy','core','content','suite']}
    for variant in suite['variants']:
        name=variant['name']
        assert name and name not in ('.','..') and all(c in 'abcdefghijklmnopqrstuvwxyz0123456789-_' for c in name)
        pictures=[]
        ram=[]
        for method in ['reference','snapshot']:
            folder=output/(name+'-'+method)
            folder.mkdir()
            paths={key:folder/key for key in ['saves','states','config','cache','info','cores','shots','system']}
            for path in paths.values():path.mkdir()
            settings=dict(base)
            settings.update(variant.get('options',{}))
            profile=folder/'profile.cfg'
            profile.write_text(''.join(k+'='+v+'\n' for k,v in settings.items()),encoding='utf-8')
            values=dict(system_directory=str(args.system.resolve() if args.system else paths['system']),
                savefile_directory=str(paths['saves']),savestate_directory=str(paths['states']),
                config_directory=str(paths['config']),rgui_config_directory=str(paths['config']),
                core_options_path=str(folder/'options.cfg'),cache_directory=str(paths['cache']),
                libretro_directory=str(paths['cores']),libretro_info_path=str(paths['info']),
                screenshot_directory=str(paths['shots']),video_driver=variant['driver'],video_threaded='false',
                video_vsync='false',video_font_enable='false',video_window_custom_size_enable='true',
                video_window_width='640',video_window_height='480',audio_enable='false',audio_driver='null',
                input_driver='null',input_joypad_driver='null',pause_nonactive='false',
                run_ahead_enabled='false',preemptive_frames_enable='false',rewind_enable='false',
                savestate_auto_load='false',savestate_auto_save='false',config_save_on_exit='false',
                auto_overrides_enable='false',auto_shaders_enable='false',auto_remaps_enable='false',
                history_list_enable='false',network_cmd_enable='false')
            config(folder/'retroarch.cfg',values)
            config(folder/'options.cfg',{k:v for k,v in settings.items() if not k.startswith('@')})
            env=dict(os.environ,SNAPSHOT_TEST_CORE=str(args.core),SNAPSHOT_TEST_OUTPUT=str(folder),
                SNAPSHOT_TEST_PROFILE=str(profile),SNAPSHOT_TEST_METHOD=method)
            command=[str(args.frontend),'-c',str(folder/'retroarch.cfg'),'-L',str(args.proxy),
                '--verbose','--log-file',str(folder/'frontend.log'),'--max-frames=80','--max-frames-ss',
                '--max-frames-ss-path='+str(folder/'shots/final.png'),'--no-patch',str(args.content)]
            with (folder/'stdout.log').open('wb') as log:
                result=subprocess.run(command,cwd=args.frontend.parent,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=180)
            assert result.returncode==0,('Frontend/test failed;preserved',folder,result.returncode)
            checks={k:int(v) for k,v in (line.split('=',1) for line in (folder/'checks.txt').read_text().splitlines())}
            assert checks['frames']==64
            if variant.get('hardware',True):assert checks['hardware_frames']>0
            assert (checks['loads']>0)==(method=='snapshot')
            assert (checks['speculative']>0)==(method=='snapshot')
            assert (checks['creates']==0)==(method=='reference')
            size,rgb=png_rgb(folder/'shots/final.png')
            colors=len({rgb[x:x+3] for x in range(0,len(rgb),3)})
            assert colors>=8,('Flat capture is not visual evidence',folder,colors)
            pictures.append((size,rgb))
            ram.append(digest(folder/'final-ram.bin'))
            records.append(dict(name=name,method=method,settings=settings,checks=checks,
                image_dimensions=size,image_colors=colors,image_sha256=hashlib.sha256(rgb).hexdigest(),ram_sha256=ram[-1],folder=str(folder)))
            (output/'results.json').write_text(json.dumps(dict(provenance=provenance,runs=records),indent=2),encoding='utf-8')
        assert ram[0]==ram[1],(name,'RAM continuation differs')
        assert pictures[0][0]==pictures[1][0],(name,'capture dimensions differ')
        differences=sum(pictures[0][1][i:i+3]!=pictures[1][1][i:i+3] for i in range(0,len(pictures[0][1]),3))
        records[-1].update(compared_pixels=len(pictures[0][1])//3,differing_pixels=differences,reference=records[-2]['folder'])
        (output/'results.json').write_text(json.dumps(dict(provenance=provenance,runs=records),indent=2),encoding='utf-8')
        assert differences==0,(name,differences,'different pixels')
        print(name,'PASS exact RAM and frontend RGB',flush=True)


if __name__=='__main__':main()
