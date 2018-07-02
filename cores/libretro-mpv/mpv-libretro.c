/* mpv media player libretro core
 * Copyright (C) 2018 Mahyar Koshkouei
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_LOCALE
#include <locale.h>
#endif

#include <mpv/client.h>
#include <mpv/render_gl.h>

#include <libretro.h>
#ifdef RARCH_INTERNAL
#include "internal_cores.h"
#define CORE_PREFIX(s) libretro_mpv_##s
#else
#define CORE_PREFIX(s) s
#endif

#include "version.h"

static struct retro_hw_render_callback hw_render;

static struct retro_log_callback logging;
static retro_log_printf_t log_cb;

static retro_video_refresh_t CORE_PREFIX(video_cb);
static retro_audio_sample_t CORE_PREFIX(audio_cb);
static retro_audio_sample_batch_t CORE_PREFIX(audio_batch_cb);
static retro_environment_t CORE_PREFIX(environ_cb);
static retro_input_poll_t CORE_PREFIX(input_poll_cb);
static retro_input_state_t CORE_PREFIX(input_state_cb);

static mpv_handle *mpv;
static mpv_render_context *mpv_gl;

/* Save the current playback time for context changes */
static int64_t playback_time = 0;

/* filepath required globaly as mpv is reopened on context change */
static char *filepath = NULL;

static volatile int frame_queue = 0;

void on_mpv_redraw(void *cb_ctx)
{
	frame_queue++;
}

static void fallback_log(enum retro_log_level level, const char *fmt, ...)
{
	(void)level;
	va_list va;
	va_start(va, fmt);
	vfprintf(stderr, fmt, va);
	va_end(va);
}

/**
 * Process various events triggered by mpv, such as printing log messages.
 *
 * \param event_block	Wait until the mpv triggers specified event. Should be
 *						NULL if no wait is required.
 */
static void process_mpv_events(mpv_event_id event_block)
{
	do
	{
		mpv_event *mp_event = mpv_wait_event(mpv, 0);
		if(event_block == MPV_EVENT_NONE &&
				mp_event->event_id == MPV_EVENT_NONE)
			break;

		if(mp_event->event_id == event_block)
			event_block = MPV_EVENT_NONE;

		if(mp_event->event_id == MPV_EVENT_LOG_MESSAGE)
		{
			struct mpv_event_log_message *msg =
				(struct mpv_event_log_message *)mp_event->data;
			log_cb(RETRO_LOG_INFO, "mpv: [%s] %s: %s",
					msg->prefix, msg->level, msg->text);
		}
		else if(mp_event->event_id == MPV_EVENT_END_FILE)
		{
			struct mpv_event_end_file *eof =
				(struct mpv_event_end_file *)mp_event->data;

			if(eof->reason == MPV_END_FILE_REASON_EOF)
				CORE_PREFIX(environ_cb)(RETRO_ENVIRONMENT_SHUTDOWN, NULL);
#if 0
			/* The following could be done instead if the file was not
			 * closed once the end was reached - allowing the user to seek
			 * back without reopening the file.
			 */
			struct retro_message ra_msg = {
				"Finished playing file", 60 * 5, /* 5 seconds */
			};

			CORE_PREFIX(environ_cb)(RETRO_ENVIRONMENT_SET_MESSAGE, &ra_msg);RETRO_ENVIRONMENT_SHUTDOWN
#endif
		}
		else if(mp_event->event_id == MPV_EVENT_NONE)
			continue;
		else
		{
			log_cb(RETRO_LOG_INFO, "mpv: %s\n",
					mpv_event_name(mp_event->event_id));
		}
	}
	while(1);
}

static void *get_proc_address_mpv(void *fn_ctx, const char *name)
{
	/* The "ISO C forbids conversion of function pointer to object pointer
	 * type" warning is suppressed.
	 */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
	void *proc_addr = (void *) hw_render.get_proc_address(name);
#pragma GCC diagnostic pop

	if(proc_addr == NULL)
		log_cb(RETRO_LOG_ERROR, "Failure obtaining %s proc address\n", name);

	return proc_addr;
}

void CORE_PREFIX(retro_init)(void)
{
	if(mpv_client_api_version() != MPV_CLIENT_API_VERSION)
	{
		log_cb(RETRO_LOG_WARN, "libmpv version mismatch. Please update or "
				"recompile mpv-libretro after updating libmpv.");
	}

	return;
}

void CORE_PREFIX(retro_deinit)(void)
{}

unsigned CORE_PREFIX(retro_api_version)(void)
{
	return RETRO_API_VERSION;
}

void CORE_PREFIX(retro_set_controller_port_device)(unsigned port, unsigned device)
{
	log_cb(RETRO_LOG_INFO, "Plugging device %u into port %u.\n", device, port);
	return;
}

void CORE_PREFIX(retro_get_system_info)(struct retro_system_info *info)
{
	memset(info, 0, sizeof(*info));
	info->library_name     = "mpv";
	info->library_version  = LIBRETRO_MPV_VERSION;
	info->need_fullpath    = true;	/* Allow MPV to load the file on its own */
	info->valid_extensions = "264|265|302|669|722|3g2|3gp|aa|aa3|aac|abc|ac3|"
		"acm|adf|adp|ads|adx|aea|afc|aix|al|amf|ams|ans|ape|apl|aqt|art|asc|"
		"ast|avc|avi|avr|avs|bcstm|bfstm|bin|bit|bmv|brstm|cdata|cdg|cdxl|cgi|"
		"cif|daud|dbm|dif|diz|dmf|dsm|dss|dtk|dts|dtshd|dv|eac3|fap|far|flac|"
		"flm|flv|fsb|g722|g723_1|g729|genh|gsm|h261|h264|h265|h26l|hevc|ice|"
		"idf|idx|ircam|it|itgz|itr|itz|ivr|j2k|lvf|m2a|m3u8|m4a|m4s|m4v|mac|"
		"mdgz|mdl|mdr|mdz|med|mid|mj2|mjpeg|mjpg|mk3d|mka|mks|mkv|mlp|mod|mov|"
		"mp2|mp3|mp4|mpa|mpc|mpeg|mpegts|mpg|mpl2|mpo|msf|mt2|mtaf|mtm|musx|"
		"mvi|mxg|nfo|nist|nut|oga|ogg|ogv|okt|oma|omg|paf|pjs|psm|ptm|pvf|qcif|"
		"rco|rgb|rsd|rso|rt|s3gz|s3m|s3r|s3z|sami|sb|sbg|scc|sdr2|sds|sdx|sf|"
		"shn|sln|smi|son|sph|ss2|stl|stm|str|sub|sup|svag|sw|tak|tco|thd|ts|"
		"tta|txt|ub|ul|ult|umx|uw|v|v210|vag|vb|vc1|viv|vob|vpk|vqe|vqf|vql|vt|"
		"vtt|wav|wsd|xl|xm|xmgz|xmr|xmv|xmz|xvag|y4m|yop|yuv|yuv10";
}

void CORE_PREFIX(retro_get_system_av_info)(struct retro_system_av_info *info)
{
	float sampling_rate = 48000.0f;

#if 0
	struct retro_variable var = { .key = "test_aspect" };
	CORE_PREFIX(environ_cb)(RETRO_ENVIRONMENT_GET_VARIABLE, &var);

	var.key = "test_samplerate";

	if(CORE_PREFIX(environ_cb)(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
		sampling_rate = strtof(var.value, NULL);
#endif
	info->timing = (struct retro_system_timing) {
		.fps = 60.0,
		.sample_rate = sampling_rate,
	};

	/* We don't know the dimensions of the video yet, so we set some good
	 * defaults in the meantime.
	 */
	info->geometry = (struct retro_game_geometry) {
		.base_width   = 256,
		.base_height  = 144,
		.max_width    = 1920,
		.max_height   = 1080,
		.aspect_ratio = -1,
	};
}

void CORE_PREFIX(retro_set_environment)(retro_environment_t cb)
{
	CORE_PREFIX(environ_cb) = cb;

	static const struct retro_variable vars[] = {
#if 0
		{ "test_samplerate", "Sample Rate; 48000" },
		{ "test_opt0", "Test option #0; false|true" },
		{ "test_opt1", "Test option #1; 0" },
		{ "test_opt2", "Test option #2; 0|1|foo|3" },
#endif
		{ NULL, NULL },
	};

	cb(RETRO_ENVIRONMENT_SET_VARIABLES, (void*)vars);

	if (cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &logging))
		log_cb = logging.log;
	else
		log_cb = fallback_log;
}

static void context_reset(void)
{
	const char *cmd[] = {"loadfile", filepath, NULL};
	int ret;

#ifdef HAVE_LOCALE
	setlocale(LC_NUMERIC, "C");
#endif

	mpv = mpv_create();

	if(!mpv)
	{
		log_cb(RETRO_LOG_ERROR, "failed creating context\n");
		exit(EXIT_FAILURE);
	}

	if((ret = mpv_initialize(mpv)) < 0)
	{
		log_cb(RETRO_LOG_ERROR, "mpv init failed: %s\n", mpv_error_string(ret));
		exit(EXIT_FAILURE);
	}

	if((ret = mpv_request_log_messages(mpv, "v")) < 0)
	{
		log_cb(RETRO_LOG_ERROR, "mpv logging failed: %s\n",
				mpv_error_string(ret));
	}

	mpv_render_param params[] = {
		{MPV_RENDER_PARAM_API_TYPE, MPV_RENDER_API_TYPE_OPENGL},
		{MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &(mpv_opengl_init_params){
			.get_proc_address = get_proc_address_mpv,
		}},
		{0}
	};

	if((ret = mpv_render_context_create(&mpv_gl, mpv, params)) < 0)
	{
		log_cb(RETRO_LOG_ERROR, "failed to initialize mpv GL context: %s\n",
				mpv_error_string(ret));
		goto err;
	}

	mpv_render_context_set_update_callback(mpv_gl, on_mpv_redraw, NULL);

	mpv_set_option_string(mpv, "ao", "libmpv");

	/* Attempt to enable hardware acceleration. MPV will fallback to software
	 * decoding on failure.
	 */
	if((ret = mpv_set_option_string(mpv, "hwdec", "auto")) < 0)
	{
		log_cb(RETRO_LOG_ERROR, "failed to set hwdec option: %s\n",
				mpv_error_string(ret));
	}

	if((ret = mpv_command(mpv, cmd)) != 0)
	{
		log_cb(RETRO_LOG_ERROR, "mpv_command failed to load input file: %s\n",
				mpv_error_string(ret));
		goto err;
	}

	/* TODO #2: Check for the highest samplerate in audio stream, and use that.
	 * Fall back to 48kHz otherwise.
	 * We currently force the audio to be sampled at 48KHz.
	 */
	mpv_set_option_string(mpv, "af", "format=s16:48000:stereo");
	//mpv_set_option_string(mpv, "opengl-swapinterval", "0");

	/* Process any events whilst we wait for playback to begin. */
	process_mpv_events(MPV_EVENT_NONE);

	/* Keep trying until mpv accepts the property. This is done to seek to the
	 * point in the file after the previous context was destroyed. If no
	 * context was destroyed previously, the file seeks to 0.
	 *
	 * This also seems to fix some black screen issues.
	 */
	if(playback_time != 0)
	{
		process_mpv_events(MPV_EVENT_PLAYBACK_RESTART);
		while(mpv_set_property(mpv,
					"playback-time", MPV_FORMAT_INT64, &playback_time) < 0)
		{}
	}

	/* The following works best when vsync is switched off in Retroarch. */
	//mpv_set_option_string(mpv, "video-sync", "display-resample");
	//mpv_set_option_string(mpv, "display-fps", "60");

	log_cb(RETRO_LOG_INFO, "Context reset.\n");

	return;

err:
	/* Print mpv logs to see why mpv failed. */
	process_mpv_events(MPV_EVENT_NONE);
	exit(EXIT_FAILURE);
}

static void context_destroy(void)
{
	mpv_get_property(mpv, "playback-time", MPV_FORMAT_INT64, &playback_time);
	mpv_render_context_free(mpv_gl);
	mpv_terminate_destroy(mpv);
	log_cb(RETRO_LOG_INFO, "Context destroyed.\n");
}

static bool retro_init_hw_context(void)
{
#if defined(HAVE_OPENGLES)
	hw_render.context_type = RETRO_HW_CONTEXT_OPENGLES2;
#else
	hw_render.context_type = RETRO_HW_CONTEXT_OPENGL;
#endif
	hw_render.context_reset = context_reset;
	hw_render.context_destroy = context_destroy;

	if (!CORE_PREFIX(environ_cb)(RETRO_ENVIRONMENT_SET_HW_RENDER, &hw_render))
		return false;

	return true;
}

void CORE_PREFIX(retro_set_audio_sample)(retro_audio_sample_t cb)
{
	CORE_PREFIX(audio_cb) = cb;
}

void CORE_PREFIX(retro_set_audio_sample_batch)(retro_audio_sample_batch_t cb)
{
	CORE_PREFIX(audio_batch_cb) = cb;
}

void CORE_PREFIX(retro_set_input_poll)(retro_input_poll_t cb)
{
	CORE_PREFIX(input_poll_cb) = cb;
}

void CORE_PREFIX(retro_set_input_state)(retro_input_state_t cb)
{
	CORE_PREFIX(input_state_cb) = cb;
}

void CORE_PREFIX(retro_set_video_refresh)(retro_video_refresh_t cb)
{
	CORE_PREFIX(video_cb) = cb;
}

void CORE_PREFIX(retro_reset)(void)
{
}

static void audio_callback(double fps)
{
	/* Obtain len samples to reduce lag. */
	int len = 48000 / (int)floor(fps);
    const unsigned int buff_len = 1024;
    uint8_t buff[buff_len];

    if(len < 4)
        len = 4;

    do
	{
        len = len - (len % 4);

        int ret = mpv_audio_callback(mpv, &buff,
                len > buff_len ? buff_len : len);

        if(ret < 0)
        {
#if 0
            log_cb(RETRO_LOG_ERROR, "mpv encountered an error in audio "
                    "callback: %d.\n", ret);
#endif
            return;
        }

        /* mpv may refuse to buffer audio if the first video frame has not
         * displayed yet. */
        if(ret == 0)
            return;

		len -= ret;

		CORE_PREFIX(audio_batch_cb)((const int16_t*)buff, ret);
	}
	while(len > 4);
}

static void retropad_update_input(void)
{
	struct Input
	{
		unsigned int l : 1;
		unsigned int r : 1;
		unsigned int a : 1;
	};
	struct Input current;
	static struct Input last;

	CORE_PREFIX(input_poll_cb)();

	/* Commands that are called on rising edge only */

	/* A ternary operator is used since input_state_cb returns an int16_t, but
	 * we only care about whether the button is on or off which is why we store
	 * the value in a single bit for each button.
	 *
	 * Unsure if saving the memory is worth the extra checks, costing CPU time,
	 * but both are incredibly miniscule anyway.
	 */
	current.l = CORE_PREFIX(input_state_cb)(0, RETRO_DEVICE_JOYPAD,
			0, RETRO_DEVICE_ID_JOYPAD_L) != 0 ? 1 : 0;
	current.r = CORE_PREFIX(input_state_cb)(0, RETRO_DEVICE_JOYPAD,
			0, RETRO_DEVICE_ID_JOYPAD_R) != 0 ? 1 : 0;
	current.a = CORE_PREFIX(input_state_cb)(0, RETRO_DEVICE_JOYPAD,
			0, RETRO_DEVICE_ID_JOYPAD_A) != 0 ? 1 : 0;

	if(current.l == 1 && last.l == 0)
		mpv_command_string(mpv, "cycle audio");

	if(current.r == 1 && last.r == 0)
		mpv_command_string(mpv, "cycle sub");

	if(current.a == 1 && last.a == 0)
		mpv_command_string(mpv, "cycle pause");

	/* TODO #3: Press and hold commands with small delay */
	if (CORE_PREFIX(input_state_cb)(0, RETRO_DEVICE_JOYPAD, 0,
			RETRO_DEVICE_ID_JOYPAD_LEFT))
		mpv_command_string(mpv, "seek -5");

	if (CORE_PREFIX(input_state_cb)(0, RETRO_DEVICE_JOYPAD, 0,
			RETRO_DEVICE_ID_JOYPAD_RIGHT))
		mpv_command_string(mpv, "seek 5");

	if (CORE_PREFIX(input_state_cb)(0, RETRO_DEVICE_JOYPAD, 0,
			RETRO_DEVICE_ID_JOYPAD_UP))
		mpv_command_string(mpv, "seek 60");

	if (CORE_PREFIX(input_state_cb)(0, RETRO_DEVICE_JOYPAD, 0,
			RETRO_DEVICE_ID_JOYPAD_DOWN))
		mpv_command_string(mpv, "seek -60");

	/* Press and hold commands */
	if (CORE_PREFIX(input_state_cb)(0, RETRO_DEVICE_JOYPAD, 0,
			RETRO_DEVICE_ID_JOYPAD_X))
		mpv_command_string(mpv, "show-progress");

	/* Instead of copying the structs as though they were a union, we assign
	 * each variable one-by-one to avoid endian issues.
	 */
	last.l = current.l;
	last.r = current.r;
	last.a = current.a;
}

void CORE_PREFIX(retro_run)(void)
{
	/* We only need to update the base video size once, and we do it here since
	 * the input file is not processed during the first
	 * retro_get_system_av_info() call.
	 */
	static bool updated_video_dimensions = false;
	static int64_t width = 0, height = 0;
	static double container_fps = 30.0f;

	if(updated_video_dimensions == false)
	{
		mpv_get_property(mpv, "dwidth", MPV_FORMAT_INT64, &width);
		mpv_get_property(mpv, "dheight", MPV_FORMAT_INT64, &height);
		mpv_get_property(mpv, "container-fps", MPV_FORMAT_DOUBLE, &container_fps);

		/* We don't know the dimensions of the video when
		 * retro_get_system_av_info is called, so we have to set it here for
		 * the correct aspect ratio.
		 * This is not a temporary change
		 */
		struct retro_game_geometry geometry = {
			.base_width   = width,
			.base_height  = height,
			/* max_width and max_height are ignored */
			.max_width    = width,
			.max_height   = height,
			/* Aspect ratio calculated automatically from base dimensions */
			.aspect_ratio = -1,
		};

		log_cb(RETRO_LOG_INFO, "Setting fps to %f\n", container_fps);

		struct retro_system_timing timing = {
			.fps = container_fps,
			.sample_rate = 48000.0f,
		};

		struct retro_system_av_info av_info = {
			.geometry = geometry,
			.timing = timing,
		};

		if(width > 0 && height > 0)
			CORE_PREFIX(environ_cb)(RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO, &av_info);

		updated_video_dimensions = true;
	}

	retropad_update_input();

	/* TODO #2: Implement an audio callback feature in to libmpv */
	audio_callback(container_fps);

	process_mpv_events(MPV_EVENT_NONE);

#if 1
	if(frame_queue > 0)
	{
		mpv_render_param params[] = {
			{MPV_RENDER_PARAM_OPENGL_FBO, &(mpv_opengl_fbo){
				.fbo = hw_render.get_current_framebuffer(),
				.w = width,
				.h = height,
			}},
			{0}
		};
		mpv_render_context_render(mpv_gl, params);
		CORE_PREFIX(video_cb)(RETRO_HW_FRAME_BUFFER_VALID, width, height, 0);
		frame_queue--;
	}
	else
		CORE_PREFIX(video_cb)(NULL, width, height, 0);
#else
	mpv_opengl_cb_draw(mpv_gl, hw_render.get_current_framebuffer(), width, height);
	CORE_PREFIX(video_cb)(RETRO_HW_FRAME_BUFFER_VALID, width, height, 0);
#endif

	return;
}

/* No save-state support */
size_t CORE_PREFIX(retro_serialize_size)(void)
{
	return 0;
}

bool CORE_PREFIX(retro_serialize)(void *data_, size_t size)
{
	return true;
}

bool CORE_PREFIX(retro_unserialize)(const void *data_, size_t size)
{
	return true;
}

bool CORE_PREFIX(retro_load_game)(const struct retro_game_info *info)
{
	/* Supported on most systems. */
	enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_XRGB8888;
	struct retro_input_descriptor desc[] = {
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A,  "Pause/Play" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X,  "Show Progress" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,  "Seek -5 seconds" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP,    "Seek +60 seconds" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,  "Seek -60 seconds" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "Seek +5 seconds" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L,     "Cycle Audio Track" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R, "Cycle Subtitle Track" },

		{ 0 },
	};

	if(info->path == NULL)
		return false;

	/* Copy the file path to a global variable as we need it in context_reset()
	 * where mpv is initialised.
	 */
	if((filepath = malloc(strlen(info->path)+1)) == NULL)
	{
		log_cb(RETRO_LOG_ERROR, "Unable to allocate memory for filepath\n");
		return false;
	}

	strcpy(filepath,info->path);

	CORE_PREFIX(environ_cb)(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, desc);

	/* Not bothered if this fails. Assuming the default is selected anyway. */
	if(CORE_PREFIX(environ_cb)(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt) == false)
	{
		log_cb(RETRO_LOG_ERROR, "XRGB8888 is not supported.\n");

		/* Try RGB565 */
		fmt = RETRO_PIXEL_FORMAT_RGB565;
		CORE_PREFIX(environ_cb)(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt);
	}

	if(retro_init_hw_context() == false)
	{
		log_cb(RETRO_LOG_ERROR, "HW Context could not be initialized\n");
		return false;
	}

	return true;
}

bool CORE_PREFIX(retro_load_game_special)(unsigned type, const struct retro_game_info *info,
		size_t num)
{
	return false;
}

void CORE_PREFIX(retro_unload_game)(void)
{
	free(filepath);
	filepath = NULL;
}

unsigned CORE_PREFIX(retro_get_region)(void)
{
	return RETRO_REGION_NTSC;
}

void *CORE_PREFIX(retro_get_memory_data)(unsigned id)
{
	return NULL;
}

size_t CORE_PREFIX(retro_get_memory_size)(unsigned id)
{
	return 0;
}

void CORE_PREFIX(retro_cheat_reset)(void)
{}

void CORE_PREFIX(retro_cheat_set)(unsigned index, bool enabled, const char *code)
{
   (void)index;
   (void)enabled;
   (void)code;
}
