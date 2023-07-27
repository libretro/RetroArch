#include <stdio.h>
#include <stdlib.h>
#ifdef __cplusplus
#include <cstring> // required for strcpy
#endif

#ifdef __linux__
#define LIBSWR "libswitchres.so"
#elif _WIN32
#define LIBSWR "libswitchres.dll"
#endif

#include <switchres/switchres_wrapper.h>

int main(int argc, char** argv)
{
	const char* err_msg;

	printf("About to open %s.\n", LIBSWR);

	// Load the lib
	LIBTYPE dlp = OPENLIB(LIBSWR);

	// Loading failed, inform and exit
	if (!dlp)
	{
		printf("Loading %s failed.\n", LIBSWR);
		printf("Error: %s\n", LIBERROR());
		exit(EXIT_FAILURE);
	}
	
	printf("Loading %s succeded.\n", LIBSWR);


	// Load the init()
	LIBERROR();
	srAPI* SRobj = (srAPI*) LIBFUNC(dlp, "srlib");
	if ((err_msg = LIBERROR()) != NULL)
	{
		printf("Failed to load srAPI: %s\n", err_msg);
		CLOSELIB(dlp);
		exit(EXIT_FAILURE);
	}

	// Testing the function
	printf("Init a new switchres_manager object:\n");
	SRobj->init();
	SRobj->init_disp(NULL, NULL);

	// Call mode + get result values
	int w = 384, h = 224;
	double rr = 59.583393;
	int interlace = 0, ret;
	sr_mode srm;

	printf("Original requested mode: %dx%d@%f%s\n", w, h, rr, interlace? "i":"");

	ret = SRobj->add_mode(w, h, rr, interlace, &srm);
	if (!ret)
	{
		printf("ERROR: Couldn't add the required mode. Exiting!\n");
		SRobj->deinit();
		exit(1);
	}
	printf("Got mode: %dx%d%c@%f\n", srm.width, srm.height, srm.interlace, srm.refresh);
	printf("Press any key to switch to new mode\n");
	getchar();
	
	ret = SRobj->switch_to_mode(srm.width, srm.height, rr, srm.interlace, &srm);
	if (!ret)
	{
		printf("ERROR: Couldn't switch to the required mode. Exiting!\n");
		SRobj->deinit();
		exit(1);
	}
	printf("Press any key to quit.\n");
	getchar();

	// Clean the mess, kiss goodnight SR
	SRobj->deinit();

	// We're done, let's close
	CLOSELIB(dlp);
}
