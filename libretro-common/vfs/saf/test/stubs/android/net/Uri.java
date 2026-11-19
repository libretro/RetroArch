package android.net;

import java.io.ByteArrayOutputStream;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/* The part of android.net.Uri the SAF VFS uses, for content:// URIs only */
public final class Uri
{
   private final String string;
   private final String authority;
   private final List<String> segments;

   private Uri(String string)
   {
      this.string = string;
      final String rest = string.substring(string.indexOf("://") + 3);
      final int slash = rest.indexOf('/');
      authority = slash < 0 ? rest : rest.substring(0, slash);
      final List<String> list = new ArrayList<String>();
      if (slash >= 0)
         for (String s : rest.substring(slash + 1).split("/"))
            if (!s.isEmpty())
               list.add(decode(s));
      segments = Collections.unmodifiableList(list);
   }

   public static Uri parse(String s)
   {
      return new Uri(s);
   }

   public String getAuthority()
   {
      return authority;
   }

   public List<String> getPathSegments()
   {
      return segments;
   }

   public static String encode(String s)
   {
      final StringBuilder b = new StringBuilder();
      for (byte c : s.getBytes(StandardCharsets.UTF_8))
      {
         if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')
               || c == '_' || c == '-' || c == '!' || c == '.' || c == '~' || c == '\'' || c == '(' || c == ')' || c == '*')
            b.append((char)c);
         else
            b.append(String.format("%%%02X", c & 0xff));
      }
      return b.toString();
   }

   public static String decode(String s)
   {
      final ByteArrayOutputStream out = new ByteArrayOutputStream();
      for (int i = 0; i < s.length(); ++i)
      {
         final char c = s.charAt(i);
         if (c == '%' && i + 2 < s.length())
         {
            out.write(Integer.parseInt(s.substring(i + 1, i + 3), 16));
            i += 2;
         }
         else
         {
            final byte[] b = String.valueOf(c).getBytes(StandardCharsets.UTF_8);
            out.write(b, 0, b.length);
         }
      }
      return new String(out.toByteArray(), StandardCharsets.UTF_8);
   }

   @Override
   public boolean equals(Object o)
   {
      return o instanceof Uri && ((Uri)o).string.equals(string);
   }

   @Override
   public int hashCode()
   {
      return string.hashCode();
   }

   @Override
   public String toString()
   {
      return string;
   }
}
