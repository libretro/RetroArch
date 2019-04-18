/* RetroArch - A frontend for libretro.
 * Copyright (C) 2010-2018 - Francisco Javier Trujillo Mata - fjtrujy
 *
 * RetroArch is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Found-
 * ation, either version 3 of the License, or (at your option) any later version.
 *
 * RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU General Public License for more details.
 * * You should have received a copy of the GNU General Public License along with RetroArch.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef COMPAT_CTYPE_H
#define COMPAT_CTYPE_H

char *strtok_r(char *str, const char *delim, char **saveptr);

unsigned long long strtoull(const char * __restrict nptr, char ** __restrict endptr, int base);

int link(const char *oldpath, const char *newpath);
int unlink(const char *path);

float strtof (const char* str, char** endptr);

#endif
