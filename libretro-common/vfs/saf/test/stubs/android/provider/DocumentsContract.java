package android.provider;

import android.content.ContentResolver;
import android.net.Uri;

import java.io.FileNotFoundException;
import java.util.List;

public final class DocumentsContract
{
   public static final class Document
   {
      public static final String COLUMN_DOCUMENT_ID = "document_id";
      public static final String COLUMN_MIME_TYPE = "mime_type";
      public static final String COLUMN_DISPLAY_NAME = "_display_name";
      public static final String COLUMN_SIZE = "_size";
      public static final String MIME_TYPE_DIR = "vnd.android.document/directory";
   }

   public static Uri buildDocumentUriUsingTree(Uri treeUri, String documentId)
   {
      return Uri.parse("content://" + treeUri.getAuthority() + "/tree/" + Uri.encode(getTreeDocumentId(treeUri)) + "/document/" + Uri.encode(documentId));
   }

   public static Uri buildChildDocumentsUriUsingTree(Uri treeUri, String parentDocumentId)
   {
      return Uri.parse("content://" + treeUri.getAuthority() + "/tree/" + Uri.encode(getTreeDocumentId(treeUri)) + "/document/" + Uri.encode(parentDocumentId) + "/children");
   }

   public static String getTreeDocumentId(Uri uri)
   {
      final List<String> s = uri.getPathSegments();
      if (s.size() >= 2 && s.get(0).equals("tree"))
         return s.get(1);
      throw new IllegalArgumentException("Invalid URI: " + uri);
   }

   public static String getDocumentId(Uri uri)
   {
      final List<String> s = uri.getPathSegments();
      if (s.size() >= 2 && s.get(0).equals("document"))
         return s.get(1);
      if (s.size() >= 4 && s.get(0).equals("tree") && s.get(2).equals("document"))
         return s.get(3);
      throw new IllegalArgumentException("Invalid URI: " + uri);
   }

   public static Uri createDocument(ContentResolver content, Uri parentDocumentUri, String mimeType, String displayName) throws FileNotFoundException
   {
      return content.provider.createDocument(parentDocumentUri, mimeType, displayName);
   }

   public static boolean deleteDocument(ContentResolver content, Uri documentUri) throws FileNotFoundException
   {
      content.provider.deleteDocument(documentUri);
      return true;
   }

   public static Uri renameDocument(ContentResolver content, Uri documentUri, String displayName) throws FileNotFoundException
   {
      return content.provider.renameDocument(documentUri, displayName);
   }

   public static Uri moveDocument(ContentResolver content, Uri sourceDocumentUri, Uri sourceParentDocumentUri, Uri targetParentDocumentUri) throws FileNotFoundException
   {
      return content.provider.moveDocument(sourceDocumentUri, sourceParentDocumentUri, targetParentDocumentUri);
   }
}
