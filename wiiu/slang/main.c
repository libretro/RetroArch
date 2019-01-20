
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <ctype.h>
#include <string.h>

int main(int argc, const char** argv)
{

   const char* slang = NULL;
   const char* vs_asm = NULL;
   const char* ps_asm = NULL;
   const char* vs_out = NULL;
   const char* ps_out = NULL;

   for(int i = 1; i < argc - 1; i+=2)
   {
      if(!strcmp(argv[i], "--slang"))
         slang = argv[i + 1];
      else if(!strcmp(argv[i], "--vsource"))
         vs_asm = argv[i + 1];
      else if(!strcmp(argv[i], "--psource"))
         ps_asm = argv[i + 1];
      else if(!strcmp(argv[i], "--vsh"))
         vs_out = argv[i + 1];
      else if(!strcmp(argv[i], "--psh"))
         ps_out = argv[i + 1];
   }

   if(!slang || !vs_out || !ps_out || (!vs_asm && ps_asm) || (vs_asm && !ps_asm))
   {
      printf("Usage :\n");
      printf("%s --slang <slang input> --vsh <vsh output> --psh <psh output>\n", argv[0]);
      printf("%s --slang <slang input> --vsource <vsh asm input> --psource <psh asm input> --vsh <vsh output> --psh <psh output>\n", argv[0]);
   }

   char* slang_buffer;
   size_t slang_filesize;
   {
      FILE* slang_file = fopen(slang, "rb");
      fseek(slang_file, 0, SEEK_END);
      slang_filesize = ftell(slang_file);
      fseek(slang_file, 0, SEEK_SET);
      slang_buffer = malloc(slang_filesize + 1);
      fread(slang_buffer, 1, slang_filesize, slang_file);
      fclose(slang_file);
   }

   slang_buffer[slang_filesize] = '\0';

   FILE* vs_out_fp = fopen(vs_out, "wb");
   FILE* ps_out_fp = fopen(ps_out, "wb");

   const char* line = "#version 150\n";
   fwrite(line, 1, strlen(line),vs_out_fp);
   fwrite(line, 1, strlen(line),ps_out_fp);

   char* next = slang_buffer;

   bool vson = true;
   bool pson = true;

   while(*next)
   {
      const char* line = next;

      while(*next && *next != '\n' && *next != '\r')
         next++;

      if (*next == '\r')
         *next++ = '\0';

      *next++ = '\0';

//      while((*next == '\n') || (*next == '\r'))
//         *next++ = '\0';

      if(strstr(line, "#version"))
         continue;

      if(strstr(line, "#pragma"))
      {
         if(strstr(line, "#pragma stage vertex"))
         {
            vson = true;
            pson = false;
         }
         else if(strstr(line, "#pragma stage fragment"))
         {
            vson = false;
            pson = true;
         }

         continue;
      }

      char* layout = strstr(line, "layout(");
      if(layout)
      {
         while(*layout != ')')
            layout++;

         layout++;

         while(*layout && isspace(*layout))
            layout++;

         if(!strncmp(layout, "uniform", 7))
            line = layout;
      }

      if(vson)
      {
         fwrite(line, 1, strlen(line),vs_out_fp);
         fputc('\n', vs_out_fp);
      }

      if(pson)
      {
         fwrite(line, 1, strlen(line),ps_out_fp);
         fputc('\n', ps_out_fp);
      }

   }

   fclose(vs_out_fp);
   fclose(ps_out_fp);

   free(slang_buffer);

   return 0;
}
