/* 
   Copyright (C) by Ronnie Sahlberg <ronniesahlberg@gmail.com> 2024
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, see <http://www.gnu.org/licenses/>.
*/

#define _FILE_OFFSET_BITS 64
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <inttypes.h>
#if !defined(__amigaos4__) && !defined(__AMIGA__) && !defined(__AROS__)
#include <poll.h>
#endif
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "smb2.h"
#include "libsmb2.h"
#include "libsmb2-raw.h"

#ifdef __AROS__
#include "asprintf.h"
#endif

struct file_context {
	int is_smb2;
	int fd;
	struct smb2_context *smb2;
	struct smb2fh *smb2fh;
	struct smb2_url *url;
};

void usage(void)
{
	fprintf(stderr, "Usage: smb2-cp <src> <dst>\n");
	fprintf(stderr, "<src>,<dst> can either be a local file or "
			"an smb2 URL.\n");
	exit(0);
}

static void
free_file_context(struct file_context *file_context)
{
	if (file_context->fd != -1) {
		close(file_context->fd);
	}
	if (file_context->smb2fh != NULL) {
		smb2_close(file_context->smb2, file_context->smb2fh);
	}
	if (file_context->smb2 != NULL) {
		smb2_destroy_context(file_context->smb2);
	}
	smb2_destroy_url(file_context->url);
	free(file_context);
}

static int
fstat_file(struct file_context *fc, struct stat *st)
{
	if (fc->is_smb2 == 0) {
		return fstat(fc->fd, st);
	} else {
		int res;
		struct smb2_stat_64 smb2_st;
		res = smb2_fstat(fc->smb2, fc->smb2fh, &smb2_st);
		st->st_dev          = 0;
		st->st_ino          = (ino_t)smb2_st.smb2_ino;
#ifndef WIN32
		st->st_mode         = 0;
		st->st_nlink        = (nlink_t)smb2_st.smb2_nlink;
		st->st_blksize      = 4096;
		st->st_blocks       = (smb2_st.smb2_size + 4096 - 1) % 4096;
#endif
		st->st_uid          = 0;
		st->st_gid          = 0;
		st->st_rdev         = 0;
		st->st_size         = (off_t)smb2_st.smb2_size;
		st->st_atime        = smb2_st.smb2_atime;
		st->st_mtime        = smb2_st.smb2_mtime;
		st->st_ctime        = smb2_st.smb2_ctime;

		return res;
	}
}

static ssize_t
file_pread(struct file_context *fc, uint8_t *buf, size_t count, off_t off)
{
	if (fc->is_smb2 == 0) {
		lseek(fc->fd, off, SEEK_SET);
		return read(fc->fd, buf, count);
	} else {
		return smb2_pread(fc->smb2, fc->smb2fh, buf, count, off);
	}
}

static ssize_t
file_pwrite(struct file_context *fc, uint8_t *buf, size_t count, off_t off)
{
	if (fc->is_smb2 == 0) {
		lseek(fc->fd, off, SEEK_SET);
		return write(fc->fd, buf, count);
	} else {
		return smb2_pwrite(fc->smb2, fc->smb2fh, buf, count, off);
	}
}

static struct file_context *
open_file(const char *url, int flags)
{
	struct file_context *file_context;

	file_context = malloc(sizeof(struct file_context));
	if (file_context == NULL) {
		fprintf(stderr, "Failed to malloc file_context\n");
		return NULL;
	}
	file_context->is_smb2 = 0;
	file_context->fd     = -1;
	file_context->smb2    = NULL;
	file_context->smb2fh  = NULL;
	file_context->url    = NULL;

	if (strncmp(url, "smb://", 6)) {
		file_context->is_smb2 = 0;
		file_context->fd = open(url, flags, 0660);
		if (file_context->fd == -1) {		
			fprintf(stderr, "Failed to open %s\n", url);
			free_file_context(file_context);
			return NULL;
		}
		return file_context;
	}

	file_context->is_smb2 = 1;

	file_context->smb2 = smb2_init_context();
	if (file_context->smb2 == NULL) {
		fprintf(stderr, "failed to init context\n");
		free_file_context(file_context);
		return NULL;
	}

	file_context->url = smb2_parse_url(file_context->smb2, url);
	if (file_context->url == NULL) {
		fprintf(stderr, "%s\n", smb2_get_error(file_context->smb2));
		free_file_context(file_context);
		return NULL;
	}

	if (smb2_connect_share(file_context->smb2, file_context->url->server,
			       file_context->url->share,
			       file_context->url->user) != 0) {
		fprintf(stderr, "Failed to mount smb2 share : %s\n",
			       smb2_get_error(file_context->smb2));
		free_file_context(file_context);
		return NULL;
	}

	file_context->smb2fh = smb2_open(file_context->smb2, file_context->url->path, flags);
	if (file_context->smb2fh == NULL) {
		fprintf(stderr, "Failed to open file %s: %s\n",
			       file_context->url->path,
			       smb2_get_error(file_context->smb2));
		free_file_context(file_context);
		return NULL;
	}
	return file_context;
}

#define BUFSIZE 1024*1024
static uint8_t buf[BUFSIZE];

int main(int argc, char *argv[])
{
	struct stat st;
	struct file_context *src;
	struct file_context *dst;
	off_t off;
	ssize_t count;
	
#ifdef WIN32
	if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
		printf("Failed to start Winsock2\n");
		return 10;
	}
#endif

#ifdef AROS
	aros_init_socket();
#endif

	if (argc != 3) {
		usage();
	}

	src = open_file(argv[1], O_RDONLY);
	if (src == NULL) {
		fprintf(stderr, "Failed to open %s\n", argv[1]);
		return 10;
	}

	dst = open_file(argv[2], O_WRONLY|O_CREAT|O_TRUNC);
	if (dst == NULL) {
		fprintf(stderr, "Failed to open %s\n", argv[2]);
		free_file_context(src);
		return 10;
	}

	if (fstat_file(src, &st) != 0) {
		fprintf(stderr, "Failed to fstat source file\n");
		free_file_context(src);
		free_file_context(dst);
		return 10;
	}

	off = 0;
	while (off < st.st_size) {
		count = (size_t)(st.st_size - off);
		if (count > BUFSIZE) {
			count = BUFSIZE;
		}
		count = file_pread(src, buf, count, off);
		if (count < 0) {
			fprintf(stderr, "Failed to read from source file\n");
			free_file_context(src);
			free_file_context(dst);
			return 10;
		}
		count = file_pwrite(dst, buf, count, off);
		if (count < 0) {
			fprintf(stderr, "Failed to write to dest file\n");
			free_file_context(src);
			free_file_context(dst);
			return 10;
		}

		off += count;
	}
	printf("copied %d bytes\n", (int)off);

	free_file_context(src);
	free_file_context(dst);

	return 0;
}
