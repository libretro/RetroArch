/*
Copyright (c) 2012, Broadcom Europe Ltd
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

// OpenMAX IL - ILCS specific types

#ifndef OMX_ILCS_h
#define OMX_ILCS_h

typedef struct OMX_PARAM_PORTSUMMARYTYPE {
   OMX_U32 nSize;            /**< Size of the structure in bytes */
   OMX_VERSIONTYPE nVersion; /**< OMX specification version information */
   OMX_U32 nNumPorts;        /**< Total number of ports */
   OMX_U32 reqSet;           /**< Which set of ports is details below, portIndex[0] is port reqSet*32 */
   OMX_U32 portDir;          /**< Bitfield, 1 if output port, 0 if input port, max 256 ports */
   OMX_U32 portIndex[32];    /**< Port Indexes */
} OMX_PARAM_PORTSUMMARYTYPE;

typedef struct OMX_PARAM_MARKCOMPARISONTYPE {
   OMX_U32 nSize;            /**< Size of the structure in bytes */
   OMX_VERSIONTYPE nVersion; /**< OMX specification version information */
   OMX_PTR mark;             /**< Pointer to be used for mark comparisons */
} OMX_PARAM_MARKCOMPARISONTYPE;

typedef struct OMX_PARAM_BRCMRECURSIONUNSAFETYPE {
   OMX_U32 nSize;
   OMX_VERSIONTYPE nVersion;
   OMX_S32 (*pRecursionUnsafe)(OMX_PTR param);
   OMX_PTR param;
} OMX_PARAM_BRCMRECURSIONUNSAFETYPE;

typedef struct OMX_PARAM_TUNNELSTATUSTYPE {
   OMX_U32 nSize;            /**< Size of the structure in bytes */
   OMX_VERSIONTYPE nVersion; /**< OMX specification version information */
   OMX_U32 nPortIndex;       /**< Port being queried */
   OMX_U32 nIndex;           /**< Query the nIndex'th port and fill in nPortIndex */
   OMX_BOOL bUseIndex;       /**< If OMX_TRUE read nIndex, otherwise read nPortIndex */
   OMX_PTR hTunneledComponent; /**< Component currently tunnelling with */
   OMX_U32 nTunneledPort;    /**< Port on tunnelled component */
} OMX_PARAM_TUNNELSTATUSTYPE;

#endif
