#!/usr/bin/env python3
"""Native frontend Metal recording; independent decoding is test-only."""
import argparse
import array
import math
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

parser = argparse.ArgumentParser()
parser.add_argument('--retroarch', required=True)
parser.add_argument('--core', required=True)
parser.add_argument('--output-dir', required=True)
args = parser.parse_args()
assert sys.platform == 'darwin', 'Native frontend acceptance requires macOS'
exe, core = str(Path(args.retroarch).resolve()), str(Path(args.core).resolve())
root = Path(args.output_dir).resolve()
root.mkdir(parents=True, exist_ok=True)
ffmpeg, ffprobe = shutil.which('ffmpeg'), shutil.which('ffprobe')
assert ffmpeg and ffprobe, 'Independent decoder/probe required'
for codec in ('h264', 'hevc'):
    for threaded in ('false', 'true'):
        for audio in ('false', 'true'):
            case = root / f'{codec}-threaded-{threaded}-audio-{audio}'
            case.mkdir()
            config, record, movie = case/'retroarch.cfg', case/'record.cfg', case/'output.mp4'
            config.write_text(f'''video_driver = "metal"
video_threaded = "{threaded}"
video_fullscreen = "false"
video_scale = "1.0"
video_vsync = "true"
video_swapchain_bit_depth = "1"
video_hdr_mode = "0"
video_record_game_only = "false"
video_record_allow_frame_drop = "true"
video_font_enable = "false"
record_driver = "metal"
audio_driver = "null"
audio_format_negotiation = "1"
audio_fastpath_s16 = "false"
menu_driver = "rgui"
menu_show_start_screen = "false"
history_list_enable = "true"
playlist_directory = "{case / 'playlists'}"
playlist_use_old_format = "false"
config_save_on_exit = "false"
pause_nonactive = "false"
savestate_auto_save = "false"
savestate_auto_load = "false"
''', encoding='utf-8')
            record.write_text(f'audio_enable = "{audio}"\nvcodec = "{codec}"\nformat = "mp4"\nvideo_bit_rate = "8000000"\n', encoding='utf-8')
            env = dict(os.environ, NATIVE_RECORD_RATE='48000', NATIVE_RECORD_LAYOUT='3')
            env.pop('NATIVE_RECORD_FLOAT', None)
            result = subprocess.run([exe, '-v', '--config', str(config), '-L', core,
                '--record', str(movie), '--recordconfig', str(record), '--max-frames', '180'],
                cwd=case, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=60)
            log = result.stdout.decode(errors='replace')
            (case/'run.log').write_text(log, encoding='utf-8')
            assert result.returncode == 0, (case, result.returncode, log[-8000:])
            done = re.search(r'Finalized (\d+) GPU frames, (\d+) PCM frames', log)
            assert done and int(done[1]) >= 20, (case, log[-8000:])
            probe = json.loads(subprocess.check_output([ffprobe, '-v', 'error', '-show_streams',
                '-show_packets', '-of', 'json', str(movie)], timeout=30))
            (case/'probe.json').write_text(json.dumps(probe, indent=2), encoding='utf-8')
            video = next(s for s in probe['streams'] if s['codec_type']=='video')
            audios = [s for s in probe['streams'] if s['codec_type']=='audio']
            assert video['codec_name']==codec and bool(audios)==(audio=='true'), probe['streams']
            packets = [p for p in probe['packets'] if p['stream_index']==video['index']]
            assert len(packets)==int(done[1]), (len(packets), done.groups())
            pts = [int(p['pts']) for p in packets]
            assert all(b>a for a,b in zip(pts,pts[1:])), pts
            tick_num,tick_den = map(int,video['time_base'].split('/'))
            assert all(abs((b-a)*tick_num/tick_den*60000/1001-round((b-a)*tick_num/tick_den*60000/1001))<1e-5
                for a,b in zip(pts,pts[1:])), (video['time_base'],pts)
            raw = subprocess.check_output([ffmpeg, '-v', 'error', '-i', str(movie), '-map', '0:v:0',
                '-pix_fmt', 'rgb24', '-fps_mode', 'passthrough', '-f', 'rawvideo', '-'], timeout=30)
            w,h = video['width'],video['height']
            assert len(raw)==len(packets)*w*h*3
            references=[(255,32,32),(32,255,32),(32,32,255),(192,192,192)]
            colors=set()
            for n in range(len(packets)):
                offset=(n*w*h+(h//2)*w+w//2)*3
                pixel=raw[offset:offset+3]
                errors=[sum(abs(a-b) for a,b in zip(pixel,color))/3 for color in references]
                assert min(errors)<8, (case,n,list(pixel),errors)
                colors.add(errors.index(min(errors)))
            assert len(colors)>=3, (case,colors)
            if audios:
                a=audios[0]
                assert a['codec_name']=='aac' and int(a['sample_rate'])==48000 and a['channels']==2,a
                assert abs(float(a['duration'])-int(done[2])/48000)<.05,(a,done.groups())
                pcm = subprocess.check_output([ffmpeg, '-v', 'error', '-i', str(movie),
                    '-map', '0:a:0', '-f', 'f32le', '-'], timeout=30)
                samples = array.array('f', pcm)
                assert len(samples)%2==0 and abs(len(samples)//2-int(done[2]))<=2048, (len(samples),done.groups())
                for channel,frequency in enumerate((440,660)):
                    values=samples[4096:min(len(samples),20480)][channel::2]
                    assert len(values)>2000, len(values)
                    omega=2*math.pi*frequency/48000
                    cosine=sum(v*math.cos(omega*i) for i,v in enumerate(values))
                    sine=sum(v*math.sin(omega*i) for i,v in enumerate(values))
                    amplitude=2*math.hypot(cosine,sine)/len(values)
                    assert abs(amplitude-.2)<.04,(case,channel,frequency,amplitude)

            histories = list(case.rglob('content_video_history.lpl'))
            entries = [entry for history in histories for entry in json.loads(history.read_text(encoding='utf-8'))['items']]
            assert any(entry['path']==str(movie) and entry['core_path']=='builtin'
                and entry['core_name']=='movieplayer' for entry in entries), (case,histories,entries)
            print(f'PASS: real Metal frontend {case.name}: {len(packets)} GPU frames, clocks, independent colors and audio contract',flush=True)

# The last normal case supplies a working configuration for lifecycle faults.
for control in ('early', 'exists'):
    destination=root/f'control-{control}.mp4'
    if control=='exists': destination.write_bytes(b'preserve existing output')
    result=subprocess.run([exe,'-v','--config',str(config),'-L',core,
        '--record',str(destination),'--recordconfig',str(record),'--max-frames',
        '1' if control=='early' else '30'],cwd=case,env=env,
        stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=60)
    log=result.stdout.decode(errors='replace')
    (root/f'control-{control}.log').write_text(log,encoding='utf-8')
    assert result.returncode==0,(control,result.returncode,log[-8000:])
    if control=='exists':
        assert destination.read_bytes()==b'preserve existing output',log
        assert '[Metal recording] Failed' in log,log
    print(f'PASS: real frontend {control}: bounded shutdown and output preservation',flush=True)

for threaded in ('false','true'):
    changed=root/f'viewport-{threaded}.mp4'
    changed_config=root/f'viewport-{threaded}.cfg'
    changed_config.write_text(config.read_text(encoding='utf-8').replace(
        'video_threaded = "true"',f'video_threaded = "{threaded}"'),encoding='utf-8')
    changed_env=dict(env,NATIVE_RECORD_VIEWPORT_CHANGE='1')
    result=subprocess.run([exe,'-v','--config',str(changed_config),'-L',core,
        '--record',str(changed),'--recordconfig',str(record),'--max-frames','180'],
        cwd=case,env=changed_env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=60)
    log=result.stdout.decode(errors='replace')
    (root/f'viewport-{threaded}.log').write_text(log,encoding='utf-8')
    assert result.returncode==0 and '[fixture] viewport geometry changed' in log,log[-8000:]
    done=re.search(r'Finalized (\d+) GPU frames, (\d+) PCM frames',log)
    assert done and 0<int(done[1])<100 and '[Metal recording] Failed' not in log,log[-8000:]
    subprocess.run([ffmpeg,'-v','error','-i',str(changed),'-f','null','-'],check=True,timeout=30)
    print(f'PASS: real frontend viewport change threaded={threaded}: retired playable recording',flush=True)
