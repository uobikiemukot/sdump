/* See LICENSE for licence details. */

/* for jpeg */
#include <jpeglib.h>

/* for png */
#include <png.h>

/* for gif/bmp/(ico not supported) */
#include "libnsgif.h"
#include "libnsbmp.h"

//#define STB_IMAGE_IMPLEMENTATION
/* to remove math.h dependency */
//#define STBI_NO_HDR
//#include "stb_image.h"

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
	/* for w3mimg */
	int current_frame;
	bool already_drew;
};

/* libjpeg functions */
struct my_jpeg_error_mgr {
	struct jpeg_error_mgr pub;
	jmp_buf setjmp_buffer;
};

void my_jpeg_exit(j_common_ptr cinfo)
{
	char last_msg[JMSG_LENGTH_MAX];
	struct my_jpeg_error_mgr *myerr = (struct my_jpeg_error_mgr *) cinfo->err;

	(*cinfo->err->format_message)(cinfo, last_msg);
	logging(ERROR, "libjpeg: %s\n", last_msg);

	longjmp(myerr->setjmp_buffer, 1);
}

void my_jpeg_error(j_common_ptr cinfo)
{
	char last_msg[JMSG_LENGTH_MAX];

	(*cinfo->err->format_message)(cinfo, last_msg);
	logging(ERROR, "libjpeg: %s\n", last_msg);
}

void my_jpeg_warning(j_common_ptr cinfo, int msg_level)
{
	char last_msg[JMSG_LENGTH_MAX];

	(*cinfo->err->format_message)(cinfo, last_msg);
	if (msg_level < 0) { /* warning */
		if (cinfo->err->num_warnings == 0 || cinfo->err->trace_level >= 3)
			logging(WARN, "libjpeg: %s\n", last_msg);
		cinfo->err->num_warnings++;
	} else { /* trace */
		if (cinfo->err->trace_level >= msg_level)
			logging(WARN, "libjpeg: %s\n", last_msg);
	}
}

bool load_jpeg(FILE *fp, struct image *img)
{
	int row_stride, size;
	JSAMPARRAY buffer;
	struct jpeg_decompress_struct cinfo;
	struct my_jpeg_error_mgr jerr;

	cinfo.err = jpeg_std_error(&jerr.pub);
	jerr.pub.error_exit = my_jpeg_exit;
	jerr.pub.emit_message = my_jpeg_warning;
	jerr.pub.output_message = my_jpeg_error;

	if (setjmp(jerr.setjmp_buffer)) {
		jpeg_destroy_decompress(&cinfo);
		return false;
	}

	jpeg_create_decompress(&cinfo);
	jpeg_stdio_src(&cinfo, fp);
	jpeg_read_header(&cinfo, TRUE);

	/* disable colormap (indexed color), grayscale -> rgb */
	cinfo.quantize_colors = FALSE;
	//cinfo.out_color_space = JCS_RGB;
	jpeg_start_decompress(&cinfo);

	img->width   = cinfo.output_width;
	img->height  = cinfo.output_height;
	img->channel = cinfo.output_components;

	size = img->width * img->height * img->channel;
	if ((img->data[0] = (uint8_t *) ecalloc(1, size)) == NULL) {
		jpeg_finish_decompress(&cinfo);
		jpeg_destroy_decompress(&cinfo);
		return false;
	}

	row_stride = cinfo.output_width * cinfo.output_components;
	buffer = (*cinfo.mem->alloc_sarray)((j_common_ptr) &cinfo, JPOOL_IMAGE, row_stride, 1);

	while (cinfo.output_scanline < cinfo.output_height) {
		jpeg_read_scanlines(&cinfo, buffer, 1);
		memcpy(img->data[0] + (cinfo.output_scanline - 1) * row_stride, buffer[0], row_stride);
	}

	jpeg_finish_decompress(&cinfo);
	jpeg_destroy_decompress(&cinfo);

	return true;
}

/* libpng function */

void my_png_error(png_structp png_ptr, png_const_charp error_msg)
{
	logging(ERROR, "libpng: %s\n", error_msg);
	if (png_ptr)
		longjmp(png_jmpbuf(png_ptr), 1);
}

void my_png_warning(png_structp png_ptr, png_const_charp warning_msg)
{
	(void) png_ptr;
	logging(WARN, "libpng: %s\n", warning_msg);
}

bool load_png(FILE *fp, struct image *img)
{
	int row_stride, size;
	png_bytep *row_pointers = NULL;
	unsigned char header[PNG_HEADER_SIZE];
	png_structp png_ptr;
	png_infop info_ptr;

	if (fread(header, 1, PNG_HEADER_SIZE, fp) != PNG_HEADER_SIZE
		|| png_sig_cmp(header, 0, PNG_HEADER_SIZE)
		|| (png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, my_png_error, my_png_warning)) == NULL)
		return false;

	if ((info_ptr = png_create_info_struct(png_ptr)) == NULL) {
		png_destroy_read_struct(&png_ptr, (png_infopp) NULL, (png_infopp) NULL);
		return false;
	}

	if (setjmp(png_jmpbuf(png_ptr))) {
		png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
		return false;
	}

	png_init_io(png_ptr, fp);
	png_set_sig_bytes(png_ptr, PNG_HEADER_SIZE);
	/* force 3 bytes per pixel image
		-	strip alpha
		-	6 bytes per pixel -> 3 bytes per pixel
		-	1,2,4 bits per color -> 8 bits per color
		-	grayscale -> rgb
		-	perform set_expand() */
	/*
	png_read_png(png_ptr, info_ptr,
		PNG_TRANSFORM_STRIP_ALPHA | PNG_TRANSFORM_STRIP_16 |
		PNG_TRANSFORM_PACKING | PNG_TRANSFORM_GRAY_TO_RGB |
		PNG_TRANSFORM_EXPAND, NULL);
	*/
	png_read_png(png_ptr, info_ptr, PNG_TRANSFORM_PACKING | PNG_TRANSFORM_EXPAND, NULL);

	img->width   = png_get_image_width(png_ptr, info_ptr);
	img->height  = png_get_image_height(png_ptr, info_ptr);
	img->channel = png_get_channels(png_ptr, info_ptr);

	size = img->width * img->height * img->channel;
	if ((img->data[0] = (uint8_t *) ecalloc(1, size)) == NULL) {
		png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
		return false;
	}

	row_stride   = png_get_rowbytes(png_ptr, info_ptr);
	row_pointers = png_get_rows(png_ptr, info_ptr);

	for (int i = 0; i < img->height; i++)
		memcpy(img->data[0] + row_stride * i, row_pointers[i], row_stride);

	png_destroy_read_struct(&png_ptr, &info_ptr, NULL);

	return true;
}

/* libns{gif,bmp} functions */
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
static inline int getint(FILE *fp)
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

static inline uint8_t pnm_normalize(int c, int type, int max_value)
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
	img->already_drew  = false;
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
