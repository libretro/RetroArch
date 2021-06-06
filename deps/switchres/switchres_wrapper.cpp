/**************************************************************

   switchres_wrapper.cpp - Switchres C wrapper API

   ---------------------------------------------------------

   Switchres   Modeline generation engine for emulation

   License     GPL-2.0+
   Copyright   2010-2021 Chris Kennedy, Antonio Giner,
                         Alexandre Wodarczyk, Gil Delescluse

 **************************************************************/

#define MODULE_API_EXPORTS
#include "switchres.h"
#include "switchres_wrapper.h"
#include "log.h"
#include <stdio.h>
#include <locale>
#ifdef __cplusplus
extern "C" {
#endif

switchres_manager* swr;


MODULE_API void sr_init() {
	setlocale(LC_NUMERIC, "C");
	swr = new switchres_manager;
	swr->parse_config("switchres.ini");
}


MODULE_API void sr_load_ini(char* config) {
	swr->parse_config(config);
	swr->display()->m_ds = swr->ds;
	swr->display()->parse_options();
}


MODULE_API unsigned char sr_init_disp(const char* scr) {
	if (scr)
		swr->set_screen(scr);
	swr->add_display();
	if (!swr->display()->init())
		return 0;
	return 1;
}


MODULE_API void sr_deinit() {
	delete swr;
}


MODULE_API void sr_set_monitor(const char *preset) {
	swr->set_monitor(preset);
}


MODULE_API void sr_set_user_mode(int width, int height, int refresh) {
	modeline user_mode = {};
	user_mode.width = width;
	user_mode.height = height;
	user_mode.refresh = refresh;
	swr->set_user_mode(&user_mode);
}


void disp_best_mode_to_sr_mode(display_manager* disp, sr_mode* srm)
{
	srm->width = disp->width();
	srm->height = disp->height();
	srm->refresh = disp->v_freq();
	srm->is_refresh_off = (disp->is_refresh_off() ? 1 : 0);
	srm->is_stretched = (disp->is_stretched() ? 1 : 0);
	srm->x_scale = disp->x_scale();
	srm->y_scale = disp->y_scale();
	srm->interlace = (disp->is_interlaced() ? 105 : 0);
}


bool sr_refresh_display(display_manager *disp)
{
	if (disp->is_mode_updated())
	{
		if (disp->update_mode(disp->best_mode()))
		{
			log_info("sr_refresh_display: mode was updated\n");
			return true;
		}
	}
	else if (disp->is_mode_new())
	{
		if (disp->add_mode(disp->best_mode()))
		{
			log_info("sr_refresh_display: mode was added\n");
			return true;
		}
	}
	else
	{
		log_info("sr_refresh_display: no refresh required\n");
		return true;
	}

	log_error("sr_refresh_display: error refreshing display\n");
	return false;
}


MODULE_API unsigned char sr_add_mode(int width, int height, double refresh, unsigned char interlace, sr_mode *return_mode) {

	log_verbose("Inside sr_add_mode(%dx%d@%f%s)\n", width, height, refresh, interlace > 0? "i":"");
	display_manager *disp = swr->display();
	if (disp == nullptr)
	{
		log_error("sr_add_mode: error, didn't get a display\n");
		return 0;
	}

	disp->get_mode(width, height, refresh, (interlace > 0? true : false));
	if (disp->got_mode())
	{
		log_verbose("sr_add_mode: got mode %dx%d@%f type(%x)\n", disp->width(), disp->height(), disp->v_freq(), disp->best_mode()->type);
		if (return_mode != nullptr) disp_best_mode_to_sr_mode(disp, return_mode);
		if (sr_refresh_display(disp))
			return 1;
	}

	printf("sr_add_mode: error adding mode\n");
	return 0;
}


MODULE_API unsigned char sr_switch_to_mode(int width, int height, double refresh, unsigned char interlace, sr_mode *return_mode) {

	log_verbose("Inside sr_switch_to_mode(%dx%d@%f%s)\n", width, height, refresh, interlace > 0? "i":"");
	display_manager *disp = swr->display();
	if (disp == nullptr)
	{
		log_error("sr_switch_to_mode: error, didn't get a display\n");
		return 0;
	}

	disp->get_mode(width, height, refresh, (interlace > 0? true : false));
	if (disp->got_mode())
	{
		log_verbose("sr_switch_to_mode: got mode %dx%d@%f type(%x)\n", disp->width(), disp->height(), disp->v_freq(), disp->best_mode()->type);
		if (return_mode != nullptr) disp_best_mode_to_sr_mode(disp, return_mode);
		if (!sr_refresh_display(disp))
			return 0;
	}

	if (disp->is_switching_required())
	{
		if (disp->set_mode(disp->best_mode()))
		{
			log_info("sr_switch_to_mode: successfully switched to %dx%d@%f\n", disp->width(), disp->height(), disp->v_freq());
			return 1;
		}
	}
	else
	{
		log_info("sr_switch_to_mode: switching not required\n");
		return 1;
	}

	log_error("sr_switch_to_mode: error switching to mode\n");
	return 0;
}


MODULE_API void sr_set_rotation (unsigned char r) {
	if (r > 0)
	{
		swr->set_rotation(true);
	}
	else
	{
		swr->set_rotation(false);
	}
}


MODULE_API void sr_set_log_level (int l) {
	swr->set_log_level(l);
}


MODULE_API void sr_set_log_callback_info (void * f) {
	swr->set_log_info_fn((void *)f);
}


MODULE_API void sr_set_log_callback_debug (void * f) {
	swr->set_log_verbose_fn((void *)f);
}


MODULE_API void sr_set_log_callback_error (void * f) {
	swr->set_log_error_fn((void *)f);
}


MODULE_API srAPI srlib = {
	sr_init,
	sr_load_ini,
	sr_deinit,
	sr_init_disp,
	sr_add_mode,
	sr_switch_to_mode,
	sr_set_monitor,
	sr_set_rotation,
	sr_set_user_mode,
	sr_set_log_level,
	sr_set_log_callback_error,
	sr_set_log_callback_info,
	sr_set_log_callback_debug,
};

#ifdef __cplusplus
}
#endif
