#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (C) 2026 The RetroArch team
"""Exercise the actual runloop decode callbacks and v1-v4 negotiation.

Uses the checked hardware backend from rh264_hw_test and a generated H.264
stream. Requires ffmpeg/libx264 and a C compiler. Builds on Linux and MinGW;
--sanitize enables ASan/UBSan with a supporting compiler.
"""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def source():
    runloop = (ROOT / "runloop.c").read_text(encoding="utf-8")
    callbacks = runloop.split("static void *runloop_video_decode_open(void)", 1)[1].split("\n#endif", 1)[0]
    callbacks = "static void *runloop_video_decode_open(void)" + callbacks
    dispatch = runloop.split("      case RETRO_ENVIRONMENT_GET_VIDEO_DECODE_INTERFACE:", 1)[1].split("      case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:", 1)[0]
    fake = (ROOT / "libretro-common/samples/formats/h264/rh264_hw_test.c").read_text(encoding="utf-8").split("/* ---- the streams ---- */", 1)[0]
    return '#include <assert.h>\n#include <libretro.h>\n#define HAVE_RMP4 1\n#define RARCH_LOG(...) ((void)0)\n' + fake + callbacks + "\nstatic bool query(void *data) {\n" + dispatch + r'''
return false;
}

/* The original allocation ends at close, with a canary immediately after. */
struct v1_interface {
 unsigned interface_version;
 void *(*open)(void);
 int (*decode)(void *,const uint8_t *,size_t,struct retro_video_decode_frame *);
 int (*drain)(void *,struct retro_video_decode_frame *);
 void (*close)(void *);
};
static void negotiation(void) {
 struct { struct v1_interface iface; uint8_t canary[64]; } old;
 struct retro_video_decode_interface current;
 unsigned i;
 memset(&old,0xa5,sizeof(old)); old.iface.interface_version=1;
 assert(query(&old.iface));
 for(i=0;i<sizeof(old.canary);i++) assert(old.canary[i]==0xa5);
 assert(old.iface.open && old.iface.decode && old.iface.drain && old.iface.close);
 memset(&current,0,sizeof(current)); current.interface_version=2;
 assert(query(&current));
 assert(current.decode_ex && current.drain_ex && current.get_frame);
 current.interface_version=3; assert(query(&current)); assert(current.get_gpu_frame);
 {size_t prefix=offsetof(struct retro_video_decode_interface,get_gpu_frame);
  unsigned char *v2=(unsigned char*)malloc(prefix+64); assert(v2);
  memset(v2,0xa5,prefix+64);((struct retro_video_decode_interface*)v2)->interface_version=2;
  assert(query(v2));for(i=0;i<64;i++) assert(v2[prefix+i]==0xa5);free(v2);}
 {size_t prefix=offsetof(struct retro_video_decode_interface,set_tag);
  unsigned char *v3=(unsigned char*)malloc(prefix+64); assert(v3);
  memset(v3,0xa5,prefix+64);((struct retro_video_decode_interface*)v3)->interface_version=3;
  assert(query(v3));for(i=0;i<64;i++) assert(v3[prefix+i]==0xa5);free(v3);}
 memset(&current,0,sizeof(current));
 current.interface_version=4; assert(query(&current)); assert(current.set_tag && current.get_tag);
 current.interface_version=5; assert(!query(&current));
 assert(!query(NULL));
}

/* Append one length-prefixed AVC parameter set as Annex B. */
static size_t parameter(uint8_t *out,const uint8_t *data,size_t *position,size_t end) {
 size_t n;
 assert(*position+2<=end);
 n=((size_t)data[*position]<<8)|data[*position+1]; *position+=2;
 assert(*position+n<=end && n+4<65536);
 out[0]=out[1]=out[2]=0;out[3]=1;memcpy(out+4,data+*position,n);*position+=n;
 return n+4;
}
static void take_gpu(struct retro_video_decode_interface *iface,void *decoder) {
 struct retro_video_decode_gpu_frame gpu,duplicate;
 int before=readbacks,result;
 inject_busy=1;output_busy=0;
 result=iface->get_gpu_frame(decoder,&gpu);assert(result==2 && readbacks==before);
 result=iface->get_gpu_frame(decoder,&gpu);inject_busy=0;
 assert(result==1 && gpu.image && gpu.width && gpu.height && gpu.abandon);
 assert(gpu.api==RETRO_VIDEO_DECODE_GPU_VULKAN);
 assert(iface->get_gpu_frame(decoder,&duplicate)==-1 && readbacks==before);
 gpu.abandon(gpu.owner,gpu.done,gpu.done_value);
}
/* A picture handed out names its own sample: times rising, each once. */
static void tag_check(struct retro_video_decode_interface *iface,void *decoder,
 const int64_t *pts,int samples,unsigned char *tagged,int64_t *last,int shown) {
 uint64_t k=iface->get_tag(decoder);
 assert(k<(uint64_t)samples && !tagged[k]);
 assert(!shown || pts[k]>*last);
 tagged[k]=1;*last=pts[k];
}
static int decode_stream(const char *path,int deferred,int tags) {
 struct retro_video_decode_interface iface;
 struct retro_video_decode_frame frame;
 rmp4_t *movie;
 rmp4_packet packet;
 const rmp4_track *track;
 uint8_t *bytes,*au;
 size_t size,pos,headers=0,au_size;
 int stream=-1,i,nal_length,shown=0,result,draining=0,busy=0;
 int64_t pts[256],last_pts=0; unsigned char tagged[256];
 void *decoder;
 bytes=slurp(path,&size);assert(bytes);
 movie=rmp4_open_memory(bytes,size);assert(movie);
 for(i=0;i<rmp4_num_tracks(movie);i++)
  if(rmp4_get_track(movie,i)->type==RMP4_TRACK_VIDEO) {stream=i;break;}
 assert(stream>=0);track=rmp4_get_track(movie,stream);
 au=(uint8_t*)malloc(1024*1024);assert(au);
 assert(track->codec_private_size>=7);
 nal_length=(track->codec_private[4]&3)+1;pos=6;
 for(i=0;i<(track->codec_private[5]&31);i++)
  headers+=parameter(au+headers,track->codec_private,&pos,track->codec_private_size);
 assert(pos<track->codec_private_size);
 {int count=track->codec_private[pos++];for(i=0;i<count;i++)
  headers+=parameter(au+headers,track->codec_private,&pos,track->codec_private_size);}
 memset(&iface,0,sizeof(iface));iface.interface_version=tags?4:deferred==2?3:deferred?2:1;assert(query(&iface));
 memset(tagged,0,sizeof(tagged));
 memset(&fake,0,sizeof(fake));readbacks=0;cur_sample=0;
 fake_ops.api=VIDEO_HW_API_VULKAN;
 rh264_set_hw_backend(&fake_ops);decoder=iface.open();assert(decoder);
 assert(iface.get_frame==NULL || iface.get_frame(decoder,&frame)==-1);
 while(!draining) {
  result=rmp4_read_packet(movie,&packet);
  if(result!=1) {draining=1;break;}
  if(packet.track!=stream) continue;
  assert(cur_sample<256);pts[cur_sample]=packet.timestamp;
  if(tags) iface.set_tag(decoder,(uint64_t)cur_sample);
  au_size=headers;headers=0;pos=0;
  while(pos<packet.size) {
   size_t n=0;
   assert(pos+(size_t)nal_length<=packet.size);
   for(i=0;i<nal_length;i++) n=(n<<8)|packet.data[pos++];
   assert(n>0 && pos+n<=packet.size && au_size+n+4<1024*1024);
   au[au_size++]=0;au[au_size++]=0;au[au_size++]=0;au[au_size++]=1;
   memcpy(au+au_size,packet.data+pos,n);au_size+=n;pos+=n;
  }
  {int before=readbacks,retries=0;
   inject_busy=deferred;
   do {
    result=deferred?iface.decode_ex(decoder,au,au_size,&frame,RETRO_VIDEO_DECODE_DEFER_PLANES)
     :iface.decode(decoder,au,au_size,&frame);
    if(result==2) {busy++;assert(iface.get_frame(decoder,&frame)==-1);}
   } while(result==2 && ++retries<8);
   inject_busy=0;
   assert(result==0 || result==1);
   if(deferred) assert(readbacks==before);
   if(result==1) {
    if(deferred) {
     assert(!frame.planes[0] && frame.width && frame.height && frame.hardware);
     if(deferred==2) take_gpu(&iface,decoder);
     else {
      assert(iface.get_frame(decoder,&frame)==1);
      assert(iface.get_frame(decoder,&frame)==1 && readbacks==before+1);
     }
    }
    assert(deferred==2 || (frame.planes[0] && frame.planes[0][0]==19));
    if(tags) tag_check(&iface,decoder,pts,cur_sample+1,tagged,&last_pts,shown);
    shown++;
   }
  }
  cur_sample++;
 }
 for(;;) {
  int before=readbacks,retries=0;
  inject_busy=deferred;
  do {
   result=deferred?iface.drain_ex(decoder,&frame,RETRO_VIDEO_DECODE_DEFER_PLANES):iface.drain(decoder,&frame);
   if(result==2) busy++;
  } while(result==2 && ++retries<8);
  inject_busy=0;
  if(deferred) assert(readbacks==before);
  if(result==0) break;
  assert(result==1);
  if(deferred) {
   if(deferred==2) take_gpu(&iface,decoder);
   else {
    assert(iface.get_frame(decoder,&frame)==1);
    assert(iface.get_frame(decoder,&frame)==1 && readbacks==before+1);
   }
  }
  assert((deferred==2 || frame.planes[0]) && frame.hardware);
  if(tags) tag_check(&iface,decoder,pts,cur_sample,tagged,&last_pts,shown);
  shown++;
 }
 if(deferred) {
  assert(iface.get_frame(decoder,&frame)==-1);
  assert(iface.decode_ex(decoder,au,1,&frame,2)==-1);
 }
 assert(!fake.errors && readbacks==(deferred==2?0:shown));
 if(deferred==2) assert(fake.abandoned==shown);
 if(tags) for(i=0;i<cur_sample;i++) assert(tagged[i]);
 if(deferred) assert(busy>0);
 iface.close(decoder);assert(fake.closed);
 rmp4_close(movie);free(bytes);free(au);rh264_set_hw_backend(NULL);
 return shown;
}
int main(int argc,char **argv) {
 int eager,deferred,gpu,tagged;
 assert(argc==2);negotiation();
 eager=decode_stream(argv[1],0,0);deferred=decode_stream(argv[1],1,0);
 gpu=decode_stream(argv[1],2,0);assert(eager==deferred && eager==gpu && eager==24);
 tagged=decode_stream(argv[1],0,1);assert(tagged==24);
 printf("PASS: v1/v2/v3 allocation canaries, v4 tags in presentation order, v3 GPU leases with zero CPU readbacks, %d eager/deferred Annex-B outputs, cached materialization, busy retries and EOF\n",eager);
 return 0;
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cc", default="cc")
    parser.add_argument("--ffmpeg", default="ffmpeg")
    parser.add_argument("--sanitize", action="store_true")
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="retro-video-interface-") as directory:
        tmp = Path(directory)
        movie = tmp / "stream.mp4"
        subprocess.run([args.ffmpeg, "-v", "error", "-y", "-f", "lavfi", "-i", "testsrc2=s=170x122:r=25",
                        "-frames:v", "24", "-c:v", "libx264", "-threads", "2", "-profile:v", "main",
                        "-x264-params", "bframes=3:b-pyramid=normal:ref=4", str(movie)], check=True)
        test = tmp / "test.c"
        test.write_text(source(), encoding="utf-8")
        binary = tmp / ("test.exe" if os.name == "nt" else "test")
        flags = ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"] if args.sanitize else []
        subprocess.run([args.cc, "-std=c89", "-O1", "-g", "-D_POSIX_C_SOURCE=200809L", *flags,
                        "-I" + str(ROOT / "libretro-common/include"), str(test),
                        str(ROOT / "libretro-common/formats/h264/rh264.c"),
                        str(ROOT / "libretro-common/formats/mp4/rmp4.c"), "-lm", "-o", str(binary)], check=True)
        subprocess.run([str(binary), str(movie)], check=True)


if __name__ == "__main__":
    main()
