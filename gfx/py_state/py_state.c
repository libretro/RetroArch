/*  SSNES - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *

 * 
 *  SSNES is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  SSNES is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with SSNES.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include <Python.h>
#include "../../boolean.h"
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "../../dynamic.h"
#include "../../libretro.h"
#include "py_state.h"
#include "../../general.h"
#include "../../compat/strl.h"
#include "../../compat/posix_string.h"
#include "../../file.h"

static PyObject* py_read_wram(PyObject *self, PyObject *args)
{
   (void)self;

   const uint8_t *data = (const uint8_t*)pretro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM);
   if (!data)
   {
      Py_INCREF(Py_None);
      return Py_None;
   }

   size_t max = pretro_get_memory_size(RETRO_MEMORY_SYSTEM_RAM);

   unsigned addr;
   if (!PyArg_ParseTuple(args, "I", &addr))
      return NULL;

   if (addr >= max || addr < 0)
   {
      Py_INCREF(Py_None);
      return Py_None;
   }

   return PyLong_FromLong(data[addr]);
}

static PyObject *py_read_input(PyObject *self, PyObject *args)
{
   (void)self;

   if (!driver.input_data)
      return PyBool_FromLong(0);

   unsigned player;
   unsigned key;
   if (!PyArg_ParseTuple(args, "II", &player, &key))
      return NULL;

   if (player > MAX_PLAYERS || player < 1 || key >= SSNES_FIRST_META_KEY)
      return NULL;

   static const struct snes_keybind *binds[MAX_PLAYERS] = {
      g_settings.input.binds[0],
      g_settings.input.binds[1],
      g_settings.input.binds[2],
      g_settings.input.binds[3],
      g_settings.input.binds[4],
   };

   int16_t res = input_input_state_func(binds, player > 1, 
         player > 2 ? RETRO_DEVICE_JOYPAD_MULTITAP : RETRO_DEVICE_JOYPAD,
         player > 2 ? player - 2 : 0,
         key);

   return PyBool_FromLong(res);
}

static PyObject *py_read_input_meta(PyObject *self, PyObject *args)
{
   (void)self;

   if (!driver.input_data)
      return PyBool_FromLong(0);

   unsigned key;
   if (!PyArg_ParseTuple(args, "I", &key))
      return NULL;

   if (key < SSNES_FIRST_META_KEY)
      return NULL;

   bool ret = input_key_pressed_func(key);
   return PyBool_FromLong(ret);
}

static PyMethodDef SNESMethods[] = {
   { "read_wram",    py_read_wram,              METH_VARARGS, "Read WRAM from system." },
   { "input",        py_read_input,             METH_VARARGS, "Read input state from system." },
   { "input_meta",   py_read_input_meta,        METH_VARARGS, "Read SSNES specific input." },
   { NULL, NULL, 0, NULL }
};

#define DECL_ATTR_SNES(attr) PyObject_SetAttrString(mod, #attr, PyLong_FromLong(RETRO_DEVICE_ID_JOYPAD_##attr))
#define DECL_ATTR_SSNES(attr) PyObject_SetAttrString(mod, #attr, PyLong_FromLong(SSNES_##attr))
static void py_set_attrs(PyObject *mod)
{
   DECL_ATTR_SNES(B);
   DECL_ATTR_SNES(Y);
   DECL_ATTR_SNES(SELECT);
   DECL_ATTR_SNES(START);
   DECL_ATTR_SNES(UP);
   DECL_ATTR_SNES(DOWN);
   DECL_ATTR_SNES(LEFT);
   DECL_ATTR_SNES(RIGHT);
   DECL_ATTR_SNES(A);
   DECL_ATTR_SNES(X);
   DECL_ATTR_SNES(L);
   DECL_ATTR_SNES(R);

   DECL_ATTR_SSNES(FAST_FORWARD_KEY);
   DECL_ATTR_SSNES(FAST_FORWARD_HOLD_KEY);
   DECL_ATTR_SSNES(LOAD_STATE_KEY);
   DECL_ATTR_SSNES(SAVE_STATE_KEY);
   DECL_ATTR_SSNES(FULLSCREEN_TOGGLE_KEY);
   DECL_ATTR_SSNES(QUIT_KEY);
   DECL_ATTR_SSNES(STATE_SLOT_PLUS);
   DECL_ATTR_SSNES(STATE_SLOT_MINUS);
   DECL_ATTR_SSNES(AUDIO_INPUT_RATE_PLUS);
   DECL_ATTR_SSNES(AUDIO_INPUT_RATE_MINUS);
   DECL_ATTR_SSNES(REWIND);
   DECL_ATTR_SSNES(MOVIE_RECORD_TOGGLE);
   DECL_ATTR_SSNES(PAUSE_TOGGLE);
   DECL_ATTR_SSNES(FRAMEADVANCE);
   DECL_ATTR_SSNES(RESET);
   DECL_ATTR_SSNES(SHADER_NEXT);
   DECL_ATTR_SSNES(SHADER_PREV);
   DECL_ATTR_SSNES(CHEAT_INDEX_PLUS);
   DECL_ATTR_SSNES(CHEAT_INDEX_MINUS);
   DECL_ATTR_SSNES(CHEAT_TOGGLE);
   DECL_ATTR_SSNES(SCREENSHOT);
   DECL_ATTR_SSNES(DSP_CONFIG);
   DECL_ATTR_SSNES(MUTE);
}

static PyModuleDef SNESModule = {
   PyModuleDef_HEAD_INIT, "snes", NULL, -1, SNESMethods,
   NULL, NULL, NULL, NULL
};

static PyObject* PyInit_SNES(void)
{
   PyObject *mod = PyModule_Create(&SNESModule);
   if (!mod)
      return NULL;

   py_set_attrs(mod);
   return mod;
}

struct py_state
{
   PyObject *main;
   PyObject *dict;
   PyObject *inst;

   bool warned_ret;
   bool warned_type;
};

static char *dupe_newline(const char *str)
{
   if (!str)
      return NULL;

   unsigned size = strlen(str) + 2;
   char *ret = (char*)malloc(size);
   if (!ret)
      return NULL;

   strlcpy(ret, str, size);
   ret[size - 2] = '\n';
   ret[size - 1] = '\0';
   return ret;
}

// Need to make sure that first-line indentation is 0. :(
static char *align_program(const char *program)
{
   char *prog = strdup(program);
   if (!prog)
      return NULL;

   size_t prog_size = strlen(program) + 1;
   char *new_prog = (char*)calloc(1, prog_size);
   if (!new_prog)
      return NULL;

   char *line = dupe_newline(strtok(prog, "\n"));
   if (!line)
   {
      free(prog);
      return NULL;
   }

   unsigned skip_chars = 0;
   while (isblank(line[skip_chars]) && line[skip_chars])
      skip_chars++;

   while (line)
   {
      unsigned length = strlen(line);
      unsigned skip_len = skip_chars > length ? length : skip_chars;

      strlcat(new_prog, line + skip_len, prog_size);

      free(line);
      line = dupe_newline(strtok(NULL, "\n"));
   }

   free(prog);
   return new_prog;
}

py_state_t *py_state_new(const char *script, unsigned is_file, const char *pyclass)
{
   SSNES_LOG("Initializing Python runtime ...\n");
   PyImport_AppendInittab("snes", &PyInit_SNES);
   Py_Initialize();
   SSNES_LOG("Initialized Python runtime.\n");

   py_state_t *handle = (py_state_t*)calloc(1, sizeof(*handle));
   PyObject *hook = NULL;

   handle->main = PyImport_AddModule("__main__");
   if (!handle->main)
      goto error;
   Py_INCREF(handle->main);

   if (is_file)
   {
      // Have to hack around the fact that the
      // FILE struct isn't standardized across environments.
      // PyRun_SimpleFile() breaks on Windows.

      char *script_ = NULL;
      if (read_file(script, (void**)&script_) < 0)
      {
         SSNES_ERR("Python: Failed to read script\n");
         goto error;
      }

      PyRun_SimpleString(script_);
      free(script_);
   }
   else
   {
      char *script_ = align_program(script);
      if (script_)
      {
         PyRun_SimpleString(script_);
         free(script_);
      }
   }

   SSNES_LOG("Python: Script loaded.\n");
   handle->dict = PyModule_GetDict(handle->main);
   if (!handle->dict)
   {
      SSNES_ERR("Python: PyModule_GetDict() failed.\n");
      goto error;
   }
   Py_INCREF(handle->dict);

   hook = PyDict_GetItemString(handle->dict, pyclass);
   if (!hook)
   {
      SSNES_ERR("Python: PyDict_GetItemString() failed.\n");
      goto error;
   }

   handle->inst = PyObject_CallFunction(hook, NULL);
   if (!handle->inst)
   {
      SSNES_ERR("Python: PyObject_CallFunction() failed.\n");
      goto error;
   }
   Py_INCREF(handle->inst);

   return handle;

error:
   PyErr_Print();
   PyErr_Clear();
   py_state_free(handle);
   return NULL;
}

void py_state_free(py_state_t *handle)
{
   if (handle)
   {
      PyErr_Print();
      PyErr_Clear();

      Py_CLEAR(handle->inst);
      Py_CLEAR(handle->dict);
      Py_CLEAR(handle->main);

      free(handle);
      Py_Finalize();
   }
}

float py_state_get(py_state_t *handle, const char *id,
      unsigned frame_count)
{
   PyObject *ret = PyObject_CallMethod(handle->inst, (char*)id, (char*)"I", frame_count);
   if (!ret)
   {
      if (!handle->warned_ret)
         SSNES_WARN("Didn't get return value from script. Bug?\n");
      handle->warned_ret = true;
      return 0.0f;
   }

   float retval = (float)PyFloat_AsDouble(ret);
   Py_DECREF(ret);
   return retval;
}

