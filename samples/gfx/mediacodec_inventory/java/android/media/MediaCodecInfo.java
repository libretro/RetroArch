package android.media;
public final class MediaCodecInfo {
 final String name; final boolean encoder,alias,hardware,software;
 public MediaCodecInfo(String n,boolean e,boolean a,boolean h,boolean s) {
  name=n; encoder=e; alias=a; hardware=h; software=s;
 }
 public boolean isEncoder(){return encoder;}
 public boolean isAlias(){return alias;}
 public boolean isHardwareAccelerated(){return hardware;}
 public boolean isSoftwareOnly(){return software;}
 public String getName(){return name;}
 public CodecCapabilities getCapabilitiesForType(String mime) {
  if (MediaCodecList.scenario==3) throw new IllegalStateException("vendor failure");
  if (name.equals("wrong.mime") || !mime.equals("video/hevc")) throw new IllegalArgumentException();
  return new CodecCapabilities();
 }
 public static final class CodecCapabilities {
  public boolean isFormatSupported(MediaFormat f) {
   if (MediaCodecList.scenario==10) throw new OutOfMemoryError("injected");
   return f.profile==2 && f.width<=1920 && f.height<=1080;
  }
 }
}
