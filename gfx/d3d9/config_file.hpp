/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 * 
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __CONFIG_FILE_HPP
#define __CONFIG_FILE_HPP

#include "../../conf/config_file.h"
#include <utility>
#include <cstdlib>

class ConfigFile
{
   public:
      ConfigFile(const std::string& _path = "") : path(_path)
      {
         conf = config_file_new(path.c_str());
         if (!conf)
            conf = config_file_new(nullptr);
      }

      ConfigFile(const ConfigFile&) = delete;
      void operator=(const ConfigFile&) = delete;

      operator bool() { return conf; }

      ConfigFile& operator=(ConfigFile&& _in)
      {
         if (conf)
         {
            if (path[0])
               config_file_write(conf, path.c_str());
            config_file_free(conf);
            conf = _in.conf;
            _in.conf = nullptr;
            path = _in.path;
         }
         return *this;
      }

      bool get(const std::string& key, int& out)
      {
         if (!conf) return false;
         int val;
         if (config_get_int(conf, key.c_str(), &val))
         {
            out = val;
            return true;
         }
         return false;
      }

      bool get(const std::string& key, char& out)
      {
         if (!conf) return false;
         char val;
         if (config_get_char(conf, key.c_str(), &val))
         {
            out = val;
            return true;
         }
         return false;
      }

      bool get(const std::string& key, bool& out)
      {
         if (!conf) return false;
         bool val;
         if (config_get_bool(conf, key.c_str(), &val))
         {
            out = val;
            return true;
         }
         return false;
      }

      bool get(const std::string& key, std::string& out)
      {
         if (!conf) return false;
         char *val;
         if (config_get_string(conf, key.c_str(), &val))
         {
            out = val;
            std::free(val);
            return out.length() > 0;
         }
         return false;
      }

      bool get(const std::string& key, double& out)
      {
         if (!conf) return false;
         double val;
         if (config_get_double(conf, key.c_str(), &val))
         {
            out = val;
            return true;
         }
         return false;
      }

      void set(const std::string& key, int val)
      {
         if (conf) config_set_int(conf, key.c_str(), val);
      }

      void set(const std::string& key, double val)
      {
         if (conf) config_set_double(conf, key.c_str(), val);
      }

      void set(const std::string& key, const std::string& val)
      {
         if (conf) config_set_string(conf, key.c_str(), val.c_str());
      }

      void set(const std::string& key, char val)
      {
         if (conf) config_set_char(conf, key.c_str(), val);
      }

      void set(const std::string& key, bool val)
      {
         if (conf) config_set_bool(conf, key.c_str(), val);
      }

      void write() { if (conf && path[0]) config_file_write(conf, path.c_str()); }

      ConfigFile(ConfigFile&& _in) { *this = std::move(_in); }

      ~ConfigFile() { if (conf) config_file_free(conf); }

   private:
      config_file_t *conf;
      std::string path;
};

#endif
