import android.content.ContentResolver;

/* What retro_vfs_init_saf asks of the activity it is handed, over FakeProvider
 * with the tree jni_test.c walks */
public final class FakeActivity
{
   public final FakeProvider provider = new FakeProvider();
   private final ContentResolver resolver = new ContentResolver(provider);

   public FakeActivity()
   {
      provider.mkdir("primary:Tree");
      provider.file("primary:Tree/big.bin", 5000000000L);
      provider.mkdir("primary:Tree/roms");
      for (int i = 0; i < 300; ++i)
         provider.file(String.format("primary:Tree/roms/game%03d.sfc", i), i);
      provider.file("primary:Tree/roms/café 🎮 \u0000x", 1);
      provider.mkdir("primary:Tree/roms/sub");
      provider.mkdir("primary:Tree/empty");
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
