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
#include <unistd.h>

enum {
	VERBOSE   = false,
	BUFSIZE   = 1024,
	MULTIPLER = 1024,
};

/* error functions */
enum loglevel {
	DEBUG = 0,
	WARN,
	ERROR,
	FATAL,
};

void logging(int loglevel, char *format, ...)
{
	va_list arg;

	static const char *loglevel2str[] = {
		[DEBUG] = "DEBUG",
		[WARN]  = "WARN",
		[ERROR] = "ERROR",
		[FATAL] = "FATAL",
	};

	if (loglevel == DEBUG && !VERBOSE)
		return;

	fprintf(stderr, ">>%s<<\t", loglevel2str[loglevel]);
	va_start(arg, format);
	vfprintf(stderr, format, arg);
	va_end(arg);
}

/* wrapper of C functions */
int eopen(const char *path, int flag)
{
	int fd;
	errno = 0;

	if ((fd = open(path, flag)) < 0) {
		logging(ERROR, "couldn't open \"%s\"\n", path);
		logging(ERROR, "open: %s\n", strerror(errno));
	}
	return fd;
}

int eclose(int fd)
{
	int ret = 0;
	errno = 0;

	if ((ret = close(fd)) < 0)
		logging(ERROR, "close: %s\n", strerror(errno));

	return ret;
}

FILE *efopen(const char *path, char *mode)
{
	FILE *fp;
	errno = 0;

	if ((fp = fopen(path, mode)) == NULL) {
		logging(ERROR, "cannot open \"%s\"\n", path);
		logging(ERROR, "fopen: %s\n", strerror(errno));
	}
	return fp;
}

int efclose(FILE *fp)
{
	int ret;
	errno = 0;

	if ((ret = fclose(fp)) < 0)
		logging(ERROR, "fclose: %s\n", strerror(errno));

	return ret;
}

void *ecalloc(size_t nmemb, size_t size)
{
	void *ptr;
	errno = 0;

	if ((ptr = calloc(nmemb, size)) == NULL)
		logging(ERROR, "calloc: %s\n", strerror(errno));

	return ptr;
}

long int estrtol(const char *nptr, char **endptr, int base)
{
	long int ret;
	errno = 0;

	ret = strtol(nptr, endptr, base);
	if (ret == LONG_MIN || ret == LONG_MAX) {
		logging(ERROR, "strtol: %s\n", strerror(errno));
		return 0;
	}

	return ret;
}

int emkstemp(char *template)
{
	int ret;
	errno = 0;

	if ((ret = mkstemp(template)) < 0) {
		logging(ERROR, "couldn't open \"%s\"\n", template);
		logging(ERROR, "mkstemp: %s\n", strerror(errno));
	}

	return ret;
}

/* some useful functions */
int str2num(char *str)
{
	if (str == NULL)
		return 0;

	return estrtol(str, NULL, 10);
}

/* image loading functions */
/* for png */
#include "../lodepng.h"

/* for gif/bmp/(ico not supported) */
#include "../libnsgif.h"
#include "../libnsbmp.h"

#define STB_IMAGE_IMPLEMENTATION
/* to remove math.h dependency */
#define STBI_NO_HDR
#include "../stb_image.h"

enum {
	CHECK_HEADER_SIZE = 8,
	BYTES_PER_PIXEL   = 4,
	PNG_HEADER_SIZE   = 8,
	MAX_FRAME_NUM     = 128, /* limit of gif frames */
};

enum filetype_t {
	TYPE_JPEG,
	TYPE_PNG,
	TYPE_BMP,
	TYPE_GIF,
	TYPE_PNM,
	TYPE_UNKNOWN,
};

struct image {
	/* normally use data[0], data[n] (n > 1) for animanion gif */
	uint8_t *data[MAX_FRAME_NUM];
	int width;
	int height;
	int channel;
	bool alpha;
	/* for animation gif */
	int delay[MAX_FRAME_NUM];
	int frame_count; /* normally 1 */
	int loop_count;
	int current_frame; /* for yaimgfb */
};

unsigned char *file_into_memory(FILE *fp, size_t *data_size)
{
	unsigned char *buffer;
	size_t n, size;

	fseek(fp, 0L, SEEK_END);
	size = ftell(fp);

	if ((buffer = ecalloc(1, size)) == NULL)
		return NULL;

	fseek(fp, 0L, SEEK_SET);
	if ((n = fread(buffer, 1, size, fp)) != size) {
		free(buffer);
		return NULL;
	}
	*data_size = size;

	return buffer;
}

bool load_jpeg(FILE *fp, struct image *img)
{
	if ((img->data[0] = (uint8_t *) stbi_load_from_file(fp, &img->width, &img->height, &img->channel, 3)) == NULL)
		return false;

	return true;
}

bool load_png(FILE *fp, struct image *img)
{
	unsigned char *mem;
	size_t size;

	if ((mem = file_into_memory(fp, &size)) == NULL)
		return false;

	if (lodepng_decode24(&img->data[0], (unsigned *) &img->width, (unsigned *) &img->height, mem, size) != 0) {
		free(mem);
		return false;
	}

	img->channel = 3;
	free(mem);
	return true;
}

/* libns{gif,bmp} functions */
void *gif_bitmap_create(int width, int height)
{
	return calloc(width * height, BYTES_PER_PIXEL);
}

void gif_bitmap_set_opaque(void *bitmap, bool opaque)
{
	(void) opaque; /* unused */
	(void) bitmap;
}

bool gif_bitmap_test_opaque(void *bitmap)
{
	(void) bitmap; /* unused */
	return false;
}

unsigned char *gif_bitmap_get_buffer(void *bitmap)
{
	return bitmap;
}

void gif_bitmap_destroy(void *bitmap)
{
	free(bitmap);
}

void gif_bitmap_modified(void *bitmap)
{
	(void) bitmap; /* unused */
	return;
}

bool load_gif(FILE *fp, struct image *img)
{
	gif_bitmap_callback_vt gif_callbacks = {
		gif_bitmap_create,
		gif_bitmap_destroy,
		gif_bitmap_get_buffer,
		gif_bitmap_set_opaque,
		gif_bitmap_test_opaque,
		gif_bitmap_modified
	};
	size_t size;
	gif_result code;
	unsigned char *mem;
	gif_animation gif;
	int i;

	gif_create(&gif, &gif_callbacks);
	if ((mem = file_into_memory(fp, &size)) == NULL)
		return false;

	code = gif_initialise(&gif, size, mem);
	if (code != GIF_OK && code != GIF_WORKING)
		goto error_initialize_failed;

	img->width   = gif.width;
	img->height  = gif.height;
	img->channel = BYTES_PER_PIXEL; /* libnsgif always return 4bpp image */
	size = img->width * img->height * img->channel;

	/* read animation gif */
	img->frame_count = (gif.frame_count < MAX_FRAME_NUM) ? gif.frame_count: MAX_FRAME_NUM - 1;
	img->loop_count = gif.loop_count;

	for (i = 0; i < img->frame_count; i++) {
		code = gif_decode_frame(&gif, i);
		if (code != GIF_OK)
			goto error_decode_failed;

		if ((img->data[i] = (uint8_t *) ecalloc(1, size)) == NULL)
			goto error_decode_failed;
		memcpy(img->data[i], gif.frame_image, size);

		img->delay[i] = gif.frames[i].frame_delay;
	}

	gif_finalise(&gif);
	free(mem);
	return true;

error_decode_failed:
	img->frame_count = i;
	for (i = 0; i < img->frame_count; i++) {
		free(img->data[i]);
		img->data[i] = NULL;
	}
	gif_finalise(&gif);
error_initialize_failed:
	free(mem);
	return false;
}

void *bmp_bitmap_create(int width, int height, unsigned int state)
{
	(void) state; /* unused */
	return calloc(width * height, BYTES_PER_PIXEL);
}

unsigned char *bmp_bitmap_get_buffer(void *bitmap)
{
	return bitmap;
}

void bmp_bitmap_destroy(void *bitmap)
{
	free(bitmap);
}

size_t bmp_bitmap_get_bpp(void *bitmap)
{
	(void) bitmap; /* unused */
	return BYTES_PER_PIXEL;
}

bool load_bmp(FILE *fp, struct image *img)
{
	bmp_bitmap_callback_vt bmp_callbacks = {
		bmp_bitmap_create,
		bmp_bitmap_destroy,
		bmp_bitmap_get_buffer,
		bmp_bitmap_get_bpp
	};
	bmp_result code;
	size_t size;
	unsigned char *mem;
	bmp_image bmp;

	bmp_create(&bmp, &bmp_callbacks);
	if ((mem = file_into_memory(fp, &size)) == NULL)
		return false;

	code = bmp_analyse(&bmp, size, mem);
	if (code != BMP_OK)
		goto error_analyse_failed;

	code = bmp_decode(&bmp);
	if (code != BMP_OK)
		goto error_decode_failed;

	img->width   = bmp.width;
	img->height  = bmp.height;
	img->channel = BYTES_PER_PIXEL; /* libnsbmp always return 4bpp image */

	size = img->width * img->height * img->channel;
	if ((img->data[0] = (uint8_t *) ecalloc(1, size)) == NULL)
		goto error_decode_failed;
	memcpy(img->data[0], bmp.bitmap, size);

	bmp_finalise(&bmp);
	free(mem);
	return true;

error_decode_failed:
	bmp_finalise(&bmp);
error_analyse_failed:
	free(mem);
	return false;
}

/* pnm functions */
inline int getint(FILE *fp)
{
	int c, n = 0;

	do {
		c = fgetc(fp);
	} while (isspace(c));

	while (isdigit(c)) {
		n = n * 10 + c - '0';
		c = fgetc(fp);
	}
	return n;
}

inline uint8_t pnm_normalize(int c, int type, int max_value)
{
	if (type == 1 || type == 4)
		return (c == 0) ? 0: 0xFF;
	else
		return 0xFF * c / max_value;
}

bool load_pnm(FILE *fp, struct image *img)
{
	int size, type, c, count, max_value = 0;

	if (fgetc(fp) != 'P')
		return false;

	type = fgetc(fp) - '0';
	img->channel = (type == 1 || type == 2 || type == 4 || type == 5) ? 1:
		(type == 3 || type == 6) ? 3: -1;

	if (img->channel == -1)
		return false;

	/* read header */
	while ((c = fgetc(fp)) != EOF) {
		if (c == '#')
			while ((c = fgetc(fp)) != '\n');
		
		if (isspace(c))
			continue;

		if (isdigit(c)) {
			ungetc(c, fp);
			img->width  = getint(fp);
			img->height = getint(fp);
			if (type != 1 && type != 4)
				max_value = getint(fp);
			break;
		}
	}

	size = img->width * img->height * img->channel;
	if ((img->data[0] = ecalloc(1, size)) == NULL)
		return false;

	/* read data */
	count = 0;
	if (1 <= type && type <= 3) {
		while ((c = fgetc(fp)) != EOF) {
			if (c == '#')
				while ((c = fgetc(fp)) != '\n');
			
			if (isspace(c))
				continue;

			if (isdigit(c)) {
				ungetc(c, fp);
				*(img->data[0] + count++) = pnm_normalize(getint(fp), type, max_value);
			}
		}
	}
	else {
		while ((c = fgetc(fp)) != EOF)
			*(img->data[0] + count++) = pnm_normalize(c, type, max_value);
	}

	return true;
}

void init_image(struct image *img)
{
	for (int i = 0; i < MAX_FRAME_NUM; i++) {
		img->data[i] = NULL;
		img->delay[i] = 0;
	}
	img->width   = 0;
	img->height  = 0;
	img->channel = 0;
	img->alpha   = false;
	/* for animation gif */
	img->frame_count   = 1;
	img->loop_count    = 0;
	img->current_frame = 0;
}

void free_image(struct image *img)
{
	for (int i = 0; i < img->frame_count; i++) {
		free(img->data[i]);
		img->data[i] = NULL;
	}
}

enum filetype_t check_filetype(FILE *fp)
{
	/*
		JPEG(JFIF): FF D8
		PNG       : 89 50 4E 47 0D 0A 1A 0A (0x89 'P' 'N' 'G' '\r' '\n' 0x1A '\n')
		GIF       : 47 49 46 (ASCII 'G' 'I' 'F')
		BMP       : 42 4D (ASCII 'B' 'M')
		PNM       : 50 [31|32|33|34|35|36] ('P' ['1' - '6'])
	*/
	uint8_t header[CHECK_HEADER_SIZE];
	static uint8_t jpeg_header[] = {0xFF, 0xD8};
	static uint8_t png_header[]  = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
	static uint8_t gif_header[]  = {0x47, 0x49, 0x46};
	static uint8_t bmp_header[]  = {0x42, 0x4D};
	size_t size;

	if ((size = fread(header, 1, CHECK_HEADER_SIZE, fp)) != CHECK_HEADER_SIZE) {
		logging(ERROR, "couldn't read header\n");
		return TYPE_UNKNOWN;
	}
	fseek(fp, 0L, SEEK_SET);

	if (memcmp(header, jpeg_header, 2) == 0)
		return TYPE_JPEG;
	else if (memcmp(header, png_header, 8) == 0)
		return TYPE_PNG;
	else if (memcmp(header, gif_header, 3) == 0)
		return TYPE_GIF;
	else if (memcmp(header, bmp_header, 2) == 0)
		return TYPE_BMP;
	else if (header[0] == 'P' && ('0' <= header[1] && header[1] <= '6'))
		return TYPE_PNM;
	else
		return TYPE_UNKNOWN;
}

bool load_image(const char *file, struct image *img)
{
	int i;
	enum filetype_t type;
	FILE *fp;

	static bool (*loader[])(FILE *fp, struct image *img) = {
		[TYPE_JPEG] = load_jpeg,
		[TYPE_PNG]  = load_png,
		[TYPE_GIF]  = load_gif,
		[TYPE_BMP]  = load_bmp,
		[TYPE_PNM]  = load_pnm,
	};

	if ((fp = efopen(file, "r")) == NULL)
		return false;
 
 	if ((type = check_filetype(fp)) == TYPE_UNKNOWN) {
		logging(ERROR, "unknown file type: %s\n", file);
		goto image_load_error;
	}

	if (loader[type](fp, img)) {
		img->alpha = (img->channel == 2 || img->channel == 4) ? true: false;
		logging(DEBUG, "image width:%d height:%d channel:%d alpha:%s\n",
			img->width, img->height, img->channel, (img->alpha) ? "true": "false");
		if (img->frame_count > 1) {
			logging(DEBUG, "frame:%d loop:%d\n", img->frame_count, img->loop_count);
			for (i = 0; i < img->frame_count; i++)
				logging(DEBUG, "delay[%u]:%u\n", i, img->delay[i]);
		}
		efclose(fp);
		return true;
	}

image_load_error:
	logging(ERROR, "image load error: %s\n", file);
	//init_image(img);
	efclose(fp);
	return false;
}

/* inline functions for accessing member of struct image:
	never access member of struct image directly */
static inline int get_frame_count(struct image *img)
{
	return img->frame_count;
}

static inline uint8_t *get_current_frame(struct image *img)
{
	return img->data[img->current_frame];
}

static inline int get_current_delay(struct image *img)
{
	return img->delay[img->current_frame];
}

static inline void increment_frame(struct image *img)
{
	img->current_frame = (img->current_frame + 1) % img->frame_count;
}

static inline int get_image_width(struct image *img)
{
	return img->width;
}

static inline int get_image_height(struct image *img)
{
	return img->height;
}

static inline int get_image_channel(struct image *img)
{
	return img->channel;
}

/* image proccessing functions:
	never use *_single functions directly */
static inline void get_rgb(struct image *img, uint8_t *data, int x, int y, uint8_t *r, uint8_t *g, uint8_t *b)
{
	uint8_t *ptr;

	ptr = data + img->channel * (y * img->width + x);

	if (img->channel <= 2) { /* grayscale (+ alpha) */
		*r = *g = *b = *ptr;
	} else {                 /* rgb (+ alpha) */
		*r = *ptr; *g = *(ptr + 1); *b = *(ptr + 2);
	}
}

static inline void get_average(struct image *img, uint8_t *data, int x_from, int y_from, int x_to, int y_to, uint8_t pixel[])
{
	int cell_num;
	uint8_t r, g, b;
	uint16_t rsum, gsum, bsum;

	rsum = gsum = bsum = 0;
	for (int y = y_from; y < y_to; y++) {
		for (int x = x_from; x < x_to; x++) {
			get_rgb(img, data, x, y, &r, &g, &b);
			rsum += r; gsum += g; bsum += b;
		}
	}

	cell_num = (y_to - y_from) * (x_to - x_from);
	if (cell_num > 1) {
		rsum /= cell_num; gsum /= cell_num; bsum /= cell_num;
	}

	if (img->channel <= 2)
		*pixel++ = rsum;
	else {
		*pixel++ = rsum; *pixel++ = gsum; *pixel++ = bsum;
	}

	if (img->alpha)
		*pixel = 0;
}

uint8_t *rotate_image_single(struct image *img, uint8_t *data, int angle)
{
	int x1, x2, y1, y2, r, dst_width, dst_height;
	uint8_t *rotated_data;
	long offset_dst, offset_src;

	static const int cos[3] = {0, -1,  0};
	static const int sin[3] = {1,  0, -1};

	int shift[3][3] = {
	/*   x_shift,         y_shift,        sign */
		{img->height - 1, 0              , -1},
		{img->width  - 1, img->height - 1,  1},
		{              0, img->width  - 1, -1}
	};

	if (angle != 90 && angle != 180 && angle != 270)
		return NULL;
	/* r == 0: clockwise        : (angle 90)  */
	/* r == 1: upside down      : (angle 180) */
	/* r == 2: counter clockwise: (angle 270) */
	r = angle / 90 - 1;
	
	if (angle == 90 || angle == 270) {
		dst_width  = img->height;
		dst_height = img->width;
	} else {
		dst_width  = img->width;
		dst_height = img->height;
	}

	if ((rotated_data = (uint8_t *) ecalloc(dst_width * dst_height, img->channel)) == NULL)
		return NULL;

	logging(DEBUG, "rotated image: %dx%d size:%d\n",
		dst_width, dst_height, dst_width * dst_height * img->channel);

	for (y2 = 0; y2 < dst_height; y2++) {
		for (x2 = 0; x2 < dst_width; x2++) {
			x1 = ((x2 - shift[r][0]) * cos[r] - (y2 - shift[r][1]) * sin[r]) * shift[r][2];
			y1 = ((x2 - shift[r][0]) * sin[r] + (y2 - shift[r][1]) * cos[r]) * shift[r][2];
			offset_src = img->channel * (y1 * img->width + x1);
			offset_dst = img->channel * (y2 * dst_width + x2);
			memcpy(rotated_data + offset_dst, data + offset_src, img->channel);
		}
	}
	free(data);

	img->width  = dst_width;
	img->height = dst_height;

	return rotated_data;
}

void rotate_image(struct image *img, int angle, bool rotate_all)
{
	uint8_t *rotated_data;

	if (rotate_all) {
		for (int i = 0; i < img->frame_count; i++)
			if ((rotated_data = rotate_image_single(img, img->data[i], angle)) != NULL)
				img->data[i] = rotated_data;
	} else {
		if ((rotated_data = rotate_image_single(img, img->data[img->current_frame], angle)) != NULL)
			img->data[img->current_frame] = rotated_data;
	}
}

uint8_t *resize_image_single(struct image *img, uint8_t *data, int disp_width, int disp_height)
{
	/* TODO: support enlarge */
	int width_rate, height_rate, resize_rate;
	int dst_width, dst_height, y_from, x_from, y_to, x_to;
	uint8_t *resized_data, pixel[img->channel];
	long offset_dst;

	width_rate  = MULTIPLER * disp_width  / img->width;
	height_rate = MULTIPLER * disp_height / img->height;
	resize_rate = (width_rate < height_rate) ? width_rate: height_rate;

	logging(DEBUG, "width_rate:%.2d height_rate:%.2d resize_rate:%.2d\n",
		width_rate, height_rate, resize_rate);

	/* only support shrink */
	if ((resize_rate / MULTIPLER) >= 1)
		return NULL;

	/* FIXME: let the same num (img->width == fb->width), if it causes SEGV, remove "+ 1" */
	dst_width  = resize_rate * img->width / MULTIPLER + 1;
	dst_height = resize_rate * img->height / MULTIPLER;

	if ((resized_data = (uint8_t *) ecalloc(dst_width * dst_height, img->channel)) == NULL)
		return NULL;

	logging(DEBUG, "resized image: %dx%d size:%d\n",
		dst_width, dst_height, dst_width * dst_height * img->channel);

	for (int y = 0; y < dst_height; y++) {
		y_from = MULTIPLER * y / resize_rate;
		y_to   = MULTIPLER * (y + 1) / resize_rate;
		for (int x = 0; x < dst_width; x++) {
			x_from = MULTIPLER * x / resize_rate;
			x_to   = MULTIPLER * (x + 1) / resize_rate;
			get_average(img, data, x_from, y_from, x_to, y_to, pixel);
			offset_dst = img->channel * (y * dst_width + x);
			memcpy(resized_data + offset_dst, pixel, img->channel);
		}
	}
	free(data);

	img->width  = dst_width;
	img->height = dst_height;

	return resized_data;
}

void resize_image(struct image *img, int disp_width, int disp_height, bool resize_all)
{
	uint8_t *resized_data;

	if (resize_all) {
		for (int i = 0; i < img->frame_count; i++)
			if ((resized_data = resize_image_single(img, img->data[i], disp_width, disp_height)) != NULL)
				img->data[i] = resized_data;
	} else {
		if ((resized_data = resize_image_single(img, img->data[img->current_frame], disp_width, disp_height)) != NULL)
			img->data[img->current_frame] = resized_data;
	}
}

uint8_t *normalize_bpp_single(struct image *img, uint8_t *data, int bytes_per_pixel)
{
	uint8_t *normalized_data, *src, *dst, r, g, b;

	if ((normalized_data = (uint8_t *)
		ecalloc(img->width * img->height, bytes_per_pixel)) == NULL)
		return NULL;

	if (img->channel <= 2) { /* grayscale (+ alpha) */
		for (int y = 0; y < img->height; y++) {
			for (int x = 0; x < img->width; x++) {
				src = data + img->channel * (y * img->width + x);
				dst = normalized_data + bytes_per_pixel * (y * img->width + x);
				*dst = *src; *(dst + 1) = *src; *(dst + 2) = *src;
			}
		}
	} else {                 /* rgb (+ alpha) */
		for (int y = 0; y < img->height; y++) {
			for (int x = 0; x < img->width; x++) {
				get_rgb(img, data, x, y, &r, &g, &b);
				dst = normalized_data + bytes_per_pixel * (y * img->width + x);
				*dst = r; *(dst + 1) = g; *(dst + 2) = b;
			}
		}
	}
	free(data);

	return normalized_data;
}

void normalize_bpp(struct image *img, int bytes_per_pixel, bool normalize_all)
{
	uint8_t *normalized_data;

	/* XXX: now only support bytes_per_pixel == 3 */
	if (bytes_per_pixel != 3)
		return;

	if (normalize_all) {
		for (int i = 0; i < img->frame_count; i++)
			if ((normalized_data = normalize_bpp_single(img, img->data[i], bytes_per_pixel)) != NULL)
				img->data[i] = normalized_data;
	} else {
		if ((normalized_data = normalize_bpp_single(img, img->data[img->current_frame], bytes_per_pixel)) != NULL)
			img->data[img->current_frame] = normalized_data;
	}
}

/* main functions */
#include "sixel.h"

char temp_file[BUFSIZE];

enum {
	SIXEL_COLORS = 256,
	SIXEL_BPP    = 3,
	TERM_WIDTH   = 1280,
	TERM_HEIGHT  = 1024,
};

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
		);
}

void remove_temp_file()
{
	extern char temp_file[BUFSIZE]; /* global */
	remove(temp_file);
}

char *make_temp_file(const char *template)
{
	extern char temp_file[BUFSIZE]; /* global */
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

void cleanup(sixel_dither_t *sixel_dither, sixel_output_t *sixel_context, struct image *img)
{
	if (sixel_dither)
		sixel_dither_unref(sixel_dither);
	if (sixel_context)
		sixel_output_unref(sixel_context);
	free_image(img);
}

int main(int argc, char **argv)
{
	const char *template = "sdump.XXXXXX";
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
		file = make_temp_file(template);

	if (file == NULL) {
		logging(FATAL, "input file not found\n");
		usage();
		return EXIT_FAILURE;
	}

	/* init */
	init_image(&img);

	if (load_image(file, &img) == false) {
		logging(FATAL, "couldn't load image\n");
		return EXIT_FAILURE;
	}

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
		goto error_occured;
	}

	/* XXX: use first frame for dither initialize */
	if (sixel_dither_initialize(sixel_dither, get_current_frame(&img), get_image_width(&img), get_image_height(&img),
		SIXEL_BPP, LARGE_AUTO, REP_AUTO, QUALITY_AUTO) != 0) {
		logging(ERROR, "couldn't initialize dither\n");
		sixel_dither_unref(sixel_dither);
		goto error_occured;
	}
	sixel_dither_set_diffusion_type(sixel_dither, DIFFUSE_AUTO);

	if ((sixel_context = sixel_output_create(sixel_write_callback, stdout)) == NULL) {
		logging(ERROR, "couldn't create sixel context\n");
		goto error_occured;
	}
	sixel_output_set_8bit_availability(sixel_context, CSIZE_7BIT);

	printf("\0337"); /* save cursor position */
	for (int i = 0; i < get_frame_count(&img); i++) {
		printf("\0338"); /* restore cursor position */
		sixel_encode(get_current_frame(&img), get_image_width(&img), get_image_height(&img), get_image_channel(&img), sixel_dither, sixel_context);
		usleep(get_current_delay(&img) * 10000); /* gif delay 1 == 1/100 sec */
		increment_frame(&img);
	}

	/* cleanup resource */
	cleanup(sixel_dither, sixel_context, &img);
	return EXIT_SUCCESS;

error_occured:
	cleanup(sixel_dither, sixel_context, &img);
	return EXIT_FAILURE;;
}
