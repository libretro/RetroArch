/* This source as presented is a modified version of original wiiuse for use 
 * with RetroArch, and must not be confused with the original software. */

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
		#include <bte/bte.h>
		#include "network.h"
		#include <ogc/lwp_queue.h>
		#include <ogc/machine/asm.h>
		#include <ogc/machine/processor.h>
		#include <ogc/lwp_wkspace.h>
	#else
	#endif
#endif

#endif
