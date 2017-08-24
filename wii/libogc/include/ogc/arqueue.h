/*-------------------------------------------------------------

arqueue.h -- ARAM task request queue implementation

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


#ifndef __ARQUEUE_H__
#define __ARQUEUE_H__

#include <gctypes.h>
#include <ogc/lwp_queue.h>
#include "aram.h"

#define ARQ_MRAMTOARAM			AR_MRAMTOARAM
#define ARQ_ARAMTOMRAM			AR_ARAMTOMRAM

#define ARQ_DEF_CHUNK_SIZE		4096

#define ARQ_PRIO_LO				0
#define ARQ_PRIO_HI				1

#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */

enum {
	ARQ_TASK_READY = 0,
	ARQ_TASK_RUNNING,
	ARQ_TASK_FINISHED
};

typedef struct _arq_request ARQRequest;
typedef void (*ARQCallback)(ARQRequest *);

struct _arq_request {
	lwp_node node;
	u32 owner,dir,prio,state;
	u32 aram_addr,mram_addr,len;
	ARQCallback callback;
};

void ARQ_Init();
void ARQ_Reset();


/*!
 * \fn void ARQ_PostRequest(ARQRequest *req,u32 owner,u32 dir,u32 prio,u32 aram_addr,u32 mram_addr,u32 len)
 * \brief Enqueue a ARAM DMA transfer request.
 *
 * \param[in] req structure to hold ARAM DMA request informations.
 * \param[in] owner unique owner id.
 * \param[in] dir direction of ARAM DMA transfer.
 * \param[in] prio priority of request. 
 * \param[in] aram_addr startaddress of buffer to be pushed onto the queue. <b><i>NOTE:</i></b> Must be 32-bytealigned.
 * \param[in] mram_addr length of data to be pushed onto the queue.
 * \param[in] len startaddress of buffer to be pushed onto the queue. <b><i>NOTE:</i></b> Must be 32-bytealigned.
 * \param[in] cb length of data to be pushed onto the queue.
 *
 * \return none
 */
void ARQ_PostRequest(ARQRequest *req,u32 owner,u32 dir,u32 prio,u32 aram_addr,u32 mram_addr,u32 len);


/*!
 * \fn void ARQ_PostRequestAsync(ARQRequest *req,u32 owner,u32 dir,u32 prio,u32 aram_addr,u32 mram_addr,u32 len,ARQCallback cb)
 * \brief Enqueue a ARAM DMA transfer request.
 *
 * \param[in] req structure to hold ARAM DMA request informations.
 * \param[in] owner unique owner id.
 * \param[in] dir direction of ARAM DMA transfer.
 * \param[in] prio priority of request. 
 * \param[in] aram_addr startaddress of buffer to be pushed onto the queue. <b><i>NOTE:</i></b> Must be 32-bytealigned.
 * \param[in] mram_addr length of data to be pushed onto the queue.
 * \param[in] len startaddress of buffer to be pushed onto the queue. <b><i>NOTE:</i></b> Must be 32-bytealigned.
 * \param[in] cb length of data to be pushed onto the queue.
 *
 * \return none
 */
void ARQ_PostRequestAsync(ARQRequest *req,u32 owner,u32 dir,u32 prio,u32 aram_addr,u32 mram_addr,u32 len,ARQCallback cb);
void ARQ_RemoveRequest(ARQRequest *req);
void ARQ_SetChunkSize(u32 size);
u32 ARQ_GetChunkSize();
void ARQ_FlushQueue();
u32 ARQ_RemoveOwnerRequest(u32 owner);

#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif
