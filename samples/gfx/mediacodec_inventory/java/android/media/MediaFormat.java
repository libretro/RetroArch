package android.media;
public final class MediaFormat
{
   public String mime;
   public int width, height, profile, colorFormat, bitrate;
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
      else
         throw new IllegalArgumentException();
   }
   public void setFloat(String key, float value)
   {
      if (!key.equals("frame-rate"))
         throw new IllegalArgumentException();
      frameRate = value;
   }
}
