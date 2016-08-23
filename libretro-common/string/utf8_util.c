#include <string/utf8_util.h>

uint32_t utf8_strlen(const char *s)
{
   int i = 0, j = 0;
   while (s[i]) {
     if ((s[i] & 0xc0) != 0x80) j++;
     i++;
   }
   return j;
}

uint8_t utf8_walkbyte(const char **string)
{
   return *((*string)++);
}

/* Does not validate the input, returns garbage if it's not UTF-8. */
uint32_t utf8_walk(const char **string)
{
   uint8_t first = utf8_walkbyte(string);
   uint32_t ret;
   
   if (first<128)
      return first;
   
   ret = 0;
   ret = (ret<<6) | (utf8_walkbyte(string)    & 0x3F);
   if (first >= 0xE0)
      ret = (ret<<6) | (utf8_walkbyte(string) & 0x3F);
   if (first >= 0xF0)
      ret = (ret<<6) | (utf8_walkbyte(string) & 0x3F);
   
   if (first >= 0xF0)
      return ret | (first&31)<<18;
   if (first >= 0xE0)
      return ret | (first&15)<<12;
   return ret | (first&7)<<6;
}
