OBJS=mmal_encode.o
BIN=hello_mmal_encode.bin
LDFLAGS+=-lmmal -lmmal_core -lmmal_components -lmmal_util -lmmal_vc_client
# Set --no-as-needed to stop the linker discarding mmal_vc_client
# as it can't see that the constructor registers a load of functionality
# with the MMAL core.
LDFLAGS+=-Wl,--no-as-needed

include ../Makefile.include
