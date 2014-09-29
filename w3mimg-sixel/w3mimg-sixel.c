/* See LICENSE for licence details. */
#include "w3mimg-sixel.h"
#include "../util.h"
#include "../loader.h"
#include "../image.h"
#include "../sixel.h"
#include "parsearg.h"

uint8_t *crop_image_single(struct tty_t *tty, struct image *img, uint8_t *data,
    int shift_x, int shift_y, int width, int height)
{
	int offset;
	uint8_t r, g, b, *cropped_data;

	if ((cropped_data = (uint8_t *) ecalloc(width * height, img->channel)) == NULL)
		return NULL;

	for (int y = 0; y < height; y++) {
		if (y >= tty->height)
			break;

		for (int x = 0; x < width; x++) {
			if (x >= tty->width)
				break;

			get_rgb(img, data, x + shift_x, y + shift_y, &r, &g, &b);

			/* update copy buffer */
			offset = img->channel * (y * width + x);
			*(cropped_data + offset + 0) = r;
			*(cropped_data + offset + 1) = g;
			*(cropped_data + offset + 2) = b;
		}
	}
	free(data);

	img->width  = width;
	img->height = height;

	return cropped_data;
}

void crop_image(struct tty_t *tty, struct image *img,
    int offset_x, int offset_y, int shift_x, int shift_y, int width, int height, bool crop_all)
{
	/*
		+- screen -----------------+
		|        ^                 |
		|        | offset_y        |
		|        v                 |
		|        +- image --+      |
		|<------>|          |      |
		|offset_x|          |      |
		|        |          |      |
		|        +----------+      |
		+--------------|-----------+
		               |
		               v
		+- image ----------------------+
		|       ^                      |
		|       | shift_y              |
		|       v                      |
		|       +- view port + ^       |
		|<----->|            | |       |
		|shift_x|            | | height|
		|       |            | |       |
		|       +------------+ v       |
		|       <-  width ->           |
		+------------------------------+
	*/
	uint8_t *cropped_data;

	if (shift_x + width > img->width)
		width = img->width - shift_x;

	if (shift_y + height > img->height)
		height = img->height - shift_y;

	if (offset_x + width > tty->width)
		width = tty->width - offset_x;

	if (offset_y + height > tty->height)
		height = tty->height - offset_y;

	if (crop_all) {
		for (int i = 0; i < img->frame_count; i++) {
			if ((cropped_data = crop_image_single(tty, img, img->data[i],
				shift_x, shift_y, width, height)) != NULL)
				img->data[i] = cropped_data;
		}
	} else {
		if ((cropped_data = crop_image_single(tty, img, img->data[img->current_frame],
			shift_x, shift_y, width, height)) != NULL)
			img->data[img->current_frame] = cropped_data;
	}
}

void w3m_draw(struct tty_t *tty, struct image imgs[], struct parm_t *parm, int op)
{
	int index, offset_x, offset_y, width, height, shift_x, shift_y, view_w, view_h;
	char *file;
	struct image *img;
	struct sixel_t sixel;

	logging(DEBUG, "w3m_%s()\n", (op == W3M_DRAW) ? "draw": "redraw");

	if (parm->argc != 11)
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

		if (!get_current_frame(img)) {
			logging(ERROR, "specify unloaded image? img[%d] is NULL\n", index);
			return;
		}
		img->already_drew = false;
	/* for ranger (python filer) */
	//}
	//if (!img->already_drew) { /* op == W3M_REDRAW */
	} else if (!img->already_drew) { /* op == W3M_REDRAW */
		char buf[BUFSIZE];
		int size;
		struct image new = *img;

		size = get_image_width(img) * get_image_height(img) * get_image_channel(img);
		if ((new.data[0] = ecalloc(size, 1)) == NULL)
			return;
		memcpy(new.data[0], get_current_frame(img), size);
		new.frame_count   = 1;
		new.current_frame = 0;

		/* XXX: at first, we need to resize, then crop */
		if (width != get_image_width(&new) || height != get_image_height(&new))
			resize_image(&new, width, height, false);

		crop_image(tty, &new, offset_x, offset_y, shift_x, shift_y,
			(view_w ? view_w: width), (view_h ? view_h: height), false);

		/* cursor move */
		snprintf(buf, BUFSIZE, "\033[%d;%dH",
			(offset_y / tty->cell_height) + 1, (offset_x / tty->cell_width) + 1);
		ewrite(tty->fd, buf, strlen(buf));

		/* sixel */
		if (!sixel_init(tty, &sixel, &new, SIXEL_PENETRATE))
			goto sixel_init_err;
		sixel_write(tty, &sixel, &new, SIXEL_PENETRATE);
		sixel_die(&sixel);

sixel_init_err:
		img->already_drew = true;
		free_image(&new);
		increment_frame(img);
	}
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

void w3m_getsize(struct tty_t *tty, struct image *img, const char *file)
{
	int width, height;
	logging(DEBUG, "w3m_getsize()\n");

	if (get_current_frame(img)) { /* cleanup preloaded image */
		free_image(img);
		init_image(img);
	}

	if (load_image(file, img)) {
		/* XXX: we should consider cell alignment */
		width  = get_image_width(img);
		height = get_image_height(img);

		if ((width % tty->cell_width) != 0)
			width  += tty->cell_width - (width % tty->cell_width);
		if ((height % tty->cell_height) != 0)
			height += tty->cell_height - (height % tty->cell_height);

		printf("%d %d\n", width, height);
		logging(DEBUG, "responce: %d %d\n", width, height);
	} else {
		printf("0 0\n");
		logging(DEBUG, "responce: 0 0\n");
	}
}

void w3m_clear(struct image img[], struct parm_t *parm)
{
	logging(DEBUG, "w3m_clear()\n");

	for (int i = 0; i < MAX_IMAGE; i++)
		img[i].already_drew = false;

	(void) img;
	(void) parm;
}

void sig_handler(int signo)
{
	extern volatile sig_atomic_t window_resized; /* global */

	if (signo == SIGWINCH)
		window_resized = true;
}

void set_terminal_size(struct tty_t *tty, int width, int height, int cols, int lines)
{
	tty->cell_width  = (width / cols);
	tty->cell_height = (height / lines);
	tty->width  = width;
	/* XXX: to avoid drawing at the bottom line,
		shrink display height a little. +7 pixel is just for me */
	tty->height = height - (tty->cell_height + 7);
}

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

bool get_tty(struct tty_t *tty)
{
	char ttyname[L_ctermid];

	/* get ttyname and open it */
	if (!ctermid(ttyname)
		|| (tty->fd = eopen(ttyname, O_RDWR)) < 0) {
		logging(ERROR, "couldn't open controlling terminal\n");
		return false;
	}
	logging(DEBUG, "ttyname:%s fd:%d\n", ttyname, tty->fd);

	return true;
}

bool check_terminal_size(struct tty_t *tty)
{
	int width, height, cols, lines;
	struct winsize ws;

	/* at first, we try to get pixel size from "struct winsize" */
	if (ioctl(tty->fd, TIOCGWINSZ, &ws)) {
		logging(ERROR, "ioctl: TIOCGWINSZ failed\n");
	} else if (ws.ws_xpixel == 0 || ws.ws_ypixel == 0) {
		logging(ERROR, "struct winsize has no pixel information\n");
	} else {
		set_terminal_size(tty, ws.ws_xpixel, ws.ws_ypixel, ws.ws_col, ws.ws_row);
		logging(DEBUG, "width:%d height%d cols:%d lines:%d\n",
			ws.ws_xpixel, ws.ws_ypixel, ws.ws_col, ws.ws_row);
		logging(DEBUG, "terminal size set by winsize\n");
		return true;
	}

	/* second, try dtterm sequence:
		CSI 14 t: request window size in pixels.
			-> responce: CSI 4 ; height ; width t
		CSI 18 t: request text area size in characters
			-> responce: CSI 8 ; height ; width t
	*/
	/* this function causes I/O block... */
	if (terminal_query(tty->fd, &height, &width, "\033[14t", "\033[4;%d;%dt")
		&& terminal_query(tty->fd, &lines, &cols, "\033[18t", "\033[8;%d;%dt")) {
		set_terminal_size(tty, width, height, cols, lines);
		logging(DEBUG, "width:%d height%d cols:%d lines:%d\n", width, height, cols, lines);
		logging(DEBUG, "terminal size set by dtterm sequence\n");
		return true;
	} else {
		logging(ERROR, "no responce for dtterm sequence\n");
	}

	/* finally, use default value */
	set_terminal_size(tty, TERM_WIDTH, TERM_HEIGHT,
		TERM_WIDTH / CELL_WIDTH, TERM_HEIGHT / CELL_HEIGHT);
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
	struct tty_t tty;
	struct image img[MAX_IMAGE];
	struct parm_t parm;
	struct winsize ws;

	if (freopen(instance_log, "a", stderr) == NULL)
		logging(ERROR, "freopen (stderr to %s) faild\n", instance_log);

	logging(DEBUG, "--- new instance ---\n");
	for (i = 0; i < argc; i++)
		logging(DEBUG, "argv[%d]:%s\n", i, argv[i]);
	logging(DEBUG, "argc:%d\n", argc);

	/* init */
	for (i = 0; i < MAX_IMAGE; i++)
		init_image(&img[i]);

	/* register signal handler for window resize event */
	struct sigaction sigact = {
		.sa_handler = sig_handler,
		.sa_flags   = SA_RESTART,
	};
	esigaction(SIGWINCH, &sigact, NULL);

	if (!get_tty(&tty))

		goto release;
	/* FIXME: when w3m uses pipe(), read() in terminal_query() causes I/O block...
		w3mimg [-test|-size] -> ok, not blocked
		echo 0;1;4;92;183;64;0;0;183;64;/path/to/image.png | w3mimg -> blocked! */
	if (QUERY_WINDOW_SIZE) {
		if (!check_terminal_size(&tty))
			goto release;
	} else {
		if (ioctl(tty.fd, TIOCGWINSZ, &ws)) {
			logging(ERROR, "ioctl: TIOCGWINSZ failed\n");
			set_terminal_size(&tty, TERM_WIDTH, TERM_HEIGHT,
				CELL_WIDTH, CELL_HEIGHT);
		} else {
			set_terminal_size(&tty, CELL_WIDTH * ws.ws_col,
				CELL_HEIGHT * ws.ws_row, ws.ws_col, ws.ws_row);
		}
	}
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
			w3m_getsize(&tty, &img[0], argv[optind]);
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
	if (freopen(log_file, "w", stderr) == NULL)
		logging(ERROR, "freopen (stderr to %s) faild\n", log_file);

	setvbuf(stderr, NULL, _IONBF, 0);
	setvbuf(stdout, NULL, _IONBF, 0);

	/* main loop */
    while (fgets(buf, BUFSIZE, stdin) != NULL) {
		if (window_resized) {
			window_resized = false;
			if (ioctl(tty.fd, TIOCGWINSZ, &ws)) {
				logging(ERROR, "ioctl: TIOCGWINSZ failed\n");
				set_terminal_size(&tty, TERM_WIDTH, TERM_HEIGHT,
					CELL_WIDTH, CELL_HEIGHT);
			} else {
				set_terminal_size(&tty, CELL_WIDTH * ws.ws_col,
					CELL_HEIGHT * ws.ws_row, ws.ws_col, ws.ws_row);
			}
			logging(DEBUG, "window resized!\n");
			logging(DEBUG, "terminal size width:%d height:%d cell_width:%d cell_height:%d\n",
				tty.width, tty.height, tty.cell_width, tty.cell_height);
		}

		if ((cp = strchr(buf, '\n')) != NULL)
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
				w3m_draw(&tty, img, &parm, op);
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
				w3m_getsize(&tty, &img[0], parm.argv[1]);
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

	fflush(stdout);
	fflush(stderr);

	efclose(stderr);

	if (tty.fd > 0)
		eclose(tty.fd);

	logging(DEBUG, "exiting...\n");
	return EXIT_SUCCESS;
}
