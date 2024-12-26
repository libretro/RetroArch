/*
 * To-do:
 * - Some sort of connection control, it only sends packets
 *   but there is no acknoledgement of a connection o keepalives
 * - Send player name
 * - Render something on-screen maybe a gui to configure IP and port
     instead of the ridiculously long strings we're using now
 * - Allow changing IP address and port in runtime
 * - Input recording / Combos
 * - Enable test input loading from menu
 * - Visualization of aux inputs (gyro, accelero, light)
*/

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>

#include <net/net_compat.h>
#include <net/net_socket.h>

#include <retro_miscellaneous.h>
#include <retro_timers.h>

#include <libretro.h>

#include <file/file_path.h>
#include <string/stdstring.h>
#include <streams/file_stream.h>
#include <formats/rjson.h>

#ifndef SOCKET_ERROR
#define SOCKET_ERROR -1
#endif

#ifdef RARCH_INTERNAL
#include "internal_cores.h"
#define NETRETROPAD_CORE_PREFIX(s) libretro_netretropad_##s
#else
#define NETRETROPAD_CORE_PREFIX(s) s
#endif

#include "remotepad.h"

#define DESC_NUM_PORTS(desc) ((desc)->port_max - (desc)->port_min + 1)
#define DESC_NUM_INDICES(desc) ((desc)->index_max - (desc)->index_min + 1)
#define DESC_NUM_IDS(desc) ((desc)->id_max - (desc)->id_min + 1)

#define DESC_OFFSET(desc, port, index, id) ( \
   port * ((desc)->index_max - (desc)->index_min + 1) * ((desc)->id_max - (desc)->id_min + 1) + \
   (index - (desc)->index_min) * ((desc)->id_max - (desc)->id_min + 1) + \
   id \
)

#define MAX_TEST_STEPS 200
#define INITIAL_FRAMES 60*5
#define ONE_TEST_STEP_FRAMES 60*5
#define KEYBOARD_OFFSET 1000
#define NETRETROPAD_SCREEN_PAD 0
#define NETRETROPAD_SCREEN_KEYBOARD 1

struct descriptor {
   int device;
   int port_min;
   int port_max;
   int index_min;
   int index_max;
   int id_min;
   int id_max;
   uint16_t *value;
};

struct remote_joypad_message {
   int port;
   int device;
   int index;
   int id;
   uint16_t state;
};

static bool keyboard_state[RETROK_LAST];
static bool keyboard_state_validated[RETROK_LAST];

static int s;
static int port;
static char server[64];
static struct sockaddr_in si_other;

static struct retro_log_callback logger;

static retro_log_printf_t NETRETROPAD_CORE_PREFIX(log_cb);
static retro_video_refresh_t NETRETROPAD_CORE_PREFIX(video_cb);
static retro_audio_sample_t NETRETROPAD_CORE_PREFIX(audio_cb);
static retro_audio_sample_batch_t NETRETROPAD_CORE_PREFIX(audio_batch_cb);
static retro_environment_t NETRETROPAD_CORE_PREFIX(environ_cb);
static retro_input_poll_t NETRETROPAD_CORE_PREFIX(input_poll_cb);
static retro_input_state_t NETRETROPAD_CORE_PREFIX(input_state_cb);

static uint16_t *frame_buf;

static struct descriptor joypad = {
   .device = RETRO_DEVICE_JOYPAD,
   .port_min = 0,
   .port_max = 0,
   .index_min = 0,
   .index_max = 0,
   .id_min = RETRO_DEVICE_ID_JOYPAD_B,
   .id_max = RETRO_DEVICE_ID_JOYPAD_R3
};

static struct descriptor analog = {
   .device = RETRO_DEVICE_ANALOG,
   .port_min = 0,
   .port_max = 0,
   .index_min = RETRO_DEVICE_INDEX_ANALOG_LEFT,
   .index_max = RETRO_DEVICE_INDEX_ANALOG_RIGHT,
   .id_min = RETRO_DEVICE_ID_ANALOG_X,
   .id_max = RETRO_DEVICE_ID_ANALOG_Y
};

static struct descriptor analog_button = {
   .device = RETRO_DEVICE_ANALOG,
   .port_min = 0,
   .port_max = 0,
   .index_min = RETRO_DEVICE_INDEX_ANALOG_BUTTON,
   .index_max = RETRO_DEVICE_INDEX_ANALOG_BUTTON,
   .id_min = RETRO_DEVICE_ID_JOYPAD_B,
   .id_max = RETRO_DEVICE_ID_JOYPAD_R3
};

static struct descriptor *descriptors[] = {
   &joypad,
   &analog,
   &analog_button
};

static uint16_t analog_item_colors[32];

static uint16_t combo_def[] =
{
   1 << RETRO_DEVICE_ID_JOYPAD_UP    | 1 << RETRO_DEVICE_ID_JOYPAD_LEFT,  /* D-pad diagonals */
   1 << RETRO_DEVICE_ID_JOYPAD_UP    | 1 << RETRO_DEVICE_ID_JOYPAD_RIGHT,
   1 << RETRO_DEVICE_ID_JOYPAD_DOWN  | 1 << RETRO_DEVICE_ID_JOYPAD_LEFT,
   1 << RETRO_DEVICE_ID_JOYPAD_DOWN  | 1 << RETRO_DEVICE_ID_JOYPAD_RIGHT,
   1 << RETRO_DEVICE_ID_JOYPAD_UP    | 1 << RETRO_DEVICE_ID_JOYPAD_DOWN,  /* D-pad opposites */
   1 << RETRO_DEVICE_ID_JOYPAD_LEFT  | 1 << RETRO_DEVICE_ID_JOYPAD_RIGHT,
   1 << RETRO_DEVICE_ID_JOYPAD_L3    | 1 << RETRO_DEVICE_ID_JOYPAD_R3,    /* Combo values for menu / exit */
   1 << RETRO_DEVICE_ID_JOYPAD_DOWN  | 1 << RETRO_DEVICE_ID_JOYPAD_Y | 1 << RETRO_DEVICE_ID_JOYPAD_L | 1 << RETRO_DEVICE_ID_JOYPAD_R,
   1 << RETRO_DEVICE_ID_JOYPAD_START | 1 << RETRO_DEVICE_ID_JOYPAD_SELECT | 1 << RETRO_DEVICE_ID_JOYPAD_L | 1 << RETRO_DEVICE_ID_JOYPAD_R,
   1 << RETRO_DEVICE_ID_JOYPAD_START | 1 << RETRO_DEVICE_ID_JOYPAD_SELECT,
   1 << RETRO_DEVICE_ID_JOYPAD_L3    | 1 << RETRO_DEVICE_ID_JOYPAD_R,
   1 << RETRO_DEVICE_ID_JOYPAD_L     | 1 << RETRO_DEVICE_ID_JOYPAD_R,
   1 << RETRO_DEVICE_ID_JOYPAD_DOWN  | 1 << RETRO_DEVICE_ID_JOYPAD_SELECT,
   1 << RETRO_DEVICE_ID_JOYPAD_L2    | 1 << RETRO_DEVICE_ID_JOYPAD_R2
};

static unsigned current_screen = NETRETROPAD_SCREEN_PAD;

typedef struct
{
   unsigned expected_button;
   char message[PATH_MAX_LENGTH];
   bool detected;
} input_test_step_t;

static input_test_step_t input_test_steps[MAX_TEST_STEPS];

static unsigned current_frame         = 0;
static unsigned next_teststep_frame   = 0;
static unsigned current_test_step     = 0;
static unsigned last_test_step        = MAX_TEST_STEPS + 1;
static uint32_t input_state_validated = 0;
static uint32_t combo_state_validated = 0;
static bool     dump_state_blocked    = false;
static bool     hide_analog_mismatch  = true;
/************************************/
/* JSON Helpers for test input file */
/************************************/

typedef struct
{
   unsigned *current_entry_uint_val;
   char **current_entry_str_val;
   unsigned expected_button;
   char *message;
} ITifJSONContext;

static bool ITifJSONObjectEndHandler(void* context)
{
   ITifJSONContext *pCtx = (ITifJSONContext*)context;

   /* Too long input is handled elsewhere, it should not lead to parse error */
   if (current_test_step >= MAX_TEST_STEPS)
      return true;

   /* Copy values read from JSON file */
   input_test_steps[current_test_step].expected_button = pCtx->expected_button;

   if (!string_is_empty(pCtx->message))
      strlcpy(
            input_test_steps[current_test_step].message, pCtx->message,
            sizeof(input_test_steps[current_test_step].message));
   else
      input_test_steps[current_test_step].message[0] = '\0';
   current_test_step++;
   last_test_step = current_test_step;

   return true;
}

static bool ITifJSONObjectMemberHandler(void* context, const char *pValue, size_t length)
{
   ITifJSONContext *pCtx = (ITifJSONContext*)context;

   /* something went wrong */
   if (pCtx->current_entry_str_val)
      return false;

   if (length)
   {
      if (string_is_equal(pValue, "expected_button"))
         pCtx->current_entry_uint_val = &pCtx->expected_button;
      else if (string_is_equal(pValue, "message"))
         pCtx->current_entry_str_val = &pCtx->message;
      /* ignore unknown members */
   }

   return true;
}

static bool ITifJSONNumberHandler(void* context, const char *pValue, size_t length)
{
   ITifJSONContext *pCtx = (ITifJSONContext*)context;

   if (pCtx->current_entry_uint_val && length && !string_is_empty(pValue))
      *pCtx->current_entry_uint_val = string_to_unsigned(pValue);
   /* ignore unknown members */

   pCtx->current_entry_uint_val = NULL;

   return true;
}

static bool ITifJSONStringHandler(void* context, const char *pValue, size_t length)
{
   ITifJSONContext *pCtx = (ITifJSONContext*)context;

   if (pCtx->current_entry_str_val && length && !string_is_empty(pValue))
   {
      if (*pCtx->current_entry_str_val)
         free(*pCtx->current_entry_str_val);

      *pCtx->current_entry_str_val = strdup(pValue);
   }
   /* ignore unknown members */

   pCtx->current_entry_str_val = NULL;

   return true;
}

/* Parses test input file referenced by file_path.
 * Does nothing if test input file does not exist. */
static bool input_test_file_read(const char* file_path)
{
   bool success            = false;
   ITifJSONContext context = {0};
   RFILE *file             = NULL;
   rjson_t* parser;

   /* Sanity check */
   if (    string_is_empty(file_path)
       || !path_is_valid(file_path)
      )
      return false;

   /* Attempt to open test input file */
   file = filestream_open(
         file_path,
         RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);

   if (!file)
   {
      NETRETROPAD_CORE_PREFIX(log_cb)(RETRO_LOG_ERROR,
            "[Remote RetroPad]: Failed to open test input file: \"%s\".\n",
            file_path);
      return false;
   }

   /* Initialise JSON parser */
   if (!(parser = rjson_open_rfile(file)))
   {
      NETRETROPAD_CORE_PREFIX(log_cb)(RETRO_LOG_ERROR,"[Remote RetroPad]: Failed to create JSON parser.\n");
      goto end;
   }

   /* Configure parser */
   rjson_set_options(parser, RJSON_OPTION_ALLOW_UTF8BOM);

   /* Read file */
   if (rjson_parse(parser, &context,
         ITifJSONObjectMemberHandler,
         ITifJSONStringHandler,
         ITifJSONNumberHandler,
         NULL, ITifJSONObjectEndHandler, NULL, NULL, /* object/array handlers */
         NULL, NULL) /* unused boolean/null handlers */
         != RJSON_DONE)
   {
      if (rjson_get_source_context_len(parser))
      {
         NETRETROPAD_CORE_PREFIX(log_cb)(RETRO_LOG_ERROR,
               "[Remote RetroPad]: Error parsing chunk of test input file: %s\n---snip---\n%.*s\n---snip---\n",
               file_path,
               rjson_get_source_context_len(parser),
               rjson_get_source_context_buf(parser));
      }
      NETRETROPAD_CORE_PREFIX(log_cb)(RETRO_LOG_WARN,
            "[Remote RetroPad]: Error parsing test input file: %s\n",
            file_path);
      NETRETROPAD_CORE_PREFIX(log_cb)(RETRO_LOG_ERROR,
            "[Remote RetroPad]: Error: Invalid JSON at line %d, column %d - %s.\n",
            (int)rjson_get_source_line(parser),
            (int)rjson_get_source_column(parser),
            (*rjson_get_error(parser) ? rjson_get_error(parser) : "format error"));
   }

   /* Free parser */
   rjson_free(parser);

   success = true;
end:
   /* Clean up leftover strings */
   if (context.message)
      free(context.message);

   /* Close log file */
   filestream_close(file);

   if (last_test_step >= MAX_TEST_STEPS)
   {
      NETRETROPAD_CORE_PREFIX(log_cb)(RETRO_LOG_WARN,"[Remote RetroPad]: too long test input json, maximum size: %d\n",MAX_TEST_STEPS);
   }
   for (current_test_step = 0; current_test_step < last_test_step; current_test_step++)
   {
      NETRETROPAD_CORE_PREFIX(log_cb)(RETRO_LOG_DEBUG,
         "[Remote RetroPad]: test step %02d read from file: button %x, message %s\n",
         current_test_step,
         input_test_steps[current_test_step].expected_button,
         input_test_steps[current_test_step].message);
   }
   current_test_step = 0;
   return success;
}

/********************************/
/* Test input file handling end */
/********************************/

static void draw_background(void)
{
   if (frame_buf)
   {
      unsigned rle, runs, count;
      /* Body is 255 * 142 within the 320 * 240 frame */
      uint16_t *pixel = frame_buf + 49 * 320 + 32;

      if (current_screen == NETRETROPAD_SCREEN_PAD)
         for (rle = 0; rle < sizeof(body); )
         {
            uint16_t color = 0;

            for (runs = body[rle++]; runs > 0; runs--)
            {
               for (count = body[rle++]; count > 0; count--)
                  *pixel++ = color;

               color = 0x4208 - color;
            }

            pixel += 65;
         }
      else if (current_screen == NETRETROPAD_SCREEN_KEYBOARD)
         for (rle = 0; rle < sizeof(keyboard_body); )
         {
            uint16_t color = 0;

            for (runs = keyboard_body[rle++]; runs > 0; runs--)
            {
               for (count = keyboard_body[rle++]; count > 0; count--)
                  *pixel++ = color;

               color = 0x4208 - color;
            }

            pixel += 65;
         }
   }
}

static void flip_screen(void)
{
   if      (current_screen == NETRETROPAD_SCREEN_PAD)
      current_screen = NETRETROPAD_SCREEN_KEYBOARD;
   else if (current_screen == NETRETROPAD_SCREEN_KEYBOARD)
      current_screen = NETRETROPAD_SCREEN_PAD;
   draw_background();
}

void NETRETROPAD_CORE_PREFIX(retro_init)(void)
{
   unsigned i;

   dump_state_blocked = false;
   frame_buf = (uint16_t*)calloc(320 * 240, sizeof(uint16_t));

   draw_background();

   /* Allocate descriptor values */
   for (i = 0; i < ARRAY_SIZE(descriptors); i++)
   {
      struct descriptor *desc = descriptors[i];
      int                size = DESC_NUM_PORTS(desc) * DESC_NUM_INDICES(desc) * DESC_NUM_IDS(desc);
      descriptors[i]->value   = (uint16_t*)calloc(size, sizeof(uint16_t));
   }

   NETRETROPAD_CORE_PREFIX(log_cb)(RETRO_LOG_INFO, "Initialising sockets...\n");
   network_init();

}

void NETRETROPAD_CORE_PREFIX(retro_deinit)(void)
{
   unsigned i;

   if (frame_buf)
      free(frame_buf);
   frame_buf = NULL;

   /* Free descriptor values */
   for (i = 0; i < ARRAY_SIZE(descriptors); i++)
   {
      free(descriptors[i]->value);
      descriptors[i]->value = NULL;
   }
}

unsigned NETRETROPAD_CORE_PREFIX(retro_api_version)(void)
{
   return RETRO_API_VERSION;
}

void NETRETROPAD_CORE_PREFIX(retro_set_controller_port_device)(
      unsigned port, unsigned device)
{
   (void)port;
   (void)device;
}

void NETRETROPAD_CORE_PREFIX(retro_get_system_info)(
      struct retro_system_info *info)
{
   memset(info, 0, sizeof(*info));
   info->library_name     = "RetroPad Remote";
   info->library_version  = "0.01";
   info->need_fullpath    = true;
   info->valid_extensions = "ratst"; /* Special test input file. */
}

void NETRETROPAD_CORE_PREFIX(retro_get_system_av_info)(
      struct retro_system_av_info *info)
{
   info->timing.fps = 60.0;
   info->timing.sample_rate = 30000.0;

   info->geometry.base_width  = 320;
   info->geometry.base_height = 240;
   info->geometry.max_width   = 320;
   info->geometry.max_height  = 240;
   info->geometry.aspect_ratio = 4.0 / 3.0;
}

static void NETRETROPAD_CORE_PREFIX(update_keyboard_cb)(bool down, unsigned keycode,
                               uint32_t character, uint16_t key_modifiers)
{
   struct retro_message message;
   char buf[255];

   if (keycode < RETROK_LAST)
   {
      keyboard_state[keycode] = down ? true : false;
      if (down && ((keycode == RETROK_a && keyboard_state[RETROK_b]) || (keycode == RETROK_b && keyboard_state[RETROK_a])))
         flip_screen();
      /* Message for the keypresses not shown as actual keys, just placeholder blocks */
      if ((keycode ==   0) ||
          (keycode ==  12) ||
          (keycode >=  33  && keycode < 39)  ||
          (keycode >=  40  && keycode < 44)  ||
          (keycode ==  58) ||
          (keycode ==  60) ||
          (keycode >=  62  && keycode < 65)  ||
          (keycode >=  94  && keycode < 96)  ||
          (keycode >= 123  && keycode < 127) ||
          (keycode == 272) ||
          (keycode >= 294  && keycode < 297) ||
          (keycode >= 309  && keycode < RETROK_LAST))
      {
         snprintf(buf, sizeof(buf), "Key pressed: %d",keycode);
            message.msg = buf;
            message.frames = 60;
            NETRETROPAD_CORE_PREFIX(environ_cb)(RETRO_ENVIRONMENT_SET_MESSAGE, &message);
      }
   }
}

static void retropad_update_input(void)
{
   unsigned i;

   /* Poll input */
   NETRETROPAD_CORE_PREFIX(input_poll_cb)();

   /* Parse descriptors */
   for (i = 0; i < ARRAY_SIZE(descriptors); i++)
   {
      int port;
      /* Get current descriptor */
      struct descriptor *desc = descriptors[i];

      /* Go through range of ports/indices/IDs */
      for (port = desc->port_min; port <= desc->port_max; port++)
      {
         int index;

         for (index = desc->index_min; index <= desc->index_max; index++)
         {
            int id;

            for (id = desc->id_min; id <= desc->id_max; id++)
            {
               struct remote_joypad_message msg;

               /* Compute offset into array */
               int offset     = DESC_OFFSET(desc, port, index, id);

               /* Get old state */
               uint16_t old   = desc->value[offset];

               /* Get new state */
               uint16_t state = NETRETROPAD_CORE_PREFIX(input_state_cb)(
                     port,
                     desc->device,
                     index,
                     id);

               /* Continue if state is unchanged */
               if (state == old)
                  continue;

               /* Update state */
               desc->value[offset] = state;

               /* Do not send analog button state - RA side is not prepared to receive it */
               if (i>1)
                  continue;

               /* Otherwise, attempt to send updated state */
               msg.port            = port;
               msg.device          = desc->device;
               msg.index           = index;
               msg.id              = id;
               msg.state           = state;

               if (sendto(s, (char*)&msg, sizeof(msg), 0, (struct sockaddr *)&si_other, sizeof(si_other)) == -1)
                  NETRETROPAD_CORE_PREFIX(log_cb)(RETRO_LOG_INFO, "Error sending data!\n");
            }
         }
      }
   }
}

static void netretropad_open_udp_socket(void)
{
   socket_target_t in_target;

   if (s && s != SOCKET_ERROR)
      socket_close(s);

   s = socket_create(
         "retropad",
         SOCKET_DOMAIN_INET,
         SOCKET_TYPE_DATAGRAM,
         SOCKET_PROTOCOL_UDP);

   if (s == SOCKET_ERROR)
      NETRETROPAD_CORE_PREFIX(log_cb)(RETRO_LOG_INFO, "socket failed");

   /* setup address structure */
   memset((char *) &si_other, 0, sizeof(si_other));

   in_target.port   = port;
   in_target.server = server;
   in_target.domain = SOCKET_DOMAIN_INET;

   socket_set_target(&si_other, &in_target);

   NETRETROPAD_CORE_PREFIX(log_cb)(RETRO_LOG_INFO, "Server IP Address: %s\n" , server);

}

void NETRETROPAD_CORE_PREFIX(retro_set_environment)(retro_environment_t cb)
{
   static const struct retro_variable vars[] = {
      { "net_retropad_port", "Port; 55400|55401|55402|55403|55404|55405|55406|55407|55408|55409|55410|55411|55412|55413|55414|55415|55416|55417|55418|55419|55420" },
      { "net_retropad_ip_octet1", "IP address part 1; 0|1|2|3|4|5|6|7|8|9|10|11|12|13|14|15|16|17|18|19|20|21|22|23|24|25|26|27|28|29|30|31|32|33|34|35|36|37|38|39|40|41|42|43|44|45|46|47|48|49|50|51|52|53|54|55|56|57|58|59|60|61|62|63|64|65|66|67|68|69|70|71|72|73|74|75|76|77|78|79|80|81|82|83|84|85|86|87|88|89|90|91|92|93|94|95|96|97|98|99|100|101|102|103|104|105|106|107|108|109|110|111|112|113|114|115|116|117|118|119|120|121|122|123|124|125|126|127|128|129|130|131|132|133|134|135|136|137|138|139|140|141|142|143|144|145|146|147|148|149|150|151|152|153|154|155|156|157|158|159|160|161|162|163|164|165|166|167|168|169|170|171|172|173|174|175|176|177|178|179|180|181|182|183|184|185|186|187|188|189|190|191|192|193|194|195|196|197|198|199|200|201|202|203|204|205|206|207|208|209|210|211|212|213|214|215|216|217|218|219|220|221|222|223|224|225|226|227|228|229|230|231|232|233|234|235|236|237|238|239|240|241|242|243|244|245|246|247|248|249|250|251|252|253|254|255" },
      { "net_retropad_ip_octet2", "IP address part 2; 0|1|2|3|4|5|6|7|8|9|10|11|12|13|14|15|16|17|18|19|20|21|22|23|24|25|26|27|28|29|30|31|32|33|34|35|36|37|38|39|40|41|42|43|44|45|46|47|48|49|50|51|52|53|54|55|56|57|58|59|60|61|62|63|64|65|66|67|68|69|70|71|72|73|74|75|76|77|78|79|80|81|82|83|84|85|86|87|88|89|90|91|92|93|94|95|96|97|98|99|100|101|102|103|104|105|106|107|108|109|110|111|112|113|114|115|116|117|118|119|120|121|122|123|124|125|126|127|128|129|130|131|132|133|134|135|136|137|138|139|140|141|142|143|144|145|146|147|148|149|150|151|152|153|154|155|156|157|158|159|160|161|162|163|164|165|166|167|168|169|170|171|172|173|174|175|176|177|178|179|180|181|182|183|184|185|186|187|188|189|190|191|192|193|194|195|196|197|198|199|200|201|202|203|204|205|206|207|208|209|210|211|212|213|214|215|216|217|218|219|220|221|222|223|224|225|226|227|228|229|230|231|232|233|234|235|236|237|238|239|240|241|242|243|244|245|246|247|248|249|250|251|252|253|254|255" },
      { "net_retropad_ip_octet3", "IP address part 3; 0|1|2|3|4|5|6|7|8|9|10|11|12|13|14|15|16|17|18|19|20|21|22|23|24|25|26|27|28|29|30|31|32|33|34|35|36|37|38|39|40|41|42|43|44|45|46|47|48|49|50|51|52|53|54|55|56|57|58|59|60|61|62|63|64|65|66|67|68|69|70|71|72|73|74|75|76|77|78|79|80|81|82|83|84|85|86|87|88|89|90|91|92|93|94|95|96|97|98|99|100|101|102|103|104|105|106|107|108|109|110|111|112|113|114|115|116|117|118|119|120|121|122|123|124|125|126|127|128|129|130|131|132|133|134|135|136|137|138|139|140|141|142|143|144|145|146|147|148|149|150|151|152|153|154|155|156|157|158|159|160|161|162|163|164|165|166|167|168|169|170|171|172|173|174|175|176|177|178|179|180|181|182|183|184|185|186|187|188|189|190|191|192|193|194|195|196|197|198|199|200|201|202|203|204|205|206|207|208|209|210|211|212|213|214|215|216|217|218|219|220|221|222|223|224|225|226|227|228|229|230|231|232|233|234|235|236|237|238|239|240|241|242|243|244|245|246|247|248|249|250|251|252|253|254|255" },
      { "net_retropad_ip_octet4", "IP address part 4; 0|1|2|3|4|5|6|7|8|9|10|11|12|13|14|15|16|17|18|19|20|21|22|23|24|25|26|27|28|29|30|31|32|33|34|35|36|37|38|39|40|41|42|43|44|45|46|47|48|49|50|51|52|53|54|55|56|57|58|59|60|61|62|63|64|65|66|67|68|69|70|71|72|73|74|75|76|77|78|79|80|81|82|83|84|85|86|87|88|89|90|91|92|93|94|95|96|97|98|99|100|101|102|103|104|105|106|107|108|109|110|111|112|113|114|115|116|117|118|119|120|121|122|123|124|125|126|127|128|129|130|131|132|133|134|135|136|137|138|139|140|141|142|143|144|145|146|147|148|149|150|151|152|153|154|155|156|157|158|159|160|161|162|163|164|165|166|167|168|169|170|171|172|173|174|175|176|177|178|179|180|181|182|183|184|185|186|187|188|189|190|191|192|193|194|195|196|197|198|199|200|201|202|203|204|205|206|207|208|209|210|211|212|213|214|215|216|217|218|219|220|221|222|223|224|225|226|227|228|229|230|231|232|233|234|235|236|237|238|239|240|241|242|243|244|245|246|247|248|249|250|251|252|253|254|255" },
      { "net_retropad_screen", "Start screen; Retropad|Keyboard tester" },
      { "net_retropad_hide_analog_mismatch", "Hide mismatching analog button inputs; True|False" },
      { NULL, NULL },
   };
   enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_RGB565;
   struct retro_keyboard_callback kcb = { NETRETROPAD_CORE_PREFIX(update_keyboard_cb) };

   cb(RETRO_ENVIRONMENT_SET_VARIABLES, (void*)vars);

   NETRETROPAD_CORE_PREFIX(environ_cb) = cb;
   bool no_content = true;
   cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &no_content);

   NETRETROPAD_CORE_PREFIX(environ_cb)(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt);

   if (cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &logger))
      NETRETROPAD_CORE_PREFIX(log_cb) = logger.log;

   NETRETROPAD_CORE_PREFIX(environ_cb)(RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK, &kcb);

}

static void netretropad_check_variables(void)
{
   struct retro_variable var, var2, var3, var4, port_var, screen_var, hide_a_var;
   var.key        = "net_retropad_ip_octet1";
   var2.key       = "net_retropad_ip_octet2";
   var3.key       = "net_retropad_ip_octet3";
   var4.key       = "net_retropad_ip_octet4";
   port_var.key   = "net_retropad_port";
   screen_var.key = "net_retropad_screen";
   hide_a_var.key = "net_retropad_hide_analog_mismatch";

   NETRETROPAD_CORE_PREFIX(environ_cb)(RETRO_ENVIRONMENT_GET_VARIABLE, &var);
   NETRETROPAD_CORE_PREFIX(environ_cb)(RETRO_ENVIRONMENT_GET_VARIABLE, &var2);
   NETRETROPAD_CORE_PREFIX(environ_cb)(RETRO_ENVIRONMENT_GET_VARIABLE, &var3);
   NETRETROPAD_CORE_PREFIX(environ_cb)(RETRO_ENVIRONMENT_GET_VARIABLE, &var4);
   NETRETROPAD_CORE_PREFIX(environ_cb)(RETRO_ENVIRONMENT_GET_VARIABLE, &port_var);
   NETRETROPAD_CORE_PREFIX(environ_cb)(RETRO_ENVIRONMENT_GET_VARIABLE, &screen_var);
   NETRETROPAD_CORE_PREFIX(environ_cb)(RETRO_ENVIRONMENT_GET_VARIABLE, &hide_a_var);

   snprintf(server, sizeof(server), "%s.%s.%s.%s", var.value, var2.value, var3.value, var4.value);
   port = atoi(port_var.value);

   if (screen_var.value &&
       ((current_screen == NETRETROPAD_SCREEN_PAD && strstr(screen_var.value,"Keyboard")) ||
        (current_screen == NETRETROPAD_SCREEN_KEYBOARD && strstr(screen_var.value,"Retropad"))))
      flip_screen();
   if (hide_a_var.value && strstr(hide_a_var.value,"True"))
      hide_analog_mismatch = true;
   else
      hide_analog_mismatch = false;

}

void NETRETROPAD_CORE_PREFIX(retro_set_audio_sample)(retro_audio_sample_t cb)
{
   NETRETROPAD_CORE_PREFIX(audio_cb) = cb;
}

void NETRETROPAD_CORE_PREFIX(retro_set_audio_sample_batch)(
      retro_audio_sample_batch_t cb)
{
   NETRETROPAD_CORE_PREFIX(audio_batch_cb) = cb;
}

void NETRETROPAD_CORE_PREFIX(retro_set_input_poll)(retro_input_poll_t cb)
{
   NETRETROPAD_CORE_PREFIX(input_poll_cb) = cb;
}

void NETRETROPAD_CORE_PREFIX(retro_set_input_state)(retro_input_state_t cb)
{
   NETRETROPAD_CORE_PREFIX(input_state_cb) = cb;
}

void NETRETROPAD_CORE_PREFIX(retro_set_video_refresh)(retro_video_refresh_t cb)
{
   NETRETROPAD_CORE_PREFIX(video_cb) = cb;
}

void NETRETROPAD_CORE_PREFIX(retro_reset)(void)
{
   netretropad_check_variables();
   netretropad_open_udp_socket();
}

void NETRETROPAD_CORE_PREFIX(retro_run)(void)
{
   int i;
   unsigned rle;
   uint32_t input_state = 0;
   uint32_t expected_input = 0;
   uint16_t *pixel      = frame_buf + 49 * 320 + 32;

   current_frame++;
   /* Update input states and send them if needed */
   retropad_update_input();

   /* Combine RetroPad input states into one value */
   for (i = joypad.id_min; i <= joypad.id_max; i++)
   {
      int offset = DESC_OFFSET(&joypad, 0, 0, i);
      if (joypad.value[offset])
         input_state |= 1 << i;

      /* Construct a red gradient representation for analog buttons */
      offset = DESC_OFFSET(&analog_button, 0, RETRO_DEVICE_INDEX_ANALOG_BUTTON, i);
      analog_item_colors[i] = (uint16_t)((int16_t)analog_button.value[offset]/1638) << 11;
   }

   for (i = analog.id_min; i <= analog.id_max; i++)
   {
      /* bitmap: x-- x- x+ x++ y-- y- y+ y++*/
      /* default analog deadzone: 0.0 - increased for convenience to 0.1, default analog threshold: 0.5 */
      /* Red gradient also calculated */
      int offset = DESC_OFFSET(&analog, 0, RETRO_DEVICE_INDEX_ANALOG_LEFT, i);
      if (     (int16_t)analog.value[offset] < -32768/2)
      {
         input_state |= 1 << (16 + i*8 + 0);
         analog_item_colors[  16 + i*8 + 0] = (uint16_t)((-1*((int16_t)analog.value[offset])-32768/2) /528) << 11;
      }
      else if ((int16_t)analog.value[offset] < -3276)
      {
         input_state |= 1 << (16 + i*8 + 1);
         analog_item_colors[  16 + i*8 + 1] = (uint16_t)((-1*((int16_t)analog.value[offset])        ) /528) << 11;
      }
      else if ((int16_t)analog.value[offset] > 32768/2)
      {
         input_state |= 1 << (16 + i*8 + 3);
         analog_item_colors[  16 + i*8 + 3] = (uint16_t)((   ((int16_t)analog.value[offset])-32768/2) /528) << 11;
      }
      else if ((int16_t)analog.value[offset] > 3276)
      {
         input_state |= 1 << (16 + i*8 + 2);
         analog_item_colors[  16 + i*8 + 2] = (uint16_t)((   ((int16_t)analog.value[offset])        ) /528) << 11;
      }

      offset = DESC_OFFSET(&analog, 0, RETRO_DEVICE_INDEX_ANALOG_RIGHT, i);
      if (     (int16_t)analog.value[offset] < -32768/2)
      {
         input_state |= 1 << (16 + i*8 + 4);
         analog_item_colors[  16 + i*8 + 4] = (uint16_t)((-1*((int16_t)analog.value[offset])-32768/2) /528) << 11;
      }
      else if ((int16_t)analog.value[offset] < -3276)
      {
         input_state |= 1 << (16 + i*8 + 5);
         analog_item_colors[  16 + i*8 + 5] = (uint16_t)((-1*((int16_t)analog.value[offset])        ) /528) << 11;
      }
      else if ((int16_t)analog.value[offset] > 32768/2)
      {
         input_state |= 1 << (16 + i*8 + 7);
         analog_item_colors[  16 + i*8 + 7] = (uint16_t)((   ((int16_t)analog.value[offset])-32768/2) /528) << 11;
      }
      else if ((int16_t)analog.value[offset] > 3276)
      {
         input_state |= 1 << (16 + i*8 + 6);
         analog_item_colors[  16 + i*8 + 6] = (uint16_t)((   ((int16_t)analog.value[offset])        ) /528) << 11;
      }
   }

   /* Input test section start. */

   /* Check for predefined combo inputs. */
   for (unsigned j = 0; j < sizeof(combo_def)/sizeof(combo_def[0]); j++)
   {
      if ((input_state & combo_def[j]) == combo_def[j])
         combo_state_validated |= 1 << j;
   }

/* Print a log for A+B combination, but only once while those are pressed */
   if (input_state & ((1 << RETRO_DEVICE_ID_JOYPAD_A | 1 << RETRO_DEVICE_ID_JOYPAD_B) & 0x0000ffff))
   {
      if (!dump_state_blocked)
      {
         NETRETROPAD_CORE_PREFIX(log_cb)(RETRO_LOG_INFO,"[Remote RetroPad]: Validated state: %08x combo: %08x\n",input_state_validated, combo_state_validated);
         dump_state_blocked = true;
      }
   }
   else if (dump_state_blocked)
      dump_state_blocked = false;

   /* Handle test step proceeding and feedback to user */
   if (current_test_step < last_test_step && current_frame > INITIAL_FRAMES)
   {
      if (current_frame > INITIAL_FRAMES + next_teststep_frame)
      {
         struct retro_message message;
         if (current_frame > INITIAL_FRAMES + 1)
            current_test_step++;
         if (current_test_step < last_test_step)
         {
            message.msg = input_test_steps[current_test_step].message;
            message.frames = ONE_TEST_STEP_FRAMES;
            NETRETROPAD_CORE_PREFIX(environ_cb)(RETRO_ENVIRONMENT_SET_MESSAGE, &message);
            next_teststep_frame = current_frame +  ONE_TEST_STEP_FRAMES - INITIAL_FRAMES;
            NETRETROPAD_CORE_PREFIX(log_cb)(RETRO_LOG_INFO,
               "[Remote RetroPad]: Proceeding to test step %d at frame %d, next: %d\n",
               current_test_step,current_frame,next_teststep_frame+INITIAL_FRAMES);
            if((input_test_steps[current_test_step].expected_button <  KEYBOARD_OFFSET && current_screen == NETRETROPAD_SCREEN_KEYBOARD) ||
               (input_test_steps[current_test_step].expected_button >= KEYBOARD_OFFSET && current_screen == NETRETROPAD_SCREEN_PAD))
                  flip_screen();
         }
         else
         {
            char buf[1024];
            unsigned pass_count = 0;
            for(unsigned i=0; i<last_test_step;i++)
               if (input_test_steps[i].detected)
                  pass_count++;
            message.msg = buf;
            snprintf(buf,sizeof(buf),"Test sequence finished, result: %d/%d inputs detected",pass_count,last_test_step);
            message.frames = ONE_TEST_STEP_FRAMES * 3;
            NETRETROPAD_CORE_PREFIX(environ_cb)(RETRO_ENVIRONMENT_SET_MESSAGE, &message);

            NETRETROPAD_CORE_PREFIX(log_cb)(RETRO_LOG_INFO,
               "[Remote RetroPad]: Test sequence finished at frame %d, result: %d/%d inputs detected\n",
               current_frame, pass_count, last_test_step);
            NETRETROPAD_CORE_PREFIX(log_cb)(RETRO_LOG_INFO,
               "[Remote RetroPad]: Validated state: %08x combo: %08x\n",
               input_state_validated, combo_state_validated);
         }
      }

      if (current_test_step < last_test_step)
      {
         bool test_success = false;
         if (input_test_steps[current_test_step].expected_button < KEYBOARD_OFFSET)
         {
            expected_input = 1 << input_test_steps[current_test_step].expected_button;
            if(input_state & expected_input)
               test_success = true;
         }
         else
         {
            expected_input = input_test_steps[current_test_step].expected_button - KEYBOARD_OFFSET;
            if (expected_input < RETROK_LAST && keyboard_state[expected_input])
               test_success = true;
         }
         if (test_success)
         {
            NETRETROPAD_CORE_PREFIX(log_cb)(RETRO_LOG_INFO,
               "[Remote RetroPad]: Test step %d successful at frame %d\n",
               current_test_step, current_frame);
            input_test_steps[current_test_step].detected = true;
            next_teststep_frame = current_frame - INITIAL_FRAMES;
         }
      }
   }
   /* Input test section end. */
   if (current_screen == NETRETROPAD_SCREEN_PAD)
   {
      for (rle = 0; rle < sizeof(retropad_buttons); )
      {
         unsigned runs;
         char      paint = 0;

         for (runs = retropad_buttons[rle++]; runs > 0; runs--)
         {
            if (paint)
            {
               unsigned button = 0;
               unsigned button_analog = 0;
               unsigned count;
               uint16_t color;

               /* 0  - 15: buttons, 16 - 31: analog x/y */
               /* 32 - 47: analog input for same buttons */
               if (retropad_buttons[rle] < 32)
                  button        = 1 <<  retropad_buttons[rle];
               else
                  button_analog = 1 << (retropad_buttons[rle] - 32);

               /* Red for active inputs */
               if (input_state & button)
               {
                  if(retropad_buttons[rle]<16)
                     color = 0xA000;
                  else
                     /* Gradient for analog axes */
                     color = analog_item_colors[retropad_buttons[rle]];
                  input_state_validated |= button;
               }
               /* Red gradient for active analog button inputs, from 0 to 0xa000 */
               else if (button_analog && analog_item_colors[retropad_buttons[rle] - 32])
               {
                  color = analog_item_colors[retropad_buttons[rle] - 32];
               }
               else if (button_analog && hide_analog_mismatch && input_state & button_analog)
                  color = 0xA000;
               else
               {
                  /* Light blue for expected input */
                  if (expected_input & button || expected_input & button_analog)
                     color = 0x7fff;
                  /* Light green for already validated input */
                  else if (input_state_validated & button || input_state_validated & button_analog)
                     color = 0xbff7;
                  /* White as default */
                  else
                     color = 0xffff;
               }

               rle++;
               for (count = retropad_buttons[rle++]; count > 0; count--)
                  *pixel++ = color;
            }
            else
               pixel += retropad_buttons[rle++];

            paint = !paint;
         }

         pixel += 65;
      }
   }
   else if (current_screen == NETRETROPAD_SCREEN_KEYBOARD)
   {
      for (rle = 0; rle < ARRAY_SIZE(keyboard_buttons); )
      {
         unsigned runs;
         char      paint = 0;

         for (runs = keyboard_buttons[rle++]; runs > 0; runs--)
         {
            if (paint)
            {
               unsigned count;
               uint16_t color;

               /* Same color scheme as for retropad buttons */
               if (keyboard_state[keyboard_buttons[rle]])
               {
                  color = 0xA000;
                  keyboard_state_validated[keyboard_buttons[rle]] = true;
               }
               else
               {
                  if (expected_input > 0 && expected_input == keyboard_buttons[rle])
                     color = 0x7fff;
                  else if (keyboard_state_validated[keyboard_buttons[rle]])
                     color = 0xbff7;
                  else
                     color = 0xffff;
               }
               rle++;

               for (count = keyboard_buttons[rle++]; count > 0; count--)
                  *pixel++ = color;
            }
            else
               pixel += keyboard_buttons[rle++];

            paint = !paint;
         }

         pixel += 65;
      }
   }

   NETRETROPAD_CORE_PREFIX(video_cb)(frame_buf, 320, 240, 640);
   retro_sleep(4);
}

bool NETRETROPAD_CORE_PREFIX(retro_load_game)(const struct retro_game_info *info)
{
   netretropad_check_variables();
   netretropad_open_udp_socket();

   /* If a .ratst file is given (only possible via command line),
    * initialize test sequence. */
   if (info)
      input_test_file_read(info->path);
   if (last_test_step > MAX_TEST_STEPS)
      current_test_step = last_test_step;
   else
   {
      struct retro_message message;
      message.msg = "Initiating test sequence...";
      message.frames = INITIAL_FRAMES;
      NETRETROPAD_CORE_PREFIX(environ_cb)(RETRO_ENVIRONMENT_SET_MESSAGE, &message);
   }

   return true;
}

void NETRETROPAD_CORE_PREFIX(retro_unload_game)(void)
{}

unsigned NETRETROPAD_CORE_PREFIX(retro_get_region)(void)
{
   return RETRO_REGION_NTSC;
}

bool NETRETROPAD_CORE_PREFIX(retro_load_game_special)(unsigned type,
      const struct retro_game_info *info, size_t num)
{
   (void)type;
   (void)info;
   (void)num;
   return false;
}

size_t NETRETROPAD_CORE_PREFIX(retro_serialize_size)(void)
{
   return 0;
}

bool NETRETROPAD_CORE_PREFIX(retro_serialize)(void *data, size_t size)
{
   (void)data;
   (void)size;
   return false;
}

bool NETRETROPAD_CORE_PREFIX(retro_unserialize)(const void *data,
      size_t size)
{
   (void)data;
   (void)size;
   return false;
}

void *NETRETROPAD_CORE_PREFIX(retro_get_memory_data)(unsigned id)
{
   (void)id;
   return NULL;
}

size_t NETRETROPAD_CORE_PREFIX(retro_get_memory_size)(unsigned id)
{
   (void)id;
   return 0;
}

void NETRETROPAD_CORE_PREFIX(retro_cheat_reset)(void)
{}

void NETRETROPAD_CORE_PREFIX(retro_cheat_set)(unsigned idx,
      bool enabled, const char *code)
{
   (void)idx;
   (void)enabled;
   (void)code;
}
