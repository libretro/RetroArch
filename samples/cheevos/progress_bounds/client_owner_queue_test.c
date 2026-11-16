/* Worker responses cross the libretro-common queue before touching a client. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <queues/mpsc_stack.h>
#include <rthreads/rthreads.h>
#include "rc_client.h"
#include "rc_error.h"

typedef struct response_message
{
   mpsc_stack_node_t node;
   rc_client_server_callback_t callback;
   void *userdata;
   const char *body;
} response_message_t;

static mpsc_stack_t responses;
static sthread_t *worker;
static uintptr_t owner;
static const char *next_body;
static unsigned callbacks, failures;
#define CHECK(c)                                                               \
   do                                                                          \
   {                                                                           \
      if (!(c))                                                                \
      {                                                                        \
         fprintf(stderr, "FAIL %u: %s\n", __LINE__, #c);                       \
         failures++;                                                           \
      }                                                                        \
   } while (0)

static void receive_response(void *userdata)
{
   response_message_t *m = (response_message_t *)userdata;
   CHECK((uintptr_t)sthread_get_current_thread_id() != owner);
   mpsc_stack_push(&responses, &m->node);
}
static void server_call(const rc_api_request_t *request,
                        rc_client_server_callback_t callback, void *userdata,
                        rc_client_t *client)
{
   response_message_t *m = (response_message_t *)calloc(1, sizeof(*m));
   (void)request;
   (void)client;
   CHECK((uintptr_t)sthread_get_current_thread_id() == owner);
   CHECK(worker == NULL && m != NULL);
   if (!m)
      exit(2);
   m->callback = callback;
   m->userdata = userdata;
   m->body = next_body;
   worker = sthread_create(receive_response, m);
   CHECK(worker != NULL);
}
static void drain_response(void)
{
   mpsc_stack_node_t *node;
   if (worker)
   {
      sthread_join(worker);
      worker = NULL;
   }
   node = mpsc_stack_drain(&responses);
   while (node)
   {
      response_message_t *m = (response_message_t *)node;
      rc_api_server_response_t response;
      node = node->next;
      memset(&response, 0, sizeof(response));
      response.body = m->body;
      response.body_length = strlen(m->body);
      response.http_status_code = 200;
      m->callback(&response, m->userdata);
      free(m);
   }
}
static void login_done(int result, const char *error, rc_client_t *client,
                       void *userdata)
{
   (void)error;
   (void)userdata;
   CHECK((uintptr_t)sthread_get_current_thread_id() == owner);
   CHECK(result == RC_OK && rc_client_get_user_info(client) != NULL);
   callbacks++;
}
static void entries_done(int result, const char *error,
                         rc_client_leaderboard_entry_list_t *list,
                         rc_client_t *client, void *userdata)
{
   (void)result;
   (void)error;
   (void)client;
   (void)userdata;
   CHECK((uintptr_t)sthread_get_current_thread_id() == owner);
   if (list)
      rc_client_destroy_leaderboard_entry_list(list);
   callbacks++;
}
int api_json_test(void);

int main(void)
{
   rc_client_t *client;
   rc_client_async_handle_t *handle;
   unsigned i;
   failures += api_json_test();
   owner = (uintptr_t)sthread_get_current_thread_id();
   mpsc_stack_init(&responses);
   client = rc_client_create(NULL, server_call);
   CHECK(client != NULL);
   if (!client)
      return 1;
   next_body = "{\"Success\":true,\"User\":\"queue-test\",\"Token\":\"test-"
               "token\",\"Score\":1,\"SoftcoreScore\":0,\"Messages\":0}";
   handle = rc_client_begin_login_with_token(client, "queue-test", "test-token",
                                             login_done, NULL);
   CHECK(handle != NULL && callbacks == 0);
   drain_response();
   CHECK(callbacks == 1);
   next_body = "{\"Success\":false,\"Error\":\"cancelled fixture\"}";
   for (i = 0; i < 100; i++)
   {
      handle = rc_client_begin_fetch_leaderboard_entries(client, 1, 1, 10,
                                                         entries_done, NULL);
      CHECK(handle != NULL);
      rc_client_abort_async(client, handle);
      rc_client_idle(client);
      drain_response();
      CHECK(callbacks == 1);
   }
   handle = rc_client_begin_fetch_leaderboard_entries(client, 1, 1, 10,
                                                      entries_done, NULL);
   CHECK(handle != NULL);
   rc_client_destroy(client);
   drain_response();
   CHECK(callbacks == 1 && mpsc_stack_empty(&responses));
   if (failures)
      return 1;
   puts("client_owner_queue_test: owner callbacks, cancellation and late "
        "destruction passed");
   return 0;
}
