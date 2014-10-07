/* See LICENSE for licence details. */
#include "sdump.h"
#include "util.h"
#include "loader.h"
#include "image.h"
#include "sixel_util.h"

void usage()
{
	printf("usage:\n"
		"\tsdump [-h] [-f] [-r angle] image\n"
		"\tcat image | sdump\n"
		"\twget -O - image_url | sdump\n"
		"options:\n"
		"\t-h: show this help\n"
		"\t-f: fit image to display\n"
		"\t-r: rotate image (90/180/270)\n"
		"\t-p: penetrate gnu screen\n"
		);
}

void remove_temp_file()
{
	extern char temp_file[PATH_MAX]; /* global */
	remove(temp_file);
}

char *make_temp_file(const char *template)
{
	extern char temp_file[PATH_MAX]; /* global */
	int fd;
	ssize_t size, file_size = 0;
	char buf[BUFSIZE], *env;

	/* stdin is tty or not */
	if (isatty(STDIN_FILENO)) {
		logging(ERROR, "stdin is neither pipe nor redirect\n");
		return NULL;
	}

	/* prepare temp file */
	memset(temp_file, 0, BUFSIZE);
	if ((env = getenv("TMPDIR")) != NULL) {
		snprintf(temp_file, BUFSIZE, "%s/%s", env, template);
	} else {
		snprintf(temp_file, BUFSIZE, "/tmp/%s", template);
	}

	if ((fd = emkstemp(temp_file)) < 0)
		return NULL;
	logging(DEBUG, "tmp file:%s\n", temp_file);

	/* register cleanup function */
	if (atexit(remove_temp_file))
		logging(ERROR, "atexit() failed\nmaybe temporary file remains...\n");

	/* read data */
	while ((size = read(STDIN_FILENO, buf, BUFSIZE)) > 0) {
		write(fd, buf, size);
		file_size += size;
	}
	eclose(fd);

	if (file_size == 0) {
		logging(ERROR, "stdin is empty\n");
		return NULL;
	}

	return temp_file;
}

void cleanup(struct sixel_t *sixel, struct image *img)
{
	sixel_die(sixel);
	free_image(img);
}

int main(int argc, char **argv)
{
	const char *template = "sdump.XXXXXX";
	char *file;
	int angle = 0, opt;
	bool resize = false, penetrate = false;
	struct winsize ws;
	struct image img;
	struct tty_t tty = {
		.fd = STDOUT_FILENO,
		.cell_width = CELL_WIDTH, .cell_height = CELL_HEIGHT,
	};
	struct sixel_t sixel = {
		.context = NULL, .dither = NULL,
	};

	/* check arg */
	while ((opt = getopt(argc, argv, "hfpr:")) != -1) {
		switch (opt) {
		case 'h':
			usage();
			return EXIT_SUCCESS;
		case 'f':
			resize = true;
			break;
		case 'p':
			penetrate = true;
			break;
		case 'r':
			angle = str2num(optarg);
			break;
		default:
			break;
		}
	}

	/* open file */
	if (optind < argc)
		file = argv[optind];
	else
		file = make_temp_file(template);

	if (file == NULL) {
		logging(FATAL, "input file not found\n");
		usage();
		return EXIT_FAILURE;
	}

	/* init */
	init_image(&img);

	FILE *fp;
	fp = efopen(file, "r");

	img.width   = 12;
	img.height  = 6;
	img.alpha   = false;
	img.channel = 2;
	img.data[0] = ecalloc(img.width * img.height, 2);

	fread(img.data[0], 1, img.width * img.height * 2, fp);

	/*
	if (load_image(file, &img) == false) {
		logging(FATAL, "couldn't load image\n");
		return EXIT_FAILURE;
	}
	*/

	if (ioctl(STDIN_FILENO, TIOCGWINSZ, &ws)) {
		logging(ERROR, "ioctl: TIOCGWINSZ failed\n");
		tty.width  = tty.cell_width * 80;
		tty.height = tty.cell_height * 24;
	} else {
		tty.width  = tty.cell_width * ws.ws_col;
		tty.height = tty.cell_height * ws.ws_row;
	}

	/* rotate/resize and draw */
	/* TODO: support color reduction for 8bpp mode */
	if (angle != 0)
		rotate_image(&img, angle, true);

	if (resize)
		resize_image(&img, tty.width, tty.height, true);

	/* sixel */
	if (!sixel_init(&tty, &sixel, &img, penetrate))
		goto error_occured;
	sixel_write(&tty, &sixel, &img, penetrate);

	/* XXX: screen never knows the height of sixel image,
		we must space manually... */
	char buf[BUFSIZE];
	int image_height, padding;
	if (penetrate) {
		image_height = get_image_height(&img);
		if ((image_height % CELL_HEIGHT) != 0)
			image_height += CELL_HEIGHT - (image_height % CELL_HEIGHT);
		padding = image_height / CELL_HEIGHT + 1;

		snprintf(buf, BUFSIZE, "\033[%dB", padding);
		ewrite(STDOUT_FILENO, buf, strlen(buf));
	}

	/* cleanup resource */
	cleanup(&sixel, &img);
	return EXIT_SUCCESS;

error_occured:
	cleanup(&sixel, &img);
	return EXIT_FAILURE;;
}
