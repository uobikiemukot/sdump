/* See LICENSE for licence details. */
#include "sdump.h"
#include "util.h"
#include "loader.h"
#include "image.h"

char temp_file[] = "/tmp/sdump.XXXXXX";

enum {
	SIXEL_COLORS = 256,
	SIXEL_BPP    = 3,
	TERM_WIDTH   = 1280,
	TERM_HEIGHT  = 1024,
};

void usage()
{
	printf("idump [-h] [-f] [-r angle] image\n"
		"-h: show this help\n"
		"-f: fit image to display\n"
		"-r: rotate image (90/180/270)\n"
		);
}

void remove_temp_file()
{
	extern char temp_file[]; /* global */
	remove(temp_file);
}

char *make_temp_file(char *template)
{
	int fd;
	ssize_t size, file_size = 0;
	char buf[BUFSIZE];
	errno = 0;

	if ((fd = mkstemp(template)) < 0) {
		perror("mkstemp");
		return NULL;
	}
	logging(DEBUG, "tmp file:%s\n", template);

	/* register cleanup function */
	if (atexit(remove_temp_file) != 0)
		logging(ERROR, "atexit() failed\nmaybe temporary file remains...\n");

	if (fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK) == -1)
		logging(ERROR, "couldn't set O_NONBLOCK flag\n");

	while ((size = read(STDIN_FILENO, buf, BUFSIZE)) > 0) {
		write(fd, buf, size);
		file_size += size;
	}
	eclose(fd);

	if (file_size == 0) {
		logging(ERROR, "stdin is empty\n");
		usage();
		return NULL;
	}

	return template;
}

int sixel_write_callback(char *data, int size, void *priv)
{
	/*
	for (int i = 0; i < size; i++) {
		if (i == (size - 1))
			break;

		if (*(data + i) == 0x1B && *(data + i + 1) == 0x5C) {
			fprintf((FILE *) priv, "\033\033\\\033P\\");
			break;
		} else {
			fwrite(data + i, 1, 1, fp);
		}
	}

	logging(DEBUG, "write callback() size:%d\n", size);
	return size;
	*/
	return fwrite(data, size, 1, (FILE *) priv);
}

int main(int argc, char **argv)
{
	extern char temp_file[]; /* global */
	char *file;
	bool resize = false;
	int angle = 0, opt;
	struct image img;
	sixel_output_t *sixel_context = NULL;
	sixel_dither_t *sixel_dither = NULL;

	/* check arg */
	while ((opt = getopt(argc, argv, "hfr:")) != -1) {
		switch (opt) {
		case 'h':
			usage();
			return EXIT_SUCCESS;
		case 'f':
			resize = true;
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
		file = make_temp_file(temp_file);

	if (file == NULL) {
		logging(FATAL, "input file not found\n");
		return EXIT_FAILURE;
	}

	/* init */
	init_image(&img);

	if (load_image(file, &img) == false)
		goto cleanup;

	/* rotate/resize and draw */
	/* TODO: support color reduction for 8bpp mode */
	if (angle != 0)
		rotate_image(&img, angle, true);

	if (resize)
		resize_image(&img, TERM_WIDTH, TERM_HEIGHT, true);

	/* sixel */
	/* XXX: libsixel only allows 3 bytes per pixel image,
		we should convert bpp when bpp is 1 or 2 or 4 */
	if (get_image_channel(&img) != SIXEL_BPP)
		normalize_bpp(&img, SIXEL_BPP, true);

	if ((sixel_dither = sixel_dither_create(SIXEL_COLORS)) == NULL) {
		logging(ERROR, "couldn't create dither\n");
		goto cleanup;
	}

	/* XXX: use first frame for dither initialize */
	if (sixel_dither_initialize(sixel_dither, get_current_frame(&img), get_image_width(&img), get_image_height(&img),
		SIXEL_BPP, LARGE_AUTO, REP_AUTO, QUALITY_AUTO) != 0) {
		logging(ERROR, "couldn't initialize dither\n");
		sixel_dither_unref(sixel_dither);
		goto cleanup;
	}
	sixel_dither_set_diffusion_type(sixel_dither, DIFFUSE_AUTO);

	if ((sixel_context = sixel_output_create(sixel_write_callback, stdout)) == NULL) {
		logging(ERROR, "couldn't create sixel context\n");
		goto cleanup;
	}
	sixel_output_set_8bit_availability(sixel_context, CSIZE_7BIT);

	//printf("\033[s"); /* save cursor position (SCO) */
	printf("\0337"); /* save cursor position */
	for (int i = 0; i < get_frame_count(&img); i++) {
		//printf("\033[u"); /* restore cursor position (SCO) */
		printf("\0338"); /* restore cursor position */
		sixel_encode(get_current_frame(&img), get_image_width(&img), get_image_height(&img), get_image_channel(&img), sixel_dither, sixel_context);
		usleep(get_current_delay(&img) * 10000); /* gif delay 1 == 1/100 sec */
		increment_frame(&img);
	}

	/* cleanup resource */
cleanup:
	if (sixel_dither)
		sixel_dither_unref(sixel_dither);
	if (sixel_context)
		sixel_output_unref(sixel_context);
	free_image(&img);

	return EXIT_SUCCESS;
}
