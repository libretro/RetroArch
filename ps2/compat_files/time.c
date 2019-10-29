/* RetroArch - A frontend for libretro.
 * Copyright (C) 2010-2019 - Francisco Javier Trujillo Mata - fjtrujy
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

/* This file improve the content of the original time.c file that belong to the PS2SDK.
The original time.c contains 4 non-static methods

void _ps2sdk_time_init(void);
void _ps2sdk_time_deinit(void);
clock_t clock(void);
time_t time(time_t *t);

So we need to duplicate all the method because this way the compiler will avoid  to import
the code that belong to the PS2SDK */

#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <SDL/SDL.h>
#include <libcdvd-common.h>
#include <osd_config.h>

#define STARTING_YEAR 2000
#define MIN_SUPPORTED_YEAR 1970
#define MAX_SUPPORTED_YEAR 2108
#define SECS_MIN 60L
#define MINS_HOUR 60L
#define HOURS_DAY 24L
#define DAYS_YEAR 365L
#define DEC(x) (10*(x/16)+(x%16))
int _days[] = {-1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, 364};

static time_t _gmtotime_t (
        int yr,     /* 0 based */
        int mo,     /* 1 based */
        int dy,     /* 1 based */
        int hr,
        int mn,
        int sc
        )
{
   int passed_years;
   long passed_days;
   long passed_seconds_current_day;
   time_t seconds_from_1970 = -1;

   if ((yr >= MIN_SUPPORTED_YEAR) || (yr <= MAX_SUPPORTED_YEAR))  {
      passed_years = (long)yr - MIN_SUPPORTED_YEAR; /* Years after 1970 */
      /* Calculate days for these years */
      passed_days = passed_years * DAYS_YEAR;
      passed_days += (passed_years >> 2) * (DAYS_YEAR + 1); /* passed leap years */
      passed_days += dy + _days[mo - 1]; /* passed days in the year */
      if ( !(yr & 3) && (mo > 2) ) {
         passed_days++; /* if current year, is a leap year */
      }
      passed_seconds_current_day = (((hr * MINS_HOUR) + mn) * SECS_MIN) + sc;
      seconds_from_1970 = (passed_days * HOURS_DAY * MINS_HOUR * SECS_MIN) + passed_seconds_current_day;
   }

   return seconds_from_1970;
}

time_t ps2_time(time_t *t) {
   time_t tim;
   sceCdCLOCK clocktime; /* defined in libcdvd.h */

   sceCdReadClock(&clocktime); /* libcdvd.a */
   configConvertToLocalTime(&clocktime);

   tim =   _gmtotime_t (DEC(clocktime.year)+ STARTING_YEAR,
                        DEC(clocktime.month),
                        DEC(clocktime.day),
                        DEC(clocktime.hour),
                        DEC(clocktime.minute),
                        DEC(clocktime.second));

	if(t)
		*t = tim;

	return tim;
}

/* Protected methods in libc */
void _ps2sdk_time_init(void)
{
   SDL_Init(SDL_INIT_TIMER);
}

/* Protected methods in libc */
void _ps2sdk_time_deinit(void)
{
   SDL_QuitSubSystem(SDL_INIT_TIMER);
}

clock_t clock(void)
{
   return SDL_GetTicks();
}

time_t time(time_t *t)
{
   time_t tim = -1;
   /* TODO: This function need to be implemented again because the SDK one is not working fine */
   return tim;
}

time_t mktime(struct tm *timeptr)
{
   time_t tim = -1;
   /* TODO: This function need to be implemented again because the SDK one is not working fine */
   return tim;
}

struct tm *localtime(const time_t *timep)
{
   return NULL;
}

size_t strftime(char *s, size_t max, const char *format, const struct tm *tm)
{
   return -1;
}

char *setlocale(int category, const char *locale)
{
   return NULL;
}