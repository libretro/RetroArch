#include <stdio.h>
#include <stdlib.h>
#include <switchres/switchres_wrapper.h>

int main(int argc, char** argv)
{
	sr_mode srm;
	int ret;

	sr_init();
	sr_set_log_level(3);

	sr_set_disp(-1);
	sr_set_monitor("arcade_31");
	sr_init_disp("0", NULL);

	sr_set_disp(-1);
	sr_set_monitor("pc_31_120");
	sr_init_disp("1", NULL);

	sr_set_disp(0);
	ret = sr_switch_to_mode(640, 480, 57, 0, &srm);

	sr_set_disp(1);
	ret = sr_switch_to_mode(320, 240, 58, 0, &srm);

	printf("Press any key to quit.\n");
	getchar();

	sr_deinit();
}
