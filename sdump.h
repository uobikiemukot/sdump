/* See LICENSE for licence details. */
#define _XOPEN_SOURCE 600
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <signal.h>
#include <unistd.h>
#include <sixel.h>

enum {
	VERBOSE     = false,
	BUFSIZE     = 1024,
	CELL_WIDTH  = 16,
	CELL_HEIGHT = 8,
};

struct tty_t {
	int fd;                      /* fd of current controlling terminal */
	int width, height;           /* terminal size (by pixel) */
	int cell_width, cell_height; /* cell_size (by pixel) */
};

char temp_file[PATH_MAX];
