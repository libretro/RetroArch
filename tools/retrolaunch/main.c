#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <limits.h>

#include "sha1.h"
#include "parser.h"
#include "cd_detect.h"
#include "rl_fnmatch.h"
#include "../../file.h"

#include "log.h"

#define SHA1_LEN 40
#define HASH_LEN SHA1_LEN

static int find_hash(int fd, const char *hash, char *game_name, size_t max_len)
{
	char token[MAX_TOKEN_LEN] = {0};
	while (1) {
		if (find_token(fd, "game") < 0) {
			return -1;
		}

		if (find_token(fd, "name") < 0) {
			return -1;
		}

		if (get_token(fd, game_name, max_len) < 0) {
			return -1;
		}

		if (find_token(fd, "sha1") < 0) {
			return -1;
		}

		if (get_token(fd, token, MAX_TOKEN_LEN) < 0) {
			return -1;
		}

		if (strcasecmp(hash, token) == 0) {
			return 0;
		}
	}
}

static int
find_rom_canonical_name(const char *hash, char *game_name, size_t max_len)
{
	// TODO: Error handling
	size_t i;
	int rv;
	int fd;
	int offs;
	char *dat_path;
	const char *dat_name;
	const char *dat_name_dot;
	struct string_list *files;

	files = dir_list_new("db", "dat", false);
	if (!files) {
		return -1;
	}

	for (i = 0; i < files->size; i++) {
		dat_path = files->elems[i].data;
		dat_name = path_basename(dat_path);

		dat_name_dot = strchr(dat_name, '.');
		if (!dat_name_dot) {
			continue;
		}

		offs = dat_name_dot - dat_name + 1;
		memcpy(game_name, dat_name, offs);

		fd = open(dat_path, O_RDONLY);
		if (fd < 0) {
			continue;
		}

		if (find_hash(fd, hash, game_name + offs, max_len - offs) == 0) {
			rv = 0;
			close(fd);
			goto clean;
		}
		close(fd);
	}
	rv = -1;
clean:
	dir_list_free(files);
	return rv;
}

static int get_sha1(const char *path, char *result)
{
	int fd;
	int rv;
	unsigned char buff[4096];
	SHA1Context sha;

	fd = open(path, O_RDONLY);
	if (fd < 0) {
		return -errno;
	}

	SHA1Reset(&sha);
	rv = 1;
	while (rv > 0) {
		rv = read(fd, buff, 4096);
		if (rv < 0) {
			close(fd);
			return -errno;
		}

		SHA1Input(&sha, buff, rv);
	}

	if (!SHA1Result(&sha)) {
		return -1;
	}

	sprintf(result, "%08X%08X%08X%08X%08X",
		sha.Message_Digest[0],
		sha.Message_Digest[1],
		sha.Message_Digest[2],
		sha.Message_Digest[3], sha.Message_Digest[4]);
	return 0;
}

struct RunInfo {
	char core[50];
	int multitap;
	int dualanalog;
};

static int get_run_info(struct RunInfo *info, char *game_name)
{
	int fd = open("./launch.conf", O_RDONLY);
	int rv;
	char token[MAX_TOKEN_LEN];
	if (fd < 0) {
		return -errno;
	}

	memset(info, 0, sizeof(struct RunInfo));

	while (1) {
		if ((rv = get_token(fd, token, MAX_TOKEN_LEN)) < 0) {
			goto clean;
		}

		if (rl_fnmatch(token, game_name, 0) != 0) {
			if ((rv = find_token(fd, ";")) < 0) {
				goto clean;
			}
			continue;
		}

		LOG_DEBUG("Matched rule '%s'", token);

		if ((rv = get_token(fd, token, MAX_TOKEN_LEN)) < 0) {
			goto clean;
		}

		break;
	}

	strncpy(info->core, token, 50);
	info->multitap = 0;
	info->dualanalog = 0;

	if ((rv = get_token(fd, token, MAX_TOKEN_LEN)) < 0) {
		goto clean;
	}

	while (strcmp(token, ";") != 0) {
		if (strcmp(token, "multitap") == 0) {
			info->multitap = 1;
		} else if (strcmp(token, "dualanalog") == 0) {
			info->dualanalog = 1;
		}

		if ((rv = get_token(fd, token, MAX_TOKEN_LEN)) < 0) {
			goto clean;
		}

	}
	rv = 0;
 clean:
	close(fd);
	return rv;
}

const char *SUFFIX_MATCH[] = {
	".a26", "a26",
	".bin", "smd",
	".gba", "gba",
	".gbc", "gbc",
	".gb", "gb",
	".gen", "smd",
	".gg", "gg",
	".nds", "nds",
	".nes", "nes",
	".pce", "pce",
	".sfc", "snes",
	".smc", "snes",
	".smd", "smd",
	".sms", "sms",
	".wsc", "wswan",
	NULL
};

static int detect_rom_game(const char *path, char *game_name, size_t max_len)
{
	char hash[HASH_LEN + 1];
	int rv;
   const char *suffix;
	const char **tmp_suffix;

	suffix = strrchr(path, '.');
	if (!suffix) {
		LOG_WARN("Could not find extension for: %s", path);
		return -EINVAL;
	}

	memset(hash, 0, sizeof(hash));

	if ((rv = get_sha1(path, hash)) < 0) {
		LOG_WARN("Could not calculate hash: %s", strerror(-rv));
	}

	if (find_rom_canonical_name(hash, game_name, max_len) < 0) {
		LOG_DEBUG("Could not detect rom with hash `%s` guessing", hash);

		for (tmp_suffix = SUFFIX_MATCH; *tmp_suffix != NULL;
		     tmp_suffix += 2) {
			if (strcasecmp(suffix, *tmp_suffix) == 0) {
				snprintf(game_name, max_len, "%s.<unknown>",
					 *(tmp_suffix + 1));
				return 0;
			}
		}
		return -EINVAL;
	}

	return 0;
}

static int detect_game(const char *path, char *game_name, size_t max_len)
{
	if ((strcasecmp(path + strlen(path) - 4, ".cue") == 0) ||
	    (strcasecmp(path + strlen(path) - 4, ".m3u") == 0)) {
		LOG_INFO("Starting CD game detection...");
		return detect_cd_game(path, game_name, max_len);
	} else {
		LOG_INFO("Starting rom game detection...");
		return detect_rom_game(path, game_name, max_len);
	}
}

#ifndef RARCH_CONSOLE
static int run_retroarch(const char *path, const struct RunInfo *info)
{
	char core_path[PATH_MAX];
	sprintf(core_path, "./cores/libretro-%s.so", info->core);
	const char *retro_argv[30] = { "retroarch",
		"-L", core_path
	};
	int argi = 3;
	if (info->multitap) {
		retro_argv[argi] = "-4";
		argi++;
		LOG_INFO("Game supports multitap");
	}

	if (info->dualanalog) {
		retro_argv[argi] = "-A";
		argi++;
		retro_argv[argi] = "1";
		argi++;
		retro_argv[argi] = "-A";
		argi++;
		retro_argv[argi] = "2";
		argi++;
		LOG_INFO("Game supports the dualshock controller");
	}

	retro_argv[argi] = strdup(path);
	argi++;
	retro_argv[argi] = NULL;
	execvp(retro_argv[0], (char * const*)retro_argv);
	return -errno;
}

int main(int argc, char *argv[])
{
	if (argc < 2) {
		return -1;
	}

	char game_name[MAX_TOKEN_LEN];
	char *path = argv[1];
	struct RunInfo info;
	int rv;

	LOG_INFO("Analyzing '%s'", path);
	if ((rv = detect_game(path, game_name, MAX_TOKEN_LEN)) < 0) {
		LOG_WARN("Could not detect game: %s", strerror(-rv));
		return -rv;
	}

	LOG_INFO("Game is `%s`", game_name);
	if ((rv = get_run_info(&info, game_name)) < 0) {
		LOG_WARN("Could not find sutable core: %s", strerror(-rv));
		return -1;
	}

	LOG_DEBUG("Usinge libretro core '%s'", info.core);
	LOG_INFO("Launching '%s'", path);

	rv = run_retroarch(path, &info);
	LOG_WARN("Could not launch retroarch: %s", strerror(-rv));
	return -rv;
}

// Stub just so that it compiles
void rarch_init_msg_queue(void) {}
#endif
