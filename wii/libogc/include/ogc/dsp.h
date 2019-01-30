/*-------------------------------------------------------------

dsp.h -- DSP subsystem

Copyright (C) 2004
Michael Wiedenbauer (shagkur)
Dave Murphy (WinterMute)

This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any
damages arising from the use of this software.

Permission is granted to anyone to use this software for any
purpose, including commercial applications, and to alter it and
redistribute it freely, subject to the following restrictions:

1.	The origin of this software must not be misrepresented; you
must not claim that you wrote the original software. If you use
this software in a product, an acknowledgment in the product
documentation would be appreciated but is not required.

2.	Altered source versions must be plainly marked as such, and
must not be misrepresented as being the original software.

3.	This notice may not be removed or altered from any source
distribution.

-------------------------------------------------------------*/

#ifndef __DSP_H__
#define __DSP_H__

/*! \file dsp.h
\brief DSP subsystem

*/

#include <gctypes.h>

/*!
 * \addtogroup dsp_taskstate DSP task states
 * \brief DSP task state indicating DSP task's current operation
 * @{
 */

#define DSPTASK_INIT		0				/*!< DSP task is initializing */
#define DSPTASK_RUN			1				/*!< DSP task is running */
#define DSPTASK_YIELD		2				/*!< DSP task has yield */
#define DSPTASK_DONE		3				/*!< DSP task is done/ready */

/*!
 * @}
 */

/*!
 * \addtogroup dsp_taskflag DSP task flags
 * \brief DSP task queue state flag indicating the task's current queue state. Multiple states are OR'd.
 * @{
 */

#define DSPTASK_CLEARALL	0x00000000		/*!< DSP task emtpy/ready */
#define DSPTASK_ATTACH		0x00000001		/*!< DSP task attached */
#define DSPTASK_CANCEL		0x00000002		/*!< DSP task canceled */

/*!
 * @}
 */

#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */

/*!
\typedef struct _dsp_task dsptask_t
\brief forward typdef to struct _dsp_task. This struture holds certain DSP task information for execution.
*/
typedef struct _dsp_task dsptask_t;

/*! \typedef void (*DSPTaskCallback)(dsptask_t *task)
\brief function pointer typedef for the user's DSP task callbacks
\param[in] task pointer to the dsp_task structure.
*/
typedef void (*DSPTaskCallback)(dsptask_t *task);

/*! \typedef void (*DSPCallback)(void)
\brief function pointer typedef for the user's DSP interrupt callback
*/
typedef void (*DSPCallback)(void);

/*! \typedef struct _dsp_task dsptask_t
\param state current task \ref dsp_taskstate "state" set
\param prio priority of the task
\param flags currnet task \ref dsp_taskflag "flag(s)" set.
\param init_vec initialization vector. depends on the DSP code to execute.
\param resume_vec resume vector. depends on the DSP code to execute.
\param iram_maddr main memory address of i-ram image. NOTE: Has to be aligned on a 32byte boundery!
\param iram_len size of i-ram image. NOTE: Should be a multiple of 32
\param iram_addr DSP i-ram address to load the image to.
\param dram_maddr main memory address of d-ram image. NOTE: Has to be aligned on a 32byte boundery!
\param dram_len size of d-ram image. NOTE: Should be a multiple of 32
\param dram_addr DSP d-ram address to load the image to.
\param init_cb pointer to the user's init callback function. Called durring task initialization.
\param res_cb pointer to the user's resume callback function. Called when the task should resume.
\param done_cb pointer to the user's done callback function. Called when the task has finished.
\param req_cb pointer to the user's request callback function. Used to retrieve data from main application durring execution.
\param next pointer to the next task in the doubly linked list.
\param prev pointer to the previous task in the doubly linked list.
*/
struct _dsp_task {
	vu32 state;
	vu32 prio;
	vu32 flags;

	void *iram_maddr;
	u32 iram_len;
	u32 iram_addr;

	void *dram_maddr;
	u32 dram_len;
	u32 dram_addr;

	u16 init_vec;
	u16 resume_vec;

	DSPTaskCallback init_cb;
	DSPTaskCallback res_cb;
	DSPTaskCallback done_cb;
	DSPTaskCallback req_cb;

	struct _dsp_task *next;
	struct _dsp_task *prev;
};

/*! \fn void DSP_Init()
\brief Initialize DSP subsystem.

\return none
*/
void DSP_Init();

/*! \fn u32 DSP_CheckMailTo()
\brief Check if mail was sent to DSP

\return 1: mail sent, 0: mail on route
*/
u32 DSP_CheckMailTo();

/*! \fn u32 DSP_CheckMailFrom()
\brief Check for mail from DSP

\return 1: has mail, 0: no mail
*/
u32 DSP_CheckMailFrom();

/*! \fn u32 DSP_ReadMailFrom()
\brief Read mail from DSP

\return mail value received
*/
u32 DSP_ReadMailFrom();

/*! \fn void DSP_AssertInt()
\brief Asserts the processor interface interrupt

\return none
*/
void DSP_AssertInt();

/*! \fn void DSP_SendMailTo(u32 mail)
\brief Send mail to DSP
\param[in] mail value to send

\return none
*/
void DSP_SendMailTo(u32 mail);

/*! \fn u32 DSP_ReadCPUtoDSP()
\brief Read back CPU->DSP mailbox

\return mail value received
*/
u32 DSP_ReadCPUtoDSP();

/*! \fn dsptask_t* DSP_AddTask(dsptask_t *task)
\brief Add a DSP task to the tasklist and start executing if necessary.
\param[in] task pointer to a dsptask_t structure which holds all necessary values for DSP task execution.

\return current task
*/
dsptask_t* DSP_AddTask(dsptask_t *task);

dsptask_t* DSP_AssertTask(dsptask_t *task);

void DSP_CancelTask(dsptask_t *task);

void DSP_Reset();

void DSP_Halt();

void DSP_Unhalt();

/*! \fn DSPCallback DSP_RegisterCallback(DSPCallback usr_cb)
\brief Register an user's interrupt callback. This may be used to handle DSP interrupts on its own. By default a system default callback is installed on DSP_Init().
\param[in] user_cb pointer to the user's interrupt callback function.
\
\return pointer to old interrupt callback function.
*/
DSPCallback DSP_RegisterCallback(DSPCallback usr_cb);

#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif
