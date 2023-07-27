#include <stdio.h>
#include <stdlib.h>
#include <switchres/switchres_wrapper.h>

int main(int argc, char** argv)
{
	sr_mode srm;
	int ret;

	sr_init();
	sr_init_disp(NULL, NULL);

	ret = sr_add_mode(384, 224, 59.63, 0, &srm);
	if (!ret)
	{
		printf("ERROR: Couldn't add the required mode. Exiting!\n");
		sr_deinit();
		exit(1);
	}
	printf("SR returned resolution: %dx%d@%f%s\n", srm.width, srm.height, srm.refresh, srm.interlace? "i" : "");
	printf("Press any key to switch to new mode\n");
	getchar();

	ret = sr_switch_to_mode(384, 224, 59.63, 0, &srm);
	if (!ret)
	{
		printf("ERROR: Couldn't switch to the required mode. Exiting!\n");
		sr_deinit();
		exit(1);
	}

	printf("Press any key to quit.\n");
	getchar();

	sr_deinit();
}
