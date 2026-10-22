/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
/* Windows CRT pipes for host-only FD ownership models. */
#ifndef ANDROID_WIN32_FD_TEST_H
#define ANDROID_WIN32_FD_TEST_H
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <unistd.h>
#ifndef F_GETFD
#define F_GETFD 1
#endif
#ifndef F_DUPFD_CLOEXEC
#define F_DUPFD_CLOEXEC 1030
#endif
#ifndef FD_CLOEXEC
#define FD_CLOEXEC 1
#endif
#define POLLIN 1
#define POLLERR 8
#define POLLHUP 16
#define POLLNVAL 32
struct pollfd
{
   int fd;
   short events, revents;
};
typedef unsigned long nfds_t;
int android_test_pipe(int pair[2]);
int android_test_close(int fd);
int android_test_fcntl(int fd, int command, ...);
int android_test_poll(struct pollfd *fds, nfds_t count, int timeout);
#ifndef ANDROID_WIN32_FD_TEST_IMPLEMENTATION
#define pipe android_test_pipe
#define close android_test_close
#define fcntl android_test_fcntl
#define poll android_test_poll
#endif
#endif
#endif
