image_core.so: image_core.c
	gcc \
		-g \
		image_core.c \
		../../libretro-common/file/file_path.c \
		../../libretro-common/lists/dir_list.c \
		../../libretro-common/compat/compat_strl.c \
		../../libretro-common/compat/compat_strcasestr.c \
		../../libretro-common/lists/string_list.c \
		../../libretro-common/file/retro_dirent.c \
		../../libretro-common/streams/file_stream.c \
		../../libretro-common/vfs/vfs_implementation.c \
		-o image_core.so \
		-DHAVE_STB_IMAGE \
		-I ../../libretro-common/include/ \
		-I../../deps/stb \
		-Wl,--no-undefined \
		-shared \
		-fPIC \
		-lm
