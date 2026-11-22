/* The forced-FMA golden lane must not execute on an older x86 CPU. */
int main(void)
{
#if (defined(__GNUC__) || defined(__clang__)) && \
      (defined(__x86_64__) || defined(__i386__))
   __builtin_cpu_init();
   return __builtin_cpu_supports("fma") ? 0 : 77;
#else
   return 77;
#endif
}
