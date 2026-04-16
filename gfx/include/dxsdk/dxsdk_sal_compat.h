/* dxsdk_sal_compat.h
 *
 * Compatibility shim that stubs out SAL annotation macros for old
 * toolchains whose <sal.h> is missing, partial, or absent entirely.
 *
 * Coverage by toolchain:
 *   * MSVC 2003 (_MSC_VER 1310): no <sal.h> exists. We define all
 *     SAL 1 and SAL 2 macros ourselves.
 *   * MSVC 2005 (_MSC_VER 1400): has <sal.h> with SAL 1 only
 *     (__in, __out, __inout, ...). We add all SAL 2 stubs.
 *   * MSVC 2008 (_MSC_VER 1500) RTM: same as 2005.
 *   * MSVC 2008 SP1 and MSVC 2010 (_MSC_VER 1600): <sal.h> ships a
 *     partial SAL 2 set (_In_, _Out_, _Inout_, ...) but is missing
 *     the buffer-size, COM-outptr, and wrapper families. We fill
 *     in the gaps.
 *   * MSVC 2012+ (_MSC_VER >= 1700): full SAL 2 in <sal.h>. The
 *     whole shim is skipped.
 *
 * Usage: #include "dxsdk_sal_compat.h" at the top of any bundled
 * DirectX header (d3dcommon.h, d3d11shader.h, d3dcompiler.h, etc.)
 * that uses SAL annotations. Place it immediately after the include
 * guard and before any declarations.
 *
 * Every macro is wrapped in #ifndef so we only fill in what the
 * toolchain is missing, avoiding C4005 redefinition warnings.
 *
 * The macros expand to nothing (or to a pass-through for the "wrapper"
 * forms like _Always_(x) / _When_(c,x)) so that declarations using
 * them parse as plain C/C++.
 *
 * This is only safe for *building* -- it discards static-analysis
 * information, which old MSVC could not consume anyway.
 */

#ifndef DXSDK_SAL_COMPAT_H
#define DXSDK_SAL_COMPAT_H

/* Only kick in on pre-VS2012 MSVC, which lacks full SAL 2.
 * _MSC_VER 1700 == VS2012, first to ship full SAL 2 broadly. */
#if defined(_MSC_VER) && _MSC_VER < 1700

/* <sal.h> first appeared in MSVC 2005 (_MSC_VER 1400).
 * MSVC 2003 and earlier have no sal.h, so don't try to include it. */
#if _MSC_VER >= 1400
#include <sal.h>
#endif

/* --- SAL 1 forms (double-underscore) --------------------------------- *
 * Present in MSVC 2005+, absent in MSVC 2003. Stub them unconditionally
 * via #ifndef so 2005+ are no-ops and 2003 gets working definitions. */
#ifndef __in
#define __in
#endif
#ifndef __in_opt
#define __in_opt
#endif
#ifndef __in_z
#define __in_z
#endif
#ifndef __in_z_opt
#define __in_z_opt
#endif
#ifndef __in_ecount
#define __in_ecount(s)
#endif
#ifndef __in_ecount_opt
#define __in_ecount_opt(s)
#endif
#ifndef __in_bcount
#define __in_bcount(s)
#endif
#ifndef __in_bcount_opt
#define __in_bcount_opt(s)
#endif
#ifndef __out
#define __out
#endif
#ifndef __out_opt
#define __out_opt
#endif
#ifndef __out_ecount
#define __out_ecount(s)
#endif
#ifndef __out_ecount_opt
#define __out_ecount_opt(s)
#endif
#ifndef __out_bcount
#define __out_bcount(s)
#endif
#ifndef __out_bcount_opt
#define __out_bcount_opt(s)
#endif
#ifndef __inout
#define __inout
#endif
#ifndef __inout_opt
#define __inout_opt
#endif
#ifndef __inout_ecount
#define __inout_ecount(s)
#endif
#ifndef __inout_bcount
#define __inout_bcount(s)
#endif
#ifndef __deref_out
#define __deref_out
#endif
#ifndef __deref_out_opt
#define __deref_out_opt
#endif
#ifndef __deref_opt_out
#define __deref_opt_out
#endif
#ifndef __reserved
#define __reserved
#endif
#ifndef __notnull
#define __notnull
#endif
#ifndef __maybenull
#define __maybenull
#endif
#ifndef __field_ecount
#define __field_ecount(s)
#endif
#ifndef __field_bcount
#define __field_bcount(s)
#endif
#ifndef __success
#define __success(c)
#endif
#ifndef __checkReturn
#define __checkReturn
#endif

/* --- SAL 2 input / output / inout, plain forms ----------------------- */
#ifndef _In_
#define _In_
#endif
#ifndef _In_opt_
#define _In_opt_
#endif
#ifndef _In_z_
#define _In_z_
#endif
#ifndef _In_opt_z_
#define _In_opt_z_
#endif
#ifndef _Out_
#define _Out_
#endif
#ifndef _Out_opt_
#define _Out_opt_
#endif
#ifndef _Outptr_
#define _Outptr_
#endif
#ifndef _Outptr_opt_
#define _Outptr_opt_
#endif
#ifndef _Outptr_result_maybenull_
#define _Outptr_result_maybenull_
#endif
#ifndef _Outptr_opt_result_maybenull_
#define _Outptr_opt_result_maybenull_
#endif
#ifndef _Outptr_result_buffer_
#define _Outptr_result_buffer_(s)
#endif
#ifndef _Outptr_result_buffer_maybenull_
#define _Outptr_result_buffer_maybenull_(s)
#endif
#ifndef _Outptr_opt_result_buffer_
#define _Outptr_opt_result_buffer_(s)
#endif
#ifndef _Inout_
#define _Inout_
#endif
#ifndef _Inout_opt_
#define _Inout_opt_
#endif
#ifndef _Ret_maybenull_
#define _Ret_maybenull_
#endif
#ifndef _Ret_opt_
#define _Ret_opt_
#endif
#ifndef _Ret_notnull_
#define _Ret_notnull_
#endif
#ifndef _Reserved_
#define _Reserved_
#endif
#ifndef _COM_Outptr_
#define _COM_Outptr_
#endif
#ifndef _COM_Outptr_opt_
#define _COM_Outptr_opt_
#endif
#ifndef _COM_Outptr_result_maybenull_
#define _COM_Outptr_result_maybenull_
#endif
#ifndef _COM_Outptr_opt_result_maybenull_
#define _COM_Outptr_opt_result_maybenull_
#endif

/* --- SAL 2 buffer / size-annotated forms ----------------------------- */
#ifndef _In_reads_
#define _In_reads_(s)
#endif
#ifndef _In_reads_opt_
#define _In_reads_opt_(s)
#endif
#ifndef _In_reads_bytes_
#define _In_reads_bytes_(s)
#endif
#ifndef _In_reads_bytes_opt_
#define _In_reads_bytes_opt_(s)
#endif
#ifndef _In_reads_z_
#define _In_reads_z_(s)
#endif
#ifndef _In_reads_or_z_
#define _In_reads_or_z_(s)
#endif
#ifndef _Out_writes_
#define _Out_writes_(s)
#endif
#ifndef _Out_writes_opt_
#define _Out_writes_opt_(s)
#endif
#ifndef _Out_writes_bytes_
#define _Out_writes_bytes_(s)
#endif
#ifndef _Out_writes_bytes_opt_
#define _Out_writes_bytes_opt_(s)
#endif
#ifndef _Out_writes_all_
#define _Out_writes_all_(s)
#endif
#ifndef _Out_writes_to_
#define _Out_writes_to_(s,c)
#endif
#ifndef _Out_writes_to_opt_
#define _Out_writes_to_opt_(s,c)
#endif
#ifndef _Inout_updates_
#define _Inout_updates_(s)
#endif
#ifndef _Inout_updates_bytes_
#define _Inout_updates_bytes_(s)
#endif
#ifndef _Inout_updates_opt_
#define _Inout_updates_opt_(s)
#endif
#ifndef _Field_size_
#define _Field_size_(s)
#endif
#ifndef _Field_size_opt_
#define _Field_size_opt_(s)
#endif
#ifndef _Field_size_bytes_
#define _Field_size_bytes_(s)
#endif
#ifndef _Field_size_bytes_opt_
#define _Field_size_bytes_opt_(s)
#endif

/* --- SAL 2 wrapper / annotation-of-annotation forms (pass-through) --- */
#ifndef _Always_
#define _Always_(x)       x
#endif
#ifndef _When_
#define _When_(c,x)       x
#endif
#ifndef _On_failure_
#define _On_failure_(x)   x
#endif
#ifndef _Post_satisfies_
#define _Post_satisfies_(c)
#endif
#ifndef _Pre_satisfies_
#define _Pre_satisfies_(c)
#endif

/* --- SAL 2 misc ------------------------------------------------------ */
#ifndef _Inexpressible_
#define _Inexpressible_(s)
#endif
#ifndef _Use_decl_annotations_
#define _Use_decl_annotations_
#endif
#ifndef _Null_terminated_
#define _Null_terminated_
#endif
#ifndef _Check_return_
#define _Check_return_
#endif
#ifndef _Must_inspect_result_
#define _Must_inspect_result_
#endif
#ifndef _Analysis_assume_
#define _Analysis_assume_(c)
#endif

#endif /* _MSC_VER < 1700 */

#endif /* DXSDK_SAL_COMPAT_H */
