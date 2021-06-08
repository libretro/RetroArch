#include <stdio.h>
#include <stdlib.h>
#include <switchres/switchres_wrapper.h>

int main(int argc, char** argv) {
	sr_mode srm;
	unsigned char ret;

	sr_init();
	sr_init_disp();

	ret = sr_add_mode(384, 224, 59.63, 0, &srm);
	if(!ret) 
	{
		printf("ERROR: couldn't add the required mode. Exiting!\n");
		sr_deinit();
		exit(1);
	}
	printf("SR returned resolution: %dx%d%c@%f\n", srm.width, srm.height, srm.interlace, srm.refresh);

	ret = sr_switch_to_mode(384, 224, 59.63, 0, &srm);
	if(!ret) 
	{
		printf("ERROR: couldn't switch to the required mode. Exiting!\n");
		sr_deinit();
		exit(1);
	}

	sr_deinit();
}
