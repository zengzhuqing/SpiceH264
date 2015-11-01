#include <stdlib.h> /* EXIT_SUCCESS */
#include <string.h> /* memcpy */

#include <libswscale/swscale.h>
#include <x264.h>

#define DEBUG

static int rgb2yuv(const uint8_t *rgb, const int width, const int height, uint8_t *yuv)
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

static x264_t *h264_encoder_init(const int width, const int height)
{
    x264_param_t param;
    x264_t *h;

    /* Get default params for preset/tuning */
    if (x264_param_default_preset(&param, "medium", "zerolatency") < 0) {
        fprintf(stderr, "Failed to get default params\n");
        return NULL;
    }

    /* Configure non-default param */
    param.i_csp = X264_CSP_I420;
    param.i_width =  width;
    param.i_height = height;
    param.b_vfr_input = 0;
    param.b_repeat_headers = 1;
    param.b_annexb = 1;

    /* Apply profile restrictions */
    if (x264_param_apply_profile(&param, "baseline") < 0) {
        fprintf(stderr, "Failed to apply profile\n");
        return NULL;
    }

    h = x264_encoder_open(&param);

    return h;
}

static int h264_encoder_encode(x264_t *h, const uint8_t *yuv, const int width, const int height, x264_nal_t **nal, int *i_frame_size)
{
    x264_picture_t pic;
    x264_picture_t pic_out;
    int luma_size;
    int chroma_size;
    static int i_frame = 0;
    int i_nal;
    int ret;

    luma_size = width * height;
    chroma_size = luma_size / 4;

    if (x264_picture_alloc(&pic, X264_CSP_I420, width, height) < 0)
        return -1;
    pic.i_pts = i_frame++;

    memcpy(pic.img.plane[0], yuv, luma_size);
    memcpy(pic.img.plane[1], yuv + luma_size, chroma_size);
    memcpy(pic.img.plane[2], yuv + luma_size + chroma_size, chroma_size);

    *i_frame_size = x264_encoder_encode(h, nal, &i_nal, &pic, &pic_out);
   
    x264_picture_clean(&pic);

    return 0;
}

static void h264_encode_frame(const uint8_t *rgb, const int width, const int height, FILE *out_fp)
{
    uint8_t *yuv;
    int ret;
    static x264_t *h;
    int frame_size;
    x264_nal_t *nal;
#ifdef DEBUG
    FILE *yuv_fp;
    yuv_fp = NULL;
#endif  
    static int flag = 0;
 
    yuv = malloc (3 * width * height / 2);
    if (yuv == NULL) {
        fprintf(stderr, "Failed to alloca YUV buffer\n");
        goto fail;
    }

    ret = rgb2yuv(rgb, width, height, yuv);
    if (ret < 0) {
        fprintf(stderr, "Failed to transfer RGB to YUV\n");
        goto fail;
    }

#ifdef DEBUG
    yuv_fp = fopen("test.yuv", "wb");
    if (yuv_fp == NULL) {
        fprintf(stderr, "Failed to open test.yuv'\n");
        goto fail;
    }
    if (fwrite(yuv, 1, 3 * width * height / 2, yuv_fp) != 3 * width * height / 2) {
        fprintf(stderr, "Failed to write data to test.yuv'\n");
        goto fail;
    }
    (void)fclose(yuv_fp);
    yuv_fp = NULL;
#endif

    if (flag == 0) {
        h = h264_encoder_init(width, height);
        if (h == NULL) {
            fprintf(stderr, "Failed to init h264 encoder\n");
            goto fail;
        }
        flag = 1;
    }

    ret = h264_encoder_encode(h, yuv, width, height, &nal, &frame_size);
    if (ret < 0) {
        fprintf(stderr, "Failed to encode h264 frame\n");
        goto fail;
    }
    
    if (fwrite(nal->p_payload, frame_size, 1, out_fp) != 1) {
        fprintf(stderr, "Failed to write h264 frame to file\n");
        goto fail;
    }
    
    printf("frame size:%d\n", frame_size);

    if (yuv == NULL) {
        free(yuv);
        yuv = NULL;
    }
#if 0
    x264_encoder_close(h);
    h = NULL;
#endif
    return;

fail:
    if (yuv != NULL) {
        free(yuv);
        yuv = NULL;
    }
#if 0
    if (h != NULL) {
        x264_encoder_close(h);
        h = NULL;
    }
#endif
}

int main(int argc, char *argv[])
{
    const char *bitmap_file, *out_h264;
    int width, height;
    int size;
    uint8_t *rgb;
    FILE *in_fp;
    FILE *out_fp;

    in_fp = NULL;
    out_fp = NULL;
    rgb = NULL;

    if (argc != 4) {
        fprintf(stderr, "Example usage: example 352x288 bitmap_file h264_file\n");
        goto fail;
    }

    if (sscanf(argv[1], "%dx%d", &width, &height) != 2) {
        fprintf(stderr, "Resolution format error\n");
        fprintf(stderr, "Example usage: example 352x288 bitmap_file h264_file\n");
        goto fail;
    }
	
    if (width <= 0 || height <= 0 || width % 2 != 0 || height % 2 != 0) {
		fprintf(stderr, "Bad image size: %d x %d\n", width, height);
		goto fail;
	}

    bitmap_file = argv[2];
    out_h264 = argv[3];
    size = width * height * 4;

    rgb = malloc(size);
    if(rgb == NULL) {
        fprintf(stderr, "Failed to alloc RGB buffer\n");
        goto fail;
    }

    in_fp = fopen(bitmap_file, "rb");
    if (in_fp == NULL) {
        fprintf(stderr, "Failed to open %s\n", bitmap_file);
        goto fail;
    }

    out_fp = fopen(out_h264, "wb");
    if (out_fp == NULL) {
        fprintf(stderr, "Failed to open %s\n", out_h264);
        goto fail;
    }

    while (1) {   
        if (fread(rgb, 1, size, in_fp) != size)
            break;
        h264_encode_frame(rgb, width, height, out_fp);
    }

    (void)fclose(in_fp);
    in_fp = NULL;

    (void)fclose(out_fp);
    out_fp = NULL;

    free(rgb);
    rgb = NULL;
   
    return EXIT_SUCCESS;

fail:
    if (rgb != NULL) {
        free(rgb);
        rgb = NULL;
    }
    if (in_fp != NULL) {
        (void)fclose(in_fp);
        in_fp = NULL;
    }
    if (out_fp != NULL) {
        (void)fclose(out_fp);
        out_fp = NULL;
    }

    return EXIT_FAILURE;
}
