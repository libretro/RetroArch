#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <libretro.h>
#include <encodings/base64.h>
#include "translation_service.h"
#include "gfx/video_frame.h"
#include "gfx/scaler/scaler.h"
#include "tasks/tasks_internal.h"
#include <audio/audio_mixer.h>
#include "tasks/task_audio_mixer.h"

#include "configuration.h"
#include "retroarch.h"
#include "verbosity.h"

#ifdef HAVE_TRANSLATE
#include "translation/translation_service.h"
#endif


typedef struct nbio_buf
{
   void *buf;
   unsigned bufsize;
   char *path;
} nbio_buf_t;

/*
bool g_translation_service_status = false;
*/

static void form_bmp_header(uint8_t *header, unsigned width, unsigned height,
                            bool is32bpp)
{
   unsigned line_size  = (width * (is32bpp?4:3) + 3) & ~3;
   unsigned size       = line_size * height + 54;
   unsigned size_array = line_size * height;

   /* Generic BMP stuff. */
   /* signature */
   header[0] = 'B';
   header[1] = 'M';
   /* file size */
   header[2] = (uint8_t)(size >> 0);
   header[3] = (uint8_t)(size >> 8);
   header[4] = (uint8_t)(size >> 16);
   header[5] = (uint8_t)(size >> 24);
   /* reserved */
   header[6] = 0;
   header[7] = 0;
   header[8] = 0;
   header[9] = 0;
   /* offset */
   header[10] = 54;
   header[11] = 0;
   header[12] = 0;
   header[13] = 0;
   /* DIB size */
   header[14] = 40;
   header[15] = 0;
   header[16] = 0;
   header[17] = 0;
   /* Width */
   header[18] = (uint8_t)(width >> 0);
   header[19] = (uint8_t)(width >> 8);
   header[20] = (uint8_t)(width >> 16);
   header[21] = (uint8_t)(width >> 24);
   /* Height */
   header[22] = (uint8_t)(height >> 0);
   header[23] = (uint8_t)(height >> 8);
   header[24] = (uint8_t)(height >> 16);
   header[25] = (uint8_t)(height >> 24);
   /* Color planes */
   header[26] = 1;
   header[27] = 0;
   /* Bits per pixel */
   header[28] = 24;
   header[29] = 0;
   /* Compression method */
   header[30] = 0;
   header[31] = 0;
   header[32] = 0;
   header[33] = 0;
   /* Image data size */
   header[34] = (uint8_t)(size_array >> 0);
   header[35] = (uint8_t)(size_array >> 8);
   header[36] = (uint8_t)(size_array >> 16);
   header[37] = (uint8_t)(size_array >> 24);
   /* Horizontal resolution */
   header[38] = 19;
   header[39] = 11;
   header[40] = 0;
   header[41] = 0;
   /* Vertical resolution */
   header[42] = 19;
   header[43] = 11;
   header[44] = 0;
   header[45] = 0;
   /* Palette size */
   header[46] = 0;
   header[47] = 0;
   header[48] = 0;
   header[49] = 0;
   /* Important color count */
   header[50] = 0;
   header[51] = 0;
   header[52] = 0;
   header[53] = 0;
}


bool run_translation_service(void)
{
   /*
      This function does all the stuff needed to translate the game screen, 
      using the url given in the settings.  Once the image from the frame
      buffer is sent to the server, the callback will write the translated
      image to the screen.

      Supported client/services (thus far)
      -VGTranslate client ( www.gitlab.com/spherebeaker/vg_translate )
      -Ztranslate client/service ( www.ztranslate.net/docs/service )
      
      To use a client, download the relevant code/release, configure
      them, and run them on your local machine, or network.  Set the 
      retroarch configuration to point to your local client (usually
      listening on localhost:4404 ) and enable translation service.

      If you don't want to run a client, you can also use a service,
      which is basically like someone running a client for you.  The
      downside here is that your retroarch device will have to have
      an internet connection, and you may have to sign up for it.
     
      To make your own server, it must listen for a POST request, which
      will consist of a json body, with the "image" field as a base64
      encoded string of a 24bit-BMP that the will be translated.  The server
      must output the translated image in the form of a json body, with
      the "image" field also as a base64 encoded, 24bit-BMP.
   */

   size_t pitch;
   unsigned width, height;
   const void *data                      = NULL;
   uint8_t *bit24_image                  = NULL;
   uint8_t *bit24_image_prev             = NULL;

   enum retro_pixel_format pixel_format  = video_driver_get_pixel_format();
   struct scaler_ctx *scaler             = calloc(1, sizeof(struct scaler_ctx));
   bool error                            = false;

   uint8_t* bmp_buffer                   = NULL;
   char* bmp64_buffer                    = NULL;
   char* json_buffer                     = NULL;

   bool retval                           = false;
   struct video_viewport vp;

   uint8_t header[54];
   int out_length                        = 0;
   char* rf1                             = "{\"image\": \"";
   char* rf2                             = "\"}\0";


   
   if (!scaler)
      goto finish;

   video_driver_cached_frame_get(&data, &width, &height, &pitch);
   if (!data)
      goto finish;
   if (data == RETRO_HW_FRAME_BUFFER_VALID)
   {
      /*
        The direct frame capture didn't work, so try getting it
        from the viewport instead.  This isn't as good as the
        raw frame buffer, since the viewport may us bilinear
        filtering, or other shaders that will completely trash
        the OCR, but it's better than nothing.
      */      
      vp.x                           = 0;
      vp.y                           = 0;
      vp.width                       = 0;
      vp.height                      = 0;
      vp.full_width                  = 0;
      vp.full_height                 = 0;

      video_driver_get_viewport_info(&vp);

      if (!vp.width || !vp.height)
         goto finish;

      bit24_image_prev = (uint8_t*)malloc(vp.width * vp.height * 3);
      bit24_image = (uint8_t*)malloc(width * height * 3);

      if (!bit24_image_prev || !bit24_image)
         goto finish;

      if (!video_driver_read_viewport(bit24_image_prev, false))
         goto finish;

      /* Rescale down to regular resolution */


      /*
      scaler->in_fmt = SCALER_FMT_BGR24;
      scaler->in_width = vp.width;
      scaler->in_height = vp.height;

      scaler->out_width = width;
      scaler->out_height = height;
      scaler->out_fmt = SCALER_FMT_BGR24;

      scaler->scaler_type = SCALER_TYPE_POINT;
      scaler_ctx_gen_filter(scaler);

      scaler->in_stride = vp.width*3;
      scaler->out_stride = width*3;

      scaler_ctx_scale_direct(scaler, bit24_image, bit24_image_prev)
      */
      bit24_image = bit24_image_prev;
      bit24_image_prev = NULL;

   }
   else
   {
      bit24_image = (uint8_t*) malloc(width*height*3);
      if (!bit24_image)
          goto finish;

      if (video_driver_get_pixel_format() == RETRO_PIXEL_FORMAT_XRGB8888)
      {
         scaler->in_fmt = SCALER_FMT_ARGB8888;
         RARCH_LOG("IN FORMAT ARGB8888\n");
      }
      else
      {
         scaler->in_fmt = SCALER_FMT_RGB565;
         RARCH_LOG("IN FORMAT RGB565\n");
      }
      video_frame_convert_to_bgr24(
         scaler,
         (uint8_t *) bit24_image,
         (const uint8_t*)data + ((int)height - 1)*pitch,
         width, height,
         -pitch);
     
      scaler_ctx_gen_reset(scaler);
   }  

   if (!bit24_image)
   {
      error = true;
      goto finish;
   }

   /*
     at this point, we should have a screenshot in the buffer, so allocate
     an array to contain the bmp image along with the bmp header as bytes,
     and then covert that to a b64 encoded array for transport in JSON.
   */
   form_bmp_header(header, width, height, false);
   bmp_buffer = (uint8_t*)malloc(width * height * 3+54);
   if (!bmp_buffer)
       goto finish;

   memcpy(bmp_buffer, header, 54*sizeof(uint8_t));
   memcpy(bmp_buffer+54, bit24_image, width*height*3*sizeof(uint8_t));

   bmp64_buffer = base64((void *) bmp_buffer, (int)(width*height*3+54),
                               &out_length);
   if (!bmp64_buffer)
      goto finish;

   /* Form request... */
   json_buffer = malloc(11+3+out_length);
   if (!json_buffer)
      goto finish;

   memcpy(json_buffer, rf1, 11*sizeof(uint8_t));  
   memcpy(json_buffer+11, bmp64_buffer, (out_length)*sizeof(uint8_t));
   memcpy(json_buffer+11+out_length, rf2, 3*sizeof(uint8_t));
  
   call_translation_server(json_buffer);
   error = false;
finish:
   if (bit24_image_prev)
      free(bit24_image_prev);
   if (bit24_image)
      free(bit24_image);

   if (scaler)
      free(scaler);

   if (bmp_buffer)
      free(bmp_buffer);

   if (bmp64_buffer)
      free(bmp64_buffer);

   if (json_buffer)
      free(json_buffer);
   return !error;
}


void handle_translation_cb(retro_task_t *task, void *task_data, void *user_data, const char *error)
{
   char* body_copy                   = NULL;
   uint8_t* raw_output_data          = NULL;
   char* raw_bmp_data                = NULL;
   struct scaler_ctx* scaler         = NULL;
   bool is_paused                    = false;
   bool is_idle                      = false;
   bool is_slowmotion                = false;
   bool is_perfcnt_enable            = false;
   http_transfer_data_t *data        = (http_transfer_data_t*)task_data;

   const char ch                     = '\"';
   const char s[2]                   = "\"";
   char* ret;
   char* string                      = NULL;

   int new_image_size                = 0;
   int new_sound_size                = 0;
   unsigned width, height;
   unsigned image_width, image_height;
   size_t pitch;
   const void* dummy_data;
   void* raw_image_data              = NULL;  
   void* raw_sound_data              = NULL;  
   settings_t *settings              = config_get_ptr();

   runloop_get_status(&is_paused, &is_idle, &is_slowmotion,
       &is_perfcnt_enable);

   if (!is_paused && settings->uints.ai_service_mode != 1)
      goto finish;

   if (!data || error)
      goto finish;
   
   data->data = (char*)realloc(data->data, data->len + 1);
   if (!data->data)
      goto finish;

   data->data[data->len] = '\0'; 

   /* Parse JSON body for the image and sound data */

   body_copy = strdup(data->data);
   int i = 0;
   int start = -1;
   char* found_string = NULL;
   int curr_state = 0;
   char curr;

   while (true)
   {
      curr = (char) *(body_copy+i);
      if (curr == '\0')
          break;
      if (curr == '\"')
      {
         if (start == -1)
            start = i;
         else
         {
            found_string = malloc(i-start);
            strncpy(found_string, body_copy+start+1, i-start-1);
            *(found_string+i-start-1) = '\0';
            if (curr_state == 1)/*image*/
            {
              raw_bmp_data = (void*) unbase64(found_string, 
                                              strlen(found_string),
                                              &new_image_size);
              curr_state = 0;
            }
            else if (curr_state == 2)
            {
              raw_sound_data = (void*) unbase64(found_string, 
                                                strlen(found_string),
                                                &new_sound_size);           
              curr_state = 0;
            }
            else if (strcmp(found_string, "image")==0)
            {
              curr_state = 1;
              free(found_string);
            }
            else if (strcmp(found_string, "sound")==0)
            {
              curr_state = 2;
              free(found_string);
            }
            else
            {
              curr_state = 0;
            }
            start = -1;
         }
      }
      i++;
   }
   if (found_string)
       free(found_string);

   if (raw_bmp_data == NULL && raw_sound_data == NULL)
   {
      error = "Invalid JSON body.";
      goto finish;
   }

   if (raw_bmp_data != NULL)
   { 
      /* Get the video frame dimensions reference */
      video_driver_cached_frame_get(&dummy_data, &width, &height, &pitch);
  
      /* Get image data (24 bit), and conver to the emulated pixel format */
      image_width = ((uint32_t) ((uint8_t) raw_bmp_data[21]) << 24) +
                    ((uint32_t) ((uint8_t) raw_bmp_data[20]) << 16) +
                    ((uint32_t) ((uint8_t) raw_bmp_data[19]) << 8) +
                    ((uint32_t) ((uint8_t) raw_bmp_data[18]) << 0);

      image_height = ((uint32_t) ((uint8_t) raw_bmp_data[25]) << 24) +
                     ((uint32_t) ((uint8_t) raw_bmp_data[24]) << 16) +
                     ((uint32_t) ((uint8_t) raw_bmp_data[23]) << 8) +
                     ((uint32_t) ((uint8_t) raw_bmp_data[22]) << 0);
      raw_image_data = raw_bmp_data+54*sizeof(uint8_t);
   
      scaler = calloc(1, sizeof(struct scaler_ctx));
      if (!scaler)
         goto finish;

      if (dummy_data == RETRO_HW_FRAME_BUFFER_VALID)
      {
         /*
           In this case, we used the viewport to grab the image
           and translate it, and we have the translated image in
           the raw_image_data buffer.
         */

         /* TODO: write to the viewport in this case */
         RARCH_LOG("Hardware frame buffer... writing to viewport not yet supported.\n");
         goto finish;
      }

      /* The assigned pitch may not be reliable.  The width of
         the video frame can change during run-time, but the
         pitch may not, so we just assign it as the width
         times the byte depth. 
      */
   
      if (video_driver_get_pixel_format() == RETRO_PIXEL_FORMAT_XRGB8888)
      {
         raw_output_data = (uint8_t*) malloc(width*height*4*sizeof(uint8_t));
         scaler->out_fmt = SCALER_FMT_ARGB8888;
         pitch = width*4;
         scaler->out_stride = width*4;
      }
      else
      {
         raw_output_data = (uint8_t*) malloc(width*height*2*sizeof(uint8_t));
         scaler->out_fmt = SCALER_FMT_RGB565;
         pitch = width*2;
         scaler->out_stride = width*1;
      }
   
      if (!raw_output_data)
         goto finish;
      scaler->in_fmt = SCALER_FMT_BGR24;
      scaler->in_width = image_width;
      scaler->in_height = image_height;
      scaler->out_width = width;
      scaler->out_height = height;
      scaler->scaler_type = SCALER_TYPE_POINT;
      scaler_ctx_gen_filter(scaler);
      scaler->in_stride = -1*width*3;

      scaler_ctx_scale_direct(scaler, raw_output_data, 
                              (uint8_t*)raw_image_data+(image_height-1)*width*3);
      video_driver_frame(raw_output_data, image_width, image_height, pitch);
   }
   if (raw_sound_data != NULL)
   {
     
      retro_task_t *t = task_init();
      nbio_buf_t *task_data = (nbio_buf_t*)calloc(1, sizeof(nbio_buf_t));
      task_data->buf = raw_sound_data;
      task_data->bufsize = new_sound_size;
      task_data->path=NULL;
      
   audio_mixer_stream_params_t params;
   nbio_buf_t *img = (nbio_buf_t*)task_data;
  
   if (!img)
      return;
   
   params.volume               = 1.0f;
   params.slot_selection_type  = AUDIO_MIXER_SLOT_SELECTION_AUTOMATIC;//user->slot_selection_type;
   params.slot_selection_idx   = 10;
   params.stream_type          = AUDIO_STREAM_TYPE_SYSTEM;//user->stream_type;
   params.type                 = AUDIO_MIXER_TYPE_WAV;
   params.state                = AUDIO_STREAM_STATE_PLAYING;
   params.buf                  = img->buf;
   params.bufsize              = img->bufsize;
   params.cb                   = NULL;
   params.basename             = NULL;

   audio_driver_mixer_add_stream(&params);

   if (img->path)
      free(img->path);
   if (params.basename != NULL)
      free(params.basename);
   free(img);

      RARCH_LOG("Loaded Sound Size: %i\n", new_sound_size);
   }
   RARCH_LOG("Translation done.\n"); 

finish:
   if (error)
      RARCH_ERR("%s: %s\n", msg_hash_to_str(MSG_DOWNLOAD_FAILED), error);
  
   if (data)
   {
      if (data->data)
         free(data->data);
      free(data);
   }
   if (user_data)
      free(user_data);

   if (body_copy)
      free(body_copy);
   
   if (raw_bmp_data)
      free(raw_bmp_data);

   if (scaler)
      free(scaler);
 
   if (raw_output_data)
      free(raw_output_data);
}


void call_translation_server(const char* body)
{
   settings_t *settings                  = config_get_ptr();

   RARCH_LOG("Server url:  %s\n", settings->arrays.ai_service_url);
   task_push_http_post_transfer(settings->arrays.ai_service_url, 
                                body, true, NULL, handle_translation_cb, NULL);
}
