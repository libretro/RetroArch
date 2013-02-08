#include <sys/stat.h>
#include <dirent.h>
#include <stddef.h>

#include "general.h"
#include "dirent_list.h"

static int compare_dirent(const void *left, const void *right)
{
   const struct dirent* l = (const struct dirent*) left;
   const struct dirent* r = (const struct dirent*) right;
   
   // Directories first
   if (l->d_type != r->d_type)
      return (l->d_type) ? -1 : 1;
   
   // Name
   return strcmp(l->d_name, r->d_name);
}

static bool is_dirent_verboten(const struct dirent* entry)
{
   if (!entry) return true;
   if (strcmp(entry->d_name, ".") == 0) return true;
   if (strcmp(entry->d_name, "..") == 0) return true;
   return false;
}

struct dirent_list* build_dirent_list(const char* path)
{
   struct dirent_list* result = 0;

   DIR* dir = opendir(path);
   if (dir)
   {
      struct dirent* ent = 0;

      // Count the number of items
      size_t count = 0;
      while ((ent = readdir(dir)))
      {
         count += is_dirent_verboten(ent) ? 0 : 1;
      }
      rewinddir(dir);

      // String buffer for 'stat'ing
      char* stat_path = malloc(strlen(path) + sizeof(ent->d_name));
      strcpy(stat_path, path);
      uint32_t last_index = strlen(stat_path);
     
      // Build and fill the result
      result = malloc(sizeof(struct dirent_list));
      result->count = count;
      result->entries = malloc(sizeof(struct dirent) * count);
      
      size_t index = 0;
      while ((ent = readdir(dir)))
      {
         if (is_dirent_verboten(ent)) continue;
         memcpy(&result->entries[index], ent, sizeof(struct dirent));
         
         // Chage dirent.d_type to a boolean indication if it is a directory
         struct stat stat_buf;
         strcat(strcat(stat_path, "/"), ent->d_name);
         stat(stat_path, &stat_buf);
         result->entries[index].d_type = S_ISDIR(stat_buf.st_mode) ? 1 : 0;
         stat_path[last_index] = 0;
         
         index ++;
      }
            
      closedir(dir);
      free(stat_path);

      qsort(result->entries, result->count, sizeof(struct dirent), &compare_dirent);
   }
   
   return result;
}

void free_dirent_list(struct dirent_list* list)
{
   if (list) free(list->entries);
   free(list);
}

const struct dirent* get_dirent_at_index(struct dirent_list* list, unsigned index)
{
   if (!list) return 0;
   return (index < list->count) ? &list->entries[index] : 0;
}

unsigned get_dirent_list_count(struct dirent_list* list)
{
   return list ? list->count : 0;
}

