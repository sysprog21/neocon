/*
 * neocon.c - An interface for changing tty devices
 *
 * Copyright (C) 2007 by OpenMoko, Inc.
 * Written by Werner Almesberger <werner@openmoko.org>
 * All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */


#include <stdlib.h>
#include <stdio.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <termios.h>
#include <fcntl.h>
#include <sys/select.h>

static char *const *ttys;
static int num_ttys;
static int bps = B115200;
static struct termios console, tty;


static void make_raw(int fd, struct termios *old)
{
    struct termios t;
    long flags;

    if (tcgetattr(fd, &t) < 0) {
	perror("tcgetattr");
	exit(1);
    }
    if (old)
	*old = t;
    cfmakeraw(&t);
    if (fd) {
	t.c_iflag  &= ~(IXON | IXOFF);
	t.c_cflag |= CLOCAL;
	t.c_cflag &= ~CRTSCTS;
	if (cfsetispeed(&t, bps) < 0) {
	    perror("cfsetispeed");
	    exit(1);
	}
	if (cfsetospeed(&t, bps) < 0) {
	    perror("cfsetospeed");
	    exit(1);
	}
    }
    if (tcsetattr(fd, TCSANOW, &t) < 0) {
	perror("tcsetattr");
	exit(1);
    }
    flags = fcntl(fd,F_GETFL);
    if (flags < 0) {
	perror("fcntl F_GETFL");
	exit(1);
    }
    if (fcntl(fd,F_SETFL,flags & ~O_NONBLOCK) < 0) {
	perror("fcntl F_GETFL");
	exit(1);
    }
}


static int open_first_tty(void)
{
    int i, fd = -1;

    for (i = 0; i != num_ttys; i++) {
	fd = open(ttys[i], O_RDWR | O_NDELAY);
	if (fd >= 0)
	    break;
    }
    if (fd >= 0)
	make_raw(fd, &tty);
    return fd;
}


static void scan(const char *s, size_t len)
{
    static int state = 0;
    size_t i;

    for (i = 0; i != len; i++)
	switch (state) {
	    case 0:
		if (s[i] == '~')
		    state++;
		else
		    state = 0;
		break;
	    case 1:
		if (s[i] == '.')
		    exit(0);
		state = 0;
		break;
	}
}


static int copy(int in, int out, int scan_escape)
{
    char buffer[2048];
    ssize_t got, wrote, pos;
 
    got = read(in, buffer, sizeof(buffer));
    if (got < 0)
	return 0;
    if (scan_escape)
	scan(buffer, got);
    for (pos = 0; pos != got; pos += wrote) {
	wrote = write(out, buffer+pos, got-pos);
	if (wrote < 0)
	    return 0;
    }
    return 1;
}


static void write_string(const char *s)
{
    int len = strlen(s);

    while (len) {
	ssize_t wrote;

	wrote = write(1, s, len);
	if (wrote < 0) {
	    perror("write");
	    exit(1);
	}
	s += wrote;
	len -= wrote;
    }
}


static void cleanup(void)
{
    if (tcsetattr(0, TCSANOW, &console) < 0)
	perror("tcsetattr");
}


static void usage(const char *name)
{
    fprintf(stderr, "usage: %s [-b bps] tty ...\n", name);
    exit(1);
}


int main(int argc, char *const *argv)
{
    char *end;
    int c;
    int fd = -1;

    while ((c = getopt(argc, argv, "")) != EOF)
	switch (c) {
	    case 'b':
		bps = strtoul(optarg, &end, 0);
		if (!*end)
		    usage(*argv);
		break;
	    default:
		usage(*argv);
	}
    num_ttys = argc-optind;
    ttys = argv+optind;

    make_raw(0, &console);
    atexit(cleanup);
    while (1) {
	struct timeval tv;
	fd_set set;
	int res;

	if (fd < 0) {
	    fd = open_first_tty();
	    if (fd > 0)
		write_string("\r\n[Open]\r\n");
	}
	FD_ZERO(&set);
	FD_SET(0, &set);
	if (fd >= 0)
	    FD_SET(fd, &set);
	tv.tv_sec = 0;
	tv.tv_usec = 100000;
	res = select(fd < 0 ? 1 : fd+1, &set, NULL, NULL, &tv);
	if (res < 0) {
	    perror("select");
	    return 1;
	}
	if (FD_ISSET(0, &set))
	    if (!copy(0, fd, 1))
		goto failed;
	if (fd >= 0 && FD_ISSET(fd, &set))
	    if (!copy(fd, 1, 0))
		goto failed;
	continue;

failed:
	write_string("\r\n[Closed]\r\n");
	(void) close(fd);
	fd = -1;
    }
    return 0;
}
