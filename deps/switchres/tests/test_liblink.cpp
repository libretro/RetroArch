#include <stdio.h>
#include <stdlib.h>
#include <switchres/switchres_wrapper.h>

int main(int argc, char** argv) {
	printf("Switchres version: %s\n" , sr_get_version());
}
