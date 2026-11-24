/* Copyright  (C) 2010-2020 The RetroArch team
*
* ---------------------------------------------------------------------------------------
* The following license statement only applies to this file (VfsImplementationSaf.java).
* ---------------------------------------------------------------------------------------
*
* Permission is hereby granted, free of charge,
* to any person obtaining a copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation the rights to
* use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
* and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
* INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
* WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

package com.libretro.common.vfs;

import android.content.ContentResolver;
import android.database.Cursor;
import android.net.Uri;
import android.os.Build;
import android.provider.DocumentsContract;
import android.provider.DocumentsContract.Document;

import java.io.Closeable;
import java.io.FileNotFoundException;
import java.util.Arrays;
import java.util.List;

public final class VfsImplementationSaf
{
   private static final String[] QUERY_ARGS_MIME_TYPE = {
      Document.COLUMN_MIME_TYPE,
   };

   private static final String[] QUERY_ARGS_DISPLAY_NAME = {
      Document.COLUMN_DISPLAY_NAME,
   };

   /**
    * A tree URI taken apart once. Nearly every call names the same tree, so the last one is kept;
    * the holder is immutable, so threads only ever see a whole one.
    */
   private static final class Tree
   {
      final String string;
      final Uri uri;
      final boolean isDocument;
      final String documentId;

      Tree(String string)
      {
         this.string = string;
         uri = Uri.parse(string);
         final List<String> segments = uri.getPathSegments();
         isDocument = (segments.size() >= 2 && segments.get(0).equals("document"))
            || (segments.size() >= 4 && segments.get(0).equals("tree") && segments.get(2).equals("document"));
         documentId = segments.size() >= 2 && segments.get(0).equals("tree") ? segments.get(1) : null;
      }

      /**
       * The document URI of a canonical path inside this tree, or null if the tree has no such path.
       */
      Uri documentUri(String path)
      {
         if (isDocument)
            return path.length() == 1 ? uri : null;
         return documentId == null ? null : DocumentsContract.buildDocumentUriUsingTree(uri, path.length() == 1 ? documentId : documentId.concat(path));
      }
   }

   private static volatile Tree lastTree;

   private static Tree getTree(String string) {
      Tree tree = lastTree;
      if (tree == null || !tree.string.equals(string))
         lastTree = tree = new Tree(string);
      return tree;
   }

   /**
    * Whether or not a path is already in the form normalizePath returns.
    */
   private static boolean isCanonical(String path) {
      final int length = path.length();
      if (length == 0 || path.charAt(0) != '/')
         return false;
      if (length == 1)
         return true;
      if (path.charAt(length - 1) == '/')
         return false;
      for (int i = 0; i < length; ++i)
      {
         if (path.charAt(i) != '/')
            continue;
         // A component that is empty, "." or ".."
         final int end = path.indexOf('/', i + 1);
         final int componentLength = (end < 0 ? length : end) - i - 1;
         if (componentLength == 0
               || (componentLength == 1 && path.charAt(i + 1) == '.')
               || (componentLength == 2 && path.charAt(i + 1) == '.' && path.charAt(i + 2) == '.'))
            return false;
      }
      return true;
   }

   /**
    * The path relative to the root of the tree as "/a/b": empty and "." components dropped, ".." resolved, never above the root.
    * Only the string is looked at: a path inside a tree has nothing to do with the device's own filesystem.
    */
   private static String normalizePath(String path) {
      if (isCanonical(path))
         return path;
      final String[] components = path.split("/");
      int count = 0;
      for (String component : components)
      {
         if (component.isEmpty() || component.equals("."))
            continue;
         if (component.equals(".."))
         {
            if (count > 0)
               --count;
            continue;
         }
         components[count++] = component;
      }
      if (count == 0)
         return "/";
      final StringBuilder builder = new StringBuilder(path.length() + 1);
      for (int i = 0; i < count; ++i)
         builder.append('/').append(components[i]);
      return builder.toString();
   }

   /**
    * The parent of a canonical path, or null for the root.
    */
   private static String getPathParent(String path) {
      final int slash = path.lastIndexOf('/');
      if (path.length() == 1)
         return null;
      return slash == 0 ? "/" : path.substring(0, slash);
   }

   /**
    * The last component of a canonical path, empty for the root.
    */
   private static String getPathFileName(String path) {
      return path.substring(path.lastIndexOf('/') + 1);
   }

   /**
    * Open a Storage Access Framework file, returning its file descriptor if successful or -1 if not.
    * The file is not guaranteed to be seeked to any particular position, so it may be a good idea to seek it immediately after opening.
    * @param content the content resolver returned by getContentResolver()
    * @param tree the URI returned by the ACTION_OPEN_DOCUMENT_TREE intent action
    * @param path path of the file to open, relative to the root directory of the tree
    * @param read whether or not to open the file with read permissions
    * @param write whether or not to open the file with write permissions
    * @param truncate if the file is opened with write permissions, whether or not to delete the contents of the file after opening it
    */
   public static int openSafFile(ContentResolver content, String tree, String path, boolean read, boolean write, boolean truncate)
   {
      if (Build.VERSION.SDK_INT < 21)
         return -1;
      final Tree t = getTree(tree);
      path = normalizePath(path);
      final Uri fileUri = t.documentUri(path);
      if (fileUri == null)
         return -1;
      final String mode;
      if (!write)
         mode = "r";
      else if (!read)
         mode = truncate ? "wt" : "wa";
      else
         mode = truncate ? "rwt" : "rw";
      boolean createdFile = false;
      while (true)
      {
         try
         {
            return content.openFileDescriptor(fileUri, mode).detachFd();
         }
         catch (FileNotFoundException | IllegalArgumentException e)
         {
            if (createdFile || !write || !truncate || t.isDocument)
               return -1;
            createdFile = true;
            final String parentPath = getPathParent(path);
            if (parentPath == null)
               return -1;
            // A create that did not land under this name may have lost to another creator of it, so the open is tried once more either way
            createDocumentExact(content, t.documentUri(parentPath), "application/octet-stream", getPathFileName(path));
         }
      }
   }

   /**
    * Delete a Storage Access Framework file or directory, returning whether or not the operation succeeded.
    * @param content the content resolver returned by getContentResolver()
    * @param tree the URI returned by the ACTION_OPEN_DOCUMENT_TREE intent action
    * @param path path of the file or directory to delete, relative to the root directory of the tree
    */
   public static boolean removeSafFile(ContentResolver content, String tree, String path)
   {
      if (Build.VERSION.SDK_INT < 21)
         return false;
      final Uri fileUri = getTree(tree).documentUri(normalizePath(path));
      if (fileUri == null)
         return false;
      try
      {
         DocumentsContract.deleteDocument(content, fileUri);
      }
      catch (FileNotFoundException | IllegalArgumentException e)
      {
         return false;
      }
      return true;
   }

   /**
    * Get the MIME type of a document, or null if it could not be queried.
    */
   private static String queryMimeType(ContentResolver content, Uri fileUri) {
      final Cursor cursor;
      try
      {
         cursor = content.query(fileUri, QUERY_ARGS_MIME_TYPE, null, null, null);
      }
      catch (IllegalArgumentException e)
      {
         return null;
      }

      if (cursor == null)
         return null;

      try
      {
         return cursor.moveToNext() ? cursor.getString(0) : null;
      }
      finally
      {
         cursor.close();
      }
   }

   private static String getDisplayName(ContentResolver content, Uri fileUri) {
      final Cursor cursor;
      try
      {
         cursor = content.query(fileUri, QUERY_ARGS_DISPLAY_NAME, null, null, null);
      }
      catch (RuntimeException e)
      {
         return null;
      }

      if (cursor == null)
         return null;

      try
      {
         return cursor.moveToNext() ? cursor.getString(0) : null;
      }
      finally
      {
         cursor.close();
      }
   }

   /**
    * Whether or not a document is named exactly name.
    * Every query is a Binder call into the document provider, so one is only made
    * if the name cannot be read from the end of a path-style document ID.
    */
   private static boolean hasName(ContentResolver content, Uri fileUri, String name) {
      if (idHasName(DocumentsContract.getDocumentId(fileUri), name))
         return true;

      return name.equals(getDisplayName(content, fileUri));
   }

   /**
    * Whether or not a path-style document ID ends in the component name.
    * False does not mean the name differs: opaque IDs say nothing about it.
    */
   private static boolean idHasName(String id, String name) {
      final int separator = id.length() - name.length() - 1;
      return separator >= 0 && id.endsWith(name) && (id.charAt(separator) == '/' || id.charAt(separator) == ':');
   }

   /**
    * Rename a document, returning its URI if it ended up named exactly name or null if not.
    * Some providers pick a different name (such as "name (1)") instead of failing when the name is taken,
    * so on a mismatch the document is renamed back to originalName.
    */
   private static Uri renameDocumentExact(ContentResolver content, Uri fileUri, String name, String originalName) {
      final Uri renamedUri;
      try
      {
         renamedUri = DocumentsContract.renameDocument(content, fileUri, name);
      }
      catch (FileNotFoundException | RuntimeException e)
      {
         return null;
      }

      // Returning null is how DocumentsContract reports a failure: a rename that keeps the ID returns fileUri
      if (renamedUri == null)
         return null;

      if (hasName(content, renamedUri, name))
         return renamedUri;

      try
      {
         DocumentsContract.renameDocument(content, renamedUri, originalName);
      }
      catch (FileNotFoundException | RuntimeException e)
      {}

      return null;
   }

   private static final String[] QUERY_ARGS_CREATED = {
      Document.COLUMN_DISPLAY_NAME,
      Document.COLUMN_MIME_TYPE,
      Document.COLUMN_SIZE,
   };

   private static final String[] QUERY_ARGS_DOCUMENT_ID = {
      Document.COLUMN_DOCUMENT_ID,
   };

   /**
    * Whether or not a directory has no children, false if it could not be listed.
    */
   private static boolean isEmptyDirectory(ContentResolver content, Uri directoryUri) {
      final Cursor cursor;
      try
      {
         cursor = content.query(DocumentsContract.buildChildDocumentsUriUsingTree(directoryUri, DocumentsContract.getDocumentId(directoryUri)), QUERY_ARGS_DOCUMENT_ID, null, null, null);
      }
      catch (RuntimeException e)
      {
         return false;
      }

      if (cursor == null)
         return false;

      try
      {
         return !cursor.moveToNext();
      }
      finally
      {
         cursor.close();
      }
   }

   /**
    * Create a document, returning its URI if it was created named exactly name or null if not.
    * Some providers change the name (such as "name (1)", or '_' for ':') instead of failing,
    * so a document created under another name is deleted, but only while it is still what a create leaves:
    * an empty directory or an empty file. A provider that hands back an existing document keeps it.
    */
   private static Uri createDocumentExact(ContentResolver content, Uri parentUri, String mimeType, String name) {
      final Uri createdUri;
      try
      {
         createdUri = DocumentsContract.createDocument(content, parentUri, mimeType, name);
         if (createdUri == null)
            return null;
         if (idHasName(DocumentsContract.getDocumentId(createdUri), name))
            return createdUri;
      }
      catch (FileNotFoundException | RuntimeException e)
      {
         return null;
      }

      // One query answers both whether the name is right after all and whether the document is safe to delete
      final Cursor cursor;
      try
      {
         cursor = content.query(createdUri, QUERY_ARGS_CREATED, null, null, null);
      }
      catch (RuntimeException e)
      {
         return null;
      }

      if (cursor == null)
         return null;

      final boolean fresh;
      try
      {
         if (!cursor.moveToNext())
            return null;
         if (name.equals(cursor.getString(0)))
            return createdUri;
         if (Document.MIME_TYPE_DIR.equals(cursor.getString(1)))
            fresh = isEmptyDirectory(content, createdUri);
         else
            fresh = !cursor.isNull(2) && cursor.getLong(2) == 0;
      }
      finally
      {
         cursor.close();
      }

      if (fresh)
      {
         try
         {
            DocumentsContract.deleteDocument(content, createdUri);
         }
         catch (FileNotFoundException | RuntimeException e)
         {}
      }

      return null;
   }

   /**
    * Move a document to another directory, returning its URI if it ended up there named exactly name or null if not.
    * A null URI from DocumentsContract means the move failed.
    */
   private static Uri moveDocumentExact(ContentResolver content, Uri fileUri, Uri fromUri, Uri toUri, String name) {
      if (Build.VERSION.SDK_INT < 24)
         return null;

      final Uri movedUri;
      try
      {
         movedUri = DocumentsContract.moveDocument(content, fileUri, fromUri, toUri);
      }
      catch (FileNotFoundException | RuntimeException e)
      {
         return null;
      }

      if (movedUri == null)
         return null;

      if (hasName(content, movedUri, name))
         return movedUri;

      try
      {
         DocumentsContract.moveDocument(content, movedUri, toUri, fromUri);
      }
      catch (FileNotFoundException | RuntimeException e)
      {}

      return null;
   }

   /**
    * Move a document to another directory under a new name, returning whether or not it got there.
    * A provider can only move a document under the name it already has, so this takes a move and a rename.
    * The name is changed first, unless newName is taken in the source directory; on failure the document is put back.
    */
   private static boolean moveAndRenameDocument(ContentResolver content, Uri fileUri, Uri fromUri, Uri toUri, String oldName, String newName) {
      if (oldName.equals(newName))
         return moveDocumentExact(content, fileUri, fromUri, toUri, newName) != null;

      final Uri renamedUri = renameDocumentExact(content, fileUri, newName, oldName);
      if (renamedUri != null)
      {
         if (moveDocumentExact(content, renamedUri, fromUri, toUri, newName) != null)
            return true;

         renameDocumentExact(content, renamedUri, oldName, newName);
         return false;
      }

      final Uri movedUri = moveDocumentExact(content, fileUri, fromUri, toUri, oldName);
      if (movedUri == null)
         return false;

      if (renameDocumentExact(content, movedUri, newName, oldName) != null)
         return true;

      moveDocumentExact(content, movedUri, toUri, fromUri, oldName);
      return false;
   }

   /**
    * Rename a Storage Access Framework file, replacing the destination if it exists, returning whether or not the operation succeeded.
    * Moving between directories needs API level 24 and a provider that supports moves.
    * Storage Access Framework has no atomic replace, so an existing destination is first renamed to the destination path with ".old" appended,
    * and is only deleted once the file has been renamed into its place; if that fails, the destination is restored.
    * Each provider call below is a Binder transaction, by far the largest cost here: replacing a file takes four and creating one takes two, plus one for a move or two for a move under a new name in place of the rename.
    * @param content the content resolver returned by getContentResolver()
    * @param tree the URI returned by the ACTION_OPEN_DOCUMENT_TREE intent action
    * @param oldPath path of the file to rename, relative to the root directory of the tree
    * @param newPath path to rename the file to, relative to the root directory of the tree
    */
   public static boolean renameSafFile(ContentResolver content, String tree, String oldPath, String newPath)
   {
      if (Build.VERSION.SDK_INT < 21)
         return false;

      final Tree t = getTree(tree);

      if (t.isDocument || t.documentId == null)
         return false;

      oldPath = normalizePath(oldPath);
      newPath = normalizePath(newPath);

      if (oldPath.equals(newPath))
         return queryMimeType(content, t.documentUri(oldPath)) != null;

      // Both paths are canonical now, so they can be taken apart as strings
      final int oldSlash = oldPath.lastIndexOf('/');
      final int newSlash = newPath.lastIndexOf('/');

      if (oldSlash + 1 == oldPath.length() || newSlash + 1 == newPath.length())
         return false;

      final boolean sameDirectory = oldSlash == newSlash && oldPath.regionMatches(0, newPath, 0, oldSlash);

      if (!sameDirectory && Build.VERSION.SDK_INT < 24)
         return false;

      final String oldName = oldPath.substring(oldSlash + 1);
      final String newName = newPath.substring(newSlash + 1);
      final Uri newUri = t.documentUri(newPath);
      String asideName = null;
      Uri asideUri = null;

      final String newMimeType = queryMimeType(content, newUri);
      if (newMimeType != null)
      {
         if (newMimeType.equals(Document.MIME_TYPE_DIR))
            return false;

         // The source may itself be called "<destination>.old", and must not be mistaken for a leftover
         asideName = newPath.concat(".old").equals(oldPath) ? newName.concat(".old2") : newName.concat(".old");

         asideUri = renameDocumentExact(content, newUri, asideName, newName);
         if (asideUri == null)
         {
            // Most likely a leftover from an earlier rename that could not clean up, which is rare enough to only look for now.
            // Deleting a directory would delete everything in it, so anything but a file stays where it is.
            final Uri leftoverUri = t.documentUri(newPath.substring(0, newSlash + 1).concat(asideName));
            final String leftoverMimeType = queryMimeType(content, leftoverUri);

            if (leftoverMimeType == null || leftoverMimeType.equals(Document.MIME_TYPE_DIR))
               return false;

            try
            {
               DocumentsContract.deleteDocument(content, leftoverUri);
            }
            catch (FileNotFoundException | RuntimeException e)
            {
               return false;
            }

            asideUri = renameDocumentExact(content, newUri, asideName, newName);
            if (asideUri == null)
               return false;
         }
      }

      final Uri oldUri = t.documentUri(oldPath);
      final boolean placed;

      if (sameDirectory)
         placed = renameDocumentExact(content, oldUri, newName, oldName) != null;
      else
         placed = moveAndRenameDocument(content, oldUri,
               t.documentUri(oldSlash == 0 ? "/" : oldPath.substring(0, oldSlash)),
               t.documentUri(newSlash == 0 ? "/" : newPath.substring(0, newSlash)),
               oldName, newName);

      if (placed)
      {
         if (asideUri != null)
         {
            // The file is in place; a failed cleanup only leaves a stale ".old" for the next rename to remove
            try
            {
               DocumentsContract.deleteDocument(content, asideUri);
            }
            catch (FileNotFoundException | RuntimeException e)
            {}
         }
         return true;
      }

      if (asideUri != null)
         renameDocumentExact(content, asideUri, newName, asideName);

      return false;
   }

   private static final String[] QUERY_ARGS_STAT_WITH_SIZE = {
      Document.COLUMN_MIME_TYPE,
      Document.COLUMN_SIZE,
   };

   /**
    * Stat a Storage Access Framework file or directory in one call: Long.MIN_VALUE if it could not be queried,
    * otherwise its size shifted left by one with the low bit set for a directory.
    * The size is 0 if includeSize is false or the provider does not know it.
    * @param content the content resolver returned by getContentResolver()
    * @param tree the URI returned by the ACTION_OPEN_DOCUMENT_TREE intent action
    * @param path path of the file or directory, relative to the root directory of the tree
    * @param includeSize whether or not to query the size
    */
   public static long statSaf(ContentResolver content, String tree, String path, boolean includeSize)
   {
      if (Build.VERSION.SDK_INT < 21)
         return Long.MIN_VALUE;
      final Uri fileUri = getTree(tree).documentUri(normalizePath(path));
      if (fileUri == null)
         return Long.MIN_VALUE;
      final Cursor cursor;
      try
      {
         cursor = content.query(fileUri, includeSize ? QUERY_ARGS_STAT_WITH_SIZE : QUERY_ARGS_MIME_TYPE, null, null, null);
      }
      catch (IllegalArgumentException e)
      {
         return Long.MIN_VALUE;
      }
      if (cursor == null)
         return Long.MIN_VALUE;
      try
      {
         if (!cursor.moveToNext())
            return Long.MIN_VALUE;
         final long size = includeSize ? Math.max(cursor.getLong(1), 0) : 0;
         return (size << 1) | (Document.MIME_TYPE_DIR.equals(cursor.getString(0)) ? 1 : 0);
      }
      finally
      {
         cursor.close();
      }
   }

   /**
    * Create a Storage Access Framework directory, returning 0 if it succeeded, -1 if it failed or -2 if the directory already exists.
    * @param content the content resolver returned by getContentResolver()
    * @param tree the URI returned by the ACTION_OPEN_DOCUMENT_TREE intent action
    * @param path path of the directory to create, relative to the root directory of the tree
    */
   public static int mkdirSaf(ContentResolver content, String tree, String path)
   {
      if (Build.VERSION.SDK_INT < 21)
         return -1;
      final Tree t = getTree(tree);
      if (t.isDocument)
         return -1;
      path = normalizePath(path);
      final Uri directoryUri = t.documentUri(path);
      if (directoryUri == null)
         return -1;
      String mimeType = queryMimeType(content, directoryUri);
      if (mimeType != null)
         return mimeType.equals(Document.MIME_TYPE_DIR) ? -2 : -1;
      final String parentPath = getPathParent(path);
      if (parentPath == null)
         return -1;
      if (createDocumentExact(content, t.documentUri(parentPath), Document.MIME_TYPE_DIR, getPathFileName(path)) != null)
         return 0;
      // Another creator may have made it between the query and the create
      mimeType = queryMimeType(content, directoryUri);
      return Document.MIME_TYPE_DIR.equals(mimeType) ? -2 : -1;
   }

   public static class SafDirectory implements Closeable
   {
      private static final String[] QUERY_ARGS = {
         Document.COLUMN_DOCUMENT_ID,
         Document.COLUMN_MIME_TYPE,
      };

      private int prefixLength;
      private Cursor cursor = null;
      private byte[] scratch = new byte[0];

      /**
       * Open a Storage Access Framework directory to list its contents.
       * @param content the content resolver returned by getContentResolver()
       * @param tree the URI returned by the ACTION_OPEN_DOCUMENT_TREE intent action
       * @param path path of the directory to open, relative to the root directory of the tree
       */
      public SafDirectory(ContentResolver content, String tree, String path)
      {
         if (Build.VERSION.SDK_INT < 21)
            return;
         final Tree t = getTree(tree);
         if (t.isDocument || t.documentId == null)
            return;
         path = normalizePath(path);
         final String documentId = path.length() == 1 ? t.documentId : t.documentId.concat(path);
         prefixLength = documentId.length();
         try
         {
            cursor = content.query(DocumentsContract.buildChildDocumentsUriUsingTree(t.uri, documentId), QUERY_ARGS, null, null, null);
         }
         catch (IllegalArgumentException e)
         {}
      }

      @Override
      public void close()
      {
         if (cursor != null)
         {
            cursor.close();
            cursor = null;
         }
      }

      /**
       * Drain up to max children into one byte array, so the native side copies a whole batch in one JNI call:
       * per child, 1 for a directory or 0 for a file, its name in modified UTF-8 (the encoding GetStringUTFChars
       * produces, which never contains a zero byte) and a terminating 0. Fewer than max children means the
       * directory is exhausted, and an empty array that there are none left.
       */
      public byte[] readdirBatch(int max)
      {
         int length = 0;
         int n = 0;

         if (cursor == null)
            return new byte[0];

         while (n < max && cursor.moveToNext())
         {
            // The name is the document ID past the directory's own, without slashes on either end
            final String id = cursor.getString(0);
            int start = prefixLength;
            int end = id.length();
            while (start < end && id.charAt(start) == '/')
               ++start;
            while (end > start && id.charAt(end - 1) == '/')
               --end;

            // Each UTF-16 unit takes at most three bytes, plus the flag and the terminator
            final int need = length + 3 * (end - start) + 2;
            if (need > scratch.length)
               scratch = Arrays.copyOf(scratch, Math.max(need, 2 * scratch.length));

            scratch[length++] = (byte)(Document.MIME_TYPE_DIR.equals(cursor.getString(1)) ? 1 : 0);
            for (int i = start; i < end; ++i)
            {
               final char c = id.charAt(i);
               if (c != 0 && c < 0x80)
                  scratch[length++] = (byte)c;
               else if (c < 0x800)
               {
                  scratch[length++] = (byte)(0xc0 | (c >> 6));
                  scratch[length++] = (byte)(0x80 | (c & 0x3f));
               }
               else
               {
                  scratch[length++] = (byte)(0xe0 | (c >> 12));
                  scratch[length++] = (byte)(0x80 | ((c >> 6) & 0x3f));
                  scratch[length++] = (byte)(0x80 | (c & 0x3f));
               }
            }
            scratch[length++] = 0;
            ++n;
         }

         if (n < max)
            close();

         return Arrays.copyOf(scratch, length);
      }
   }
}
