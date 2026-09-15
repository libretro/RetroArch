/* Stand-in for ps2sdk's <kernel.h>, so retro_atomic.h's PS2 backend can
 * be built and exercised on a host.  It declares only what that backend
 * uses, and models the EE's interrupt mask as a flag with the same
 * contract as the real pair:
 *
 *   DIntr() masks the interrupts and returns non-zero only when that
 *   call is the one that masked them.
 *   EIntr() unmasks them and returns their state on entry.
 *
 * The counters let the test assert that every masked section is
 * balanced and that a nested one leaves the mask alone on the way out.
 */

#ifndef PS2STUB_KERNEL_H
#define PS2STUB_KERNEL_H

extern int ps2stub_eie;       /* interrupts enabled */
extern int ps2stub_di_calls;
extern int ps2stub_ei_calls;
extern int ps2stub_nest;      /* open masked sections */
extern int ps2stub_max_nest;

int DIntr(void);
int EIntr(void);

#endif
