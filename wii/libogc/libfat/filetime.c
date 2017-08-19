/*
 filetime.c
 Conversion of file time and date values to various other types

 Copyright (c) 2006 Michael "Chishm" Chisholm
	
 Redistribution and use in source and binary forms, with or without modification,
 are permitted provided that the following conditions are met:

  1. Redistributions of source code must retain the above copyright notice,
     this list of conditions and the following disclaimer.
  2. Redistributions in binary form must reproduce the above copyright notice,
     this list of conditions and the following disclaimer in the documentation and/or
     other materials provided with the distribution.
  3. The name of the author may not be used to endorse or promote products derived
     from this software without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE
 LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/


#include <time.h>
#include "filetime.h"
#include "common.h"

#define MAX_HOUR 23
#define MAX_MINUTE 59
#define MAX_SECOND 59

#define MAX_MONTH 11
#define MIN_MONTH 0
#define MAX_DAY 31
#define MIN_DAY 1

uint16_t _FAT_filetime_getTimeFromRTC (void) {
#ifdef USE_RTC_TIME
	struct tm timeParts;
	time_t epochTime;
	
	if (time(&epochTime) == (time_t)-1) {
		return 0;
	}
	localtime_r(&epochTime, &timeParts);

	// Check that the values are all in range.
	// If they are not, return 0 (no timestamp)
	if ((timeParts.tm_hour < 0) || (timeParts.tm_hour > MAX_HOUR))	return 0;
	if ((timeParts.tm_min < 0) || (timeParts.tm_min > MAX_MINUTE)) return 0;
	if ((timeParts.tm_sec < 0) || (timeParts.tm_sec > MAX_SECOND)) return 0;
	
	return (
		((timeParts.tm_hour & 0x1F) << 11) |
		((timeParts.tm_min & 0x3F) << 5) |
		((timeParts.tm_sec >> 1) & 0x1F) 
	);
#else
	return 0;
#endif
}


uint16_t _FAT_filetime_getDateFromRTC (void) {
#ifdef USE_RTC_TIME
	struct tm timeParts;
	time_t epochTime;
	
	if (time(&epochTime) == (time_t)-1) {
		return 0;
	}
	localtime_r(&epochTime, &timeParts);

	if ((timeParts.tm_mon < MIN_MONTH) || (timeParts.tm_mon > MAX_MONTH)) return 0;
	if ((timeParts.tm_mday < MIN_DAY) || (timeParts.tm_mday > MAX_DAY)) return 0;
	
	return ( 
		(((timeParts.tm_year - 80) & 0x7F) <<9) |	// Adjust for MS-FAT base year (1980 vs 1900 for tm_year)
		(((timeParts.tm_mon + 1) & 0xF) << 5) |
		(timeParts.tm_mday & 0x1F)
	);
#else
	return 0;
#endif
}

time_t _FAT_filetime_to_time_t (uint16_t t, uint16_t d) {
	struct tm timeParts;

	timeParts.tm_hour = t >> 11;
	timeParts.tm_min = (t >> 5) & 0x3F;
	timeParts.tm_sec = (t & 0x1F) << 1;
	
	timeParts.tm_mday = d & 0x1F;
	timeParts.tm_mon = ((d >> 5) & 0x0F) - 1;
	timeParts.tm_year = (d >> 9) + 80;
	
	timeParts.tm_isdst = 0;
	
	return mktime(&timeParts);
}
