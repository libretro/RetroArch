image_core.so:
	gcc \
		image_core.c \
		../../libretro-common/file/file_path.c \
		../../libretro-common/lists/dir_list.c \
		../../libretro-common/compat/compat_strl.c \
		../../libretro-common/compat/compat_strcasestr.c \
		../../libretro-common/lists/string_list.c \
		../../libretro-common/file/retro_dirent.c \
		-o image_core.so \
		-DHAVE_STB_IMAGE \
		-I ../../libretro-common/include/ \
		-I../../deps/stb \
		-shared \
		-fPIC \
		-lm
