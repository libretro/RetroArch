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

#ifndef STDINT_H
#define STDINT_H

typedef unsigned long     uintptr_t;
typedef signed long       intptr_t;

typedef signed char       int8_t;
typedef signed short      int16_t;
typedef signed int        int32_t;
typedef signed long       int64_t;
typedef unsigned char     uint8_t;
typedef unsigned short    uint16_t;
typedef unsigned int      uint32_t;
typedef unsigned long     uint64_t;

#define	STDIN_FILENO	0	/* standard input file descriptor */
#define	STDOUT_FILENO	1	/* standard output file descriptor */
#define	STDERR_FILENO	2	/* standard error file descriptor */

#define INT8_C(val)  val##c
#define INT16_C(val) val##h
#define INT32_C(val) val##i
#define INT64_C(val) val##l

#define UINT8_C(val)  val##uc
#define UINT16_C(val) val##uh
#define UINT32_C(val) val##ui
#define UINT64_C(val) val##ul

#endif /* STDINT_H */
