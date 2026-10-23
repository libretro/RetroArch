/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include "record_android_capture_device.h"
#include <stddef.h>

static record_android_capture_device_t *record_acdevice_owners;

bool record_android_capture_device_attach(
      record_android_capture_device_t *owner)
{
   record_android_capture_device_t *p;
   if (!owner || !owner->context || !owner->device || !owner->prepare
         || !owner->destroyed || owner->prepared)
      return false;
   for (p = record_acdevice_owners; p; p = p->next)
      if (p == owner)
         return false;
   owner->next = record_acdevice_owners;
   record_acdevice_owners = owner;
   return true;
}

void record_android_capture_device_detach(
      record_android_capture_device_t *owner)
{
   record_android_capture_device_t **link = &record_acdevice_owners;
   if (!owner)
      return;
   while (*link && *link != owner)
      link = &(*link)->next;
   if (*link)
   {
      *link = owner->next;
      owner->next = NULL;
   }
}

bool record_android_capture_device_pending(VkDevice device)
{
   record_android_capture_device_t *p;
   if (!device)
      return false;
   for (p = record_acdevice_owners; p; p = p->next)
      if (p->device == device)
         return true;
   return false;
}

bool record_android_capture_device_prepare(VkDevice device, bool queue_quiesced)
{
   record_android_capture_device_t *p;
   bool ready = true;
   if (!device)
      return false;
   for (p = record_acdevice_owners; p; p = p->next)
      if (p->device == device && !p->prepared)
      {
         if (p->prepare(p->context, queue_quiesced))
            p->prepared = true;
         else
            ready = false;
      }
   return ready;
}

bool record_android_capture_device_destroyed(VkDevice device)
{
   record_android_capture_device_t *p;
   if (!device)
      return false;
   for (p = record_acdevice_owners; p; p = p->next)
      if (p->device == device && !p->prepared)
         return false;
   for (p = record_acdevice_owners; p; p = p->next)
      if (p->device == device)
      {
         p->device = VK_NULL_HANDLE;
         p->destroyed(p->context);
      }
   return true;
}
