/* devkitPPC is missing a few functions that are kinda needed for some cores.
 * This should add them back in.
 */

#include <unistd.h>
#include <stdio.h>
#include <wiiu/os.h>
#include <pwd.h>
#include <features/features_cpu.h>

//This is usually in libogc; we can't use that on wiiu
int usleep(useconds_t microseconds) {
	OSSleepTicks(us_to_ticks(microseconds));
	return 0;
}

//Can't find this one anywhere for some reason :/
//This could probably be done a lot better with some love
int access(const char* path, int mode) {
	return 0; //TODO temp hack, real code below

	FILE* fd = fopen(path, "rb");
	if (fd < 0) {
		fclose(fd);
		//We're supposed to set errono here
		return -1;
	} else {
		fclose(fd);
		return 0;
	}
}

//Just hardcode the Linux User ID, we're not on linux anyway
//Feasible cool addition: nn::act for this?
uid_t getuid() {
	return 1000;
}

//Fake user info
//Not thread safe, but avoids returning local variable, so...
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

//Try to vaugely spoof the POISX clock. Epoch is off by about 30
//years, so this could be better...
//Only has second accuracy since I'm lazy.
int clock_gettime(clockid_t clk_id, struct timespec* tp) {
	int64_t time_usec = cpu_features_get_time_usec();
	tp->tv_sec = time_usec / 1000000;
	return 0;
}
