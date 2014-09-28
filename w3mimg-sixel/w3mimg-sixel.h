/* See LICENSE for licence details. */
#define _XOPEN_SOURCE 600
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>

/* for Mac OS X? */
#define SIGWINCH 28

enum w3m_op {
	W3M_DRAW = 0,
	W3M_REDRAW,
	W3M_STOP,
	W3M_SYNC,
	W3M_NOP,
	W3M_GETSIZE,
	W3M_CLEAR,
	NUM_OF_W3M_FUNC,
};

enum {
	VERBOSE            = false,   /* if false, suppress "DEBUG" level logging */
	BUFSIZE            = 1024,
	MAX_IMAGE          = 1024,
	/* default value */
	CELL_WIDTH         = 8,
	CELL_HEIGHT        = 16,
	/* this values will be updated at window resize */
	TERM_WIDTH         = 1280,
	TERM_HEIGHT        = 1024,
	/* for select */
	SELECT_TIMEOUT     = 100000, /* usec */
	SELECT_CHECK_LIMIT = 4,
	/* experimental features: buggy! */
	QUERY_WINDOW_SIZE  = false,
	SIXEL_PENETRATE    = false,
};

struct tty_t {
	int fd;                      /* fd of current controlling terminal */
	int width, height;           /* terminal size (by pixel) */
	int cell_width, cell_height; /* cell_size (by pixel) */
};

const char *instance_log = "/tmp/w3mimg-sixel.instance.log";
const char *log_file     = "/tmp/w3mimg-sixel.log";
volatile sig_atomic_t window_resized = false;
