/* devkitPPC is missing a few functions that are kinda needed for some cores.
 * This should add them back in.
 */

#include <unistd.h>
#include <stdio.h>
#include <wiiu/os.h>
#include <wiiu/ac.h>
#include <wiiu/types.h>
#include <pwd.h>
#include <sys/reent.h>
#include <ifaddrs.h>
#include <errno.h>
#include <time.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>

#include <verbosity.h>

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
uid_t getuid(void)
{
   return 1000;
}

/* Fake user info */
/* Not thread safe, but avoids returning local variable, so... */
struct passwd out;
struct passwd* getpwuid(uid_t uid)
{
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
   void* ptimezone)
{
   OSTime cosTime;
   uint64_t cosSecs;
   uint32_t cosUSecs;
   time_t unixSecs;

   /* We need somewhere to put our output */
   if (ptimeval == NULL)
   {
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
int clock_gettime(clockid_t clk_id, struct timespec* tp)
{
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

/**
 * Implementation of getifaddrs() and freeifaddrs() for WiiU.
 */

// the Wii U doesn't define an interface name, so we'll use something generic.
static const char *wiiu_iface_name = "eth0";

/**
 * Allocate and zeroize the hunk of memory for the ifaddrs struct and its contents; the struct will be filled
 * out later.
 *
 * returns NULL if any of the memory allocations fail.
 */
static struct ifaddrs *buildEmptyIfa(void)
{
   struct ifaddrs *result = (struct ifaddrs *)malloc(sizeof(struct ifaddrs));
   if (result)
   {
      memset(result, 0, sizeof(struct ifaddrs));
      result->ifa_name = strdup(wiiu_iface_name);
      result->ifa_addr = (struct sockaddr *)malloc(sizeof(struct sockaddr_in));
      result->ifa_netmask = (struct sockaddr *)malloc(sizeof(struct sockaddr_in));
      result->ifa_dstaddr = (struct sockaddr *)malloc(sizeof(struct sockaddr_in));

      if (!result->ifa_name || !result->ifa_addr || !result->ifa_netmask || !result->ifa_dstaddr)
         goto error;

      memset(result->ifa_addr, 0, sizeof(struct sockaddr_in));
      result->ifa_addr->sa_family = AF_INET;
      memset(result->ifa_netmask, 0, sizeof(struct sockaddr_in));
      result->ifa_netmask->sa_family = AF_INET;
      memset(result->ifa_dstaddr, 0, sizeof(struct sockaddr_in));
      result->ifa_dstaddr->sa_family = AF_INET;
   }

   return result;
error:
   freeifaddrs(result);
   return NULL;
}

static int getAssignedAddress(struct sockaddr_in *sa)
{
   ACIpAddress addr;
   int result;
   if (!sa)
      return -1;
   result = ACGetAssignedAddress(&addr);
   if (result == 0)
      sa->sin_addr.s_addr = addr;

   return result;
}

static int getAssignedSubnet(struct sockaddr_in *sa)
{
   ACIpAddress mask;
   int result;
   if (!sa)
      return -1;

   result = ACGetAssignedSubnet(&mask);
   if (result == 0)
      sa->sin_addr.s_addr = mask;

   return result;
}

static int getBroadcastAddress(struct sockaddr_in *sa, struct sockaddr_in *addr, struct sockaddr_in *mask)
{
   if (!sa || !addr || !mask)
      return -1;

   sa->sin_addr.s_addr = addr->sin_addr.s_addr | (~mask->sin_addr.s_addr);
   return 0;
}

static struct ifaddrs *getWiiUInterfaceAddressData(void)
{
   struct ifaddrs *result = buildEmptyIfa();

   if (result)
   {
      if (getAssignedAddress((struct sockaddr_in *)result->ifa_addr) < 0 ||
            getAssignedSubnet((struct sockaddr_in *)result->ifa_netmask) < 0 ||
            getBroadcastAddress((struct sockaddr_in *)result->ifa_dstaddr,
               (struct sockaddr_in *)result->ifa_addr,
               (struct sockaddr_in *)result->ifa_netmask) < 0) {
         goto error;
      }
   }

   return result;

error:
   freeifaddrs(result);
   return NULL;
}

int getifaddrs(struct ifaddrs **ifap)
{
   if (!ifap)
      return -1;
   *ifap = getWiiUInterfaceAddressData();

   return (*ifap == NULL) ? -1 : 0;
}

void freeifaddrs(struct ifaddrs *ifp)
{
   if (ifp)
   {
      if (ifp->ifa_name)
      {
         free(ifp->ifa_name);
         ifp->ifa_name = NULL;
      }

      if (ifp->ifa_addr)
      {
         free(ifp->ifa_addr);
         ifp->ifa_addr = NULL;
      }

      if (ifp->ifa_netmask)
      {
         free(ifp->ifa_netmask);
         ifp->ifa_netmask = NULL;
      }

      if (ifp->ifa_dstaddr)
      {
         free(ifp->ifa_dstaddr);
         ifp->ifa_dstaddr = NULL;
      }
      free(ifp);
   }
}
