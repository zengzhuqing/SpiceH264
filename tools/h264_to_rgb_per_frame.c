#include <sys/stat.h>		/* stat */
#include <unistd.h>		/* stat */

#include <libavcodec/avcodec.h>
#include <libavutil/frame.h> /* AVFrame */
#include <libavformat/avformat.h> /* av_register_all */
#include <libswscale/swscale.h>

static int yuv2rgb(const uint8_t *yuv, const int width, const int height, uint8_t *rgb)
{
    struct SwsContext *sws;
    const uint8_t *yuv_slice[3];
    int yuv_stride[3];
    uint8_t *rgb_slice[3];
    int rgb_stride[3];
    int n;

    sws = sws_getContext(width, height, PIX_FMT_YUV420P,
                width, height, PIX_FMT_RGB32,
                1, NULL, NULL, NULL);
    if (sws == NULL) {
        fprintf(stderr, "Failed to get swscale context\n");
        return -1;
    }

    yuv_slice[0] = yuv;
    yuv_slice[1] = yuv + width * height;
    yuv_slice[2] = yuv_slice[1] + width * height / 4;
    yuv_stride[0] = width;
    yuv_stride[1] = width / 2;
    yuv_stride[2] = width / 2;
    rgb_slice[0] = rgb;
    rgb_slice[1] = NULL;
    rgb_slice[2] = NULL;
    rgb_stride[0] = width * 4;
    rgb_stride[1] = 0;
    rgb_stride[2] = 0;

    n = sws_scale(sws, yuv_slice, yuv_stride, 0, height,
            rgb_slice, rgb_stride);
    sws_freeContext(sws);
    sws = NULL;

    if (n != height){
        fprintf(stderr, "Failed to run swscale\n");
        return -1;
    }

    return 0;
}

static AVCodecContext *h264_decoder_init(const int width, const int height)
{
    AVCodec *codec;
    AVCodecContext *ctx;
    int result;

    codec = NULL;
    ctx = NULL;
    
    av_register_all();

    avcodec_register_all();

    codec = avcodec_find_decoder(AV_CODEC_ID_H264);

    ctx = avcodec_alloc_context3(codec);

    ctx->width = width;
    ctx->height = height;

    result = avcodec_open2(ctx, codec, NULL);
    if (result < 0) {
        fprintf(stderr, "Can't open decoder\n");
        goto fail;
    }

    return ctx;

fail:
    return NULL; 
}

static int video_decode_example(const int width, const int height,
                    uint8_t *frame[], int frame_size[], int frame_count,
                    const char *out_filename)
{
    FILE *out_fp;
    AVCodecContext *ctx;
    AVFrame *fr;
    AVPacket pkt;
    int idx;
    int result;
    int got_frame;
    uint8_t *yuv;
    int yuv_size;
    uint8_t *rgb;
    int n;

    out_fp = NULL;
    fr = NULL;

    out_fp = fopen(out_filename, "wb");
    if (out_fp == NULL) {
        fprintf(stderr, "Failed to open rgb output file\n");
        goto fail;
    }
    
    ctx = h264_decoder_init(width, height);
    if (ctx == NULL) {
        fprintf(stderr, "Failed to open h264 decoder\n");
        goto fail;
    }
    
    fr = av_frame_alloc();
    if (fr == NULL) {
        fprintf(stderr, "Failed to allocate frame\n");
        goto fail;
    }
    
    av_init_packet(&pkt);
        
    yuv_size = 3 * ctx->width * ctx->height / 2;
    yuv = malloc(yuv_size);
    if (yuv == NULL) {
        fprintf(stderr, "Failed to allocate buffer for yuv\n");
        goto fail;
    }
    rgb = malloc(4 * width *height);
    if (rgb == NULL) {
        fprintf(stderr, "Failed to allocate buffer for rgb\n");
        goto fail;
    }            

    for (idx = 0; idx < frame_count; idx++) {
        pkt.data = frame[idx];
        pkt.size = frame_size[idx];

        result = avcodec_decode_video2(ctx, fr, &got_frame, &pkt);
        if (result < 0) {
            fprintf(stderr, "Failed to decode frame\n");
            goto fail;
        }
    
        if (got_frame) {
            n = av_image_copy_to_buffer(yuv, yuv_size, 
                    (const uint8_t* const *)fr->data, (const int*) fr->linesize,
                    ctx->pix_fmt, ctx->width, ctx->height, 1);
            if (n < 0) {
                fprintf(stderr, "Can't copy image to buffer\n");
                goto fail;
            }
            //color space transfer
            result = yuv2rgb(yuv, width, height, rgb);
            if (result < 0) {
                fprintf(stderr, "Failed to transfer yuv to rgb\n");
                goto fail;
            }
            if (fwrite(rgb, 4 * width * height, 1, out_fp) != 1) {
                fprintf(stderr, "Failed to write to output file\n");
                goto fail;
            }
        } 
    }

    (void)fclose(out_fp);
    out_fp = NULL;
    av_free_packet(&pkt);
    av_frame_free(&fr);
    avcodec_free_context(&ctx);
    free(yuv);
    yuv = NULL;
    free(rgb);
    rgb = NULL;
    
    return 0;   

fail:
    //FIXME: ctx, fr, pkt
    if (out_fp != NULL) {
        (void)fclose(out_fp);
        out_fp = NULL;
    }
    if (yuv != NULL) {
        free(yuv);
        yuv = NULL;
    }  
    if (rgb != NULL) {
        free(rgb);
        rgb = NULL;
    }  
}

int main(int argc, char **argv)
{
    FILE *in_file;
    uint8_t *frame[1];
    int frame_size[1];
    struct stat stat_buf;
    int n;
    int ret;
    char *in_filename;
    char *out_filename;
    int width;
    int height;

    in_file = NULL;

    if (argc != 4) {
        fprintf(stderr, "Example usage: example 352x288 h264_file rgb_file\n");
        return 1;
    }

    if (sscanf(argv[1], "%dx%d", &width, &height) != 2) {
        fprintf(stderr, "Failed to get resolution from input\n");
        exit(1);
    }

    in_filename = argv[2];
    out_filename = argv[3];

    if (stat(in_filename, &stat_buf) < 0) {
        fprintf(stderr, "Failed to stat '%s'\n", in_filename);
        exit(1);
    }

    frame_size[0] = stat_buf.st_size;

    frame[0] = malloc(frame_size[0]);
    if (frame[0] == NULL) {
        fprintf(stderr, "Failed to allocate frame buffer\n");
        exit(1);
    }

    in_file = fopen(in_filename, "rb");
    if (in_file == NULL) {
        fprintf(stderr, "Failed to open h264 stream file\n");
        exit(1);
    }

    n = fread(frame[0], frame_size[0], 1, in_file);
    if (n != 1) {
        fprintf(stderr, "Failed to read %d bytes from '%s'\n",
            frame_size, argv[2]);
        exit(1);
    }
    (void)fclose(in_file);
    in_file = NULL;

    ret = video_decode_example(width, height, frame, frame_size , 1, out_filename);
    fprintf(stderr, "ret = %d\n", ret);

    return 0;
}
