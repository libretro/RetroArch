/* Stand-in for the macOS SDK's <sys/sysctl.h>, for the compile matrix's Metal lane */
int sysctlbyname(const char *, void *, unsigned long *, void *, unsigned long);
