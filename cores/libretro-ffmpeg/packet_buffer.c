#include "packet_buffer.h"

struct AVPacketNode {
   AVPacket *data;
   struct AVPacketNode *next;
   struct AVPacketNode *previous;
};
typedef struct AVPacketNode AVPacketNode_t;

struct packet_buffer
{
   AVPacketNode_t *head;
   AVPacketNode_t *tail;
   size_t size;
};

packet_buffer_t *packet_buffer_create()
{
   packet_buffer_t *b = malloc(sizeof(packet_buffer_t));
   if (!b)
      return NULL;

   memset(b, 0, sizeof(packet_buffer_t));

   return b;
}

void packet_buffer_destroy(packet_buffer_t *packet_buffer)
{
   AVPacketNode_t *node;

   if (!packet_buffer)
      return;

   if (packet_buffer->head)
   {
      node = packet_buffer->head;
      while (node)
      {
         AVPacketNode_t *next = node->next;
         av_packet_free(&node->data);
         free(node);
         node = next;
      }
   }

   free(packet_buffer);
}

void packet_buffer_clear(packet_buffer_t **packet_buffer)
{
   if (!packet_buffer)
      return;

   packet_buffer_destroy(*packet_buffer);
   *packet_buffer = packet_buffer_create();
}

bool packet_buffer_empty(packet_buffer_t *packet_buffer)
{
   if (!packet_buffer)
      return true;

   return packet_buffer->size == 0;
}

size_t packet_buffer_size(packet_buffer_t *packet_buffer)
{
   if (!packet_buffer)
      return 0;

   return packet_buffer->size;
}

void packet_buffer_add_packet(packet_buffer_t *packet_buffer, AVPacket *pkt)
{
   AVPacketNode_t *new_head = (AVPacketNode_t *) malloc(sizeof(AVPacketNode_t));
   new_head->data = av_packet_alloc();

   av_packet_move_ref(new_head->data, pkt);

   if (packet_buffer->head)
   {
      new_head->next = packet_buffer->head;
      packet_buffer->head->previous = new_head;
   }
   else
   {
      new_head->next = NULL;
      packet_buffer->tail = new_head;
   }

   packet_buffer->head = new_head;
   packet_buffer->head->previous = NULL;
   packet_buffer->size++;
}

void packet_buffer_get_packet(packet_buffer_t *packet_buffer, AVPacket *pkt)
{
   AVPacketNode_t *new_tail = NULL;

   if (packet_buffer->tail == NULL)
      return;

   av_packet_move_ref(pkt, packet_buffer->tail->data);
   if (packet_buffer->tail->previous)
   {
      new_tail = packet_buffer->tail->previous;
      new_tail->next = NULL;
   }
   else
      packet_buffer->head = NULL;

   av_packet_free(&packet_buffer->tail->data);
   free(packet_buffer->tail);

   packet_buffer->tail = new_tail;
   packet_buffer->size--;
}

int64_t packet_buffer_peek_start_pts(packet_buffer_t *packet_buffer)
{
   if (!packet_buffer->tail)
      return 0;

   return packet_buffer->tail->data->pts;
}

int64_t packet_buffer_peek_end_pts(packet_buffer_t *packet_buffer)
{
   if (!packet_buffer->tail)
      return 0;

   return packet_buffer->tail->data->pts + packet_buffer->tail->data->duration;
}
