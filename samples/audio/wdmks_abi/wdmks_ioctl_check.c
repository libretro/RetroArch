/* The CTL_CODE pieces this driver carries its own fallbacks for.
 *
 * METHOD_NEITHER and the access bits live in winioctl.h, which some
 * toolchains' windows.h pulls in and some do not - the griffin build
 * under MSVC gets neither, which is how the first version of this
 * driver failed to compile there. The driver defines them behind
 * #ifndef, which is only correct while the values match the system's.
 * This includes winioctl.h as well and asserts exactly that, and
 * builds both IOCTL codes through the system's own CTL_CODE macro to
 * check the arithmetic the driver spells out by hand.
 *
 * Nothing runs: the assertions are the test. */
#include <windows.h>
#include <winioctl.h>
#include "audio/drivers/wdmks.c"
#define SAME(n,a,b) typedef char assert_##n[((long)(a)==(long)(b))?1:-1]
SAME(m_neither, METHOD_NEITHER,    3);
SAME(a_any,     FILE_ANY_ACCESS,   0);
SAME(a_write,   FILE_WRITE_ACCESS, 2);
SAME(ioc_prop,  RA_IOCTL_KS_PROPERTY,     CTL_CODE(0x2f, 0x000, METHOD_NEITHER, FILE_ANY_ACCESS));
SAME(ioc_write, RA_IOCTL_KS_WRITE_STREAM, CTL_CODE(0x2f, 0x004, METHOD_NEITHER, FILE_WRITE_ACCESS));
