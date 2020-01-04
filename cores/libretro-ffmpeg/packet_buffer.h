#ifndef __LIBRETRO_SDK_PACKETBUFFER_H__
#define __LIBRETRO_SDK_PACKETBUFFER_H__

#include <retro_common_api.h>

#include <boolean.h>
#include <stdint.h>

#include <libavcodec/avcodec.h>

#include <retro_miscellaneous.h>

RETRO_BEGIN_DECLS

/**
 * packet_buffer
 *
 * Just a simple double linked list for AVPackets.
 *
 */
struct packet_buffer;
typedef struct packet_buffer packet_buffer_t;

/**
 * packet_buffer_create:
 *
 * Create a packet_buffer.
 *
 * Returns: A packet buffer.
 */
packet_buffer_t *packet_buffer_create();

/**
 * packet_buffer_destroy:
 * @packet_buffer      : packet buffer
 *
 * Destroys a packet buffer.
 *
 **/
void packet_buffer_destroy(packet_buffer_t *packet_buffer);

/**
 * packet_buffer_clear:
 * @packet_buffer      : packet buffer
 *
 * Clears a packet buffer by re-creating it.
 *
 **/
void packet_buffer_clear(packet_buffer_t **packet_buffer);

/**
 * packet_buffer_empty:
 * @packet_buffer      : packet buffer
 *
 * Return true if the buffer is empty;
 *
 **/
bool packet_buffer_empty(packet_buffer_t *packet_buffer);

/**
 * packet_buffer_size:
 * @packet_buffer      : packet buffer
 *
 * Returns the number of AVPackets the buffer currently
 * holds.
 *
 **/
size_t packet_buffer_size(packet_buffer_t *packet_buffer);

/**
 * packet_buffer_add_packet:
 * @packet_buffer      : packet buffer
 * @pkt                : packet
 *
 * Copies the given packet into the selected buffer.
 *
 **/
void packet_buffer_add_packet(packet_buffer_t *packet_buffer, AVPacket *pkt);

/**
 * packet_buffer_get_packet:
 * @packet_buffer      : packet buffer
 * @pkt                : packet
 *
 * Get the next packet. User needs to unref the packet with av_packet_unref().
 *
 **/
void packet_buffer_get_packet(packet_buffer_t *packet_buffer, AVPacket *pkt);

/**
 * packet_buffer_peek_start_pts:
 * @packet_buffer      : packet buffer
 *
 * Returns the start pts of the next packet in the buffer.
 *
 **/
int64_t packet_buffer_peek_start_pts(packet_buffer_t *packet_buffer);

/**
 * packet_buffer_peek_end_pts:
 * @packet_buffer      : packet buffer
 *
 * Returns the end pts of the next packet in the buffer.
 *
 **/
int64_t packet_buffer_peek_end_pts(packet_buffer_t *packet_buffer);

RETRO_END_DECLS

#endif
