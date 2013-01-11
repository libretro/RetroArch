#include "parser.h"

#include <errno.h>
#include <string.h>
#include <stdlib.h>

ssize_t get_token(int fd, char *token, size_t max_len)
{
	char *c = token;
	int rv;
	ssize_t len = 0;
	int in_string = 0;

	while (1) {
		rv = read(fd, c, 1);
		if (rv == 0) {
			return 0;
		} else if (rv < 1) {
			switch (errno) {
			case EINTR:
			case EAGAIN:
				continue;
			default:
				return -errno;
			}
		}

		switch (*c) {
		case ' ':
		case '\t':
		case '\r':
		case '\n':
			if (c == token) {
				continue;
			}

			if (!in_string) {
				*c = '\0';
				return len;
			}
			break;
		case '\"':
			if (c == token) {
				in_string = 1;
				continue;
			}

			*c = '\0';
			return len;
		}

		len++;
		c++;
		if (len == (ssize_t)max_len) {
			*c = '\0';
			return len;
		}
	}
}

int find_token(int fd, const char *token)
{
	int tmp_len = strlen(token);
	char *tmp_token = (char*)calloc(tmp_len, 1);
	if (!tmp_token) {
		return -1;
	}
	while (strncmp(tmp_token, token, tmp_len) != 0) {
		if (get_token(fd, tmp_token, tmp_len) <= 0) {
			return -1;
		}
	}

	return 0;
}
