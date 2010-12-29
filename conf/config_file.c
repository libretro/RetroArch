#include "config_file.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

struct entry_list
{
   char *key;
   char *value;
   struct entry_list *next;
};

struct config_file
{
   struct entry_list *entries;
};

static char *getaline(FILE *file)
{
   char *newline = malloc(9);
   size_t cur_size = 8;
   size_t index = 0;

   int in = getc(file);
   while (in != EOF && in != '\n')
   {
      if (index == cur_size)
      {
         cur_size *= 2;
         newline = realloc(newline, cur_size + 1);
      }

      newline[index++] = in;
      in = getc(file);
   }
   newline[index] = '\0';
   return newline; 
}

static bool parse_line(struct entry_list *list, char *line)
{
   // Remove everything after comment.
   char *comment = strchr(line, '#');
   if (comment)
      *comment = '\0';

   // Skips to first character.
   while (isspace(*line))
      line++;

   char *key = malloc(9);
   size_t cur_size = 8;
   size_t index = 0;

   while (isgraph(*line))
   {
      if (index == cur_size)
      {
         cur_size *= 2;
         key = realloc(key, cur_size + 1);
      }

      key[index++] = *line++;
   }
   key[index] = '\0';
   list->key = key;

   while (isspace(*line))
      line++;

   // If we don't have an equal sign here, we've got an invalid string...
   if (*line != '=')
   {
      list->key = NULL;
      free(key);
      return false;
   }
   line++;

   while (isspace(*line))
      line++;

   // We have a full string. Read until next ".
   if (*line == '"')
   {
      char *tok = strtok(line + 1, "\"");
      if (tok == NULL)
      {
         list->key = NULL;
         free(key);
         return false;
      }
      list->value = strdup(tok);
   }
   else // We don't have that... Read till next space.
   {
      char *tok = strtok(line, " \t\f");
      if (tok == NULL)
      {
         list->key = NULL;
         free(key);
         return false;
      }
      list->value = strdup(tok);
   }

   return true;
}

static void print_config(config_file_t *conf)
{
   struct entry_list *tmp = conf->entries;
   while (tmp != NULL)
   {
      printf("Key: \"%s\", Value: \"%s\"\n", tmp->key, tmp->value);
      tmp = tmp->next;
   }
}

config_file_t *config_file_new(const char *path)
{
   FILE *file = fopen(path, "r");
   if (!file)
      return NULL;

   struct config_file *conf = calloc(1, sizeof(*conf));
   if (conf == NULL)
      return NULL;

   struct entry_list *tail = conf->entries;

   while (!feof(file))
   {
      struct entry_list *list = calloc(1, sizeof(*list));
      char *line = getaline(file);

      if (line)
      {
         if (parse_line(list, line))
         {
            if (conf->entries == NULL)
            {
               conf->entries = list;
               tail = list;
            }
            else
            {
               tail->next = list;
               tail = list;
            }
         }
         free(line);
      }
   }
   fclose(file);

   print_config(conf);

   return conf;
}

void config_file_free(config_file_t *conf)
{
   if (conf != NULL)
   {
      struct entry_list *tmp = conf->entries;
      struct entry_list *old = tmp;
      while (tmp != NULL)
      {
         free(tmp->key);
         free(tmp->value);
         old = tmp;
         tmp = tmp->next;
         free(old);
      }
      free(conf);
   }
}

bool config_get_double(config_file_t *conf, const char *key, double *in)
{
   struct entry_list *list = conf->entries;

   while (list != NULL)
   {
      if (strcmp(key, list->key) == 0)
      {
         *in = strtod(list->value, NULL);
         return true;
      }
      list = list->next;
   }
   return false;
}

bool config_get_int(config_file_t *conf, const char *key, int *in)
{
   struct entry_list *list = conf->entries;

   while (list != NULL)
   {
      if (strcmp(key, list->key) == 0)
      {
         *in = strtol(list->value, NULL, 0);
         return true;
      }
      list = list->next;
   }
   return false;
}

bool config_get_char(config_file_t *conf, const char *key, char *in)
{
   struct entry_list *list = conf->entries;

   while (list != NULL)
   {
      if (strcmp(key, list->key) == 0)
      {
         if (strlen(list->value) > 1)
            return false;
         *in = *list->value;
         return true;
      }
      list = list->next;
   }
   return false;
}

bool config_get_string(config_file_t *conf, const char *key, char **str)
{
   struct entry_list *list = conf->entries;

   while (list != NULL)
   {
      if (strcmp(key, list->key) == 0)
      {
         *str = strdup(list->value);
         return true;
      }
      list = list->next;
   }
   return false;
}

bool config_get_bool(config_file_t *conf, const char *key, bool *in)
{
   struct entry_list *list = conf->entries;

   while (list != NULL)
   {
      if (strcmp(key, list->key) == 0)
      {
         if (strcasecmp(list->value, "true") == 0)
            *in = true;
         else if (strcasecmp(list->value, "1") == 0)
            *in = true;
         else if (strcasecmp(list->value, "false") == 0)
            *in = false;
         else if (strcasecmp(list->value, "0") == 0)
            *in = false;
         else
            return false;

         return true;
      }
      list = list->next;
   }
   return false;
}


