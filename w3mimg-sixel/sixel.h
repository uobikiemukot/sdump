/* See LICENSE for licence details. */
#include <sixel.h>

enum {
	SIXEL_COLORS = 256,
	SIXEL_BPP    = 3,
};

struct sixel_t {
	sixel_output_t *context;
	sixel_dither_t *dither;
	FILE *output;
};

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

bool sixel_init(struct sixel_t *sixel, struct image *img, FILE *fp)
{
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

	if ((sixel->context = sixel_output_create(sixel_write_callback, fp)) == NULL) {
		logging(ERROR, "couldn't create sixel context\n");
		return false;
	}
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

void sixel_output(struct sixel_t *sixel, struct image *img)
{
	//printf("\0337"); /* save cursor position */
	sixel_encode(get_current_frame(img), get_image_width(img), get_image_height(img),
		get_image_channel(img), sixel->dither, sixel->context);
	increment_frame(img);
	//printf("\0338"); /* save cursor position */
}
