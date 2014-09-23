/* See LICENSE for licence details. */
#include "w3mimg_sixel.h"
#include "../util.h"
#include "../loader.h"
#include "../image.h"
#include "parsearg.h"
#include "sixel.h"

enum {
	TERM_WIDTH   = 1280,
	TERM_HEIGHT  = 1024,
};

static const char *instance_log = "/tmp/w3mimg_sixel.instance.log";
static const char *log_file = "/tmp/w3mimg_sixel.log";

void w3m_draw(struct sixel_t *sixel, struct image imgs[], struct parm_t *parm, int op)
{
	int index, offset_x, offset_y, width, height, shift_x, shift_y, view_w, view_h;
	char *file;
	struct image *img;

	logging(DEBUG, "w3m_%s()\n", (op == W3M_DRAW) ? "draw": "redraw");

	if (parm->argc != 11 || op == W3M_REDRAW)
	//if (parm->argc != 11)
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
		resize_image(img, width, height, true);

	//draw_image(fb, img, offset_x, offset_y, shift_x, shift_y,
		//(view_w ? view_w: width), (view_h ? view_h: height), false);

	/* sixel */
	char *sixel_file;
	int length;
	FILE *fp;

	length = strlen(file) + 7; /* file + ".sixel" + '\0' */
	if ((sixel_file = ecalloc(length, sizeof(char))) == NULL) 
		return;

	snprintf(sixel_file, length, "%s%s", file, ".sixel");
	logging(DEBUG, "sixel_file: %s\n", sixel_file);

	logging(DEBUG, "create new file\n");
	if ((fp = efopen(sixel_file, "w")) == NULL) {
		free(sixel_file);
		return;
	}
	sixel_init(sixel, img, fp);
	sixel_output(sixel, img);
	sixel_die(sixel);

	efclose(fp);
	free(sixel_file);
	/* drawing */
	/*
	int in, out, size;
	char *tty, buf[BUFSIZE];
	//if ((tty = ttyname(STDIN_FILENO)) == NULL
	if ((out = eopen("/dev/stdout", O_WRONLY)) < 0
		|| (in = eopen(sixel_file, O_RDONLY)) < 0) {
		logging(ERROR, "file open failed: %d %d\n", out, in);
		return;
	}
	logging(DEBUG, "out: %d\n", out);

	while ((size = read(in, buf, BUFSIZE)) > 0)
		write(out, buf, size);
	close(out);
	close(in);
	snprintf(buf, BUFSIZE, "cat %s &", sixel_file);
	system(buf);
	free(sixel_file);
	*/
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

	if (load_image(file, img))
		printf("%d %d\n", get_image_width(img), get_image_height(img));
	else
		printf("0 0\n");
}

void w3m_clear(struct image img[], struct parm_t *parm)
{
	logging(DEBUG, "w3m_clear()\n");

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
	struct image img[MAX_IMAGE];
	struct parm_t parm;

	if (freopen(instance_log, "a", stderr) == NULL)
		logging(ERROR, "freopen (stderr to %s) faild\n", instance_log);

	logging(DEBUG, "--- new instance ---\n");
	for (i = 0; i < argc; i++)
		logging(DEBUG, "argv[%d]:%s\n", i, argv[i]);
	logging(DEBUG, "argc:%d\n", argc);

	/* init */
	for (i = 0; i < MAX_IMAGE; i++)
		init_image(&img[i]);
	
	sixel.dither = NULL;
	sixel.context = NULL;
	sixel.output = NULL;

	/* check args */
	optind = 1;
	while (optind < argc) {
		if (strncmp(argv[optind], "-test", 5) == 0) {
			printf("%d %d\n", TERM_WIDTH, TERM_HEIGHT);
			goto release;
		}
		else if (strncmp(argv[optind], "-size", 5) == 0 && ++optind < argc) {
			w3m_getsize(&img[0], argv[optind]);
			goto release;
		}
		optind++;
	}

	/* for avoiding deadlock, reopen stder again
		w3m --+------------------+----------------------------------------------> (end)
			  | fork()/pipe()    |fork()/popen()    |               |
			  |                  v                  v               v
			  |                  w3mimg -> (end)    w3mimg -> (end) w3mimg -> (end)
			  |                  (get image size)
			  |                  |
			  |                  v
			  |                  w3mimg_instance.log
			  v
			  w3mimg -------------------------------------------------------------> end)
			  (drawing)
			  |
			  v
			  w3mimg_sixel.log (file lock)
		*/
	if (freopen(log_file, "w", stderr) == NULL)
		logging(ERROR, "freopen (stderr to %s) faild\n", log_file);
	file_lock(stderr);

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
				w3m_draw(&sixel, img, &parm, op);
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
	logging(DEBUG, "exiting...\n");

	fflush(stdout);
	fflush(stderr);

	file_unlock(stderr);
	efclose(stderr);

	return EXIT_SUCCESS;
}
