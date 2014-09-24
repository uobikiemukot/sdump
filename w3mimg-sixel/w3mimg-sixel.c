/* See LICENSE for licence details. */
#define _XOPEN_SOURCE 600
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
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>

#include "../util.h"
#include "../loader.h"
#include "../image.h"
#include "parsearg.h"
#include "sixel.h"

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
	BUFSIZE     = 1024,
	MAX_IMAGE   = 1024,
	/* default value */
	CELL_WIDTH  = 8,
	CELL_HEIGHT = 16,
	TERM_WIDTH  = 1280,
	TERM_HEIGHT = 1024,
	/* for select */
	SELECT_TIMEOUT     = 100000,  /* usec */
	SELECT_CHECK_LIMIT = 2,
};

struct tty_t {
	int fd;                      /* fd of current controlling terminal */
	int width, height;           /* terminal size (by pixel) */
	int cell_width, cell_height; /* cell_size (by pixel) */
};

const char *instance_log = "/tmp/w3mimg-sixel.instance.log";
const char *log_file     = "/tmp/w3mimg-sixel.log";

void draw_sixel_image(struct tty_t *tty, struct sixel_t *sixel, struct image *img,
	int offset_x, int offset_y, const char *file, int op)
{
	char *sixel_file, buf[BUFSIZE];
	int length, fd, size;
	FILE *fp;

	length = strlen(file) + 7; /* file + ".sixel" + '\0' */
	if ((sixel_file = ecalloc(length, sizeof(char))) == NULL)
		return;

	snprintf(sixel_file, length, "%s%s", file, ".sixel");
	logging(DEBUG, "sixel_file: %s\n", sixel_file);

	if (op == W3M_DRAW) {
		logging(DEBUG, "create new sixel file\n");
		img->already_drawed = false;

		if ((fp = efopen(sixel_file, "w")) == NULL) {
			logging(DEBUG, "couldn't open new sixel file\n");
			goto file_open_err;
		}
		sixel_init(sixel, img, fp);
		sixel_output(sixel, img);
		sixel_die(sixel);
		efclose(fp);
	} else if (op == W3M_REDRAW && !img->already_drawed) {
		logging(DEBUG, "cat sixel file\n");
		img->already_drawed = true;

		if ((fd = eopen(sixel_file, O_RDONLY)) < 0) {
			logging(DEBUG, "sixel file not found\n");
			goto file_open_err;
		}
		snprintf(buf, BUFSIZE, "\033[%d;%dH",
			(offset_y / tty->cell_height) + 1, (offset_x / tty->cell_width) + 1);
		ewrite(tty->fd, buf, strlen(buf));

		while ((size = read(fd, buf, BUFSIZE)) > 0)
			ewrite(tty->fd, buf, size);
		close(fd);
	}
file_open_err:
	free(sixel_file);
}

void w3m_draw(struct tty_t *tty, struct sixel_t *sixel, struct image imgs[], struct parm_t *parm, int op)
{
	int index, offset_x, offset_y, width, height, shift_x, shift_y, view_w, view_h;
	char *file;
	struct image *img;

	logging(DEBUG, "w3m_%s()\n", (op == W3M_DRAW) ? "draw": "redraw");

	if (parm->argc != 11)
	//if (parm->argc != 11 || op == W3M_REDRAW)
		return;

	index     = str2num(parm->argv[1]) - 1; /* 1 origin */
	offset_x  = str2num(parm->argv[2]);
	offset_y  = str2num(parm->argv[3]);
	width     = str2num(parm->argv[4]);
	height    = str2num(parm->argv[5]);
	shift_x   = str2num(parm->argv[6]);
	shift_y   = str2num(parm->argv[7]);
	view_w    = str2num(parm->argv[8]);
	view_h    = str2num(parm->argv[9]);
	file      = parm->argv[10];

	if (index < 0)
		index = 0;
	else if (index >= MAX_IMAGE)
		index = MAX_IMAGE - 1;
	img = &imgs[index];

	logging(DEBUG, "index:%d offset_x:%d offset_y:%d shift_x:%d shift_y:%d view_w:%d view_h:%d\n",
		index, offset_x, offset_y, shift_x, shift_y, view_w, view_h);

	if (op == W3M_DRAW) {
		if (get_current_frame(img)) { /* cleanup preloaded image */
			free_image(img);
			init_image(img);
		}
		if (load_image(file, img) == false)
			return;
	}

	if (!get_current_frame(img)) {
		logging(ERROR, "specify unloaded image? img[%d] is NULL\n", index);
		return;
	}
	increment_frame(img);

	/* XXX: maybe need to resize at this time */
	if (width != get_image_width(img) || height != get_image_height(img))
		resize_image(img, width, height, false);

	/* FIXME: buggy crop */
	//crop_image(img, offset_x, offset_y, shift_x, shift_y,
		//(view_w ? view_w: width), (view_h ? view_h: height), false);

	/* sixel */
	draw_sixel_image(tty, sixel, img, offset_x, offset_y, file, op);
}

void w3m_stop()
{
	logging(DEBUG, "w3m_stop()\n");
}

void w3m_sync()
{
	logging(DEBUG, "w3m_sync()\n");
}

void w3m_nop()
{
	logging(DEBUG, "w3m_nop()\n");
	printf("\n");
}

void w3m_getsize(struct image *img, const char *file)
{
	logging(DEBUG, "w3m_getsize()\n");

	if (get_current_frame(img)) { /* cleanup preloaded image */
		free_image(img);
		init_image(img);
	}

	if (load_image(file, img)) {
		printf("%d %d\n", get_image_width(img), get_image_height(img));
		logging(DEBUG, "responce: %d %d\n", get_image_width(img), get_image_height(img));
	} else {
		printf("0 0\n");
		logging(DEBUG, "responce: 0 0\n");
	}
}

void w3m_clear(struct image img[], struct parm_t *parm)
{
	logging(DEBUG, "w3m_clear()\n");

	for (int i = 0; i < MAX_IMAGE; i++)
		img[i].already_drawed = false;
	/*
	int offset_x, offset_y, width, height;

	if (parm->argc != 5)
		return;

	offset_x  = str2num(parm->argv[1]);
	offset_y  = str2num(parm->argv[2]);
	width     = str2num(parm->argv[3]);
	height    = str2num(parm->argv[4]);

	(void) offset_x;
	(void) offset_y;
	(void) width;
	(void) height;
	*/

	(void) img;
	(void) parm;
}

/*
bool file_lock(FILE *fp)
{
	struct flock lock;

	lock.l_type = F_WRLCK;
	lock.l_whence = SEEK_SET;
	lock.l_start = 0;
	lock.l_len = 0;
	lock.l_pid = getpid();

	if (fcntl(fileno(fp), F_SETLKW, &lock) == -1)
		return false;

	return true;
}

bool file_unlock(FILE *fp)
{
	struct flock lock;

	lock.l_type = F_UNLCK;
	lock.l_whence = SEEK_SET;
	lock.l_start = 0;
	lock.l_len = 0;
	lock.l_pid = getpid();

	if (fcntl(fileno(fp), F_SETLK, &lock) == -1)
		return false;

	return true;
}
*/

int check_fds(fd_set *fds, struct timeval *tv, int fd)
{
	FD_ZERO(fds);
	FD_SET(fd, fds);
	tv->tv_sec  = 0;
	tv->tv_usec = SELECT_TIMEOUT;
	return eselect(fd + 1, fds, NULL, NULL, tv);
}

bool terminal_query(int ttyfd, int *height, int *width, const char *send_seq, const char *recv_format)
{
	int ret, check_count;
	char buf[BUFSIZE], *ptr;
	ssize_t size, left, length;
	struct timeval tv;
	fd_set fds;

	length = strlen(send_seq);
	if ((size = ewrite(ttyfd, send_seq, length)) != length) {
		logging(DEBUG, "write error (data:%d != wrote:%d)\n", size, length);
		return false;
	}

	ptr = buf;
	left = BUFSIZE - 1;

	check_count = 0;
	while (check_count < SELECT_CHECK_LIMIT) {
		if ((ret = check_fds(&fds, &tv, ttyfd)) < 0)
			continue;

		if (FD_ISSET(ttyfd, &fds)) {
			/* FIXME: read is blocked!!! */
			if ((size = read(ttyfd, ptr, left)) > 0) {
				*(ptr + size) = '\0';
				logging(DEBUG, "buf: %s\n", buf);
				if (sscanf(buf, recv_format, height, width) == 2)
					return true;
			}
			ptr  += size;
			left -= size;
		}
		check_count++;
	}
	return false;
}

bool check_terminal_size(struct tty_t *tty)
{
	char ttyname[L_ctermid];
	struct winsize wsize;

	/* get ttyname and open it */
	if (!ctermid(ttyname)
		|| (tty->fd = eopen(ttyname, O_RDWR)) < 0) {
		logging(ERROR, "couldn't open controlling terminal\n");
		return false;
	}
	logging(DEBUG, "ttyname:%s fd:%d\n", ttyname, tty->fd);

	/* at first, we try to get pixel size from "struct winsize" */
	if (ioctl(tty->fd, TIOCGWINSZ, &wsize)) {
		logging(ERROR, "ioctl: TIOCGWINSZ failed\n");
	} else if (wsize.ws_xpixel == 0 || wsize.ws_ypixel == 0) {
		logging(ERROR, "struct winsize has no pixel information\n");
	} else {
		tty->width  = wsize.ws_xpixel;
		tty->height = wsize.ws_ypixel;
		tty->cell_width  = tty->width / wsize.ws_col;
		tty->cell_height = tty->height / wsize.ws_row;
		logging(DEBUG, "terminal size set by winsize\n");
		return true;
	}

	/* second, try dtterm sequence:
		CSI 14 t: request window size in pixels.
			-> responce: CSI 4 ; height ; width t
		CSI 18 t: request text area size in characters
			-> responce: CSI 8 ; height ; width t
	*/
	/* this function causes I/O block...
	int cols, lines;
	if (terminal_query(tty->fd, &tty->height, &tty->width, "\033[14t", "\033[4;%d;%dt")
		&& terminal_query(tty->fd, &lines, &cols, "\033[18t", "\033[8;%d;%dt")) {
		tty->cell_width  = tty->width / cols;
		tty->cell_height = tty->height / lines;
		logging(DEBUG, "wsize.col:%d wsize.row:%d dtterm.col:%d dtterm.row:%d\n",
			wsize.ws_col, wsize.ws_row, cols, lines);
		logging(DEBUG, "terminal size set by dtterm sequence\n");
		return true;
	} else {
		logging(ERROR, "no responce for dtterm sequence\n");
	}
	*/

	/* finally, use default value */
	tty->width  = TERM_WIDTH;
	tty->height = TERM_HEIGHT;
	tty->cell_width  = CELL_WIDTH;
	tty->cell_height = CELL_HEIGHT;
	logging(DEBUG, "terminal size set by default value\n");

	return true;
}

int main(int argc, char *argv[])
{
	/*
	command line option
	  -bg    : background color (for transparent image?)
	  -x     : image position offset x
	  -y     : image position offset x
	  -test  : request display size (response "width height\n")
	  -size  : request image size  (response "width height\n")
	  -anim  : number of max frame of animation image?
	  -margin: margin of clear region?
	  -debug : debug flag (not used)
	*/
	/*
	w3mimg protocol
	  0  1  2 ....
	 +--+--+--+--+ ...... +--+--+
	 |op|; |args             |\n|
	 +--+--+--+--+ .......+--+--+

	 args is separeted by ';'
	 op   args
	  0;  params    draw image
	  1;  params    redraw image
	  2;  -none-    terminate drawing
	  3;  -none-    sync drawing
	  4;  -none-    nop, sync communication
	                response '\n'
	  5;  path      get size of image,
	                response "<width> <height>\n"
	  6;  params(6) clear image

	  params
	   <n>;<x>;<y>;<w>;<h>;<sx>;<sy>;<sw>;<sh>;<path>
	  params(6)
	   <x>;<y>;<w>;<h>
	*/
	int i, op, optind;
	char buf[BUFSIZE], *cp;
	struct sixel_t sixel;
	struct tty_t tty;
	struct image img[MAX_IMAGE];
	struct parm_t parm;

	if (freopen(instance_log, "a", stderr) == NULL)
		logging(ERROR, "freopen (stderr to %s) faild\n", instance_log);
	//file_lock(stderr);

	logging(DEBUG, "--- new instance ---\n");
	for (i = 0; i < argc; i++)
		logging(DEBUG, "argv[%d]:%s\n", i, argv[i]);
	logging(DEBUG, "argc:%d\n", argc);

	/* init */
	for (i = 0; i < MAX_IMAGE; i++)
		init_image(&img[i]);

	sixel.dither  = NULL;
	sixel.context = NULL;

	if (!check_terminal_size(&tty))
		goto release;

	logging(DEBUG, "terminal size width:%d height:%d cell_width:%d cell_height:%d\n",
		tty.width, tty.height, tty.cell_width, tty.cell_height);

	/* check args */
	optind = 1;
	while (optind < argc) {
		if (strncmp(argv[optind], "-test", 5) == 0) {
			printf("%d %d\n", tty.width, tty.height);
			logging(DEBUG, "responce: %d %d\n", tty.width, tty.height);
			goto release;
		}
		else if (strncmp(argv[optind], "-size", 5) == 0 && ++optind < argc) {
			w3m_getsize(&img[0], argv[optind]);
			goto release;
		}
		optind++;
	}

	/* for avoiding simultaneous-write, reopen stder again
		w3m --+------------------+----------------------------------------------> (end)
			  | fork()/pipe()    |fork()/popen()    |               |
			  |                  v                  v               v
			  |                  w3mimg -> (end)    w3mimg -> (end) w3mimg -> (end)
			  |                  (get image size)
			  |                  |
			  |                  v
			  |                  w3mimg_instance.log
			  v
			  w3mimg -----------------------------------------------------------> (end)
			  (drawing)
			  |
			  v
			  w3mimg_sixel.log (file lock)
	*/
	/*
	if (freopen(log_file, "w", stderr) == NULL)
		logging(ERROR, "freopen (stderr to %s) faild\n", log_file);
	*/
	//file_lock(stderr);

	setvbuf(stderr, NULL, _IONBF, 0);
	setvbuf(stdout, NULL, _IONBF, 0);

	/* main loop */
    while (fgets(buf, BUFSIZE, stdin) != NULL) {
		if ((cp = strchr(buf, '\n')) == NULL) {
			logging(ERROR, "lbuf overflow? (couldn't find newline) buf length:%d\n", strlen(buf));
			continue;
		}
		*cp = '\0';

		logging(DEBUG, "stdin: %s\n", buf);

		reset_parm(&parm);
		parse_arg(buf, &parm, ';', isgraph);

		if (parm.argc <= 0)
			continue;

		op = str2num(parm.argv[0]);
		if (op < 0 || op >= NUM_OF_W3M_FUNC)
			continue;

		switch (op) {
			case W3M_DRAW:
			case W3M_REDRAW:
				w3m_draw(&tty, &sixel, img, &parm, op);
				break;
			case W3M_STOP:
				w3m_stop();
				break;
			case W3M_SYNC:
				w3m_sync();
				break;
			case W3M_NOP:
				w3m_nop();
				break;
			case W3M_GETSIZE:
				if (parm.argc != 2) 
					break;
				w3m_getsize(&img[0], parm.argv[1]);
				break;
			case W3M_CLEAR:
				w3m_clear(img, &parm);
				break;
			default:
				break;
		}
    }

	/* release */
release:
	for (i = 0; i < MAX_IMAGE; i++)
		free_image(&img[i]);
	sixel_die(&sixel);

	fflush(stdout);
	fflush(stderr);

	//file_unlock(stderr);
	efclose(stderr);

	if (tty.fd > 0)
		eclose(tty.fd);

	logging(DEBUG, "exiting...\n");
	return EXIT_SUCCESS;
}
