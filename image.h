/* See LICENSE for licence details. */
/* this header file depends loader.h */

/* inline functions:
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

/* some image proccessing functions:
	never use *_single functions directly */
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
