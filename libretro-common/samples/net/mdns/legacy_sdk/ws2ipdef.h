/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: MIT */
/* ws2ipdef.h as shipped before the Windows 10 1703 SDK, which has no
 * IP_RECVTTL. The mingw lane of mdns.yml builds the transport with
 * -isystem pointing here. */
#include_next <ws2ipdef.h>
#undef IP_RECVTTL
