/*
Copyright (c) 2012, Broadcom Europe Ltd
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

// ---- Include Files -------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>
#include <signal.h>
#include <assert.h>
#include <string.h>

#include "interface/vmcs_host/vc_tvservice.h"

#define TV_SUPPORTED_MODE_T TV_SUPPORTED_MODE_NEW_T
#define vc_tv_hdmi_get_supported_modes vc_tv_hdmi_get_supported_modes_new
#define vc_tv_hdmi_power_on_explicit vc_tv_hdmi_power_on_explicit_new

// ---- Public Variables ----------------------------------------------------

// ---- Private Constants and Types -----------------------------------------

// Logging macros (for remapping to other logging mechanisms, i.e., vcos_log)
#define LOG_ERR(  fmt, arg... )  fprintf( stderr, "[E] " fmt "\n", ##arg )
#define LOG_WARN( fmt, arg... )  fprintf( stderr, "[W] " fmt "\n", ##arg )
#define LOG_INFO( fmt, arg... )  fprintf( stderr, "[I] " fmt "\n", ##arg )
#define LOG_DBG(  fmt, arg... )  fprintf( stdout, "[D] " fmt "\n", ##arg )

// Standard output log (for printing normal information to users)
#define LOG_STD(  fmt, arg... )  fprintf( stdout, fmt "\n", ##arg )

// Maximum length of option string (3 characters max for each option + NULL)
#define OPTSTRING_LEN  ( sizeof( long_opts ) / sizeof( *long_opts ) * 3 + 1 )

// Maximum mode ID
#define MAX_MODE_ID (127)

// Maximum status string length
#define MAX_STATUS_STR_LENGTH (128)

// ---- Private Variables ---------------------------------------------------

enum
{
   OPT_PREFERRED = 'p',
   OPT_EXPLICIT  = 'e',
   OPT_NTSC      = 't',
   OPT_OFF       = 'o',
   OPT_SDTVON    = 'c',
   OPT_MODES     = 'm',
   OPT_MONITOR   = 'M',
   OPT_STATUS    = 's',
   OPT_DUMPEDID  = 'd',
   OPT_AUDIOSUP  = 'a',
   OPT_SHOWINFO  = 'i',
   OPT_JSON      = 'j',
   OPT_HELP      = 'h',
   OPT_NAME      = 'n',
   // Options from this point onwards don't have any short option equivalents
   OPT_FIRST_LONG_OPT = 0x80,
};

static struct option long_opts[] =
{
   // name                 has_arg              flag     val
   // -------------------  ------------------   ----     ---------------
   { "preferred",          no_argument,         NULL,    OPT_PREFERRED },
   { "explicit",           required_argument,   NULL,    OPT_EXPLICIT },
   { "ntsc",               no_argument,         NULL,    OPT_NTSC },
   { "off",                no_argument,         NULL,    OPT_OFF },
   { "sdtvon",             required_argument,   NULL,    OPT_SDTVON },
   { "modes",              required_argument,   NULL,    OPT_MODES },
   { "monitor",            no_argument,         NULL,    OPT_MONITOR },
   { "status",             no_argument,         NULL,    OPT_STATUS },
   { "dumpedid",           required_argument,   NULL,    OPT_DUMPEDID},
   { "audio",              no_argument,         NULL,    OPT_AUDIOSUP},
   { "json",               no_argument,         NULL,    OPT_JSON },
   { "info",               required_argument,   NULL,    OPT_SHOWINFO},
   { "help",               no_argument,         NULL,    OPT_HELP },
   { "name",               no_argument,         NULL,    OPT_NAME },
   { 0,                    0,                   0,       0 }
};

static VCOS_EVENT_T quit_event;

// ---- Private Functions ---------------------------------------------------

static void show_usage( void )
{
   LOG_STD( "Usage: tvservice [OPTION]..." );
   LOG_STD( "  -p, --preferred                   Power on HDMI with preferred settings" );
   LOG_STD( "  -e, --explicit=\"GROUP MODE DRIVE\" Power on HDMI with explicit GROUP (CEA, DMT, CEA_3D_SBS, CEA_3D_TB, CEA_3D_FP, CEA_3D_FS)\n"
            "                                      MODE (see --modes) and DRIVE (HDMI, DVI)" );
   LOG_STD( "  -t, --ntsc                        Use NTSC frequency for HDMI mode (e.g. 59.94Hz rather than 60Hz)" );
   LOG_STD( "  -c, --sdtvon=\"MODE ASPECT [P]\"    Power on SDTV with MODE (PAL or NTSC) and ASPECT (4:3 14:9 or 16:9) Add P for progressive" );
   LOG_STD( "  -o, --off                         Power off the display" );
   LOG_STD( "  -m, --modes=GROUP                 Get supported modes for GROUP (CEA, DMT)" );
   LOG_STD( "  -M, --monitor                     Monitor HDMI events" );
   LOG_STD( "  -s, --status                      Get HDMI status" );
   LOG_STD( "  -a, --audio                       Get supported audio information" );
   LOG_STD( "  -d, --dumpedid <filename>         Dump EDID information to file" );
   LOG_STD( "  -j, --json                        Use JSON format for --modes output" );
   LOG_STD( "  -n, --name                        Print the device ID from EDID" );
   LOG_STD( "  -h, --help                        Print this information" );
}

static void create_optstring( char *optstring )
{
   char *short_opts = optstring;
   struct option *option;

   // Figure out the short options from our options structure
   for ( option = long_opts; option->name != NULL; option++ )
   {
      if (( option->flag == NULL ) && ( option->val < OPT_FIRST_LONG_OPT ))
      {
         *short_opts++ = (char)option->val;

         if ( option->has_arg != no_argument )
         {
            *short_opts++ = ':';
         }

         // Optional arguments require two ':'
         if ( option->has_arg == optional_argument )
         {
            *short_opts++ = ':';
         }
      }
   }
   *short_opts++ = '\0';
}

/* Return the string presentation of aspect ratio */
static const char *aspect_ratio_str(HDMI_ASPECT_T aspect_ratio) {
   switch(aspect_ratio) {
   case HDMI_ASPECT_4_3:
      return "4:3";
   case HDMI_ASPECT_14_9:
      return "14:9";
   case HDMI_ASPECT_16_9:
      return "16:9";
   case HDMI_ASPECT_5_4:
      return "5:4";
   case HDMI_ASPECT_16_10:
      return "16:10";
   case HDMI_ASPECT_15_9:
      return "15:9";
   case HDMI_ASPECT_64_27:
      return "64:27 (21:9)";
   default:
      return "unknown AR";
   }
}

/* Return the string presentation of aspect ratio */
static const char *aspect_ratio_sd_str(SDTV_ASPECT_T aspect_ratio) {
   switch(aspect_ratio) {
   case SDTV_ASPECT_4_3:
      return "4:3";
   case SDTV_ASPECT_14_9:
      return "14:9";
   case SDTV_ASPECT_16_9:
      return "16:9";
   default:
      return "unknown AR";
   }
}

//Print a string and update the offset into the status buffer
//Return non-zero if string is truncated, zero otherwise
static int status_sprintf(char *buf, size_t buflen, size_t *offset, const char *fmt, ...) {
   int ret;
   size_t length;
   va_list ap;
   va_start(ap,fmt);
   length = (size_t) vcos_safe_vsprintf(buf, buflen, *offset, fmt, ap);
   va_end(ap);
   if(length >= buflen) {
      ret = -1;
      *offset = buflen;
   } else {
      ret = 0;
      *offset = length;
   }
   return ret;
}

/* Return the string representation of 3D support bit mask */
static const char* threed_str(uint32_t struct_3d_mask, int json_output) {
#define THREE_D_FORMAT_NAME_MAX_LEN 10 //Including the separator
   static const char* three_d_format_names[] = { //See HDMI_3D_STRUCT_T bit fields
      "FP", "F-Alt", "L-Alt", "SbS-Full",
      "Ldep", "Ldep+Gfx", "TopBot", "SbS-HH",
      "SbS-OLOR", "SbS-OLER", "SbS-ELOR", "SbS-ELER"
   };
   //Longest possible string is the concatenation of all of the above
   static char struct_desc[vcos_countof(three_d_format_names)*THREE_D_FORMAT_NAME_MAX_LEN+4] = {0};
   const size_t struct_desc_length = sizeof(struct_desc);
   size_t j, added = 0, offset = 0;
   int tmp = 0;
   if(!json_output)
      tmp = status_sprintf(struct_desc, struct_desc_length, &offset, "3D:");
   if(struct_3d_mask) {
      for(j = 0; !tmp && j < vcos_countof(three_d_format_names); j++) {
         if(struct_3d_mask & (1 << j)) {
            tmp = status_sprintf(struct_desc, struct_desc_length, &offset, json_output ? "\"%s\"," : "%s|", three_d_format_names[j]);
            added++;
         }
      }
      if(!tmp && added > 0)
         struct_desc[strlen(struct_desc)-1] = '\0'; //Get rid of final "|"
   } else if(!tmp && !added && !json_output) {
      status_sprintf(struct_desc, struct_desc_length, &offset, "none");
   }

   return struct_desc;
}

static int get_modes( HDMI_RES_GROUP_T group, int json_output)
{
   static TV_SUPPORTED_MODE_T supported_modes[MAX_MODE_ID];
   HDMI_RES_GROUP_T preferred_group;
   uint32_t preferred_mode;
   int num_modes;
   int i;

   vcos_assert(( group == HDMI_RES_GROUP_CEA ) ||
               ( group == HDMI_RES_GROUP_DMT ));

   memset(supported_modes, 0, sizeof(supported_modes));

   num_modes = vc_tv_hdmi_get_supported_modes( group, supported_modes,
                                               vcos_countof(supported_modes),
                                               &preferred_group,
                                               &preferred_mode );
   if ( num_modes < 0 )
   {
      LOG_ERR( "Failed to get modes" );
      return -1;
   }

   if (json_output)
   {
      LOG_STD( "[" );
   }
   else
   {
      LOG_STD( "Group %s has %u modes:",
               HDMI_RES_GROUP_NAME(group), num_modes );
   }

   for ( i = 0; i < num_modes; i++ )
   {
      char p[8] = {0};
      if( supported_modes[i].pixel_rep )
         vcos_safe_sprintf(p, sizeof(p)-1, 0, "x%d ", supported_modes[i].pixel_rep);

      if (json_output)
      {
         LOG_STD( "{ \"code\":%u, \"width\":%u, \"height\":%u, \"rate\":%u, \"aspect_ratio\":\"%s\", \"scan\":\"%s\", \"3d_modes\":[%s] }%s",
                  supported_modes[i].code, supported_modes[i].width,
                  supported_modes[i].height, supported_modes[i].frame_rate,
                  aspect_ratio_str(supported_modes[i].aspect_ratio),
                  supported_modes[i].scan_mode ? "i" : "p",
                  supported_modes[i].struct_3d_mask ? threed_str(supported_modes[i].struct_3d_mask, 1) : "",
                  (i+1 < num_modes) ? "," : "");
      }
      else
      {
         int preferred = supported_modes[i].group == preferred_group && supported_modes[i].code == preferred_mode;
         LOG_STD( "%s mode %u: %ux%u @ %uHz %s, clock:%uMHz %s%s %s",
                  preferred ? "  (prefer)" : supported_modes[i].native ? "  (native)" : "          ",
                  supported_modes[i].code, supported_modes[i].width,
                  supported_modes[i].height, supported_modes[i].frame_rate,
                  aspect_ratio_str(supported_modes[i].aspect_ratio),
                  supported_modes[i].pixel_freq / 1000000U, p,
                  supported_modes[i].scan_mode ? "interlaced" : "progressive",
                  supported_modes[i].struct_3d_mask ? threed_str(supported_modes[i].struct_3d_mask, 0) : "");
      }
   }

   if (json_output)
   {
      LOG_STD( "]" );
   }
   return 0;
}

static const char *status_mode( TV_DISPLAY_STATE_T *tvstate ) {
   static char mode_str[MAX_STATUS_STR_LENGTH] = {0};
   int tmp = 0;
   size_t offset = 0;
   if(tvstate->state & ( VC_HDMI_HDMI | VC_HDMI_DVI )) {
      //HDMI or DVI on
      tmp = status_sprintf(mode_str, MAX_STATUS_STR_LENGTH, &offset, (tvstate->state & VC_HDMI_HDMI) ? "HDMI" : "DVI");
      if(!tmp) {
         //We should still have space at this point
         switch(tvstate->display.hdmi.group) {
         case HDMI_RES_GROUP_CEA:
            tmp = status_sprintf(mode_str, MAX_STATUS_STR_LENGTH, &offset, " CEA (%d)", tvstate->display.hdmi.mode); break;
         case HDMI_RES_GROUP_DMT:
            tmp = status_sprintf(mode_str, MAX_STATUS_STR_LENGTH, &offset, " DMT (%d)", tvstate->display.hdmi.mode); break;
         default: break;
         }
      }
      if(!tmp && tvstate->display.hdmi.format_3d) {
         switch(tvstate->display.hdmi.format_3d) {
         case HDMI_3D_FORMAT_SBS_HALF:
            tmp = status_sprintf(mode_str, MAX_STATUS_STR_LENGTH, &offset, " 3D SbS"); break;
         case HDMI_3D_FORMAT_TB_HALF:
            tmp = status_sprintf(mode_str, MAX_STATUS_STR_LENGTH, &offset, " 3D T&B"); break;
         case HDMI_3D_FORMAT_FRAME_PACKING:
            tmp = status_sprintf(mode_str, MAX_STATUS_STR_LENGTH, &offset, " 3D FP"); break;
         case HDMI_3D_FORMAT_FRAME_SEQUENTIAL:
            tmp = status_sprintf(mode_str, MAX_STATUS_STR_LENGTH, &offset, " 3D FS"); break;
         default: break;
         }
      }
      if(!tmp) {
         if (tvstate->state & VC_HDMI_HDMI)
            //Only HDMI mode can signal pixel encoding in AVI infoframe
            switch(tvstate->display.hdmi.pixel_encoding) {
            case HDMI_PIXEL_ENCODING_RGB_LIMITED:
               tmp = status_sprintf(mode_str, MAX_STATUS_STR_LENGTH, &offset, " RGB lim"); break;
            case HDMI_PIXEL_ENCODING_RGB_FULL:
               tmp = status_sprintf(mode_str, MAX_STATUS_STR_LENGTH, &offset, " RGB full"); break;
            case HDMI_PIXEL_ENCODING_YCbCr444_LIMITED:
               tmp = status_sprintf(mode_str, MAX_STATUS_STR_LENGTH, &offset, " YCbCr444 lim"); break;
            case HDMI_PIXEL_ENCODING_YCbCr444_FULL:
               tmp = status_sprintf(mode_str, MAX_STATUS_STR_LENGTH, &offset, " YCbCr444 full"); break;
            case HDMI_PIXEL_ENCODING_YCbCr422_LIMITED:
               tmp = status_sprintf(mode_str, MAX_STATUS_STR_LENGTH, &offset, " YCbCr422 lim"); break;
            case HDMI_PIXEL_ENCODING_YCbCr422_FULL:
               tmp = status_sprintf(mode_str, MAX_STATUS_STR_LENGTH, &offset, " YCbCr422 full"); break;
            default: break;
            }
         else
            //DVI will always be RGB, and CEA mode will have limited range, DMT mode full range
            tmp = status_sprintf(mode_str, MAX_STATUS_STR_LENGTH, &offset,
                        (tvstate->display.hdmi.group == HDMI_RES_GROUP_CEA)?
                        " RGB lim" : " RGB full");
      }

      //This is the format's aspect ratio, not the one
      //signaled in the AVI infoframe
      if(!tmp)
         tmp = status_sprintf(mode_str, MAX_STATUS_STR_LENGTH, &offset, " %s", aspect_ratio_str(tvstate->display.hdmi.aspect_ratio));

      if(!tmp &&tvstate->display.hdmi.pixel_rep) {
         tmp = status_sprintf(mode_str, MAX_STATUS_STR_LENGTH, &offset, " x%d", tvstate->display.hdmi.pixel_rep);
      }
      if(!tmp && tvstate->state & VC_HDMI_HDCP_AUTH) {
         tmp = status_sprintf(mode_str, MAX_STATUS_STR_LENGTH, &offset, " HDCP");
      }
   } else if(tvstate->state & ( VC_SDTV_NTSC | VC_SDTV_PAL )) {
      if(!tmp) {
         if(tvstate->state & VC_SDTV_NTSC) {
            tmp = status_sprintf(mode_str, MAX_STATUS_STR_LENGTH, &offset, "NTSC");
         } else {
            tmp = status_sprintf(mode_str, MAX_STATUS_STR_LENGTH, &offset, "PAL");
         }
      }
      if(!tmp && tvstate->display.sdtv.cp_mode) {
         switch(tvstate->display.sdtv.cp_mode) {
         case SDTV_CP_MACROVISION_TYPE1:
            tmp = status_sprintf(mode_str, MAX_STATUS_STR_LENGTH, &offset, " Macrovision type 1"); break;
         case SDTV_CP_MACROVISION_TYPE2:
            tmp = status_sprintf(mode_str, MAX_STATUS_STR_LENGTH, &offset, " Macrovision type 2"); break;
         case SDTV_CP_MACROVISION_TYPE3:
            tmp = status_sprintf(mode_str, MAX_STATUS_STR_LENGTH, &offset, " Macrovision type 3"); break;
         case SDTV_CP_MACROVISION_TEST1:
            tmp = status_sprintf(mode_str, MAX_STATUS_STR_LENGTH, &offset, " Macrovision test 1"); break;
         case SDTV_CP_MACROVISION_TEST2:
            tmp = status_sprintf(mode_str, MAX_STATUS_STR_LENGTH, &offset, " Macrovision test 2"); break;
         case SDTV_CP_CGMS_COPYFREE:
            tmp = status_sprintf(mode_str, MAX_STATUS_STR_LENGTH, &offset, " CGMS copy free"); break;
         case SDTV_CP_CGMS_COPYNOMORE:
            tmp = status_sprintf(mode_str, MAX_STATUS_STR_LENGTH, &offset, " CGMS copy no more"); break;
         case SDTV_CP_CGMS_COPYONCE:
            tmp = status_sprintf(mode_str, MAX_STATUS_STR_LENGTH, &offset, " CGMS copy once"); break;
         case SDTV_CP_CGMS_COPYNEVER:
            tmp = status_sprintf(mode_str, MAX_STATUS_STR_LENGTH, &offset, " CGMS copy never"); break;
         case SDTV_CP_WSS_COPYFREE:
            tmp = status_sprintf(mode_str, MAX_STATUS_STR_LENGTH, &offset, " WSS copy free"); break;
         case SDTV_CP_WSS_COPYRIGHT_COPYFREE:
            tmp = status_sprintf(mode_str, MAX_STATUS_STR_LENGTH, &offset, " WSS (c) copy free"); break;
         case SDTV_CP_WSS_NOCOPY:
            tmp = status_sprintf(mode_str, MAX_STATUS_STR_LENGTH, &offset, " WSS no copy"); break;
         case SDTV_CP_WSS_COPYRIGHT_NOCOPY:
            tmp = status_sprintf(mode_str, MAX_STATUS_STR_LENGTH, &offset, " WSS (c) no copy"); break;
         default: break;
         }
      }
      //This is the format's aspect ratio
      tmp = status_sprintf(mode_str, MAX_STATUS_STR_LENGTH, &offset, " %s", aspect_ratio_sd_str(tvstate->display.sdtv.display_options.aspect));
   } else if (tvstate->state & VC_LCD_ATTACHED_DEFAULT) {
      status_sprintf(mode_str, MAX_STATUS_STR_LENGTH, &offset, "LCD");
   } else {
      status_sprintf(mode_str, MAX_STATUS_STR_LENGTH, &offset, "TV is off");
   }

   return mode_str;
}

static int get_status( void )
{
   TV_DISPLAY_STATE_T tvstate;
   if( vc_tv_get_display_state( &tvstate ) == 0) {
      //The width/height parameters are in the same position in the union
      //for HDMI and SDTV
      HDMI_PROPERTY_PARAM_T property;
      property.property = HDMI_PROPERTY_PIXEL_CLOCK_TYPE;
      vc_tv_hdmi_get_property(&property);
      float frame_rate = property.param1 == HDMI_PIXEL_CLOCK_TYPE_NTSC ? tvstate.display.hdmi.frame_rate * (1000.0f/1001.0f) : tvstate.display.hdmi.frame_rate;

      if(tvstate.display.hdmi.width && tvstate.display.hdmi.height) {
         LOG_STD( "state 0x%x [%s], %ux%u @ %.2fHz, %s", tvstate.state,
                  status_mode(&tvstate),
                  tvstate.display.hdmi.width, tvstate.display.hdmi.height,
                  frame_rate,
                  tvstate.display.hdmi.scan_mode ? "interlaced" : "progressive" );
      } else {
         LOG_STD( "state 0x%x [%s]", tvstate.state, status_mode(&tvstate));
      }
   } else {
      LOG_STD( "Error getting current display state");
   }
  return 0;
}

static int get_audiosup( void )
{
  uint8_t sample_rates[] = {32, 44, 48, 88, 96, 176, 192};
  uint8_t sample_sizes[] = {16, 20, 24};
  const char *formats[] = {"Reserved", "PCM", "AC3", "MPEG1", "MP3", "MPEG2", "AAC", "DTS", "ATRAC", "DSD", "EAC3", "DTS_HD", "MLP", "DST", "WMAPRO", "Extended"};
  int i, j, k;
  for (k=EDID_AudioFormat_ePCM; k<EDID_AudioFormat_eMaxCount; k++) {
    int num_channels = 0, max_sample_rate = 0, max_sample_size = 0;
    for (i=1; i<=8; i++) {
      if (vc_tv_hdmi_audio_supported(k, i, EDID_AudioSampleRate_e44KHz, EDID_AudioSampleSize_16bit ) == 0)
        num_channels = i;
    }
    for (i=0, j=EDID_AudioSampleRate_e32KHz; j<=EDID_AudioSampleRate_e192KHz; i++, j<<=1) {
      if (vc_tv_hdmi_audio_supported(k, 1, j, EDID_AudioSampleSize_16bit ) == 0)
        max_sample_rate = i;
    }
    if (k==EDID_AudioFormat_ePCM) {
      for (i=0, j=EDID_AudioSampleSize_16bit; j<=EDID_AudioSampleSize_24bit; i++, j<<=1) {
        if (vc_tv_hdmi_audio_supported(k, 1, EDID_AudioSampleRate_e44KHz, j ) == 0)
          max_sample_size = i;
      }
      if (num_channels>0)
        LOG_STD("%8s supported: Max channels: %d, Max samplerate:%4dkHz, Max samplesize %2d bits.", formats[k], num_channels, sample_rates[max_sample_rate], sample_sizes[max_sample_size]);
    } else {
      for (i=0; i<256; i++) {
        if (vc_tv_hdmi_audio_supported(k, 1, EDID_AudioSampleRate_e44KHz, i ) == 0)
          max_sample_size = i;
      }
      if (num_channels>0)
        LOG_STD("%8s supported: Max channels: %d, Max samplerate:%4dkHz, Max rate %4d kb/s.", formats[k], num_channels, sample_rates[max_sample_rate], 8*max_sample_size);
    }
  }
  return 0;
}

static int dump_edid( const char *filename )
{
   uint8_t buffer[128];
   size_t written = 0, offset = 0;
   int i, extensions = 0;
   FILE *fp = NULL;
   int siz = vc_tv_hdmi_ddc_read(offset, sizeof (buffer), buffer);
   offset += sizeof( buffer);
   /* First block always exist */
   if (siz == sizeof(buffer) && (fp = fopen(filename, "wb")) != NULL) {
      written += fwrite(buffer, 1, sizeof buffer, fp);
      extensions = buffer[0x7e]; /* This tells you how many more blocks to read */
      for(i = 0; i < extensions; i++, offset += sizeof( buffer)) {
         siz = vc_tv_hdmi_ddc_read(offset, sizeof( buffer), buffer);
         if(siz == sizeof(buffer)) {
            written += fwrite(buffer, 1, sizeof (buffer), fp);
         } else {
            break;
         }
      }
   }
   if (fp)
      fclose(fp);
   if(written) {
      LOG_STD( "Written %d bytes to %s", written, filename);
   } else {
      LOG_STD( "Nothing written!");
   }
   return written < sizeof(buffer);
}

static int show_info( int on )
{
   return vc_tv_show_info(on);
}

static void control_c( int signum )
{
    (void)signum;

    LOG_STD( "Shutting down..." );

    vcos_event_signal( &quit_event );
}

static void tvservice_callback( void *callback_data,
                                uint32_t reason,
                                uint32_t param1,
                                uint32_t param2 )
{
   (void)callback_data;
   (void)param1;
   (void)param2;

   switch ( reason )
   {
      case VC_HDMI_UNPLUGGED:
      {
         LOG_INFO( "HDMI cable is unplugged" );
         break;
      }
      case VC_HDMI_ATTACHED:
      {
         LOG_INFO( "HDMI is attached" );
         break;
      }
      case VC_HDMI_DVI:
      {
         LOG_INFO( "HDMI in DVI mode" );
         break;
      }
      case VC_HDMI_HDMI:
      {
         LOG_INFO( "HDMI in HDMI mode" );
         break;
      }
      case VC_HDMI_HDCP_UNAUTH:
      {
         LOG_INFO( "HDCP authentication is broken" );
         break;
      }
      case VC_HDMI_HDCP_AUTH:
      {
         LOG_INFO( "HDCP is active" );
         break;
      }
      case VC_HDMI_HDCP_KEY_DOWNLOAD:
      {
         LOG_INFO( "HDCP key download" );
         break;
      }
      case VC_HDMI_HDCP_SRM_DOWNLOAD:
      {
         LOG_INFO( "HDCP revocation list download" );
         break;
      }
      default:
      {
         // Ignore all other reasons
         LOG_INFO( "Callback with reason %d", reason );
         break;
      }
   }
}

static int start_monitor( void )
{
   if ( vcos_event_create( &quit_event, "tvservice" ) != VCOS_SUCCESS )
   {
      LOG_ERR( "Failed to create quit event" );

      return -1;
   }

   // Handle the INT and TERM signals so we can quit
   signal( SIGINT, control_c );
   signal( SIGTERM, control_c );

   vc_tv_register_callback( &tvservice_callback, NULL );

   return 0;
}

static int power_on_preferred( void )
{
   int ret;

   LOG_STD( "Powering on HDMI with preferred settings" );

   ret = vc_tv_hdmi_power_on_preferred();
   if ( ret != 0 )
   {
      LOG_ERR( "Failed to power on HDMI with preferred settings" );
   }

   return ret;
}

static int power_on_explicit( HDMI_RES_GROUP_T group,
                              uint32_t mode, uint32_t drive )
{
   int ret;

   LOG_STD( "Powering on HDMI with explicit settings (%s mode %u)",
            group == HDMI_RES_GROUP_CEA ? "CEA" : group == HDMI_RES_GROUP_DMT ? "DMT" : "CUSTOM", mode );

   ret = vc_tv_hdmi_power_on_explicit( drive, group, mode );
   if ( ret != 0 )
   {
      LOG_ERR( "Failed to power on HDMI with explicit settings (%s mode %u)",
               group == HDMI_RES_GROUP_CEA ? "CEA" : group == HDMI_RES_GROUP_DMT ? "DMT" : "CUSTOM", mode );
   }

   return ret;
}

static int set_property(HDMI_PROPERTY_T prop, uint32_t param1, uint32_t param2)
{
   int ret;
   HDMI_PROPERTY_PARAM_T property;
   property.property = prop;
   property.param1 = param1;
   property.param2 = param2;
   //LOG_DBG( "Setting property %d with params %d, %d", prop, param1, param2);
   ret = vc_tv_hdmi_set_property(&property);
   if(ret != 0)
   {
      LOG_ERR( "Failed to set property %d", prop);
   }
   return ret;
}

static int power_on_sdtv( SDTV_MODE_T mode,
                              SDTV_ASPECT_T aspect, int sdtv_progressive )
{
   int ret;
   SDTV_OPTIONS_T options;
   memset(&options, 0, sizeof options);
   options.aspect = aspect;
   if (sdtv_progressive)
      mode |= SDTV_MODE_PROGRESSIVE;
   LOG_STD( "Powering on SDTV with explicit settings (mode:%d aspect:%d)",
            mode, aspect );

   ret = vc_tv_sdtv_power_on(mode, &options);
   if ( ret != 0 )
   {
      LOG_ERR( "Failed to power on SDTV with explicit settings (mode:%d aspect:%d)",
               mode, aspect );
   }

   return ret;
}

static int power_off( void )
{
   int ret;

   LOG_STD( "Powering off HDMI");

   ret = vc_tv_power_off();
   if ( ret != 0 )
   {
      LOG_ERR( "Failed to power off HDMI" );
   }

   return ret;
}

// ---- Public Functions -----------------------------------------------------

int main( int argc, char **argv )
{
   int32_t ret;
   char optstring[OPTSTRING_LEN];
   int  opt;
   int  opt_preferred = 0;
   int  opt_explicit = 0;
   int  opt_ntsc = 0;
   int  opt_sdtvon = 0;
   int  opt_off = 0;
   int  opt_modes = 0;
   int  opt_monitor = 0;
   int  opt_status = 0;
   int  opt_audiosup = 0;
   int  opt_dumpedid = 0;
   int  opt_showinfo = 0;
   int  opt_3d = 0;
   int  opt_json = 0;
   int  opt_name = 0;

   char *dumpedid_filename = NULL;
   VCHI_INSTANCE_T    vchi_instance;
   VCHI_CONNECTION_T *vchi_connection;
   HDMI_RES_GROUP_T power_on_explicit_group = HDMI_RES_GROUP_INVALID;
   uint32_t         power_on_explicit_mode;
   uint32_t         power_on_explicit_drive = HDMI_MODE_HDMI;
   HDMI_RES_GROUP_T get_modes_group = HDMI_RES_GROUP_INVALID;
   SDTV_MODE_T sdtvon_mode = SDTV_MODE_NTSC;
   SDTV_ASPECT_T sdtvon_aspect = SDTV_ASPECT_UNKNOWN;
   int sdtvon_progressive = 0;

   // Initialize VCOS
   vcos_init();

   // Create the option string that we will be using to parse the arguments
   create_optstring( optstring );

   // Parse the command line arguments
   while (( opt = getopt_long_only( argc, argv, optstring, long_opts,
                                    NULL )) != -1 )
   {
      switch ( opt )
      {
         case 0:
         {
            // getopt_long returns 0 for entries where flag is non-NULL
            break;
         }
         case OPT_PREFERRED:
         {
            opt_preferred = 1;
            break;
         }
         case OPT_EXPLICIT:
         {
            char group_str[32], drive_str[32];

            /* coverity[secure_coding] String length specified, so can't overflow */
            int s = sscanf( optarg, "%31s %u %31s", group_str, &power_on_explicit_mode, drive_str );
            if ( s != 2 && s != 3 )
            {
               LOG_ERR( "Invalid arguments '%s'", optarg );
               goto err_out;
            }

            // Check the group first
            if ( vcos_strcasecmp( "CEA", group_str ) == 0 )
            {
               power_on_explicit_group = HDMI_RES_GROUP_CEA;
            }
            else if ( vcos_strcasecmp( "DMT", group_str ) == 0 )
            {
               power_on_explicit_group = HDMI_RES_GROUP_DMT;
            }
            else if ( vcos_strcasecmp( "CEA_3D", group_str ) == 0  ||
                      vcos_strcasecmp( "CEA_3D_SBS", group_str ) == 0)
            {
               power_on_explicit_group = HDMI_RES_GROUP_CEA;
               opt_3d = 1;
            }
            else if ( vcos_strcasecmp( "CEA_3D_TB", group_str ) == 0 )
            {
               power_on_explicit_group = HDMI_RES_GROUP_CEA;
               opt_3d = 2;
            }
            else if ( vcos_strcasecmp( "CEA_3D_FP", group_str ) == 0 )
            {
               power_on_explicit_group = HDMI_RES_GROUP_CEA;
               opt_3d = 3;
            }
            else if ( vcos_strcasecmp( "CEA_3D_FS", group_str ) == 0 )
            {
               power_on_explicit_group = HDMI_RES_GROUP_CEA;
               opt_3d = 4;
            }
            else
            {
               LOG_ERR( "Invalid group '%s'", group_str );
               goto err_out;
            }
            if (s==3)
            {
               if (vcos_strcasecmp( "HDMI", drive_str ) == 0 )
               {
                  power_on_explicit_drive = HDMI_MODE_HDMI;
               }
               else if (vcos_strcasecmp( "DVI", drive_str ) == 0 )
               {
                  power_on_explicit_drive = HDMI_MODE_DVI;
               }
               else
               {
                  LOG_ERR( "Invalid drive '%s'", drive_str );
                  goto err_out;
               }
            }
            // Then check if mode is a sane number
            if ( power_on_explicit_mode > MAX_MODE_ID )
            {
               LOG_ERR( "Invalid mode '%u'", power_on_explicit_mode );
               goto err_out;
            }

            opt_explicit = 1;
            break;
         }
         case OPT_NTSC:
         {
            opt_ntsc = 1;
            break;
         }
         case OPT_SDTVON:
         {
            char mode_str[32], aspect_str[32], progressive_str[32];
            int s = sscanf( optarg, "%s %s %s", mode_str, aspect_str, progressive_str );
            if ( s != 2 && s != 3 )
            {
               LOG_ERR( "Invalid arguments '%s'", optarg );
               goto err_out;
            }

            // Check the group first
            if ( vcos_strcasecmp( "NTSC", mode_str ) == 0 )
            {
               sdtvon_mode = SDTV_MODE_NTSC;
            }
            else if ( vcos_strcasecmp( "NTSC_J", mode_str ) == 0 )
            {
               sdtvon_mode = SDTV_MODE_NTSC_J;
            }
            else if ( vcos_strcasecmp( "PAL", mode_str ) == 0 )
            {
               sdtvon_mode = SDTV_MODE_PAL;
            }
            else if ( vcos_strcasecmp( "PAL_M", mode_str ) == 0 )
            {
               sdtvon_mode = SDTV_MODE_PAL_M;
            }
            else
            {
               LOG_ERR( "Invalid mode '%s'", mode_str );
               goto err_out;
            }

            if ( vcos_strcasecmp( "4:3", aspect_str ) == 0 )
            {
               sdtvon_aspect = SDTV_ASPECT_4_3;
            }
            else if ( vcos_strcasecmp( "14:9", aspect_str ) == 0 )
            {
               sdtvon_aspect = SDTV_ASPECT_14_9;
            }
            else if ( vcos_strcasecmp( "16:9", aspect_str ) == 0 )
            {
               sdtvon_aspect = SDTV_ASPECT_16_9;
            }

            if (s == 3 && vcos_strcasecmp( "P", progressive_str ) == 0 )
            {
              sdtvon_progressive = 1;
            }
            opt_sdtvon = 1;
            break;
         }
         case OPT_OFF:
         {
            opt_off = 1;
            break;
         }
         case OPT_MODES:
         {
            if ( vcos_strcasecmp( "CEA", optarg ) == 0 )
            {
               get_modes_group = HDMI_RES_GROUP_CEA;
            }
            else if ( vcos_strcasecmp( "DMT", optarg ) == 0 )
            {
               get_modes_group = HDMI_RES_GROUP_DMT;
            }
            else
            {
               LOG_ERR( "Invalid group '%s'", optarg );
               goto err_out;
            }

            opt_modes = 1;
            break;
         }
         case OPT_MONITOR:
         {
            opt_monitor = 1;
            break;
         }
         case OPT_STATUS:
         {
            opt_status = 1;
            break;
         }
         case OPT_AUDIOSUP:
         {
            opt_audiosup = 1;
            break;
         }
         case OPT_DUMPEDID:
         {
            opt_dumpedid = 1;
            dumpedid_filename = optarg;
            break;
         }
         case OPT_SHOWINFO:
         {
            opt_showinfo = atoi(optarg)+1;
            break;
         }
         case OPT_JSON:
         {
            opt_json = 1;
            break;
         }
         case OPT_NAME:
         {
            opt_name = 1;
            break;
         }
         default:
         {
            LOG_ERR( "Unrecognized option '%d'\n", opt );
            goto err_usage;
         }
         case '?':
         case OPT_HELP:
         {
            goto err_usage;
         }
      } // end switch
   } // end while

   argc -= optind;
   argv += optind;

   if (( optind == 1 ) || ( argc > 0 ))
   {
      if ( argc > 0 )
      {
         LOG_ERR( "Unrecognized argument -- '%s'", *argv );
      }

      goto err_usage;
   }

   if (( opt_preferred + opt_explicit + opt_sdtvon > 1 ))
   {
      LOG_ERR( "Conflicting power on options" );
      goto err_usage;
   }

   if ((( opt_preferred == 1 ) || ( opt_explicit == 1 ) || ( opt_sdtvon == 1)) && ( opt_off == 1 ))
   {
      LOG_ERR( "Cannot power on and power off simultaneously" );
      goto err_out;
   }

   // Initialize the VCHI connection
   ret = vchi_initialise( &vchi_instance );
   if ( ret != 0 )
   {
      LOG_ERR( "Failed to initialize VCHI (ret=%d)", ret );
      goto err_out;
   }

   ret = vchi_connect( NULL, 0, vchi_instance );
   if ( ret != 0)
   {
      LOG_ERR( "Failed to create VCHI connection (ret=%d)", ret );
      goto err_out;
   }

//   LOG_INFO( "Starting tvservice" );

   // Initialize the tvservice
   vc_vchi_tv_init( vchi_instance, &vchi_connection, 1 );

   if ( opt_monitor == 1 )
   {
      LOG_STD( "Starting to monitor for HDMI events" );

      if ( start_monitor() != 0 )
      {
         goto err_stop_service;
      }
   }

   if ( opt_modes == 1 )
   {
      if ( get_modes( get_modes_group, opt_json ) != 0 )
      {
         goto err_stop_service;
      }
   }

   if ( opt_preferred == 1 )
   {
      if(set_property( HDMI_PROPERTY_3D_STRUCTURE, HDMI_3D_FORMAT_NONE, 0) != 0)
      {
         goto err_stop_service;
      }
      if ( power_on_preferred() != 0 )
      {
         goto err_stop_service;
      }
   }
   else if ( opt_explicit == 1 )
   {
      //Distinguish between turning on 3D side by side and 3D top/bottom
      if(opt_3d == 0 && set_property( HDMI_PROPERTY_3D_STRUCTURE, HDMI_3D_FORMAT_NONE, 0) != 0)
      {
         goto err_stop_service;
      }
      else if(opt_3d == 1 && set_property( HDMI_PROPERTY_3D_STRUCTURE, HDMI_3D_FORMAT_SBS_HALF, 0) != 0)
      {
         goto err_stop_service;
      }
      else if(opt_3d == 2 && set_property( HDMI_PROPERTY_3D_STRUCTURE, HDMI_3D_FORMAT_TB_HALF, 0) != 0)
      {
         goto err_stop_service;
      }
      else if(opt_3d == 3 && set_property( HDMI_PROPERTY_3D_STRUCTURE, HDMI_3D_FORMAT_FRAME_PACKING, 0) != 0)
      {
         goto err_stop_service;
      }
      else if(opt_3d == 4 && set_property( HDMI_PROPERTY_3D_STRUCTURE, HDMI_3D_FORMAT_FRAME_SEQUENTIAL, 0) != 0)
      {
         goto err_stop_service;
      }
      if (set_property( HDMI_PROPERTY_PIXEL_CLOCK_TYPE, opt_ntsc ? HDMI_PIXEL_CLOCK_TYPE_NTSC : HDMI_PIXEL_CLOCK_TYPE_PAL, 0) != 0)
      {
         goto err_stop_service;
      }
      if ( power_on_explicit( power_on_explicit_group,
                              power_on_explicit_mode, power_on_explicit_drive ) != 0 )
      {
         goto err_stop_service;
      }
   }
   else if ( opt_sdtvon == 1 )
   {
      if ( power_on_sdtv( sdtvon_mode,
                              sdtvon_aspect, sdtvon_progressive ) != 0 )
      {
         goto err_stop_service;
      }
   }
   else if (opt_off == 1 )
   {
      if ( power_off() != 0 )
      {
         goto err_stop_service;
      }
   }

   if ( opt_status == 1 )
   {
      if ( get_status() != 0 )
      {
         goto err_stop_service;
      }
   }

   if ( opt_audiosup == 1 )
   {
      if ( get_audiosup() != 0 )
      {
         goto err_stop_service;
      }
   }

   if ( opt_dumpedid == 1 )
   {
      if ( dump_edid(dumpedid_filename) != 0 )
      {
         goto err_stop_service;
      }
   }

   if ( opt_showinfo )
   {
      if ( show_info(opt_showinfo-1) != 0 )
      {
         goto err_stop_service;
      }
   }

   if ( opt_name == 1 )
   {
      TV_DEVICE_ID_T id;
      memset(&id, 0, sizeof(id));
      if(vc_tv_get_device_id(&id) == 0) {
         if(id.vendor[0] == '\0' || id.monitor_name[0] == '\0') {
            LOG_ERR( "No device present" );
         } else {
            LOG_STD( "device_name=%s-%s", id.vendor, id.monitor_name);
         }
      } else {
         LOG_ERR( "Failed to obtain device name" );
      }
   }

   if ( opt_monitor == 1 )
   {
      // Wait until we get the signal to exit
      vcos_event_wait( &quit_event );

      vcos_event_delete( &quit_event );
   }

err_stop_service:
//   LOG_INFO( "Stopping tvservice" );

   // Stop the tvservice
   vc_vchi_tv_stop();

   // Disconnect the VCHI connection
   vchi_disconnect( vchi_instance );

   exit( 0 );

err_usage:
   show_usage();

err_out:
   exit( 1 );
}
