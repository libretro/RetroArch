/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RECORD_VT_TEST_PACKET_H
#define RECORD_VT_TEST_PACKET_H
static bool prefix(FILE *out, const uint8_t *data, size_t size)
{
   static const uint8_t start[4] = {0,0,0,1};
   return fwrite(start,1,4,out)==4 && fwrite(data,1,size,out)==size;
}
static bool headers(FILE *out, CMVideoFormatDescriptionRef format, bool hevc, int *length)
{
   const uint8_t *data;
   size_t size, count, i;
   OSStatus status;
   count=0;
   for (i=0; !i || i<count; i++)
   {
      status=hevc ? CMVideoFormatDescriptionGetHEVCParameterSetAtIndex(format,i,&data,&size,&count,length)
                  : CMVideoFormatDescriptionGetH264ParameterSetAtIndex(format,i,&data,&size,&count,length);
      if (status || !count || count>16 || *length<1 || *length>4)
      { fprintf(stderr,"Header export failed: status %d index %lu count %lu length %d\n",
            (int)status,(unsigned long)i,(unsigned long)count,*length); return false; }
      if (!prefix(out,data,size)) return false;
   }
   return true;
}
static bool packet(FILE *out, CMSampleBufferRef sample, int length)
{
   CMBlockBufferRef block=CMSampleBufferGetDataBuffer(sample);
   size_t size=block?CMBlockBufferGetDataLength(block):0, at=0, n;
   unsigned i;
   uint8_t *bytes;
   bool ok=false;
   if (!size || size>4*1024*1024) return false;
   bytes=(uint8_t*)malloc(size); if (!bytes) return false;
   if (CMBlockBufferCopyDataBytes(block,0,size,bytes)) goto done;
   while (at<size)
   {
      if (size-at<(unsigned)length) goto done;
      n=0;
      for (i=0;i<(unsigned)length;i++) n=(n<<8)|bytes[at++];
      if (!n || n>size-at || !prefix(out,bytes+at,n)) goto done;
      at+=n;
   }
   ok=true;
done:
   free(bytes); return ok;
}
#endif
