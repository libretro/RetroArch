package android.media;
public final class MediaCodecInfo
{
   final String name;
   final boolean encoder, alias, hardware, software;
   public MediaCodecInfo(String n, boolean e, boolean a, boolean h, boolean s)
   {
      name = n;
      encoder = e;
      alias = a;
      hardware = h;
      software = s;
   }
   public boolean isEncoder()
   {
      return encoder;
   }
   public boolean isAlias()
   {
      return alias;
   }
   public boolean isHardwareAccelerated()
   {
      return hardware;
   }
   public boolean isSoftwareOnly()
   {
      return software;
   }
   public String getName()
   {
      return name;
   }
   public CodecCapabilities getCapabilitiesForType(String mime)
   {
      if (MediaCodecList.scenario == 3)
         throw new IllegalStateException("vendor failure");
      if (name.equals("wrong.mime")
            || !(mime.equals("video/hevc")
                  || (MediaCodecList.scenario == 13
                        && (mime.equals("video/av01")
                              || mime.equals("video/x-vnd.on2.vp9")))))
         throw new IllegalArgumentException();
      return new CodecCapabilities();
   }
   public static final class CodecCapabilities
   {
      public static final int COLOR_FormatSurface = 0x7f000789;
      public boolean isFormatSupported(MediaFormat f)
      {
         if (MediaCodecList.scenario == 10 || MediaCodecList.scenario == 12)
            throw new OutOfMemoryError("injected");
         if (MediaCodecList.scenario == 11)
            return f.profile == 2 && f.width <= 1920 && f.height <= 1080
                  && f.colorFormat == COLOR_FormatSurface
                  && f.bitrate >= 1000000 && f.bitrate <= 20000000
                  && f.frameRate >= 24 && f.frameRate <= 60 && f.level == 4096
                  && f.standard == 1 && f.transfer == 3
                  && (f.range == 1 || f.range == 2) && f.hdr == null;
         if (MediaCodecList.scenario == 13)
            return f.profile == 4096 && f.level == 4096 && f.standard == 6
                  && f.transfer == 6 && (f.range == 1 || f.range == 2)
                  && f.colorFormat == COLOR_FormatSurface && f.hdr != null
                  && f.hdr.remaining() == 25 && f.hdr.get(0) == 0
                  && (f.hdr.get(17) & 255) == 0xe8 && f.hdr.get(18) == 3;
         if (MediaCodecList.scenario == 15)
            return false;
         return f.profile == 2 && f.width <= 1920 && f.height <= 1080;
      }
   }
}
