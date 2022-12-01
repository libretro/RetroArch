/* xdelta3 - delta compression tools and library
   Copyright 2016 Joshua MacDonald

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

   http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/
#ifndef __XDELTA3_LIST__
#define __XDELTA3_LIST__

#define XD3_MAKELIST(LTYPE,ETYPE,LNAME)                                 \
                                                                        \
static inline ETYPE*                                                    \
LTYPE ## _entry (LTYPE* l)                                              \
{                                                                       \
  return (ETYPE*) ((char*) l - (ptrdiff_t) &((ETYPE*) 0)->LNAME);       \
}                                                                       \
                                                                        \
static inline void                                                      \
LTYPE ## _init (LTYPE *l)                                               \
{                                                                       \
  l->next = l;                                                          \
  l->prev = l;                                                          \
}                                                                       \
                                                                        \
static inline void                                                      \
LTYPE ## _add (LTYPE *prev, LTYPE *next, LTYPE *ins)                    \
{                                                                       \
  next->prev = ins;                                                     \
  prev->next = ins;                                                     \
  ins->next  = next;                                                    \
  ins->prev  = prev;                                                    \
}                                                                       \
                                                                        \
static inline void                                                      \
LTYPE ## _push_back (LTYPE *l, ETYPE *i)                                \
{                                                                       \
  LTYPE ## _add (l->prev, l, & i->LNAME);                               \
}                                                                       \
                                                                        \
static inline void                                                      \
LTYPE ## _del (LTYPE *next,                                             \
	       LTYPE *prev)                                             \
{                                                                       \
  next->prev = prev;                                                    \
  prev->next = next;                                                    \
}                                                                       \
                                                                        \
static inline ETYPE*                                                    \
LTYPE ## _remove (ETYPE *f)                                             \
{                                                                       \
  LTYPE *i = f->LNAME.next;                                             \
  LTYPE ## _del (f->LNAME.next, f->LNAME.prev);                         \
  return LTYPE ## _entry (i);                                           \
}                                                                       \
                                                                        \
static inline ETYPE*                                                    \
LTYPE ## _pop_back (LTYPE *l)                                           \
{                                                                       \
  LTYPE *i = l->prev;                                                   \
  LTYPE ## _del (i->next, i->prev);                                     \
  return LTYPE ## _entry (i);                                           \
}                                                                       \
                                                                        \
static inline ETYPE*                                                    \
LTYPE ## _pop_front (LTYPE *l)                                          \
{                                                                       \
  LTYPE *i = l->next;                                                   \
  LTYPE ## _del (i->next, i->prev);                                     \
  return LTYPE ## _entry (i);                                           \
}                                                                       \
                                                                        \
static inline int                                                       \
LTYPE ## _empty (LTYPE *l)                                              \
{                                                                       \
  return l == l->next;                                                  \
}                                                                       \
                                                                        \
static inline ETYPE*                                                    \
LTYPE ## _front (LTYPE *f)                                              \
{                                                                       \
  return LTYPE ## _entry (f->next);                                     \
}                                                                       \
                                                                        \
static inline ETYPE*                                                    \
LTYPE ## _back (LTYPE *f)                                               \
{                                                                       \
  return LTYPE ## _entry (f->prev);                                     \
}                                                                       \
                                                                        \
static inline int                                                       \
LTYPE ## _end (LTYPE *f, ETYPE *i)                                      \
{                                                                       \
  return f == & i->LNAME;                                               \
}                                                                       \
                                                                        \
static inline ETYPE*                                                    \
LTYPE ## _next (ETYPE *f)                                               \
{                                                                       \
  return LTYPE ## _entry (f->LNAME.next);                               \
}                                                                       \
                                                                        \
static inline usize_t                                                   \
LTYPE ## _length (LTYPE *l)                                             \
{                                                                       \
  LTYPE *p;                                                             \
  usize_t c = 0;                                                        \
                                                                        \
  for (p = l->next; p != l; p = p->next)                                \
    {                                                                   \
      c += 1;                                                           \
    }                                                                   \
                                                                        \
  return c;                                                             \
}                                                                       \
                                                                        \
typedef int unused_ ## LTYPE

#endif
