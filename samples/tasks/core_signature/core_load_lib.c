/* Copyright (C) 2026 The RetroArch team
 * SPDX-License-Identifier: GPL-3.0-or-later */

/* core_load_file's test library; CORE_LOAD_ID tells its builds apart */
int core_load_test_id(void);

int core_load_test_id(void)
{
   return CORE_LOAD_ID;
}
