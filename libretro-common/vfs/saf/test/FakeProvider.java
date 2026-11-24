import android.content.ContentResolver;
import android.database.Cursor;
import android.net.Uri;
import android.os.ParcelFileDescriptor;
import android.provider.DocumentsContract;
import android.provider.DocumentsContract.Document;

import java.io.File;
import java.io.FileDescriptor;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.RandomAccessFile;
import java.lang.reflect.Field;
import java.util.ArrayList;
import java.util.List;
import java.util.TreeMap;

/* An in-memory model of ExternalStorageProvider: path-style document IDs
 * ("primary:Tree/a/b"), creates and renames that pick a free name
 * ("name (1)", "base (1).ext") instead of failing, ':' turned into '_'.
 * Every entry point is one Binder transaction on a device and is counted. */
final class FakeProvider implements ContentResolver.Provider
{
   static final class Node
   {
      final boolean dir;
      long size;

      Node(boolean dir, long size)
      {
         this.dir = dir;
         this.size = size;
      }
   }

   final TreeMap<String, Node> docs = new TreeMap<String, Node>();
   int transactions;
   int nextFd = 100;
   /* Runs once, inside the next create, before the name is chosen */
   Runnable beforeCreate;
   /* Misbehaving provider: a create hands back an existing document of the sanitized name */
   boolean createReturnsExisting;
   /* With a directory here, files are real files in it and opens hand out real descriptors,
    * as ExternalStorageProvider's do */
   File backing;
   private final List<RandomAccessFile> lent = new ArrayList<RandomAccessFile>();

   File backingFile(String id)
   {
      return new File(backing, id.replace(':', '_').replace('/', '_'));
   }

   /* The descriptor number, owned by the caller from now on: the Java side forgets it and the
    * stream is kept reachable so nothing ever closes it behind the caller's back */
   private int lend(RandomAccessFile file) throws IOException
   {
      try
      {
         final FileDescriptor fd = file.getFD();
         final Field field = FileDescriptor.class.getDeclaredField("fd");
         field.setAccessible(true);
         final int n = field.getInt(fd);
         field.setInt(fd, -1);
         lent.add(file);
         return n;
      }
      catch (ReflectiveOperationException e)
      {
         throw new IOException(e);
      }
   }

   FakeProvider()
   {
      docs.put("primary:", new Node(true, 0));
   }

   void mkdir(String id)
   {
      docs.put(id, new Node(true, 0));
   }

   void file(String id, long size)
   {
      docs.put(id, new Node(false, size));
   }

   boolean has(String id)
   {
      return docs.containsKey(id);
   }

   static String childPrefix(String id)
   {
      return id.endsWith(":") ? id : id + "/";
   }

   static String nameOf(String id)
   {
      final int i = Math.max(id.lastIndexOf('/'), id.lastIndexOf(':'));
      return id.substring(i + 1);
   }

   static String parentOf(String id)
   {
      final int slash = id.lastIndexOf('/');
      return slash >= 0 ? id.substring(0, slash) : id.substring(0, id.indexOf(':') + 1);
   }

   List<String> children(String id)
   {
      final String prefix = childPrefix(id);
      final List<String> out = new ArrayList<String>();
      for (String k : docs.tailMap(prefix, false).keySet())
      {
         if (!k.startsWith(prefix))
            break;
         if (k.indexOf('/', prefix.length()) < 0)
            out.add(k);
      }
      return out;
   }

   private String uniqueChild(String parentId, String name, boolean dir)
   {
      final String prefix = childPrefix(parentId);
      if (!docs.containsKey(prefix + name))
         return prefix + name;
      final int dot = dir ? -1 : name.lastIndexOf('.');
      final String base = dot > 0 ? name.substring(0, dot) : name;
      final String ext = dot > 0 ? name.substring(dot) : "";
      for (int n = 1;; ++n)
         if (!docs.containsKey(prefix + base + " (" + n + ")" + ext))
            return prefix + base + " (" + n + ")" + ext;
   }

   private static Uri uriFor(Uri anyTreeUri, String id)
   {
      return DocumentsContract.buildDocumentUriUsingTree(anyTreeUri, id);
   }

   private static String idOf(Uri uri) throws FileNotFoundException
   {
      try
      {
         return DocumentsContract.getDocumentId(uri);
      }
      catch (IllegalArgumentException e)
      {
         throw new FileNotFoundException(e.getMessage());
      }
   }

   @Override
   public Cursor query(Uri uri, String[] projection)
   {
      ++transactions;
      final List<String> segments = uri.getPathSegments();
      final List<String> rows = new ArrayList<String>();
      final String id;
      try
      {
         id = idOf(uri);
      }
      catch (FileNotFoundException e)
      {
         throw new IllegalArgumentException(e.getMessage());
      }
      if (segments.get(segments.size() - 1).equals("children"))
      {
         final Node parent = docs.get(id);
         if (parent == null || !parent.dir)
            return null;
         rows.addAll(children(id));
      }
      else
      {
         /* DocumentsProvider.query logs a missing document and answers null */
         if (!docs.containsKey(id))
            return null;
         rows.add(id);
      }
      return new RowCursor(this, rows, projection);
   }

   @Override
   public ParcelFileDescriptor openFile(Uri uri, String mode) throws FileNotFoundException
   {
      ++transactions;
      final String id = idOf(uri);
      final Node node = docs.get(id);
      if (node == null || node.dir)
         throw new FileNotFoundException(uri.toString());
      if (mode.indexOf('t') >= 0)
         node.size = 0;
      if (backing == null)
         return new ParcelFileDescriptor(nextFd++);
      try
      {
         final RandomAccessFile file = new RandomAccessFile(backingFile(id), mode.indexOf('w') >= 0 ? "rw" : "r");
         if (mode.indexOf('t') >= 0)
            file.setLength(0);
         return new ParcelFileDescriptor(lend(file));
      }
      catch (IOException e)
      {
         throw new FileNotFoundException(e.getMessage());
      }
   }

   @Override
   public Uri createDocument(Uri parentUri, String mimeType, String name) throws FileNotFoundException
   {
      ++transactions;
      final String parentId = idOf(parentUri);
      final Node parent = docs.get(parentId);
      if (parent == null || !parent.dir)
         throw new FileNotFoundException(parentUri.toString());
      if (beforeCreate != null)
      {
         final Runnable r = beforeCreate;
         beforeCreate = null;
         r.run();
      }
      final boolean dir = Document.MIME_TYPE_DIR.equals(mimeType);
      final String sanitized = name.replace(':', '_');
      if (createReturnsExisting && docs.containsKey(childPrefix(parentId) + sanitized))
         return uriFor(parentUri, childPrefix(parentId) + sanitized);
      final String id = uniqueChild(parentId, sanitized, dir);
      docs.put(id, new Node(dir, 0));
      return uriFor(parentUri, id);
   }

   @Override
   public void deleteDocument(Uri uri) throws FileNotFoundException
   {
      ++transactions;
      final String id = idOf(uri);
      if (!docs.containsKey(id))
         throw new FileNotFoundException(uri.toString());
      final String prefix = childPrefix(id);
      docs.remove(id);
      while (true)
      {
         final String k = docs.ceilingKey(prefix);
         if (k == null || !k.startsWith(prefix))
            break;
         docs.remove(k);
      }
   }

   private void moveTree(String from, String to)
   {
      final TreeMap<String, Node> moved = new TreeMap<String, Node>();
      final String prefix = childPrefix(from);
      moved.put(to, docs.remove(from));
      while (true)
      {
         final String k = docs.ceilingKey(prefix);
         if (k == null || !k.startsWith(prefix))
            break;
         moved.put(childPrefix(to) + k.substring(prefix.length()), docs.remove(k));
      }
      docs.putAll(moved);
   }

   @Override
   public Uri renameDocument(Uri uri, String name) throws FileNotFoundException
   {
      ++transactions;
      final String id = idOf(uri);
      final Node node = docs.get(id);
      if (node == null)
         throw new FileNotFoundException(uri.toString());
      final String to = uniqueChild(parentOf(id), name.replace(':', '_'), node.dir);
      moveTree(id, to);
      return uriFor(uri, to);
   }

   @Override
   public Uri moveDocument(Uri uri, Uri fromUri, Uri toUri) throws FileNotFoundException
   {
      ++transactions;
      final String id = idOf(uri);
      if (!docs.containsKey(id))
         throw new FileNotFoundException(uri.toString());
      final String to = childPrefix(idOf(toUri)) + nameOf(id);
      if (docs.containsKey(to))
         throw new IllegalStateException("Already exists");
      moveTree(id, to);
      return uriFor(uri, to);
   }

   static final class RowCursor implements Cursor
   {
      private final FakeProvider provider;
      private final List<String> rows;
      private final String[] projection;
      private int pos = -1;

      RowCursor(FakeProvider provider, List<String> rows, String[] projection)
      {
         this.provider = provider;
         this.rows = rows;
         this.projection = projection;
      }

      private Object value(int column)
      {
         final String id = rows.get(pos);
         final Node node = provider.docs.get(id);
         final String c = projection[column];
         if (c.equals(Document.COLUMN_DOCUMENT_ID))
            return id;
         if (c.equals(Document.COLUMN_MIME_TYPE))
            return node.dir ? Document.MIME_TYPE_DIR : "application/octet-stream";
         if (c.equals(Document.COLUMN_DISPLAY_NAME))
            return nameOf(id);
         if (c.equals(Document.COLUMN_SIZE))
            return node.dir ? null : Long.valueOf(provider.backing != null ? provider.backingFile(id).length() : node.size);
         throw new IllegalArgumentException(c);
      }

      public int getCount() { return rows.size(); }
      public boolean moveToNext() { return ++pos < rows.size(); }
      public String getString(int column) { final Object v = value(column); return v == null ? null : v.toString(); }
      public long getLong(int column) { final Object v = value(column); return v == null ? 0 : ((Long)v).longValue(); }
      public boolean isNull(int column) { return value(column) == null; }
      public void close() {}
   }
}
