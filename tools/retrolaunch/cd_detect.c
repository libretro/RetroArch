#include "parser.h"

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <libgen.h>
#include <stdlib.h>

#include "log.h"

#define MAGIC_LEN 16

struct MagicEntry {
    char* system_name;
    char* magic;
};

static struct MagicEntry MAGIC_NUMBERS[] = {
    {"ps1", "\x00\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\x00\x00\x02\x00\x02\x00"},
    {"pcecd", "\x82\xb1\x82\xcc\x83\x76\x83\x8d\x83\x4f\x83\x89\x83\x80\x82\xcc\x92"},
    {"scd", "\x00\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\x00\x00\x02\x00\x01\x53"},
    {NULL, NULL}
};

static int find_first_data_track(const char* cue_path, int32_t* offset,
                                 char* track_path, size_t max_len) {
    int rv;
    int fd = -1;
    char tmp_token[MAX_TOKEN_LEN];
    int m, s, f;
    char* cue_path_copy;
    char* cue_dir;
    cue_path_copy = strdup(cue_path);
    cue_dir = dirname(cue_path_copy);

    fd = open(cue_path, O_RDONLY);
    if (fd < 0) {
        LOG_WARN("Could not open CUE file '%s': %s", cue_path,
                 strerror(errno));
        rv = -errno;
        goto free_path_copy;
    }

    LOG_DEBUG("Parsing CUE file '%s'...", cue_path);

    while (get_token(fd, tmp_token, MAX_TOKEN_LEN) > 0) {
        if (strcmp(tmp_token, "FILE") == 0) {
            get_token(fd, tmp_token, MAX_TOKEN_LEN);
            snprintf(track_path, max_len, "%s/%s",
                    cue_dir, tmp_token);

        } else if (strcasecmp(tmp_token, "TRACK") == 0) {
            get_token(fd, tmp_token, MAX_TOKEN_LEN);
            get_token(fd, tmp_token, MAX_TOKEN_LEN);
            if (strcasecmp(tmp_token, "AUDIO") == 0) {
                continue;
            }

            find_token(fd, "INDEX");
            get_token(fd, tmp_token, MAX_TOKEN_LEN);
            get_token(fd, tmp_token, MAX_TOKEN_LEN);
            if (sscanf(tmp_token, "%02d:%02d:%02d", &m, &s, &f) < 3) {
                LOG_WARN("Error parsing time stamp '%s'", tmp_token);
                return -errno;
            }
            *offset = ((m * 60) * (s * 75) * f) * 25;

            LOG_DEBUG("Found 1st data track on file '%s+%d'",
                    track_path, *offset);

            rv = 0;
            goto clean;
        }
    }

    rv = -EINVAL;

clean:
    close(fd);
free_path_copy:
    free(cue_path_copy);
    return rv;
}

static int find_ps1_canonical_name(const char* game_id, char* game_name,
                                   size_t max_len) {
    int fd;
    char tmp_token[MAX_TOKEN_LEN];
    int rv = 0;
    fd = open("cddb/ps1.idlst", O_RDONLY);
    if (fd < 0) {
        LOG_WARN("Could not open id list: %s", strerror(errno));
        return -errno;
    }

    while (get_token(fd, tmp_token, MAX_TOKEN_LEN) > 0) {
        if(strcasecmp(tmp_token, game_id) != 0) {
            get_token(fd, tmp_token, max_len);
            continue;
        }

        if ((rv = get_token(fd, game_name, max_len)) < 0) {
            goto clean;
        }

        rv = 0;
        goto clean;
    }

    rv = -ENOENT;
clean:
    close(fd);
    return rv;
}

static int detect_ps1_game(const char* track_path, int32_t offset,
                          char* game_name, size_t max_len) {
    int rv;
    char buff[4096];
    char* pattern = "cdrom:";
    char* c;
    char* id_start;
    int i;

    int fd = open(track_path, O_RDONLY);
    if (fd < 0) {
        LOG_DEBUG("Could not open data track: %s", strerror(errno));
        return -errno;
    }

    if (pread(fd, buff, 10, 0x9340) > 0) {
        buff[10] = '\0';
        buff[4] = '-';
        LOG_DEBUG("Found disk label '%s'", buff);
        rv = find_ps1_canonical_name(buff, game_name, max_len);
        if (rv == 0) {
            goto clean;
        }
    }
    c = pattern;
    while (1) {
        rv = read(fd, buff, 4096);
        if (rv < 0) {
            rv = -errno;
            goto clean;
        }

        for(i = 0; i < 4096; i++) {
            if (*c == buff[i]) {
                c++;
            } else {
                c = pattern;
                continue;
            }

            if (*c == '\0') {
                id_start = &buff[i] + 1;
                *strchr(id_start, ';') = '\0';
                c = strrchr(id_start, '\\') + 1;
                if (c != NULL) {
                    id_start = c;
                }
                id_start[4] = '-';
                id_start[8] = id_start[9];
                id_start[9] = id_start[10];
                id_start[10] = '\0';
                LOG_DEBUG("Found ps1 id %s", id_start);
                rv = find_ps1_canonical_name(id_start, game_name, max_len);
                goto clean;
            }
        }
    }
    rv = -EINVAL;
clean:
    close(fd);
    return rv;
}

static int detect_system(const char* track_path, int32_t offset,
        char** system_name) {
    int rv;
    char magic[MAGIC_LEN];
    int fd;
    //struct MagicEntry entry;
    int i;

    fd = open(track_path, O_RDONLY);
    if (fd < 0) {
        LOG_WARN("Could not open data track of file '%s': %s",
                 track_path, strerror(errno));
        rv = -errno;
        goto clean;
    }

    if (pread(fd, magic, MAGIC_LEN, offset) < MAGIC_LEN) {
        LOG_WARN("Could not read data from file '%s' at offset %d: %s",
                track_path, offset, strerror(errno));
        rv = -errno;
        goto clean;
    }

    LOG_DEBUG("Comparing with known magic numbers...");
    for (i = 0; MAGIC_NUMBERS[i].system_name != NULL; i++) {
        if (memcmp(MAGIC_NUMBERS[i].magic, magic, MAGIC_LEN) == 0) {
            *system_name = MAGIC_NUMBERS[i].system_name;
            rv = 0;
            goto clean;
        }
    }

    LOG_WARN("Could not find compatible system");
    rv = -EINVAL;
clean:
    close(fd);
    return rv;
}

int find_fist_cue(const char* m3u_path, char* cue_path, size_t max_len) {
    char c;
    int skip = 0;
    int midstream = 0;
    char tmp_path[PATH_MAX];

    int fd = open(m3u_path, O_RDONLY);
    if (fd < 0) {
        LOG_WARN("Could not open m3u '%s': %s", m3u_path, strerror(errno));
        return -errno;
    }

    strncpy(tmp_path, m3u_path, PATH_MAX);
    strcpy(cue_path, dirname(tmp_path));
    cue_path += strlen(cue_path);
    cue_path[0] = '/';
    cue_path++;

    while ((read(fd, &c, 1) > 0)) {
        switch (c) {
            case '#':
                if (!midstream) {
                    skip = 1;
                }
                break;
            case '\n':
                if (skip) {
                    skip = 0;
                } else if(midstream) {
                    cue_path[0] = '\0';
                    return 0;
                }
                break;
            case ' ':
                if (!midstream) {
                    break;
                }
            default:
                if (!skip) {
                    midstream = 1;
                    cue_path[0] = c;
                    cue_path++;
                }
        }
    }

    return -EINVAL;
}

int detect_cd_game(const char* target_path, char* game_name, size_t max_len) {
    char cue_path[PATH_MAX];
    char track_path[PATH_MAX];
    int32_t offset = 0;
    char* system_name = NULL;
    int rv;
    if (strcasecmp(target_path + strlen(target_path) - 4, ".m3u") == 0) {
        rv = find_fist_cue(target_path, cue_path, PATH_MAX);
        if (rv < 0) {
            LOG_WARN("Could not parse m3u: %s", strerror(-rv));
            return rv;
        }

    } else {
        strncpy(cue_path, target_path, max_len);
    }

    rv = find_first_data_track(cue_path, &offset, track_path, PATH_MAX);
    if (rv < 0) {
        LOG_WARN("Could not find valid data track: %s", strerror(-rv));
        return rv;
    }

    LOG_DEBUG("Reading 1st data track...");

    if ((rv = detect_system(track_path, offset, &system_name)) < 0) {
        return rv;
    }


    LOG_DEBUG("Detected %s media", system_name);

    snprintf(game_name, max_len, "%s.", system_name);
    game_name += strlen(system_name) + 1;
    max_len -= strlen(system_name) + 1;
    if (strcmp(system_name, "ps1") == 0) {
        if (detect_ps1_game(track_path, offset, game_name, max_len) == 0) {
            return 0;
        }
    }

    snprintf(game_name, max_len, "<unknown>");
    return 0;
}
