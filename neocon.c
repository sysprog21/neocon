/*
 * neocon.c - An interface for changing tty devices
 *
 * Copyright (C) 2007, 2008 by OpenMoko, Inc.
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

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <termios.h>
#include <fcntl.h>
#include <assert.h>
#include <sys/time.h>
#include <sys/select.h>
#ifdef __APPLE__
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <IOKit/serial/ioss.h>
#endif

#define MAX_BUF 2048

static char *const *ttys;
static int num_ttys;
static int curr_tty = -1; /* start with first tty */
static speed_t speed = B115200;
static struct termios console, tty;
static FILE *log = NULL;
static bool timestamp = false;

#define ESC "\033"
static char escape = '~';

static struct bps {
    speed_t speed;
    int bps;
} bps_tab[] = {
    { B300, 300 },
    { B1200, 1200 },
    { B2400, 2400 },
    { B9600, 9600 },
    { B19200, 19200 },
    { B38400, 38400 },
    { B57600, 57600 },
    { B115200, 115200 },
#ifdef B230400
    { B230400, 230400 },
#endif
#ifdef B460800
    { B460800, 460800 },
#endif
#ifdef B500000
    { B500000, 500000 },
#endif
#ifdef B576000
    { B576000, 576000 },
#endif
#ifdef B921600
    { B921600, 921600 },
#endif
#ifdef B1000000
    { B1000000, 1000000 },
#endif
#ifdef B1152000
    { B1152000, 1152000 },
#endif
#ifdef B1500000
    { B1500000, 1500000 },
#endif
#ifdef B2000000
    { B2000000, 2000000 },
#endif
#ifdef B2500000
    { B2500000, 2500000 },
#endif
#ifdef B3000000
    { B3000000, 3000000 },
#endif
#ifdef B3500000
    { B3500000, 3500000 },
#endif
#ifdef B4000000
    { B4000000, 4000000 },
#endif
    { 0, 0 },
};

static speed_t bps_to_speed(int bps)
{
#ifdef __APPLE__
    return bps;
#endif

    for (const struct bps *p = bps_tab; p->bps; p++) {
        if (p->bps == bps)
            return p->speed;
    }
    fprintf(stderr, "no such speed: %d bps\n", bps);
    exit(1);
}

static void make_raw(int fd, struct termios *old)
{
    struct termios t;
    if (tcgetattr(fd, &t) < 0) {
        perror("tcgetattr");
        exit(1);
    }

    if (old)
        *old = t;
    cfmakeraw(&t);
    if (fd) {
        t.c_iflag &= ~(IXON | IXOFF);
        t.c_cflag |= CLOCAL;
        t.c_cflag &= ~CRTSCTS;
#ifdef __APPLE__
        if (ioctl(fd, IOSSIOSPEED, &speed) == -1) {
            printf("Error %d calling ioctl( ..., IOSSIOSPEED, ... )\n", errno);
        }
#endif
        if (cfsetispeed(&t, speed) < 0) {
            perror("cfsetispeed");
            exit(1);
        }
        if (cfsetospeed(&t, speed) < 0) {
            perror("cfsetospeed");
            exit(1);
        }
    }
    if (tcsetattr(fd, TCSANOW, &t) < 0) {
        perror("tcsetattr");
        exit(1);
    }

    long flags = fcntl(fd, F_GETFL);
    if (flags < 0) {
        perror("fcntl F_GETFL");
        exit(1);
    }
    if (fcntl(fd, F_SETFL, flags & ~O_NONBLOCK) < 0) {
        perror("fcntl F_GETFL");
        exit(1);
    }
}

static int open_next_tty(void)
{
    int fd = -1;

    for (int i = 0; i != num_ttys; i++) {
        curr_tty = (curr_tty + 1) % num_ttys;
        fd = open(ttys[curr_tty], O_RDWR | O_NDELAY);
        if (fd >= 0)
            break;
    }
    if (fd >= 0)
        make_raw(fd, &tty);
    return fd;
}

/*
 * Return true if the user manually forces a device change.
 */
static bool scan(const char *s, size_t len)
{
    static int state = 0;
    bool res = false;

    for (const char *p = s; p != s + len; p++) {
        switch (state) {
        case 0:
            if (*p == escape)
                state++;
            else
                state = 0;
            break;
        case 1:
            if (*p == '.')
                exit(0);
            if (*p == 'n')
                res = true;
            state = 0;
            break;
        }
    }
    return res;
}

static bool write_log(const char *buf, ssize_t len)
{
    ssize_t wrote = fwrite(buf, 1, len, log);
    if (wrote == len)
        return true;

    fprintf(stderr, "write failed. closing log file.\n");
    fclose(log);
    log = NULL;
    return false;
}

static bool add_timestamp(void)
{
    struct timeval tv;
    if (gettimeofday(&tv, NULL) < 0) {
        perror("gettimeofday");
        exit(1);
    }

    char buf[40]; /* be generous */
    int len = sprintf(buf, "%lu.%06lu ", (unsigned long)tv.tv_sec,
                      (unsigned long)tv.tv_usec);
    return write_log(buf, len);
}

static void do_log(const char *buf, ssize_t len)
{
    static bool nl = true; /* we are at the beginning of a new line */
    char tmp[MAX_BUF];

    assert(len <= MAX_BUF);
    const char *from = buf;
    char *to = tmp;
    while (from != buf + len) {
        if (*from == '\r') {
            from++;
            continue;
        }
        if (nl && timestamp)
            if (!add_timestamp())
                return;
        nl = false;
        if (*from == '\n') {
            *to++ = *from++;
            if (!write_log(tmp, to - tmp))
                return;
            to = tmp;
            nl = true;
            continue;
        }
        *to++ = *from < ' ' || *from > '~' ? '#' : *from;
        from++;
    }
    write_log(tmp, to - tmp);
}

static bool copy(int in, int out, int from_user, int single)
{
    char buffer[MAX_BUF];

    ssize_t got = read(in, buffer, single ? 1 : sizeof(buffer));
    if (got <= 0)
        return false;

    if (from_user) {
        if (scan(buffer, got))
            return false;
    } else {
        if (log)
            do_log(buffer, got);
    }

    for (ssize_t pos = 0; pos != got;) {
        ssize_t wrote = write(out, buffer + pos, got - pos);
        if (wrote < 0)
            return false;
        pos += wrote;
    }
    return true;
}

static void write_string(const char *s)
{
    int len = strlen(s);
    while (len) {
        ssize_t wrote = write(1, s, len);
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
    write(1, "\n", 1);
}

static void usage(const char *name)
{
    fprintf(
        stderr,
        "Usage: %s [-b bps] [-e escape] [-l logfile [-a] [-T]] [-t delay_ms] tty ...\n"
        "  -a           append to the log file if it already exists\n"
        "  -b bps       set the TTY to the specified bit rate\n"
        "  -e escape    set the escape character (default: ~)\n"
        "  -l logfile   log all output to the specified file\n"
        "  -t delay_ms  wait the specified amount of time between input characters\n"
        "  -T           add timestamps to the log file\n",
        name);
    exit(1);
}

extern char **environ;
int main(int argc, char *const *argv)
{
    char *end;
    bool append = false;
    const char *logfile = NULL;
    int throttle_us = 0;
    bool throttle = false;

    int c;
    while ((c = getopt(argc, argv, "ab:e:hl:t:T")) != EOF) {
        switch (c) {
        case 'a':
            append = true;
            break;
        case 'b': {
            int bps = strtoul(optarg, &end, 0);
            if (*end)
                usage(*argv);
            speed = bps_to_speed(bps);
            break;
        }
        case 'e':
            if (strlen(optarg) != 1)
                usage(*argv);
            escape = *optarg;
            break;
        case 'l':
            logfile = optarg;
            break;
        case 't':
            throttle_us = strtoul(optarg, &end, 0) * 1000;
            if (*end)
                usage(*argv);
            break;
        case 'T':
            timestamp = true;
            break;
        default:
            usage(*argv);
        }
    }
    num_ttys = argc - optind;
    ttys = argv + optind;

    if (logfile) {
        log = fopen(logfile, append ? "a" : "w");
        if (!log) {
            perror(logfile);
            exit(1);
        }
        setlinebuf(log);
    }

    /* restore the terminal */
    /* See 'man 4 console_codes' for details:
     * "ESC c"        -- Reset
     * "ESC ( B"      -- Select G0 Character Set (B = US)
     * "ESC [ m"      -- Reset all display attributes
     * "ESC [ J"      -- Erase to the end of screen
     * "ESC [ ? 25 h" -- Make cursor visible
     */
    printf(ESC "c" ESC "(B" ESC "[m" ESC "[J" ESC "[?25h");

    /* Make sure stdout gets drained before opening tty. */
    fflush(NULL);

    make_raw(0, &console);
    atexit(cleanup);

    int fd = -1;
    while (1) {
        if (fd < 0) {
            fd = open_next_tty();
            if (fd > 0) {
                char buf[1024]; /* should be enough */

                sprintf(buf, "\r\r[Open %s]\r\n", ttys[curr_tty]);
                write_string(buf);
            }
        }

        fd_set set;
        FD_ZERO(&set);
        if (!throttle)
            FD_SET(0, &set);
        if (fd >= 0)
            FD_SET(fd, &set);

        struct timeval tv = {
            .tv_sec = 0,
            .tv_usec = throttle ? throttle_us : 100000,
        };
        int res = select(fd < 0 ? 1 : fd + 1, &set, NULL, NULL, &tv);
        if (res < 0) {
            perror("select");
            return 1;
        }
        if (!res)
            throttle = false;
        if (FD_ISSET(0, &set)) {
            if (throttle_us)
                throttle = true;
            if (!copy(0, fd, 1, throttle_us != 0))
                goto failed;
        }
        if (fd >= 0 && FD_ISSET(fd, &set))
            if (!copy(fd, 1, 0, 0))
                goto failed;
        continue;

failed:
        write_string("\r\n[Closed]\r\n");
        (void)close(fd);
        fd = -1;
    }
    return 0;
}
