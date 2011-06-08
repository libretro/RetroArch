/*  SSNES - A Super Nintendo Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2010-2011 - Hans-Kristian Arntzen
 *
 *  Some code herein may be based on code found in BSNES.
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
#include <stdbool.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "dynamic.h"
#include "libsnes.hpp"
#include "py_state.h"
#include "general.h"
#include "strl.h"

#define PY_READ_FUNC_DECL(RAMTYPE) py_read_##RAMTYPE
#define PY_READ_FUNC(RAMTYPE) \
   static PyObject* PY_READ_FUNC_DECL(RAMTYPE) (PyObject *self, PyObject *args) \
   { \
      (void)self; \
\
      const uint8_t *data = psnes_get_memory_data(SNES_MEMORY_##RAMTYPE); \
      if (!data) \
      { \
         Py_INCREF(Py_None); \
         return Py_None; \
      } \
      unsigned max = psnes_get_memory_size(SNES_MEMORY_##RAMTYPE); \
\
      unsigned addr; \
      if (!PyArg_ParseTuple(args, "I", &addr)) \
         return NULL; \
\
      if (addr >= max || addr < 0) \
      { \
         Py_INCREF(Py_None); \
         return Py_None; \
      } \
\
      return PyLong_FromLong((long)data[addr]); \
   }

PY_READ_FUNC(WRAM)
PY_READ_FUNC(VRAM)
PY_READ_FUNC(APURAM)
PY_READ_FUNC(CGRAM)
PY_READ_FUNC(OAM)

static PyObject* py_read_input(PyObject *self, PyObject *args)
{
   (void)self;

   if (!driver.input_data)
      return PyBool_FromLong(0);

   unsigned player;
   unsigned key;
   if (!PyArg_ParseTuple(args, "II", &player, &key))
      return NULL;

   if (player > MAX_PLAYERS || player < 1)
      return NULL;

   const struct snes_keybind *binds[MAX_PLAYERS];
   for (int i = 0; i < MAX_PLAYERS; i++)
      binds[i] = g_settings.input.binds[i];

   int16_t res = driver.input->input_state(driver.input_data, 
         binds, player > 1, 
         player > 2 ? SNES_DEVICE_MULTITAP : SNES_DEVICE_JOYPAD,
         player > 2 ? player - 2 : 0,
         key);

   return PyBool_FromLong((long)res);
}

static PyMethodDef SNESMethods[] = {
   { "read_wram",    PY_READ_FUNC_DECL(WRAM),   METH_VARARGS, "Read WRAM from SNES." },
   { "read_vram",    PY_READ_FUNC_DECL(VRAM),   METH_VARARGS, "Read VRAM from SNES." },
   { "read_apuram",  PY_READ_FUNC_DECL(APURAM), METH_VARARGS, "Read APURAM from SNES." },
   { "read_cgram",   PY_READ_FUNC_DECL(CGRAM),  METH_VARARGS, "Read CGRAM from SNES." },
   { "read_oam",     PY_READ_FUNC_DECL(OAM),    METH_VARARGS, "Read OAM from SNES." },
   { "input",        py_read_input,             METH_VARARGS, "Read input state from SNES." },
   { NULL, NULL, 0, NULL }
};

static void py_set_attrs(PyObject *mod)
{
   PyObject_SetAttrString(mod, "B", PyLong_FromLong(SNES_DEVICE_ID_JOYPAD_B));
   PyObject_SetAttrString(mod, "Y", PyLong_FromLong(SNES_DEVICE_ID_JOYPAD_Y));
   PyObject_SetAttrString(mod, "SELECT", PyLong_FromLong(SNES_DEVICE_ID_JOYPAD_SELECT));
   PyObject_SetAttrString(mod, "START", PyLong_FromLong(SNES_DEVICE_ID_JOYPAD_START));
   PyObject_SetAttrString(mod, "UP", PyLong_FromLong(SNES_DEVICE_ID_JOYPAD_UP));
   PyObject_SetAttrString(mod, "DOWN", PyLong_FromLong(SNES_DEVICE_ID_JOYPAD_DOWN));
   PyObject_SetAttrString(mod, "LEFT", PyLong_FromLong(SNES_DEVICE_ID_JOYPAD_LEFT));
   PyObject_SetAttrString(mod, "RIGHT", PyLong_FromLong(SNES_DEVICE_ID_JOYPAD_RIGHT));
   PyObject_SetAttrString(mod, "A", PyLong_FromLong(SNES_DEVICE_ID_JOYPAD_A));
   PyObject_SetAttrString(mod, "X", PyLong_FromLong(SNES_DEVICE_ID_JOYPAD_X));
   PyObject_SetAttrString(mod, "L", PyLong_FromLong(SNES_DEVICE_ID_JOYPAD_L));
   PyObject_SetAttrString(mod, "R", PyLong_FromLong(SNES_DEVICE_ID_JOYPAD_R));
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
   char *ret = malloc(size);
   if (!ret)
      return NULL;

   strlcpy(ret, str, size);
   ret[size - 2] = '\n';
   ret[size - 1] = '\0';
   return ret;
}

// Need to make sure that first-line indentation is 0. :(
static char* align_program(const char *program)
{
   char *prog = strdup(program);
   if (!prog)
      return NULL;

   size_t prog_size = strlen(program) + 1;
   char *new_prog = calloc(1, prog_size);
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

py_state_t *py_state_new(const char *script, bool is_file, const char *pyclass)
{
   PyImport_AppendInittab("snes", &PyInit_SNES);
   Py_Initialize();

   py_state_t *handle = calloc(1, sizeof(*handle));

   handle->main = PyImport_AddModule("__main__");
   if (!handle->main)
      goto error;

   if (is_file)
   {
      FILE *file = fopen(script, "r");
      if (!file)
         goto error;
      PyRun_SimpleFile(file, script);
      fclose(file);
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

   handle->dict = PyModule_GetDict(handle->main);
   if (!handle->dict)
      goto error;

   PyObject *hook = PyDict_GetItemString(handle->dict, pyclass);
   if (!hook)
      goto error;

   handle->inst = PyObject_CallFunction(hook, NULL);
   if (!handle->inst)
      goto error;

   return handle;

error:
   PyErr_Print();
   py_state_free(handle);
   return NULL;
}

void py_state_free(py_state_t *handle)
{
   if (handle)
   {
      if (handle->main)
         Py_DECREF(handle->main);
      if (handle->dict)
         Py_DECREF(handle->dict);
      if (handle->inst)
         Py_DECREF(handle->inst);

      free(handle);
   }

   Py_Finalize();
}

int py_state_get(py_state_t *handle, const char *id,
      unsigned frame_count)
{
   PyObject *ret = PyObject_CallMethod(handle->inst, (char*)id, (char*)"I", frame_count);
   if (!ret)
   {
      if (!handle->warned_ret)
         SSNES_WARN("Didn't get return value from script! Bug?\n");
      handle->warned_ret = true;
      return 0;
   }

   int retval = 0;
   if (PyLong_Check(ret))
      retval = (int)PyLong_AsLong(ret);
   else
   {
      if (!handle->warned_type)
         SSNES_WARN("Didn't get long compatible value from script! Bug?\n");
      handle->warned_type = true;
   }

   Py_DECREF(ret);
   return retval;
}

