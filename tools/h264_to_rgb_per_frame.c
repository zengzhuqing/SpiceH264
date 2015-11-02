#include <sys/stat.h>		/* stat */
#include <unistd.h>		/* stat */

#include "libavcodec/avcodec.h" 
#include "libavutil/frame.h" /* AVFrame */
#include "libavformat/avformat.h" /* av_register_all */

static int video_decode_example(uint8_t *frame, const int frame_size,
                        const int width, const int height,
                        const char *output_filename)
{
    AVCodec *codec;
    AVCodecContext *ctx;
    AVFrame *fr;
    uint8_t *byte_buffer;
    AVPacket pkt;
    int number_of_written_bytes;
    int got_frame = 0;
    int byte_buffer_size;
    int result;
    FILE *out_fp;
   
    codec = NULL;
    ctx = NULL;
    fr = NULL;
    byte_buffer = NULL;
  
    avcodec_register_all();

    codec = avcodec_find_decoder(AV_CODEC_ID_H264);

    ctx = avcodec_alloc_context3(codec);

    ctx->width = width;
    ctx->height = height;
    
    result = avcodec_open2(ctx, codec, NULL);
    if (result < 0) {
        av_log(ctx, AV_LOG_ERROR, "Can't open decoder\n");
        return result;
    }

    fr = av_frame_alloc();
    if (!fr) {
        av_log(NULL, AV_LOG_ERROR, "Can't allocate frame\n");
        return AVERROR(ENOMEM);
    }
    
    //for yuv420p 12 bytes is OK
    byte_buffer_size = 3 * ctx->width * ctx->height / 2;
    byte_buffer = av_malloc(byte_buffer_size);
    if (!byte_buffer) {
        fprintf(stderr, "Failed to allocate buffer, size: %d\n", byte_buffer);
        return AVERROR(ENOMEM);
    }

    av_init_packet(&pkt);
    out_fp = fopen(output_filename, "wb");
    if(out_fp == NULL) {
        av_log(NULL, AV_LOG_ERROR, "Can't open output file\n");
        return AVERROR(ENOMEM);
    }

    pkt.data = frame; 
    pkt.size = frame_size;

    result = avcodec_decode_video2(ctx, fr, &got_frame, &pkt);
    if (result < 0) {
        fprintf(stderr, "Failed to decode frame\n");
        return; 
    }
    if (got_frame) {
        number_of_written_bytes = av_image_copy_to_buffer(byte_buffer, byte_buffer_size,
                                    (const uint8_t* const *)fr->data, (const int*) fr->linesize,
                                    ctx->pix_fmt, ctx->width, ctx->height, 1);
            if (number_of_written_bytes < 0) {
                av_log(NULL, AV_LOG_ERROR, "Can't copy image to buffer\n");
                return number_of_written_bytes;
            }
            result = fwrite(byte_buffer, sizeof(uint8_t), byte_buffer_size, out_fp);
            if(result != byte_buffer_size) {
                av_log(NULL, AV_LOG_ERROR, "Can't write yuv data to output file\n");
                return result;
            } 
    } 
    av_free_packet(&pkt);

    fclose(out_fp);
    out_fp = NULL; 
    av_free_packet(&pkt);
    av_frame_free(&fr);
    avcodec_free_context(&ctx);
    av_freep(&byte_buffer);

    return 0;
}

int main(int argc, char **argv)
{
    FILE *in_file;
    uint8_t *frame;
    int frame_size;
    struct stat stat_buf;
    int n;
    int ret;
    char *in_filename;
    char *out_filename;
    int width;
    int height;
 
    in_file = NULL;
    frame = NULL;

    if (argc != 4) {
        fprintf(stderr, "Example usage: example 352x288 h264_file yuv_file\n");
        return 1;
    }

    if (sscanf(argv[1], "%dx%d", &width, &height) != 2) {
        fprintf(stderr, "Failed to get resolution from input\n");
        exit(1);
    }

    in_filename = argv[2];
    out_filename = argv[3];

    av_register_all();

    if (stat(in_filename, &stat_buf) < 0) {
        fprintf(stderr, "Failed to stat '%s'\n", in_filename);
        exit(1);
    }
   
    frame_size = stat_buf.st_size; 

    frame = malloc(frame_size);
    if (frame == NULL) {
        fprintf(stderr, "Failed to allocate frame buffer\n");
        exit(1); 
    }

    in_file = fopen(in_filename, "rb");
    if (in_file == NULL) {
        fprintf(stderr, "Failed to open h264 stream file\n");
        exit(1); 
    }

    n = fread(frame, frame_size, 1, in_file);
    if (n != 1) {
        fprintf(stderr, "Failed to read %d bytes from '%s'\n", 
            frame_size, argv[2]);
        exit(1);
    } 
    (void)fclose(in_file);
    in_file = NULL;

    ret = video_decode_example(frame, frame_size, width, height, out_filename);

    return 0;
}
