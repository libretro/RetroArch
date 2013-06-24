#include "core_info.h"
#include "general.h"
#include <math.h>
#include <dirent.h>

core_info_list_t *get_core_info_list()
{
   DIR *dirp;
   struct dirent* direntp;
   int count=0, i=0;
   core_info_t *core_info;
   core_info_list_t *core_info_list;

   if(!*g_settings.libretro)
      return NULL;

   dirp = opendir(g_settings.libretro);
   if( dirp == NULL )
      return NULL;

   //Count number of cores
   for(;;)
   {
      direntp = readdir( dirp );
      if( direntp == NULL ) break;
      count++;
   }
   rewinddir(dirp);

   if(count == 2)
   {
      //Only . and ..
      closedir(dirp);
      return NULL;
   }

   core_info = (core_info_t*)malloc(count*sizeof(core_info_t));
   core_info_list = malloc(sizeof(core_info_list_t));
   core_info_list->list = core_info;
   count = 0;

   for(;;)
   {
      direntp = readdir( dirp );
      if( direntp == NULL ) break;
      if(strcmp((char*)direntp->d_name, ".")==0 || strcmp((char*)direntp->d_name, "..")==0)
         continue;
      core_info[count++].path = strdup((char*)direntp->d_name);
   }

   core_info_list->count = count;

   for(i=0;i<count;++i)
   {
      char info_path[255];
      snprintf(info_path, sizeof(info_path), "app/native/modules/");
      strncat(info_path, core_info[i].path, sizeof(info_path)-strlen(info_path)-1);
      char * substr = strrchr(info_path, '_');
      if(substr)
      {
         info_path[strlen(info_path) - strlen(substr)] = '\0';
         strncat(info_path, ".info", sizeof(info_path)-strlen(info_path)-1);
         core_info[i].data = config_file_new(info_path);

         if(core_info[i].data)
         {
            config_get_string(core_info[i].data, "display_name", &core_info[i].displayName);
            config_get_string(core_info[i].data, "supported_extensions", &core_info[i].supportedExtensions);
         } else {
            core_info[i].displayName = "Not Supported";
         }
      }
   }

   closedir(dirp);

   return core_info_list;
}

void free_core_info_list(core_info_list_t * core_info_list)
{
   int i;
   for(i=0;i<core_info_list->count;++i)
   {
      free(core_info_list->list[i].path);
      free(core_info_list->list[i].displayName);
      free(core_info_list->list[i].supportedExtensions);
      config_file_free(core_info_list->list[i].data);
   }
   free(core_info_list->list);
   free(core_info_list);
}
