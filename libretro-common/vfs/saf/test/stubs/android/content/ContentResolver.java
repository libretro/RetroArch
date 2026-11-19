package android.content;

import android.database.Cursor;
import android.net.Uri;
import android.os.ParcelFileDescriptor;

import java.io.FileNotFoundException;

/* Every call is one Binder transaction on a device; the test provider counts them */
public class ContentResolver
{
   public interface Provider
   {
      Cursor query(Uri uri, String[] projection);
      ParcelFileDescriptor openFile(Uri uri, String mode) throws FileNotFoundException;
      Uri createDocument(Uri parentUri, String mimeType, String name) throws FileNotFoundException;
      void deleteDocument(Uri uri) throws FileNotFoundException;
      Uri renameDocument(Uri uri, String name) throws FileNotFoundException;
      Uri moveDocument(Uri uri, Uri fromUri, Uri toUri) throws FileNotFoundException;
   }

   public final Provider provider;

   public ContentResolver(Provider provider)
   {
      this.provider = provider;
   }

   public final Cursor query(Uri uri, String[] projection, String selection, String[] selectionArgs, String sortOrder)
   {
      return provider.query(uri, projection);
   }

   public final ParcelFileDescriptor openFileDescriptor(Uri uri, String mode) throws FileNotFoundException
   {
      return provider.openFile(uri, mode);
   }
}
