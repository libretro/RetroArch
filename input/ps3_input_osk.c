/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2014 - Daniel De Matteis
 * 
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdint.h>
#include <stdlib.h>
#include <sys/memory.h>
#include <wchar.h>

typedef struct ps3_osk
{
   unsigned int osk_memorycontainer;
   wchar_t init_message[CELL_OSKDIALOG_STRING_SIZE + 1];
   wchar_t message[CELL_OSKDIALOG_STRING_SIZE + 1];
   wchar_t text_buf[CELL_OSKDIALOG_STRING_SIZE + 1];
   uint32_t flags;
   sys_memory_container_t containerid;
   CellOskDialogPoint pos;
   CellOskDialogInputFieldInfo inputFieldInfo;
   CellOskDialogCallbackReturnParam outputInfo;
   CellOskDialogParam dialogParam;
} ps3_osk_t;

#define OSK_IN_USE 1

static void *oskutil_init(size_t size)
{
   ps3_osk_t *params = (ps3_osk_t*)calloc(1, sizeof(*params));

   if (!params)
      return NULL;

   params->flags = 0;
   if (size)
      params->osk_memorycontainer =  size; 
   else
      params->osk_memorycontainer =  1024*1024*2;

   return params;
}

static void oskutil_free(void *data)
{
   ps3_osk_t *params = (ps3_osk_t*)data;

   if (params)
      free(params);
}

static bool oskutil_enable_key_layout(void *data)
{
   (void)data;

   if (pOskSetKeyLayoutOption(CELL_OSKDIALOG_10KEY_PANEL | CELL_OSKDIALOG_FULLKEY_PANEL) < 0)
      return false;

   return true;
}

static void oskutil_create_activation_parameters(void *data)
{
   ps3_osk_t *params = (ps3_osk_t*)data;
   params->dialogParam.controlPoint.x = 0.0;
   params->dialogParam.controlPoint.y = 0.0;

   int32_t LayoutMode = CELL_OSKDIALOG_LAYOUTMODE_X_ALIGN_CENTER | CELL_OSKDIALOG_LAYOUTMODE_Y_ALIGN_TOP;
   pOskSetLayoutMode(LayoutMode);

   params->dialogParam.osk_allowed_panels = 
      CELL_OSKDIALOG_PANELMODE_ALPHABET |
      CELL_OSKDIALOG_PANELMODE_NUMERAL | 
      CELL_OSKDIALOG_PANELMODE_NUMERAL_FULL_WIDTH |
      CELL_OSKDIALOG_PANELMODE_ENGLISH;

   params->dialogParam.firstViewPanel = CELL_OSKDIALOG_PANELMODE_ENGLISH;
   params->dialogParam.osk_prohibit_flags = 0;
}

static void oskutil_write_message(void *data, const void *data_msg)
{
   ps3_osk_t *params = (ps3_osk_t*)data;
   const wchar_t *msg = (const wchar_t*)data_msg;
   params->inputFieldInfo.osk_inputfield_message = (uint16_t*)msg;
}

static void oskutil_write_initial_message(void *data, const void *data_msg)
{
   ps3_osk_t *params = (ps3_osk_t*)data;
   const wchar_t *msg = (const wchar_t*)data_msg;
   params->inputFieldInfo.osk_inputfield_starttext = (uint16_t*)msg;
}

static bool oskutil_start(void *data) 
{
   ps3_osk_t *params = (ps3_osk_t*)data;

   if (params->flags & OSK_IN_USE)
   {
      RARCH_WARN("OSK util already initialized and in use\n");
      return true;
   }

   if (sys_memory_container_create(&params->containerid, params->osk_memorycontainer) < 0)
      goto do_deinit;

   params->outputInfo.osk_callback_return_param = CELL_OSKDIALOG_INPUT_FIELD_RESULT_OK;
   params->outputInfo.osk_callback_num_chars = 256;
   params->outputInfo.osk_callback_return_string = (uint16_t *)params->text_buf;

   memset(params->text_buf, 0, sizeof(*params->text_buf));

   params->inputFieldInfo.osk_inputfield_max_length = CELL_OSKDIALOG_STRING_SIZE;	

   oskutil_create_activation_parameters(params);

   if (!oskutil_enable_key_layout(params))
      return (false);

   if (pOskLoadAsync(params->containerid, &params->dialogParam, &params->inputFieldInfo) < 0)
      goto do_deinit;

   params->flags |= OSK_IN_USE;

   return true;

do_deinit:
   RARCH_ERR("Could not properly initialize OSK util.\n");
   return false;
}

static void *oskutil_get_text_buf(void *data)
{
   ps3_osk_t *osk = (ps3_osk_t*)data;
   return osk->text_buf;
}

static void oskutil_lifecycle(void *data, uint64_t status)
{
   ps3_osk_t *osk = (ps3_osk_t*)data;

   switch (status)
   {
      case CELL_SYSUTIL_OSKDIALOG_LOADED:
         break;
      case CELL_SYSUTIL_OSKDIALOG_INPUT_CANCELED:
         RARCH_LOG("CELL_SYSUTIL_OSKDIALOG_INPUT_CANCELED.\n");
         pOskAbort(); //fall-through
      case CELL_SYSUTIL_OSKDIALOG_FINISHED:
         if (status == CELL_SYSUTIL_OSKDIALOG_FINISHED)
            RARCH_LOG("CELL_SYSUTIL_OSKDIALOG_FINISHED.\n");

         pOskUnloadAsync(&osk->outputInfo);

         if (osk->outputInfo.result == CELL_OSKDIALOG_INPUT_FIELD_RESULT_OK)
         {
            RARCH_LOG("Setting MODE_OSK_ENTRY_SUCCESS.\n");
            g_extern.lifecycle_state |= (1ULL << MODE_OSK_ENTRY_SUCCESS);
         }
         else
         {
            RARCH_LOG("Setting MODE_OSK_ENTRY_FAIL.\n");
            g_extern.lifecycle_state |= (1ULL << MODE_OSK_ENTRY_FAIL);
         }

         osk->flags &= ~OSK_IN_USE;
         break;
      case CELL_SYSUTIL_OSKDIALOG_UNLOADED:
         RARCH_LOG("CELL_SYSUTIL_OSKDIALOG_UNLOADED.\n");
         sys_memory_container_destroy(osk->containerid);
         break;
   }
}

const input_osk_driver_t input_ps3_osk = {
   oskutil_init,
   oskutil_free,
   oskutil_enable_key_layout,
   oskutil_create_activation_parameters,
   oskutil_write_message,
   oskutil_write_initial_message,
   oskutil_start,
   oskutil_lifecycle,
   oskutil_get_text_buf,
   "ps3osk"
};
