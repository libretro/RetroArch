
#include <libretro-common/pathsubstition/path_substition.h>

#include <retroarch.h>
#include <lists/string_list.h>

char* get_substitute_path(const char* path)
{
   //return path;

   if (!path || !*path)
      return path;
   
   //char* resolved_path = path;
   char* resolved_path = strdup(path);

   settings_t* settings = config_get_ptr();
   if (!string_is_empty(settings->paths.path_substitution))
   {
      struct string_list* path_substs = string_split(settings->paths.path_substitution, ";");
      if (path_substs)
      {
         for (int i = 0; i < path_substs->size; i++)
         {
            struct string_list* path_replace = string_split(path_substs->elems[i].data, "->");
            if (path_replace)
            {
               if (path_replace->size == 2)
               {
                  char* destfound = strstr(resolved_path, path_replace->elems[1].data);
                  if (!destfound)
                  {
                     char* sourcefound = strstr(resolved_path, path_replace->elems[0].data);
                     if (sourcefound)
                     {
                        RARCH_LOG("[Path Substition]: REPLACED '%s' with '%s' in '%s'.\n", path_replace->elems[0].data, path_replace->elems[1].data, resolved_path);

                        char* tmp = resolved_path;
                        resolved_path = string_replace_substring(resolved_path, path_replace->elems[0].data, path_replace->elems[1].data);
                        free(tmp);

                        RARCH_LOG("[Path Substition]: Final path: '%s''.\n", resolved_path);
                     }
                  }
               }
               string_list_free(path_replace);
            }
         }
         string_list_free(path_substs);
      }
   }

   return resolved_path;
}

void substitute_path(char* path)
{
   char* resolved_path = get_substitute_path(path);
   strcpy(path, resolved_path);
   free(resolved_path);
}
