/*
 * To-do:
 * - Analog support
 * - Some sort of connection control, it only sends packets
 *   but there is no acknoledgement of a connection o keepalives
 * - Send player name
 * - Render something on-screen maybe a gui to configure IP and port
     instead of the ridiculously long strings we're using now
 * - Allow changing IP address and port in runtime
 * - Support other platforms
 * - Input recording / Combos
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
   index * ((desc)->id_max - (desc)->id_min + 1) + \
   id \
)

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

static struct descriptor *descriptors[] = {
   &joypad,
   &analog
};

void NETRETROPAD_CORE_PREFIX(retro_init)(void)
{
   unsigned i;

   frame_buf = (uint16_t*)calloc(320 * 240, sizeof(uint16_t));

   if (frame_buf)
   {
      unsigned rle, runs, count;
      uint16_t *pixel = frame_buf + 49 * 320 + 32;

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
   }

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
   info->need_fullpath    = false;
   info->valid_extensions = ""; /* Nothing. */
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

               /* Attempt to send updated state */
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

void NETRETROPAD_CORE_PREFIX(retro_set_environment)(retro_environment_t cb)
{
   static const struct retro_variable vars[] = {
      { "net_retropad_port", "Port; 55400|55401|55402|55403|55404|55405|55406|55407|55408|55409|55410|55411|55412|55413|55414|55415|55416|55417|55418|55419|55420" },
      { "net_retropad_ip_octet1", "IP address part 1; 0|1|2|3|4|5|6|7|8|9|10|11|12|13|14|15|16|17|18|19|20|21|22|23|24|25|26|27|28|29|30|31|32|33|34|35|36|37|38|39|40|41|42|43|44|45|46|47|48|49|50|51|52|53|54|55|56|57|58|59|60|61|62|63|64|65|66|67|68|69|70|71|72|73|74|75|76|77|78|79|80|81|82|83|84|85|86|87|88|89|90|91|92|93|94|95|96|97|98|99|100|101|102|103|104|105|106|107|108|109|110|111|112|113|114|115|116|117|118|119|120|121|122|123|124|125|126|127|128|129|130|131|132|133|134|135|136|137|138|139|140|141|142|143|144|145|146|147|148|149|150|151|152|153|154|155|156|157|158|159|160|161|162|163|164|165|166|167|168|169|170|171|172|173|174|175|176|177|178|179|180|181|182|183|184|185|186|187|188|189|190|191|192|193|194|195|196|197|198|199|200|201|202|203|204|205|206|207|208|209|210|211|212|213|214|215|216|217|218|219|220|221|222|223|224|225|226|227|228|229|230|231|232|233|234|235|236|237|238|239|240|241|242|243|244|245|246|247|248|249|250|251|252|253|254|255" },
      { "net_retropad_ip_octet2", "IP address part 2; 0|1|2|3|4|5|6|7|8|9|10|11|12|13|14|15|16|17|18|19|20|21|22|23|24|25|26|27|28|29|30|31|32|33|34|35|36|37|38|39|40|41|42|43|44|45|46|47|48|49|50|51|52|53|54|55|56|57|58|59|60|61|62|63|64|65|66|67|68|69|70|71|72|73|74|75|76|77|78|79|80|81|82|83|84|85|86|87|88|89|90|91|92|93|94|95|96|97|98|99|100|101|102|103|104|105|106|107|108|109|110|111|112|113|114|115|116|117|118|119|120|121|122|123|124|125|126|127|128|129|130|131|132|133|134|135|136|137|138|139|140|141|142|143|144|145|146|147|148|149|150|151|152|153|154|155|156|157|158|159|160|161|162|163|164|165|166|167|168|169|170|171|172|173|174|175|176|177|178|179|180|181|182|183|184|185|186|187|188|189|190|191|192|193|194|195|196|197|198|199|200|201|202|203|204|205|206|207|208|209|210|211|212|213|214|215|216|217|218|219|220|221|222|223|224|225|226|227|228|229|230|231|232|233|234|235|236|237|238|239|240|241|242|243|244|245|246|247|248|249|250|251|252|253|254|255" },
      { "net_retropad_ip_octet3", "IP address part 3; 0|1|2|3|4|5|6|7|8|9|10|11|12|13|14|15|16|17|18|19|20|21|22|23|24|25|26|27|28|29|30|31|32|33|34|35|36|37|38|39|40|41|42|43|44|45|46|47|48|49|50|51|52|53|54|55|56|57|58|59|60|61|62|63|64|65|66|67|68|69|70|71|72|73|74|75|76|77|78|79|80|81|82|83|84|85|86|87|88|89|90|91|92|93|94|95|96|97|98|99|100|101|102|103|104|105|106|107|108|109|110|111|112|113|114|115|116|117|118|119|120|121|122|123|124|125|126|127|128|129|130|131|132|133|134|135|136|137|138|139|140|141|142|143|144|145|146|147|148|149|150|151|152|153|154|155|156|157|158|159|160|161|162|163|164|165|166|167|168|169|170|171|172|173|174|175|176|177|178|179|180|181|182|183|184|185|186|187|188|189|190|191|192|193|194|195|196|197|198|199|200|201|202|203|204|205|206|207|208|209|210|211|212|213|214|215|216|217|218|219|220|221|222|223|224|225|226|227|228|229|230|231|232|233|234|235|236|237|238|239|240|241|242|243|244|245|246|247|248|249|250|251|252|253|254|255" },
      { "net_retropad_ip_octet4", "IP address part 4; 0|1|2|3|4|5|6|7|8|9|10|11|12|13|14|15|16|17|18|19|20|21|22|23|24|25|26|27|28|29|30|31|32|33|34|35|36|37|38|39|40|41|42|43|44|45|46|47|48|49|50|51|52|53|54|55|56|57|58|59|60|61|62|63|64|65|66|67|68|69|70|71|72|73|74|75|76|77|78|79|80|81|82|83|84|85|86|87|88|89|90|91|92|93|94|95|96|97|98|99|100|101|102|103|104|105|106|107|108|109|110|111|112|113|114|115|116|117|118|119|120|121|122|123|124|125|126|127|128|129|130|131|132|133|134|135|136|137|138|139|140|141|142|143|144|145|146|147|148|149|150|151|152|153|154|155|156|157|158|159|160|161|162|163|164|165|166|167|168|169|170|171|172|173|174|175|176|177|178|179|180|181|182|183|184|185|186|187|188|189|190|191|192|193|194|195|196|197|198|199|200|201|202|203|204|205|206|207|208|209|210|211|212|213|214|215|216|217|218|219|220|221|222|223|224|225|226|227|228|229|230|231|232|233|234|235|236|237|238|239|240|241|242|243|244|245|246|247|248|249|250|251|252|253|254|255" },

      { NULL, NULL },
   };
   enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_RGB565;
   cb(RETRO_ENVIRONMENT_SET_VARIABLES, (void*)vars);

   NETRETROPAD_CORE_PREFIX(environ_cb) = cb;
   bool no_content = true;
   cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &no_content);

   NETRETROPAD_CORE_PREFIX(environ_cb)(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt);

   if (cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &logger))
      NETRETROPAD_CORE_PREFIX(log_cb) = logger.log;
}

static void netretropad_check_variables(void)
{
   struct retro_variable var, var2, var3, var4, port_var;
   var.key      = "net_retropad_ip_octet1";
   var2.key     = "net_retropad_ip_octet2";
   var3.key     = "net_retropad_ip_octet3";
   var4.key     = "net_retropad_ip_octet4";
   port_var.key = "net_retropad_port";

   NETRETROPAD_CORE_PREFIX(environ_cb)(RETRO_ENVIRONMENT_GET_VARIABLE, &var);
   NETRETROPAD_CORE_PREFIX(environ_cb)(RETRO_ENVIRONMENT_GET_VARIABLE, &var2);
   NETRETROPAD_CORE_PREFIX(environ_cb)(RETRO_ENVIRONMENT_GET_VARIABLE, &var3);
   NETRETROPAD_CORE_PREFIX(environ_cb)(RETRO_ENVIRONMENT_GET_VARIABLE, &var4);
   NETRETROPAD_CORE_PREFIX(environ_cb)(RETRO_ENVIRONMENT_GET_VARIABLE, &port_var);

   snprintf(server, sizeof(server), "%s.%s.%s.%s", var.value, var2.value, var3.value, var4.value);
   port = atoi(port_var.value);
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
{}

void NETRETROPAD_CORE_PREFIX(retro_run)(void)
{
   int i;
   unsigned rle;
   unsigned input_state = 0;
   uint16_t *pixel      = frame_buf + 49 * 320 + 32;

   /* Update input states and send them if needed */
   retropad_update_input();

   /* Combine RetroPad input states into one value */
   for (i = joypad.id_min; i <= joypad.id_max; i++)
   {
      int offset = DESC_OFFSET(&joypad, 0, 0, i);
      if (joypad.value[offset])
         input_state |= 1 << i;
   }

   for (rle = 0; rle < sizeof(retropad_buttons); )
   {
      unsigned runs;
      char      paint = 0;

      for (runs = retropad_buttons[rle++]; runs > 0; runs--)
      {
         unsigned button = paint ? 1 << retropad_buttons[rle++] : 0;

         if (paint)
         {
            unsigned count;
            uint16_t color = (input_state & button) ? 0x0500 : 0xffff;

            for (count = retropad_buttons[rle++]; count > 0; count--)
               *pixel++ = color;
         }
         else
            pixel += retropad_buttons[rle++];

         paint = !paint;
      }

      pixel += 65;
   }

   NETRETROPAD_CORE_PREFIX(video_cb)(frame_buf, 320, 240, 640);

   retro_sleep(4);
}

bool NETRETROPAD_CORE_PREFIX(retro_load_game)(const struct retro_game_info *info)
{
   socket_target_t in_target;

   netretropad_check_variables();

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
