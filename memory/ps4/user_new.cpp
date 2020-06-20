#include <new>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <string>
#include <memory>
#include <algorithm>
#include <sstream>
#include "../../defines/ps4_defines.h"
#include "user_mem.h"

void *user_new(std::size_t size) throw(std::bad_alloc)
{
  void *ptr;

  if (size == 0)
    size = 1;

  while ((ptr = (void *)malloc(size)) == NULL)
  {
    std::new_handler handler = std::get_new_handler();

    if (!handler)
      throw std::bad_alloc();
    else
      (*handler)();
  }
  return ptr;
}

void *user_new(std::size_t size, const std::nothrow_t& x) throw()
{
  void *ptr;
  (void)x;

  if (size == 0)
    size = 1;

  while ((ptr = (void *)malloc(size)) == NULL)
  {
    std::new_handler handler = std::get_new_handler();

    if (!handler)
      return NULL;

    try
    {
      (*handler)();
    }
    catch (std::bad_alloc)
    {
      return NULL;
    }
  }
  return ptr;
}

void *user_new_array(std::size_t size) throw(std::bad_alloc)
{
  return user_new(size);
}

void *user_new_array(std::size_t size, const std::nothrow_t& x) throw()
{
  return user_new(size, x);
}

void user_delete(void *ptr) throw()
{
  if (ptr != NULL)
    free(ptr);
}

void user_delete(void *ptr, const std::nothrow_t& x) throw()
{
  (void)x;
  if (ptr != NULL)
    free(ptr);
}

void user_delete_array(void *ptr) throw()
{
  user_delete(ptr);
}

void user_delete_array(void *ptr, const std::nothrow_t& x) throw()
{
  user_delete(ptr, x);
}

void *operator new(std::size_t size) throw(std::bad_alloc)
{
  return user_new(size);
}

void *operator new(std::size_t size, const std::nothrow_t& x) throw()
{
  return user_new(size, x);
}

void *operator new[](std::size_t size) throw(std::bad_alloc)
{
  return user_new_array(size);
}

void *operator new[](std::size_t size, const std::nothrow_t& x) throw()
{
  return user_new_array(size, x);
}

void operator delete(void *ptr) throw()
{
  user_delete(ptr);
}

void operator delete(void *ptr, const std::nothrow_t& x) throw()
{
  user_delete(ptr, x);
}

void operator delete[](void *ptr) throw()
{
  user_delete_array(ptr);
}

void operator delete[](void *ptr, const std::nothrow_t& x) throw()
{
  user_delete_array(ptr, x);
}

#if (__cplusplus >= 201402L) // C++14
void user_delete(void *ptr, std::size_t size) throw()
{
  (void)size;
  if (ptr != NULL)
    free(ptr);
}

void user_delete(void *ptr, std::size_t size, const std::nothrow_t& x) throw()
{
  (void)x;
  if (ptr != NULL)
    free(ptr);
}

void user_delete_array(void *ptr, std::size_t size) throw()
{
  user_delete(ptr, size);
}

void user_delete_array(void *ptr, std::size_t size, const std::nothrow_t& x) throw()
{
  user_delete(ptr, size, x);
}

void operator delete(void *ptr, std::size_t size) throw()
{
  user_delete(ptr, size);
}

void operator delete(void *ptr, std::size_t size, const std::nothrow_t& x) throw()
{
  user_delete(ptr, size, x);
}

void operator delete[](void *ptr, std::size_t size) throw()
{
  user_delete_array(ptr, size);
}

void operator delete[](void *ptr, std::size_t size, const std::nothrow_t& x) throw()
{
  user_delete_array(ptr, size, x);
}
#endif // C++14
