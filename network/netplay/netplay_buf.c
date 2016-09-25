/*  RetroArch - A frontend for libretro.
 *  Copyright (C)      2016 - Gregor Richards
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include <net/net_compat.h>
#include <net/net_socket.h>

#include "netplay_private.h"

static size_t buf_used(struct socket_buffer *sbuf)
{
   if (sbuf->end < sbuf->start)
   {
      size_t newend = sbuf->end;
      while (newend < sbuf->start) newend += sbuf->bufsz;
      return newend - sbuf->start;
   }

   return sbuf->end - sbuf->start;
}

static size_t buf_unread(struct socket_buffer *sbuf)
{
   if (sbuf->end < sbuf->read)
   {
      size_t newend = sbuf->end;
      while (newend < sbuf->read) newend += sbuf->bufsz;
      return newend - sbuf->read;
   }

   return sbuf->end - sbuf->read;
}

static size_t buf_remaining(struct socket_buffer *sbuf)
{
   return sbuf->bufsz - buf_used(sbuf) - 1;
}

bool netplay_init_socket_buffer(struct socket_buffer *sbuf, size_t size)
{
   sbuf->data = malloc(size);
   if (sbuf->data == NULL)
      return false;
   sbuf->bufsz = size;
   sbuf->start = sbuf->read = sbuf->end = 0;
   return true;
}

void netplay_deinit_socket_buffer(struct socket_buffer *sbuf)
{
   if (sbuf->data)
      free(sbuf->data);
}

void netplay_clear_socket_buffer(struct socket_buffer *sbuf)
{
   sbuf->start = sbuf->read = sbuf->end = 0;
}

bool netplay_send(struct socket_buffer *sbuf, int sockfd, const void *buf, size_t len)
{
   if (buf_remaining(sbuf) < len)
   {
      /* Need to force a blocking send */
      if (!netplay_send_flush(sbuf, sockfd, true))
         return false;
   }

   if (buf_remaining(sbuf) < len)
   {
      /* Can only be that this is simply too big for our buffer, in which case
       * we just need to do a blocking send */
      if (!socket_send_all_blocking(sockfd, buf, len, false))
         return false;
      return true;
   }

   /* Copy it into our buffer */
   if (sbuf->bufsz - sbuf->end < len)
   {
      /* Half at a time */
      size_t chunka = sbuf->bufsz - sbuf->end,
             chunkb = len - chunka;
      memcpy(sbuf->data + sbuf->end, buf, chunka);
      memcpy(sbuf->data, (const unsigned char *) buf + chunka, chunkb);
      sbuf->end = chunkb;

   }
   else
   {
      /* Straight in */
      memcpy(sbuf->data + sbuf->end, buf, len);
      sbuf->end += len;

   }

   /* Flush what we can immediately */
   return netplay_send_flush(sbuf, sockfd, false);
}

bool netplay_send_flush(struct socket_buffer *sbuf, int sockfd, bool block)
{
   ssize_t sent;

   if (buf_used(sbuf) == 0)
      return true;

   if (sbuf->end > sbuf->start)
   {
      /* Usual case: Everything's in order */
      if (block)
      {
         if (!socket_send_all_blocking(sockfd, sbuf->data + sbuf->start, buf_used(sbuf), false))
            return false;
         sbuf->start = sbuf->end = 0;

      }
      else
      {
         sent = socket_send_all_nonblocking(sockfd, sbuf->data + sbuf->start, buf_used(sbuf), false);
         if (sent < 0)
            return false;
         sbuf->start += sent;

         if (sbuf->start == sbuf->end)
            sbuf->start = sbuf->end = 0;

      }

   }
   else
   {
      /* Unusual case: Buffer overlaps break */
      if (block)
      {
         if (!socket_send_all_blocking(sockfd, sbuf->data + sbuf->start, sbuf->bufsz - sbuf->start, false))
            return false;
         sbuf->start = 0;
         return netplay_send_flush(sbuf, sockfd, true);

      }
      else
      {
         sent = socket_send_all_nonblocking(sockfd, sbuf->data + sbuf->start, sbuf->bufsz - sbuf->start, false);
         if (sent < 0)
            return false;
         sbuf->start += sent;

         if (sbuf->start >= sbuf->bufsz)
         {
            sbuf->start = 0;
            return netplay_send_flush(sbuf, sockfd, false);

         }

      }

   }

   return true;
}

ssize_t netplay_recv(struct socket_buffer *sbuf, int sockfd, void *buf, size_t len, bool block)
{
   bool error;
   ssize_t recvd;
   
   /* Receive whatever we can into the buffer */
   if (sbuf->end > sbuf->start)
   {
      error = false;
      recvd = socket_receive_all_nonblocking(sockfd, &error,
         sbuf->data + sbuf->end, sbuf->bufsz - sbuf->end -
         ((sbuf->start == 0) ? 1 : 0));
      if (recvd < 0 || error)
         return -1;
      sbuf->end += recvd;
      if (sbuf->end >= sbuf->bufsz)
      {
         sbuf->end = 0;
         error = false;
         recvd = socket_receive_all_nonblocking(sockfd, &error, sbuf->data, sbuf->start - 1);
         if (recvd < 0 || error)
            return -1;
         sbuf->end += recvd;

      }

   }
   else
   {
      error = false;
      recvd = socket_receive_all_nonblocking(sockfd, &error, sbuf->data + sbuf->end, sbuf->start - sbuf->end - 1);
      if (recvd < 0 || error)
         return -1;
      sbuf->end += recvd;

   }

   /* Now copy it into the reader */
   if (sbuf->end > sbuf->read || (sbuf->bufsz - sbuf->read) >= len)
   {
      size_t unread = buf_unread(sbuf);
      if (len <= unread)
      {
         memcpy(buf, sbuf->data + sbuf->read, len);
         sbuf->read += len;
         if (sbuf->read >= sbuf->bufsz)
            sbuf->read = 0;
         recvd = len;

      }
      else
      {
         memcpy(buf, sbuf->data + sbuf->read, unread);
         sbuf->read += unread;
         if (sbuf->read >= sbuf->bufsz)
            sbuf->read = 0;
         recvd = unread;

      }

   }
   else
   {
      /* Our read goes around the edge */
      size_t chunka = sbuf->bufsz - sbuf->read,
             pchunklen = len - chunka,
             chunkb = (pchunklen >= sbuf->end) ? sbuf->end : pchunklen;
      memcpy(buf, sbuf->data + sbuf->read, chunka);
      memcpy((unsigned char *) buf + chunka, sbuf->data, chunkb);
      sbuf->read = chunkb;
      recvd = chunka + chunkb;

   }

   /* Perhaps block for more data */
   if (block)
   {
      sbuf->start = sbuf->read;
      if (recvd < len)
      {
         if (!socket_receive_all_blocking(sockfd, (unsigned char *) buf + recvd, len - recvd))
            return -1;
         recvd = len;

      }
   }

   return recvd;
}

void netplay_recv_reset(struct socket_buffer *sbuf)
{
   sbuf->read = sbuf->start;
}

void netplay_recv_flush(struct socket_buffer *sbuf)
{
   sbuf->start = sbuf->read;
}
