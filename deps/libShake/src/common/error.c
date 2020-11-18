#include "error.h"

Shake_ErrorCode errorCode = SHAKE_EC_UNSET;

Shake_Status Shake_EmitErrorCode(Shake_ErrorCode ec)
{
	errorCode = ec;
	return SHAKE_ERROR;
}

Shake_ErrorCode Shake_GetErrorCode(void)
{
	return errorCode;
}
