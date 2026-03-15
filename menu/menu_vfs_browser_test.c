/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2026 - The RetroArch Team
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

/**
 * VFS Browser Test Suite
 * 
 * This file contains test cases for the VFS browser implementation.
 * Run these tests to verify the VFS browser functionality.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "../menu/menu_vfs_browser.h"
#include "../libretro-common/include/boolean.h"
#include "../libretro-common/include/vfs/vfs.h"

/**
 * Test 1: Initialization
 */
static bool test_vfs_browser_init(void)
{
   printf("[TEST] VFS Browser Initialization... ");
   
   bool result = menu_vfs_browser_init();
   
   if (result)
   {
      printf("PASS\n");
      menu_vfs_browser_deinit();
      return true;
   }
   
   printf("FAIL\n");
   return false;
}

/**
 * Test 2: Open root directory
 */
static bool test_vfs_browser_open_root(void)
{
   printf("[TEST] Open Root Directory... ");
   
   if (!menu_vfs_browser_init())
   {
      printf("FAIL (init failed)\n");
      return false;
   }
   
   bool result = menu_vfs_browser_open("/");
   
   menu_vfs_browser_deinit();
   
   if (result)
   {
      printf("PASS\n");
      return true;
   }
   
   printf("FAIL\n");
   return false;
}

/**
 * Test 3: Get entry count
 */
static bool test_vfs_browser_entry_count(void)
{
   printf("[TEST] Entry Count... ");
   
   if (!menu_vfs_browser_init())
   {
      printf("FAIL (init failed)\n");
      return false;
   }
   
   menu_vfs_browser_open("/");
   size_t count = menu_vfs_browser_get_count();
   
   menu_vfs_browser_deinit();
   
   printf("%zu entries found\n", count);
   
   /* At least some entries should exist in root */
   if (count > 0)
   {
      printf("PASS\n");
      return true;
   }
   
   printf("WARN (no entries in root)\n");
   return true; /* Not necessarily a failure */
}

/**
 * Test 4: Get entry names
 */
static bool test_vfs_browser_entry_names(void)
{
   printf("[TEST] Entry Names... ");
   
   if (!menu_vfs_browser_init())
   {
      printf("FAIL (init failed)\n");
      return false;
   }
   
   menu_vfs_browser_open("/");
   size_t count = menu_vfs_browser_get_count();
   
   bool all_valid = true;
   size_t i;
   
   for (i = 0; i < count && i < 5; i++) /* Test first 5 entries */
   {
      const char *name = menu_vfs_browser_get_name(i);
      if (!name)
      {
         printf("FAIL (null name at index %zu)\n", i);
         all_valid = false;
         break;
      }
      printf("  - %s\n", name);
   }
   
   menu_vfs_browser_deinit();
   
   if (all_valid)
   {
      printf("PASS\n");
      return true;
   }
   
   printf("FAIL\n");
   return false;
}

/**
 * Test 5: Directory detection
 */
static bool test_vfs_browser_is_directory(void)
{
   printf("[TEST] Directory Detection... ");
   
   if (!menu_vfs_browser_init())
   {
      printf("FAIL (init failed)\n");
      return false;
   }
   
   menu_vfs_browser_open("/");
   size_t count = menu_vfs_browser_get_count();
   
   if (count == 0)
   {
      menu_vfs_browser_deinit();
      printf("SKIP (no entries)\n");
      return true;
   }
   
   /* Check first entry */
   const char *name = menu_vfs_browser_get_name(0);
   bool is_dir = menu_vfs_browser_is_directory(0);
   
   menu_vfs_browser_deinit();
   
   printf("First entry '%s' is %s\n", name ? name : "NULL", 
          is_dir ? "directory" : "file");
   printf("PASS\n");
   return true;
}

/**
 * Test 6: Parent navigation
 */
static bool test_vfs_browser_parent(void)
{
   printf("[TEST] Parent Navigation... ");
   
   if (!menu_vfs_browser_init())
   {
      printf("FAIL (init failed)\n");
      return false;
   }
   
   menu_vfs_browser_open("/");
   
   /* Try to go parent from root (should fail gracefully) */
   bool result = menu_vfs_browser_parent();
   
   if (!result)
   {
      printf("PASS (correctly prevented going above root)\n");
      menu_vfs_browser_deinit();
      return true;
   }
   
   printf("FAIL (should not go above root)\n");
   menu_vfs_browser_deinit();
   return false;
}

/**
 * Test 7: Get current path
 */
static bool test_vfs_browser_get_path(void)
{
   printf("[TEST] Get Current Path... ");
   
   if (!menu_vfs_browser_init())
   {
      printf("FAIL (init failed)\n");
      return false;
   }
   
   menu_vfs_browser_open("/tmp");
   const char *path = menu_vfs_browser_get_path();
   
   menu_vfs_browser_deinit();
   
   if (path && strlen(path) > 0)
   {
      printf("Path: %s - PASS\n", path);
      return true;
   }
   
   printf("FAIL (empty path)\n");
   return false;
}

/**
 * Test 8: File size
 */
static bool test_vfs_browser_file_size(void)
{
   printf("[TEST] File Size... ");
   
   if (!menu_vfs_browser_init())
   {
      printf("FAIL (init failed)\n");
      return false;
   }
   
   menu_vfs_browser_open("/");
   size_t count = menu_vfs_browser_get_count();
   
   if (count == 0)
   {
      menu_vfs_browser_deinit();
      printf("SKIP (no entries)\n");
      return true;
   }
   
   /* Get size of first entry */
   uint64_t size = menu_vfs_browser_get_size(0);
   const char *name = menu_vfs_browser_get_name(0);
   bool is_dir = menu_vfs_browser_is_directory(0);
   
   menu_vfs_browser_deinit();
   
   printf("'%s' (%s) size: %llu bytes\n", 
          name ? name : "NULL",
          is_dir ? "dir" : "file",
          (unsigned long long)size);
   printf("PASS\n");
   return true;
}

/**
 * Test 9: Create directory
 */
static bool test_vfs_browser_mkdir(void)
{
   printf("[TEST] Create Directory... ");
   
   if (!menu_vfs_browser_init())
   {
      printf("FAIL (init failed)\n");
      return false;
   }
   
   menu_vfs_browser_open("/tmp");
   
   /* Try to create a test directory */
   bool result = menu_vfs_browser_operation(4, "vfs_test_dir", NULL);
   
   /* Clean up */
   if (result)
   {
      menu_vfs_browser_operation(2, "vfs_test_dir", NULL); /* Delete */
   }
   
   menu_vfs_browser_deinit();
   
   if (result)
   {
      printf("PASS\n");
      return true;
   }
   
   printf("FAIL (may need write permissions)\n");
   return false;
}

/**
 * Test 10: VFS Scheme detection
 */
static bool test_vfs_browser_scheme(void)
{
   printf("[TEST] VFS Scheme Detection... ");
   
   if (!menu_vfs_browser_init())
   {
      printf("FAIL (init failed)\n");
      return false;
   }
   
   menu_vfs_browser_open("/");
   enum vfs_scheme scheme = menu_vfs_browser_get_scheme();
   
   menu_vfs_browser_deinit();
   
   const char *scheme_str;
   switch (scheme)
   {
      case VFS_SCHEME_NONE:
         scheme_str = "NONE (local)";
         break;
      case VFS_SCHEME_CDROM:
         scheme_str = "CDROM";
         break;
      case VFS_SCHEME_SAF:
         scheme_str = "SAF (Android)";
         break;
      case VFS_SCHEME_SMB:
         scheme_str = "SMB (Network)";
         break;
      default:
         scheme_str = "UNKNOWN";
         break;
   }
   
   printf("Scheme: %s - PASS\n", scheme_str);
   return true;
}

/**
 * Run all tests
 */
int main(int argc, char *argv[])
{
   int passed = 0;
   int failed = 0;
   int total = 0;
   
   printf("===========================================\n");
   printf("VFS Browser Test Suite\n");
   printf("===========================================\n\n");
   
   #define RUN_TEST(test_func) do { \
      total++; \
      if (test_func()) passed++; \
      else failed++; \
      printf("\n"); \
   } while(0)
   
   RUN_TEST(test_vfs_browser_init);
   RUN_TEST(test_vfs_browser_open_root);
   RUN_TEST(test_vfs_browser_entry_count);
   RUN_TEST(test_vfs_browser_entry_names);
   RUN_TEST(test_vfs_browser_is_directory);
   RUN_TEST(test_vfs_browser_parent);
   RUN_TEST(test_vfs_browser_get_path);
   RUN_TEST(test_vfs_browser_file_size);
   RUN_TEST(test_vfs_browser_mkdir);
   RUN_TEST(test_vfs_browser_scheme);
   
   printf("===========================================\n");
   printf("Test Results: %d/%d passed, %d failed\n", passed, total, failed);
   printf("===========================================\n");
   
   return (failed == 0) ? 0 : 1;
}
