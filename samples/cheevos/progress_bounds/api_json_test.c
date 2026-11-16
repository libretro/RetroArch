/* Real rcheevos response readers over immutable, exactly bounded HTTP bodies. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../../deps/rcheevos/src/rapi/rc_api_common.h"
#include "rc_api_runtime.h"
#include "rc_error.h"

static int failures;
#define CHECK(c) do { if (!(c)) { \
   fprintf(stderr, "API JSON FAIL %d: %s\n", __LINE__, #c); failures++; } } while (0)

int api_json_test(void)
{
   static const char document[] =
      "{\"Success\":true,\"Error\":null,\"\\u004eame\":\"A\\b\\f\\n\\r\\t\\/\\\\\\\"\\u00e9\\uD83D\\uDE00\","
      "\"Items\":[1,4294967295],\"Nested\":{\"first\":1,\"second\":[]},\"Name\":\"ignored duplicate\"}";
   static const char game[] =
      "{\"Success\":true,\"PatchData\":{\"ID\":1,\"Title\":\"Test\\nGame\",\"ConsoleID\":3,"
      "\"ImageIcon\":\"/Images/123.png\",\"RichPresencePatch\":\"Display:\\nTest\","
      "\"Achievements\":[{\"ID\":5,\"Title\":\"First\",\"Description\":\"A\\tB\",\"Flags\":3,"
      "\"Points\":5,\"MemAddr\":\"0xH0000=1\",\"Author\":\"Author\",\"BadgeName\":\"12345\","
      "\"Created\":1234,\"Modified\":1235}],"
      "\"Leaderboards\":[]}}";
   rc_json_field_t fields[] = { RC_JSON_NEW_FIELD("Success"), RC_JSON_NEW_FIELD("Error"),
      RC_JSON_NEW_FIELD("Code"), RC_JSON_NEW_FIELD("Name"), RC_JSON_NEW_FIELD("Items"),
      RC_JSON_NEW_FIELD("Nested"), RC_JSON_NEW_FIELD("Missing") };
   rc_api_server_response_t http;
   rc_api_response_t response;
   rc_api_fetch_game_data_response_t fetched;
   const char *name = NULL;
   uint32_t *items = NULL, count = 0;
   size_t i;
   memset(&http, 0, sizeof(http));
   http.body = document;
   http.body_length = sizeof(document) - 1;
   http.http_status_code = 200;
   memset(&response, 0, sizeof(response));
   rc_buffer_init(&response.buffer);
   CHECK(rc_json_parse_server_response(&response, &http, fields, 7) == RC_OK);
   CHECK(response.succeeded && !response.error_message);
   CHECK(fields[3].value_start >= document && fields[3].value_end < document + sizeof(document));
   CHECK(fields[4].array_size == 2 && fields[5].array_size == 2);
   CHECK(fields[3].validated && fields[4].validated && fields[5].validated);
   CHECK(!fields[6].validated);
   {
      rc_json_field_t nested[] = {RC_JSON_NEW_FIELD("second"), RC_JSON_NEW_FIELD("first")};
      rc_json_field_t raw = RC_JSON_NEW_FIELD("Raw");
      static const char invalid[] = "{\"first\":+1}";
      CHECK(rc_json_get_required_object(nested, 2, &response, &fields[5], "Nested"));
      CHECK(nested[0].validated && nested[0].array_size == 0 && nested[1].validated);
      raw.value_start = invalid;
      raw.value_end = invalid + sizeof(invalid) - 1;
      CHECK(!rc_json_get_required_object(nested, 2, &response, &raw, "Raw"));
      CHECK(!nested[0].validated && !nested[1].validated);
   }
   CHECK(rc_json_get_required_string(&name, &response, &fields[3], "Name"));
   CHECK(name && !strcmp(name, "A\b\f\n\r\t/\\\"\xc3\xa9\xf0\x9f\x98\x80"));
   CHECK(rc_json_field_string_matches(&fields[3], name));
   CHECK(rc_json_get_required_unum_array(&items, &count, &response, &fields[4], "Items") == RC_OK);
   CHECK(count == 2 && items && items[0] == 1 && items[1] == UINT32_MAX);
   CHECK(!rc_json_get_required_string(&name, &response, &fields[6], "Missing"));
   CHECK(response.error_message && strstr(response.error_message, "Missing"));
   rc_buffer_destroy(&response.buffer);

   http.body_length = sizeof(game) - 1;
   {
      char *body = (char *)malloc(http.body_length);
      CHECK(body != NULL);
      if (!body) return failures;
      memcpy(body, game, http.body_length);
      http.body = body;
      CHECK(rc_api_process_fetch_game_data_server_response(&fetched, &http) == RC_OK);
      memset(body, 0, http.body_length);
      free(body);
   }
   CHECK(fetched.id == 1 && fetched.console_id == 3 && fetched.num_achievements == 1);
   CHECK(fetched.title && !strcmp(fetched.title, "Test\nGame"));
   CHECK(fetched.image_name && !strcmp(fetched.image_name, "123"));
   if (fetched.num_achievements == 1)
   {
      CHECK(fetched.achievements[0].id == 5);
      CHECK(!strcmp(fetched.achievements[0].description, "A\tB"));
      CHECK(!strcmp(fetched.achievements[0].badge_name, "12345"));
   }
   rc_api_destroy_fetch_game_data_response(&fetched);
   for (i = 1; i < sizeof(game) - 1; i++)
   {
      char *body = (char *)malloc(i);
      CHECK(body != NULL);
      if (!body) return failures;
      memcpy(body, game, i);
      http.body = body;
      http.body_length = i;
      CHECK(rc_api_process_fetch_game_data_server_response(&fetched, &http) != RC_OK);
      rc_api_destroy_fetch_game_data_response(&fetched);
      free(body);
   }
   printf("rcheevos borrowed response tests: %s\n", failures ? "FAILED" : "passed");
   return failures;
}
