/* devkitPPC is missing a few functions that are kinda needed for some cores.
 * This should add them back in.
 */

#include <unistd.h>
#include <stdio.h>
#include <wiiu/os.h>
#include <pwd.h>
#include <features/features_cpu.h>
#include <sys/reent.h>
#include <errno.h>
#include <time.h>

/* This is usually in libogc; we can't use that on wiiu */
int usleep(useconds_t microseconds) {
	OSSleepTicks(us_to_ticks(microseconds));
	return 0;
}

/* Can't find this one anywhere for some reason :/ */
/* This could probably be done a lot better with some love */
int access(const char* path, int mode) {
	return 0; /* TODO temp hack, real code below */

	FILE* fd = fopen(path, "rb");
	if (fd < 0) {
		fclose(fd);
		/* We're supposed to set errono here */
		return -1;
	} else {
		fclose(fd);
		return 0;
	}
}

/* Just hardcode the Linux User ID, we're not on linux anyway */
/* Feasible cool addition: nn::act for this? */
uid_t getuid() {
	return 1000;
}

/* Fake user info */
/* Not thread safe, but avoids returning local variable, so... */
struct passwd out;
struct passwd* getpwuid(uid_t uid) {
	out.pw_name = "retroarch";
	out.pw_passwd = "Wait, what?";
	out.pw_uid = uid;
	out.pw_gid = 1000;
	out.pw_gecos = "retroarch";
	out.pw_dir = "sd:/";
	out.pw_shell = "/vol/system_slc/fw.img";

	return &out;
}

/* Basic Cafe OS clock thingy. */
int _gettimeofday_r(struct _reent *ptr,
   struct timeval* ptimeval,
   void* ptimezone) {

   OSTime cosTime;
   uint64_t cosSecs;
   uint32_t cosUSecs;
   time_t unixSecs;

   /* We need somewhere to put our output */
   if (ptimeval == NULL) {
      errno = EFAULT;
      return -1;
   }

   /* Get Cafe OS clock in seconds; epoch 2000-01-01 00:00 */
   cosTime = OSGetTime();
   cosSecs = ticks_to_sec(cosTime);

   /* Get extra milliseconds */
   cosUSecs = ticks_to_us(cosTime) - (cosSecs * 1000000);

   /* Convert to Unix time, epoch 1970-01-01 00:00.
   Constant value is seconds between 1970 and 2000.
   time_t is 32bit here, so the Wii U is vulnerable to the 2038 problem. */
   unixSecs = cosSecs + 946684800;

   ptimeval->tv_sec = unixSecs;
   ptimeval->tv_usec = cosUSecs;
   return 0;
}

/* POSIX clock in all its glory */
int clock_gettime(clockid_t clk_id, struct timespec* tp) {
   struct timeval ptimeval = { 0 };
   int ret = 0;
   OSTime cosTime;

   if (tp == NULL) {
      errno = EFAULT;
      return -1;
   }

   switch (clk_id) {
      case CLOCK_REALTIME:
         /* Just wrap gettimeofday. Cheating, I know. */
         ret = _gettimeofday_r(NULL, &ptimeval, NULL);
         if (ret) return -1;

         tp->tv_sec = ptimeval.tv_sec;
         tp->tv_nsec = ptimeval.tv_usec * 1000;
      break;
      default:
         errno = EINVAL;
         return -1;
   }
   return 0;
}
