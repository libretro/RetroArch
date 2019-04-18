gong.so: gong.c
	gcc \
		-std=c89 \
		-pedantic \
		-Wall \
		-Wno-unused-variable \
		-g \
		gong.c \
		-I../../libretro-common/include/ \
		-shared \
		-fPIC \
		-Wl,--no-undefined \
		-lm \
		-o gong.so

clean:
	rm -f gong.so
