#!/usr/bin/env python3
"""Independent AAC-LC packet-cut experiment; not MediaCodec device acceptance.

Requires CC and FFmpeg. Compiles the actual timing policy, then measures encoder
lag from a chirp and compares independently decoded source after silent preroll.
"""
from pathlib import Path
import array,json,math,os,shlex,shutil,subprocess,tempfile
ff=shutil.which('ffmpeg');assert ff
results=[]
def decode(path,channels):
 p=subprocess.run([ff,'-v','error','-i',str(path),'-f','f32le','-'],check=True,stdout=subprocess.PIPE)
 a=array.array('f');a.frombytes(p.stdout);return a
with tempfile.TemporaryDirectory(prefix='aac-live-prime-') as tmp:
 d=Path(tmp)
 here=Path(__file__).resolve().parent; root=here.parents[2]
 binary=d/('timing.exe' if os.name=='nt' else 'timing')
 subprocess.run(shlex.split(os.environ.get('CC','cc'))+['-O1','-std=c89','-pedantic','-Wall','-Wextra','-Werror','-Wno-long-long','-Wframe-larger-than=2048','-I'+str(root/'libretro-common/include'),str(here/'stream_timing_test.c'),str(root/'record/record_aac_stream_timing.c'),'-o',str(binary)],check=True)
 timing={int(row[0]):list(map(int,row[1:])) for row in (line.split() for line in subprocess.check_output([str(binary),'--epochs'],text=True).splitlines())}
 for rate in (44100,48000):
  for channels in (1,2):
   # FFmpeg AAC-LC has a measured 1024-frame delay in this experiment only.
   # Android delay remains runtime-calibrated; do not promote this constant.
   delay=1024; prefix=8192; guard=4096; preroll=2048
   source_input,origin,preroll,video_epoch=timing[rate]
   source_decoded=source_input+delay
   data=array.array('h')
   for i in range(source_input+8192):
    for c in range(channels):
     value=0
     if 2048 <= i < 4096:
      t=(i-2048)/2048; value=int(12000*math.sin(math.pi*t)**2*math.sin(2*math.pi*(400+300*c)*(i-2048)/rate))
     if source_input <= i:
      at=i-source_input;value=int(10000*math.sin(2*math.pi*(997+701*c)*at/rate))
     data.append(value)
   raw=d/'input.pcm';raw.write_bytes(data.tobytes());encoded=d/'full.aac'
   subprocess.run([ff,'-v','error','-f','s16le','-ar',str(rate),'-ac',str(channels),'-i',str(raw),'-c:a','aac','-b:a','192k','-f','adts',str(encoded),'-y'],check=True)
   packet=[];blob=encoded.read_bytes();at=0
   while at<len(blob):
    assert blob[at]==255 and blob[at+1]&240==240
    size=((blob[at+3]&3)<<11)|(blob[at+4]<<3)|(blob[at+5]>>5)
    assert size>=7 and at+size<=len(blob)
    packet.append(blob[at:at+size]);at+=size
   stream=d/'live.aac';stream.write_bytes(b''.join(packet[origin//1024:]))
   ref=decode(encoded,channels);live=decode(stream,channels)
   # Recover delay independently from the encoded chirp, rather than trusting
   # the encoder-specific input assumption used to construct this trial.
   errors=[]
   reference=[data[i*channels]/32768 for i in range(2048,4096)]
   for candidate in range(4097):
    error=sum((ref[(2048+candidate+i)*channels]-reference[i])**2 for i in range(2048))
    errors.append(error)
   measured=min(range(len(errors)),key=errors.__getitem__)
   assert measured==delay,(rate,channels,measured,delay)
   assert errors[measured]<sum(x*x for x in reference)*.02
   expected=ref[(origin+preroll)*channels:(origin+preroll+8192)*channels]
   actual=live[preroll*channels:(preroll+8192)*channels]
   assert len(actual)==len(expected)==8192*channels
   delta=max(abs(a-b) for a,b in zip(actual,expected))
   silence=max(abs(x) for x in live[:1024*channels])
   energy=sum(x*x for x in actual)/len(actual)
   assert delta < 0.0001,(rate,channels,delta)
   assert silence < 0.0001,(rate,channels,silence)
   assert energy > .02
   # Removing the offset must put the beginning in preroll, not source time.
   assert sum(x*x for x in live[:1024*channels])/(1024*channels)<1e-8
   results.append(dict(rate=rate,channels=channels,measured_encoder_delay=measured,input_epoch=source_input,decoded_epoch=source_decoded,drop_frames=origin,preroll=preroll,max_source_error=delta,initial_silence_peak=silence,source_energy=energy))
Path('aac-live-priming-experiment.json').write_text(json.dumps(dict(scope='FFmpeg AAC-LC synthetic encoded-packet experiment; not native MediaCodec validation',cases=results),indent=2)+'\n')
print(json.dumps(results))
