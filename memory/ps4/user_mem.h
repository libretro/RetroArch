#ifndef _USER_MEM_H
#define _USER_MEM_H

#define MEM_SIZE (0xA0000000) /* 2600 MiB */
#define MEM_ALIGN (16UL * 1024)

#if defined(__cplusplus)
extern "C" {
#endif
int malloc_init(void);
int malloc_finalize(void);
char *strdup(const char *s);
char *strndup(const char *s, size_t n);
int asprintf(char **bufp, const char *format, ...);
int vasprintf(char **bufp, const char *format, va_list ap);
void get_user_mem_size(size_t *max_mem, size_t *cur_mem);
#if defined(__cplusplus)
}
#endif

#if defined(__cplusplus)
void *user_new(std::size_t size) throw(std::bad_alloc);
void *user_new(std::size_t size, const std::nothrow_t& x) throw();
void *user_new_array(std::size_t size) throw(std::bad_alloc);
void *user_new_array(std::size_t size, const std::nothrow_t& x) throw();
void user_delete(void *ptr) throw();
void user_delete(void *ptr, const std::nothrow_t& x) throw();
void user_delete_array(void *ptr) throw();
void user_delete_array(void *ptr, const std::nothrow_t& x) throw();
#if (__cplusplus >= 201402L) // C++14
void user_delete(void *ptr, std::size_t size) throw();
void user_delete(void *ptr, std::size_t size, const std::nothrow_t& x) throw();
void user_delete_array(void *ptr, std::size_t size) throw();
void user_delete_array(void *ptr, std::size_t size, const std::nothrow_t& x) throw();
#endif // __cplusplus >= 201402L
#endif // __cplusplus
#endif // _USER_MEM_H
