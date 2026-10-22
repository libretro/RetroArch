package android.media;
public final class MediaFormat
{
   public String mime;
   public int width, height, profile;
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
      if (!key.equals("profile"))
         throw new IllegalArgumentException();
      profile = value;
   }
}
