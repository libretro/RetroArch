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

   static void pathsStayInsideTheTree()
   {
      final FakeProvider p = provider();
      final ContentResolver c = new ContentResolver(p);
      // Named like directories that are symlinks on a device or host root (/bin, /etc, /sdcard, /d)
      p.mkdir("primary:Tree/bin");
      p.mkdir("primary:Tree/sdcard");
      p.mkdir("primary:Tree/a");
      p.mkdir("primary:Tree/a/c");
      p.file("primary:Tree/a/c/d.srm", 3);
      check(VfsImplementationSaf.statSaf(c, TREE, "/bin", false) == 1, "a path is not resolved against the device's own filesystem (/bin)");
      check(VfsImplementationSaf.statSaf(c, TREE, "sdcard", false) == 1, "a path is not resolved against the device's own filesystem (sdcard)");
      check(VfsImplementationSaf.statSaf(c, TREE, "/a/./b/../c//d.srm", true) == (3 << 1), "'.', '..' and empty components resolve as strings");
      check(VfsImplementationSaf.statSaf(c, TREE, "/../../a/c/", false) == 1, "'..' never climbs above the root of the tree");
   }

   static void statInOneQuery()
   {
      final FakeProvider p = provider();
      final ContentResolver c = new ContentResolver(p);
      p.file("primary:Tree/big.bin", 5000000000L);
      p.mkdir("primary:Tree/dir");
      p.transactions = 0;
      final long file = VfsImplementationSaf.statSaf(c, TREE, "/big.bin", true);
      final long dir = VfsImplementationSaf.statSaf(c, TREE, "/dir", true);
      final long missing = VfsImplementationSaf.statSaf(c, TREE, "/missing", true);
      final long nosize = VfsImplementationSaf.statSaf(c, TREE, "/big.bin", false);
      final long root = VfsImplementationSaf.statSaf(c, TREE, "/", false);
      check(file == (5000000000L << 1), "stat packs a file's size above the directory bit");
      check(dir == 1 && root == 1, "stat sets the directory bit");
      check(missing == Long.MIN_VALUE, "stat of a missing document is Long.MIN_VALUE");
      check(nosize == 0, "stat without the size reports 0 for it");
      check(p.transactions == 5, "stat is one provider call each");
   }

   static void openExistingIsOneCall()
   {
      final FakeProvider p = provider();
      final ContentResolver c = new ContentResolver(p);
      p.file("primary:Tree/game.srm", 10);
      p.mkdir("primary:Tree/saves");
      p.transactions = 0;
      final int fd = VfsImplementationSaf.openSafFile(c, TREE, "/game.srm", true, false, false);
      final int r = VfsImplementationSaf.mkdirSaf(c, TREE, "/saves");
      check(fd >= 0 && r == -2 && p.transactions == 2, "opening an existing file and creating an existing directory are one provider call each");
   }

   /* The native side's reading of a batch: flag, name, terminator */
   static int decodeBatch(byte[] batch, java.util.List<String> names, java.util.List<Boolean> dirs)
   {
      int n = 0;
      for (int i = 0; i < batch.length;)
      {
         final boolean dir = batch[i++] != 0;
         final int start = i;
         while (batch[i] != 0)
            ++i;
         final StringBuilder b = new StringBuilder();
         for (int j = start; j < i;)
         {
            final int c = batch[j] & 0xff;
            if (c < 0x80)
               { b.append((char)c); j += 1; }
            else if (c < 0xe0)
               { b.append((char)(((c & 0x1f) << 6) | (batch[j + 1] & 0x3f))); j += 2; }
            else
               { b.append((char)(((c & 0x0f) << 12) | ((batch[j + 1] & 0x3f) << 6) | (batch[j + 2] & 0x3f))); j += 3; }
         }
         names.add(b.toString());
         dirs.add(dir);
         ++i;
         ++n;
      }
      return n;
   }

   static void readdirBatches()
   {
      final FakeProvider p = provider();
      final ContentResolver c = new ContentResolver(p);
      final java.util.Set<String> want = new java.util.TreeSet<String>();
      p.mkdir("primary:Tree/roms");
      for (int i = 0; i < 300; ++i)
      {
         final String name = String.format("game%03d.sfc", i);
         p.file("primary:Tree/roms/" + name, i);
         want.add(name);
      }
      final String odd = "caf\u00e9 \ud83c\udfae \u0000x";
      p.file("primary:Tree/roms/" + odd, 1);
      p.mkdir("primary:Tree/roms/sub");
      want.add(odd);
      want.add("sub");

      final VfsImplementationSaf.SafDirectory d = new VfsImplementationSaf.SafDirectory(c, TREE, "/roms/");
      final java.util.List<String> names = new java.util.ArrayList<String>();
      final java.util.List<Boolean> dirs = new java.util.ArrayList<Boolean>();
      final int first = decodeBatch(d.readdirBatch(256), names, dirs);
      final int second = decodeBatch(d.readdirBatch(256), names, dirs);
      final byte[] third = d.readdirBatch(256);
      d.close();
      check(first == 256 && second == 46 && third.length == 0, "readdir batches are full until the last, then empty");
      check(new java.util.TreeSet<String>(names).equals(want) && names.size() == want.size(), "readdir names every child once, in modified UTF-8");
      check(dirs.get(names.indexOf("sub")) && !dirs.get(names.indexOf("game000.sfc")), "readdir flags directories");

      boolean modifiedUtf8 = false;
      final byte[] one = new VfsImplementationSaf.SafDirectory(c, TREE, "/roms").readdirBatch(512);
      final byte[] nul = { (byte)0xc0, (byte)0x80 };
      for (int i = 0; i + 1 < one.length; ++i)
         if (one[i] == nul[0] && one[i + 1] == nul[1])
            modifiedUtf8 = true;
      check(modifiedUtf8, "a NUL in a name is C0 80, as GetStringUTFChars has it");

      final byte[] empty = new VfsImplementationSaf.SafDirectory(c, TREE, "/missing").readdirBatch(256);
      check(empty.length == 0, "readdir of a missing directory is empty");
   }

   public static void main(String[] args)
   {
      mkdirRenamedByProvider();
      mkdirLosesRace();
      openLosesRace();
      openCreates();
      openRenamedByProvider();
      deleteKeepsExisting();
      pathsStayInsideTheTree();
      statInOneQuery();
      openExistingIsOneCall();
      readdirBatches();

      if (failures != 0)
      {
         System.out.println(failures + " SAF test(s) failed");
         System.exit(1);
      }
      System.out.println("All SAF tests passed.");
   }
}
