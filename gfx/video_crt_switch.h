static int ben_core_width;
static int ben_core_height;
static int ben_tmp_width;
static int ben_tmp_height;
static int orig_width;			
static int orig_height;
static float ben_core_hz;
static int set_ben_core_hz;
static float ben_tmp_core_hz;


void switch_res_core(int width, int height, float hz);
void check_first_run();
void screen_setup_aspect(int width, int height);
void switch_res_crt(int width, int height);
void aspect_ratio_switch(int width,int height);
void switch_crt_hz();
void switch_res(int width, int height, int f_restore);
void video_restore();