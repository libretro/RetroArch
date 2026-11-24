#include <windows.h>

unsigned shim_mbtowc_calls = 0;
unsigned shim_wctomb_calls = 0;

static int acp_next(const unsigned char *in, size_t n, size_t *i,
      wchar_t *wc)
{
   unsigned c = in[*i];
   if (c >= 0x81 && c <= 0x9F && *i + 1 < n)
   {
      *wc = (wchar_t)(0x3000 + ((c - 0x81) << 8) + in[*i + 1]);
      *i += 2;
   }
   else
   {
      *wc = (wchar_t)c;
      *i += 1;
   }
   return 1;
}

static int utf8_next(const unsigned char *in, size_t n, size_t *i,
      wchar_t *wc)
{
   unsigned c = in[*i];
   if (c < 0x80)
   {
      *wc = (wchar_t)c;
      *i += 1;
   }
   else if ((c & 0xE0) == 0xC0 && *i + 1 < n)
   {
      *wc = (wchar_t)(((c & 0x1F) << 6) | (in[*i + 1] & 0x3F));
      *i += 2;
   }
   else if (*i + 2 < n)
   {
      *wc = (wchar_t)(((c & 0x0F) << 12) | ((in[*i + 1] & 0x3F) << 6)
            | (in[*i + 2] & 0x3F));
      *i += 3;
   }
   else
      return 0;
   return 1;
}

int MultiByteToWideChar(UINT cp, DWORD flags, LPCSTR in, int in_len,
      LPWSTR out, int out_len)
{
   const unsigned char *u = (const unsigned char*)in;
   size_t n = in_len < 0 ? strlen(in) + 1 : (size_t)in_len;
   size_t i = 0;
   int count = 0;
   (void)flags;
   shim_mbtowc_calls++;
   while (i < n)
   {
      wchar_t wc;
      if (!(cp == CP_UTF8 ? utf8_next(u, n, &i, &wc)
               : acp_next(u, n, &i, &wc)))
         return 0;
      if (out_len)
      {
         if (count >= out_len)
         {
            wmemset(out, (wchar_t)0x2620, (size_t)out_len);
            return 0;
         }
         out[count] = wc;
      }
      count++;
   }
   return count;
}

int WideCharToMultiByte(UINT cp, DWORD flags, LPCWSTR in, int in_len,
      LPSTR out, int out_len, LPCSTR def, LPBOOL used_def)
{
   size_t n = in_len < 0 ? wcslen(in) + 1 : (size_t)in_len;
   size_t i;
   int count = 0;
   (void)flags;
   (void)def;
   (void)used_def;
   shim_wctomb_calls++;
   for (i = 0; i < n; i++)
   {
      unsigned c = (unsigned)in[i];
      unsigned char b[3];
      int k, m;
      if (cp != CP_UTF8)
         return 0;
      if (c < 0x80)
      {
         b[0] = (unsigned char)c;
         m    = 1;
      }
      else if (c < 0x800)
      {
         b[0] = (unsigned char)(0xC0 | (c >> 6));
         b[1] = (unsigned char)(0x80 | (c & 0x3F));
         m    = 2;
      }
      else
      {
         b[0] = (unsigned char)(0xE0 | (c >> 12));
         b[1] = (unsigned char)(0x80 | ((c >> 6) & 0x3F));
         b[2] = (unsigned char)(0x80 | (c & 0x3F));
         m    = 3;
      }
      if (out_len)
      {
         if (count + m > out_len)
         {
            memset(out, '#', (size_t)out_len);
            return 0;
         }
         for (k = 0; k < m; k++)
            out[count + k] = (char)b[k];
      }
      count += m;
   }
   return count;
}
