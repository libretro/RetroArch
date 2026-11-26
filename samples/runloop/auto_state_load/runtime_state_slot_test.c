#include <limits.h>
#include "../../../runtime_file.c"

static const char *fixture;
static unsigned errors;
static unsigned failures;

#define CHECK_SLOT(condition) do { \
   if (!(condition)) { \
      fprintf(stderr, "runtime slot line %d: %s\n", __LINE__, #condition); \
      failures++; \
   } \
} while (0)

int64_t filestream_read_file(const char *path, void **data, int64_t *len)
{
   (void)path;
   *len = (int64_t)strlen(fixture);
   *data = strdup(fixture);
   return *data != NULL;
}

bool path_is_valid(const char *path) { (void)path; return true; }
void RARCH_ERR(const char *fmt, ...) { (void)fmt; errors++; }

int main(void)
{
   runtime_log_t log;
   memset(&log, 0, sizeof(log));
   strcpy(log.path, "test.lrtl");
   log.state_slot = UINT_MAX;

   fixture = "{\"state_slot\":\"9\"}";
   runtime_log_read_file(&log);
   CHECK_SLOT(log.state_slot == 9);

   fixture = "{\"runtime\":\"0:00:01\"}";
   runtime_log_read_file(&log);
   CHECK_SLOT(log.state_slot == UINT_MAX && log.runtime.seconds == 1);
   CHECK_SLOT(errors == 0);

   fixture = "{\"state_slot\":\"0\"}";
   runtime_log_read_file(&log);
   CHECK_SLOT(log.state_slot == 0);

   fixture = "{\"state_slot\":\" +0\"}";
   runtime_log_read_file(&log);
   CHECK_SLOT(log.state_slot == 0 && errors == 0);
   fixture = "{\"state_slot\":\"4294967295\"}";
   runtime_log_read_file(&log);
   CHECK_SLOT(log.state_slot == UINT_MAX && errors == 0);

   log.state_slot = 9;
   fixture = "{\"state_slot\":\"invalid\"}";
   runtime_log_read_file(&log);
   CHECK_SLOT(errors > 0 && log.state_slot == 9);

   fixture = "{\"state_slot\":\"4294967296\"}";
   runtime_log_read_file(&log);
   CHECK_SLOT(log.state_slot == 9);

   log.state_slot = 9;
   fixture = "{\"state_slot\":\"-4294967295\"}";
   runtime_log_read_file(&log);
   CHECK_SLOT(log.state_slot == 9);

   log.state_slot = 9;
   fixture = "{\"play_count\":\"4294967296\",\"state_slot\":\"8\"}";
   runtime_log_read_file(&log);
   CHECK_SLOT(log.state_slot == 9);

   errno = ERANGE;
   fixture = "{\"play_count\":\"2\",\"state_slot\":\"8\"}";
   runtime_log_read_file(&log);
   CHECK_SLOT(log.state_slot == 8 && log.play_count == 2);
   puts(failures ? "runtime state-slot failures" : "runtime state-slot tests passed");
   return failures ? 1 : 0;
}
