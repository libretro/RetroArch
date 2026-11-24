import android.content.ContentResolver;

/* What retro_vfs_init_saf asks of the activity it is handed, over FakeProvider
 * with the tree jni_test.c walks */
public final class FakeActivity
{
   public final FakeProvider provider = new FakeProvider();
   private final ContentResolver resolver = new ContentResolver(provider);

   public FakeActivity()
   {
      final String backing = System.getProperty("saf.backing");
      if (backing != null)
         provider.backing = new java.io.File(backing);
      provider.mkdir("primary:Tree");
      provider.file("primary:Tree/big.bin", 5000000000L);
      provider.mkdir("primary:Tree/roms");
      for (int i = 0; i < 300; ++i)
         provider.file(String.format("primary:Tree/roms/game%03d.sfc", i), i);
      provider.file("primary:Tree/roms/café 🎮 \u0000x", 1);
      provider.mkdir("primary:Tree/roms/sub");
      provider.mkdir("primary:Tree/empty");
      if (backing != null)
         provider.file("primary:Tree/src.bin", 0);
   }

   /* jni_copy_test fills src.bin's backing file before copying it */
   public String backingPath(String id)
   {
      return provider.backingFile(id).getPath();
   }

   public ClassLoader getClassLoader()
   {
      return FakeActivity.class.getClassLoader();
   }

   public ContentResolver getContentResolver()
   {
      return resolver;
   }
}
