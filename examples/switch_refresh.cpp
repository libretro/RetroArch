// Test switching of refresh rate only
// Requires working update method
//
// Build: g++ -o switch_refresh switch_refresh.cpp -I ../ -L ../ -ldl -lswitchres -lSDL2 -lSDL2_ttf -ldrm

#include <stdio.h>
#include <stdlib.h>
#include <switchres/switchres_wrapper.h>

int main(int argc, char** argv)
{
	sr_mode srm;

	sr_set_log_level(3);
	sr_init();
	sr_set_disp(-1);
	sr_set_monitor("arcade_31");
	sr_init_disp("1", NULL);

	printf("Testing first refresh (50Hz). Press any key...\n");
	getchar();

	if (!sr_add_mode(648, 480, 50, 0, &srm))
		goto error;

	if (!sr_set_mode(srm.id))
		goto error;

	printf("Testing second refresh (60Hz). Press any key...\n");
	getchar();

	if(!sr_add_mode(648, 480, 60, 0, &srm))
		goto error;

	if(!sr_set_mode(srm.id))
		goto error;

	printf("Success. Press any key to quit.\n");
	getchar();

	sr_deinit();
	exit(0);

error:
	printf("ERROR: Exiting!\n");
	sr_deinit();
	exit(1);
}
