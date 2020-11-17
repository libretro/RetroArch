#ifndef _SHAKE_ERROR_H_
#define _SHAKE_ERROR_H_

#include "shake.h"

Shake_Status Shake_EmitErrorCode(Shake_ErrorCode ec);
Shake_ErrorCode Shake_GetErrorCode(void);

#endif
