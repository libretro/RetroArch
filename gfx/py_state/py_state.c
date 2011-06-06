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
#include "dynamic.h"
#include "libsnes.hpp"
#include <stdlib.h>
#include "py_state.h"

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

static PyMethodDef SNESMethods[] = {
   { "read_wram",    PY_READ_FUNC_DECL(WRAM),   METH_VARARGS, "Read WRAM from SNES." },
   { "read_vram",    PY_READ_FUNC_DECL(VRAM),   METH_VARARGS, "Read VRAM from SNES." },
   { "read_apuram",  PY_READ_FUNC_DECL(APURAM), METH_VARARGS, "Read APURAM from SNES." },
   { "read_cgram",   PY_READ_FUNC_DECL(CGRAM),  METH_VARARGS, "Read CGRAM from SNES." },
   { "read_oam",     PY_READ_FUNC_DECL(OAM),    METH_VARARGS, "Read OAM from SNES." },
   { NULL, NULL, 0, NULL }
};

static PyModuleDef SNESModule = {
   PyModuleDef_HEAD_INIT, "snes", NULL, -1, SNESMethods,
   NULL, NULL, NULL, NULL
};

static PyObject* PyInit_SNES(void)
{
   return PyModule_Create(&SNESModule);
}

struct py_state
{
   PyObject *main;
   PyObject *dict;
   PyObject *inst;
};

py_state_t *py_state_new(const char *script_path, const char *pyclass)
{
   PyImport_AppendInittab("snes", &PyInit_SNES);
   Py_Initialize();

   py_state_t *handle = calloc(1, sizeof(*handle));

   handle->main = PyImport_AddModule("__main__");
   if (!handle->main)
      goto error;

   FILE *file = fopen(script_path, "r");
   if (!file)
      goto error;
   PyRun_SimpleFile(file, script_path);
   fclose(file);

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
      return 0;

   int retval = 0;
   if (PyLong_Check(ret))
      retval = (int)PyLong_AsLong(ret);

   Py_DECREF(ret);
   return retval;
}

