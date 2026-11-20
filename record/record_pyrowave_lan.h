/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RECORD_PYROWAVE_LAN_H
#define RECORD_PYROWAVE_LAN_H
#include <boolean.h>
#include <stddef.h>
#include <stdint.h>
#include <retro_common_api.h>

typedef struct record_pyrowave_lan_peer
{
   char name[64];
   char key[40];
   uint8_t address[4];
   uint16_t port;
} record_pyrowave_lan_peer_t;

RETRO_BEGIN_DECLS
void record_pyrowave_lan_poll(uint64_t now_ms);
void record_pyrowave_lan_stop(void);
size_t record_pyrowave_lan_count(void);
/* Main-thread borrow, valid until the next poll. */
const record_pyrowave_lan_peer_t *record_pyrowave_lan_peer(size_t index);
bool record_pyrowave_lan_send(const char *key);
bool record_pyrowave_lan_sending(void);
const char *record_pyrowave_lan_target(void);
bool record_pyrowave_lan_directory(char *directory, size_t size);
RETRO_END_DECLS
#endif
