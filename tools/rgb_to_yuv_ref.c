#include <stdio.h>		/* fclose fopen fread fwrite */
#include <stdlib.h>		/* calloc free malloc */
#include <sys/stat.h>		/* stat */
#include <unistd.h>		/* stat */

#include <libswscale/swscale.h>

static int rgb2yuv(const char *rgb, int width, int height, char *yuv)
{
	struct SwsContext *sws;
	const uint8_t *rgb_slice[3];
	int rgb_stride[3];
	uint8_t *yuv_slice[3];
	int yuv_stride[3];
	int n;

	sws = sws_getContext(width, height, PIX_FMT_RGB32,
				width, height, PIX_FMT_YUV420P,
				1, NULL, NULL, NULL);
	if (sws == NULL) {
		return -1;
	}

	rgb_slice[0] = rgb;
	rgb_slice[1] = NULL;
	rgb_slice[2] = NULL;
	rgb_stride[0] = width * 4;
	rgb_stride[1] = 0;
	rgb_stride[2] = 0;
	yuv_slice[0] = yuv;
	yuv_slice[1] = yuv + width * height;
	yuv_slice[2] = yuv_slice[1] + width * height / 4;
	yuv_stride[0] = width;
	yuv_stride[1] = width / 2;
	yuv_stride[2] = width / 2;
	n = sws_scale(sws, rgb_slice, rgb_stride, 0, height,
			yuv_slice, yuv_stride);

	sws_freeContext(sws);
	sws = NULL;

	if (n != height)
		return -1;
	return 0;
}

int main(int argc, char *argv[])
{
	const char *in_file, *out_file;
	int width, height, size;
	struct stat stat_buf;
	char *rgb;
	char *yuv;
	FILE *fp;
	int n;

	rgb = NULL;
	yuv = NULL;
	fp = NULL;
	if (argc != 5) {
		fprintf(stderr, "Usage: %s <input .rgb file> <width> <height>"
			" <output .yuv file>\n", argv[0]);
		goto fail;
	}

	in_file = argv[1];
	out_file = argv[4];
	width = atoi(argv[2]);
	height = atoi(argv[3]);
	if (width <= 0 || height <= 0 || width % 2 != 0 || height % 2 != 0) {
		fprintf(stderr, "Bad image size: %d x %d\n", width, height);
		goto fail;
	}
	size = width * height * 4;

	if (stat(in_file, &stat_buf) < 0) {
		fprintf(stderr, "Failed to stat '%s'\n", in_file);
		goto fail;
	}
	if (stat_buf.st_size != size) {
		fprintf(stderr, "Bad input file size %d for %dx%d image\n",
			stat_buf.st_size, width, height);
		goto fail;
	}

	rgb = malloc(size);
	if (rgb == NULL) {
		fprintf(stderr, "Failed to allocate RGB buffer\n");
		goto fail;
	}
	fp = fopen(in_file, "rb");
	if (fp == NULL) {
		fprintf(stderr, "Failed to open '%s'\n", in_file);
		goto fail;
	}
	n = fread(rgb, 1, size, fp);
	if (n != size) {
		fprintf(stderr, "Failed to read %d bytes from '%s'\n",
			size, in_file);
		goto fail;
	}
	(void)fclose(fp);
	fp = NULL;

	size = 3 * width * height / 2;
	yuv = calloc(1, size);
	if (yuv == NULL) {
		fprintf(stderr, "Failed to allocate YUV buffer\n");
		goto fail;
	}

	if (rgb2yuv(rgb, width, height, yuv) < 0) {
		fprintf(stderr, "Failed to convert RGB to YUV\n");
		goto fail;
	}

	fp = fopen(out_file, "wb");
	if (fp == NULL) {
		fprintf(stderr, "Failed to create '%s'\n", out_file);
		goto fail;
	}
	n = fwrite(yuv, 1, size, fp);
	if (n != size) {
		fprintf(stderr, "Failed to write %d bytes to '%s'\n",
			size, out_file);
		goto fail;
	}
	(void)fclose(fp);
	fp = NULL;
	free(yuv);
	yuv = NULL;
	free(rgb);
	rgb = NULL;

	return EXIT_SUCCESS;

fail:
	if (fp != NULL) {
		(void)fclose(fp);
		fp = NULL;
	}
	if (yuv != NULL) {
		free(yuv);
		yuv = NULL;
	}
	if (rgb != NULL) {
		free(rgb);
		rgb = NULL;
	}
	return EXIT_FAILURE;
}
