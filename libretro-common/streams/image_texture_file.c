/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (image_texture_file.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */


/* The path side of image loading: formats/image_texture.c decodes the
 * bytes it is given, and this reads them from the file. */

#include <stdlib.h>
#include <string.h>

#include <formats/image.h>
#include <formats/data_transfer.h>
#include <streams/data_transfer_file.h>
#include <streams/image_texture_file.h>

bool image_texture_load_request(struct texture_image *out_img,
      const char *path, const image_texture_request_t *req,
      bool (*should_abort)(void *ud), void *ud)
{
   return image_texture_load_request_ex(out_img, path, req,
         should_abort, ud, NULL);
}

bool image_texture_load_request_ex(struct texture_image *out_img,
      const char *path, const image_texture_request_t *req,
      bool (*should_abort)(void *ud), void *ud, int *png_probe)
{
   enum image_type_enum type = image_texture_get_type(path);

   if (png_probe)
      *png_probe = -1;

   if (type != IMAGE_TYPE_NONE)
   {
      /* A zero budget fills to completion in one blocking call, which
       * is this API's contract */
      struct data_transfer *dt = data_transfer_open_prefix(path, 0);
      if (dt)
      {
         size_t file_len    = 0;
         const uint8_t *ptr = NULL;
         bool kept          = false;
         bool loaded;

         data_transfer_iterate(dt, 0);
         ptr = data_transfer_ptr(dt, &file_len);
         loaded = data_transfer_complete(dt) && ptr && file_len
            && image_texture_load_buffer_probe(dt, &kept, out_img, type,
                  ptr, file_len, req, should_abort, ud, png_probe);
         if (!kept)
            data_transfer_free(dt);
         if (loaded)
            return true;
      }
   }
   /* nothing to decode leaves the image empty */
   if (out_img)
      image_texture_load_buffer_probe(NULL, NULL, out_img, type, NULL, 0,
            req, NULL, NULL, NULL);
   return false;
}


bool image_texture_load_ex(struct texture_image *out_img,
      const char *path, bool (*should_abort)(void *ud), void *ud)
{
   image_texture_request_t req;
   req.rgba            = out_img->supports_rgba;
   req.want_10bit      = out_img->pix10;
   req.want_fp16       = false;
   req.want_compressed = true;
   return image_texture_load_request(out_img, path, &req,
         should_abort, ud);
}

bool image_texture_load(struct texture_image *out_img, const char *path)
{
   return image_texture_load_ex(out_img, path, NULL, NULL);
}
