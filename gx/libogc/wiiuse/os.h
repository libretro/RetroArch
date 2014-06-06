#ifndef __OS_H__
#define __OS_H__

#ifdef WIN32
	/* windows */
	#define isnan(x)		_isnan(x)
	#define isinf(x)		!_finite(x)

	/* disable warnings I don't care about */
	#pragma warning(disable:4244)		/* possible loss of data conversion	*/
	#pragma warning(disable:4273)		/* inconsistent dll linkage			*/
	#pragma warning(disable:4217)
#else
	/* nix/gekko */
	#ifdef GEKKO
		#include <gccore.h>
		#include <ogcsys.h>
		#include <bte.h>
		#include <network.h>
     
#ifndef _CPU_ISR_Disable
#define _CPU_ISR_Disable( _isr_cookie ) \
  { register u32 _disable_mask = 0; \
	_isr_cookie = 0; \
    __asm__ __volatile__ ( \
	  "mfmsr %0\n" \
	  "rlwinm %1,%0,0,17,15\n" \
	  "mtmsr %1\n" \
	  "extrwi %0,%0,1,16" \
	  : "=&r" ((_isr_cookie)), "=&r" ((_disable_mask)) \
	  : "0" ((_isr_cookie)), "1" ((_disable_mask)) \
	); \
  }
#endif

#ifndef _CPU_ISR_Restore
#define _CPU_ISR_Restore( _isr_cookie )  \
  { register u32 _enable_mask = 0; \
	__asm__ __volatile__ ( \
    "    cmpwi %0,0\n" \
	"    beq 1f\n" \
	"    mfmsr %1\n" \
	"    ori %1,%1,0x8000\n" \
	"    mtmsr %1\n" \
	"1:" \
	: "=r"((_isr_cookie)),"=&r" ((_enable_mask)) \
	: "0"((_isr_cookie)),"1" ((_enable_mask)) \
	); \
  }
#endif

		#include <lwp_wkspace.h>

	#endif
#endif

#endif
