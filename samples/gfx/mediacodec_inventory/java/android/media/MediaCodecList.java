package android.media;
public final class MediaCodecList
{
   public static int scenario = 0, calls = 0;
   public MediaCodecList(int mode)
   {
      if (mode != 1)
         throw new IllegalArgumentException();
      calls++;
   }
   public MediaCodecInfo[] getCodecInfos()
   {
      if (scenario == 8)
         return new MediaCodecInfo[4097];
      if (scenario == 9)
         return new MediaCodecInfo[] {null};
      String name = scenario == 5   ? "x".repeat(300)
                    : scenario == 6 ? "codec.\u00e9"
                                    : "c2.test.hevc";
      MediaCodecInfo good =
          new MediaCodecInfo(name, scenario == 7, false, true, false);
      if (scenario == 4)
         return new MediaCodecInfo[] {
             new MediaCodecInfo("wrong.mime", false, false, true, false), good};
      return new MediaCodecInfo[] {
          new MediaCodecInfo("software", false, false, false, true),
          new MediaCodecInfo("contradictory", false, false, true, true),
          new MediaCodecInfo("alias", false, true, true, false), good};
   }
}
