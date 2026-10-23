package android.media;
public final class MediaFormat
{
   public String mime;
   public int width, height, profile, colorFormat, bitrate, level, standard,
         transfer, range;
   public java.nio.ByteBuffer hdr;
   public float frameRate;
   public static MediaFormat createVideoFormat(String m, int w, int h)
   {
      MediaFormat f = new MediaFormat();
      f.mime = m;
      f.width = w;
      f.height = h;
      return f;
   }
   public void setInteger(String key, int value)
   {
      if (key.equals("profile"))
         profile = value;
      else if (key.equals("color-format"))
         colorFormat = value;
      else if (key.equals("bitrate"))
         bitrate = value;
      else if (key.equals("level"))
         level = value;
      else if (key.equals("color-standard"))
         standard = value;
      else if (key.equals("color-transfer"))
         transfer = value;
      else if (key.equals("color-range"))
         range = value;
      else
         throw new IllegalArgumentException();
   }
   public void setByteBuffer(String key, java.nio.ByteBuffer value)
   {
      if (MediaCodecList.scenario == 14)
         throw new OutOfMemoryError("injected");
      if (!key.equals("hdr-static-info"))
         throw new IllegalArgumentException();
      hdr = value;
   }
   public void setFloat(String key, float value)
   {
      if (!key.equals("frame-rate"))
         throw new IllegalArgumentException();
      frameRate = value;
   }
}
