import android.content.ContentResolver;

import com.libretro.common.vfs.VfsImplementationSaf;

/* Runs VfsImplementationSaf against FakeProvider. Build and run: make check */
public final class SafTest
{
   static final String TREE = "content://com.android.externalstorage.documents/tree/primary%3ATree";
   static int failures;

   static void check(boolean ok, String what)
   {
      System.out.println((ok ? "[SUCCESS] " : "[FAILED] ") + what);
      if (!ok)
         ++failures;
   }

   static FakeProvider provider()
   {
      final FakeProvider p = new FakeProvider();
      p.mkdir("primary:Tree");
      return p;
   }

   static void mkdirRenamedByProvider()
   {
      final FakeProvider p = provider();
      final int r = VfsImplementationSaf.mkdirSaf(new ContentResolver(p), TREE, "/saves/content:x");
      p.mkdir("primary:Tree/saves");
      final int r2 = VfsImplementationSaf.mkdirSaf(new ContentResolver(p), TREE, "/saves/content:x");
      check(r == -1, "mkdir under a missing parent fails");
      check(r2 == -1 && p.children("primary:Tree/saves").isEmpty(), "mkdir the provider renames fails and leaves nothing behind");
   }

   static void mkdirLosesRace()
   {
      final FakeProvider p = provider();
      p.beforeCreate = new Runnable() { public void run() { p.mkdir("primary:Tree/states"); } };
      final int r = VfsImplementationSaf.mkdirSaf(new ContentResolver(p), TREE, "/states");
      check(r == -2 && p.children("primary:Tree").size() == 1, "mkdir that loses the create to another creator reports it exists");
   }

   static void openLosesRace()
   {
      final FakeProvider p = provider();
      p.beforeCreate = new Runnable() { public void run() { p.file("primary:Tree/game.srm", 0); } };
      final int fd = VfsImplementationSaf.openSafFile(new ContentResolver(p), TREE, "/game.srm", false, true, true);
      check(fd >= 0 && p.children("primary:Tree").size() == 1, "open for write that loses the create opens the winner's file");
   }

   static void openCreates()
   {
      final FakeProvider p = provider();
      final int fd = VfsImplementationSaf.openSafFile(new ContentResolver(p), TREE, "/game.srm", false, true, true);
      check(fd >= 0 && p.has("primary:Tree/game.srm"), "open for write creates a missing file");
      final int rd = VfsImplementationSaf.openSafFile(new ContentResolver(p), TREE, "/missing.srm", true, false, false);
      check(rd == -1 && !p.has("primary:Tree/missing.srm"), "open for read of a missing file fails without creating it");
   }

   static void openRenamedByProvider()
   {
      final FakeProvider p = provider();
      final int fd = VfsImplementationSaf.openSafFile(new ContentResolver(p), TREE, "/a:b.srm", false, true, true);
      check(fd == -1 && p.children("primary:Tree").isEmpty(), "open for write the provider renames fails and leaves nothing behind");
   }

   static void deleteKeepsExisting()
   {
      final FakeProvider p = provider();
      p.createReturnsExisting = true;
      p.mkdir("primary:Tree/a_b");
      p.file("primary:Tree/a_b/save.srm", 8192);
      final int r = VfsImplementationSaf.mkdirSaf(new ContentResolver(p), TREE, "/a:b");
      check(r == -1 && p.has("primary:Tree/a_b/save.srm"), "a create answered with an existing directory never deletes it");

      p.file("primary:Tree/c_d.srm", 8192);
      final int fd = VfsImplementationSaf.openSafFile(new ContentResolver(p), TREE, "/c:d.srm", false, true, true);
      check(fd == -1 && p.has("primary:Tree/c_d.srm") && p.docs.get("primary:Tree/c_d.srm").size == 8192,
            "a create answered with an existing file never deletes it");
   }

   public static void main(String[] args)
   {
      mkdirRenamedByProvider();
      mkdirLosesRace();
      openLosesRace();
      openCreates();
      openRenamedByProvider();
      deleteKeepsExisting();

      if (failures != 0)
      {
         System.out.println(failures + " SAF test(s) failed");
         System.exit(1);
      }
      System.out.println("All SAF tests passed.");
   }
}
