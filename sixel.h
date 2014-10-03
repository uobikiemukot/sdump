/* See LICENSE for licence details. */
#include <sixel.h>

enum {
	SIXEL_COLORS = 256,
	SIXEL_BPP    = 3,
	/* gnu screen escape sequence buffer: 512?
		minus
			"\033P"  at the beginning: 2
			"\033\\" at the end      : 2 */
	SCREEN_BUF_LIMIT = 512 - 4,
};

struct sixel_t {
	sixel_output_t *context;
	sixel_dither_t *dither;
};

int sixel_callback_penetrate(char *data, int size, void *priv)
{
	struct tty_t *tty = (struct tty_t *) priv;
	char *ptr;
	ssize_t wsize, left;

	logging(DEBUG, "callback_penetrate() data size:%d\n", size);

	ptr  = data;
	left = size;

	while (ptr < (data + size)) {
		/* require libsixel r1228.74d7907 and feature-customize-dcs-envelope branch */
		ewrite(tty->fd, "\033P", 2);
		wsize = ewrite(tty->fd, ptr, (left > SCREEN_BUF_LIMIT) ? SCREEN_BUF_LIMIT: left);
		ewrite(tty->fd, "\033\\", 2);

		ptr  += wsize;
		left -= wsize;
	}

	return true;
}

int sixel_callback(char *data, int size, void *priv)
{
	struct tty_t *tty = (struct tty_t *) priv;
	char *ptr;
	ssize_t wsize, left;

	logging(DEBUG, "callback() data size:%d\n", size);

	ptr  = data;
	left = size;

	while (ptr < (data + size)) {
		wsize = ewrite(tty->fd, ptr, left);
		ptr  += wsize;
		left -= wsize;
	}

	return true;
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
	} else {				 /* rgb (+ alpha) */
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

bool sixel_init(struct tty_t *tty, struct sixel_t *sixel, struct image *img, bool enable_penetrate)
{
	int (*callback_func)(char *data, int size, void *priv);

	/* XXX: libsixel only allows 3 bytes per pixel image,
		we should convert bpp when bpp is 1 or 2 or 4 */
	if (get_image_channel(img) != SIXEL_BPP)
		normalize_bpp(img, SIXEL_BPP, true);

	if ((sixel->dither = sixel_dither_create(SIXEL_COLORS)) == NULL) {
		logging(ERROR, "couldn't create dither\n");
		return false;
	}

	/* XXX: use first frame for dither initialize */
	if (sixel_dither_initialize(sixel->dither, get_current_frame(img),
		get_image_width(img), get_image_height(img),
		SIXEL_BPP, LARGE_AUTO, REP_AUTO, QUALITY_AUTO) != 0) {
		logging(ERROR, "couldn't initialize dither\n");
		sixel_dither_unref(sixel->dither);
		return false;
	}
	sixel_dither_set_diffusion_type(sixel->dither, DIFFUSE_AUTO);

	callback_func = (enable_penetrate) ? sixel_callback_penetrate: sixel_callback;
	if ((sixel->context = sixel_output_create(callback_func, (void *) tty)) == NULL) {
		logging(ERROR, "couldn't create sixel context\n");
		return false;
	}
	if (enable_penetrate)
		sixel_output_set_skip_dcs_envelope(sixel->context, 1);
	sixel_output_set_8bit_availability(sixel->context, CSIZE_7BIT);

	return true;
}

void sixel_die(struct sixel_t *sixel)
{
	if (sixel->dither)
		sixel_dither_unref(sixel->dither);

	if (sixel->context)
		sixel_output_unref(sixel->context);
}

void sixel_write(struct tty_t *tty, struct sixel_t *sixel, struct image *img, bool enable_penetrate)
{
	if (enable_penetrate) {
		ewrite(tty->fd, "\033P\033P\033\\", 6);
		sixel_encode(get_current_frame(img), get_image_width(img), get_image_height(img),
			get_image_channel(img), sixel->dither, sixel->context);
		ewrite(tty->fd, "\033P\033\033\\\033P\\\033\\", 10);
	} else {
		sixel_encode(get_current_frame(img), get_image_width(img), get_image_height(img),
			get_image_channel(img), sixel->dither, sixel->context);
	}
}
