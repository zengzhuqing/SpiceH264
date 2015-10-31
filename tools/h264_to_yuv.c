/*
 * refer from ffmpeg/test/api/api-h264-test.c and ffmpeg/ffplay.c 
 */

#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"

static int video_decode_example(const char *input_filename, const char *output_filename)
{
    AVCodec *codec = NULL;
    AVCodecContext *origin_ctx = NULL;
    AVFrame *fr = NULL;
    uint8_t *byte_buffer = NULL;
    AVPacket pkt;
    AVFormatContext *fmt_ctx = NULL;
    int number_of_written_bytes;
    int video_stream;
    int got_frame = 0;
    int byte_buffer_size;
    int i = 0;
    int result;
    int end_of_stream = 0;
    FILE *out_fp;

    result = avformat_open_input(&fmt_ctx, input_filename, NULL, NULL);
    if (result < 0) {
        av_log(NULL, AV_LOG_ERROR, "Can't open file\n");
        return result;
    }

    result = avformat_find_stream_info(fmt_ctx, NULL);
    if (result < 0) {
        av_log(NULL, AV_LOG_ERROR, "Can't get stream info\n");
        return result;
    }

    video_stream = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (video_stream < 0) {
      av_log(NULL, AV_LOG_ERROR, "Can't find video stream in input file\n");
      return -1;
    }

    origin_ctx = fmt_ctx->streams[video_stream]->codec;

    codec = avcodec_find_decoder(origin_ctx->codec_id);
    if (!codec) {
        av_log(NULL, AV_LOG_ERROR, "Can't find decoder\n");
        return -1;
    }

    result = avcodec_open2(origin_ctx, codec, NULL);
    if (result < 0) {
        av_log(origin_ctx, AV_LOG_ERROR, "Can't open decoder\n");
        return result;
    }

    fr = av_frame_alloc();
    if (!fr) {
        av_log(NULL, AV_LOG_ERROR, "Can't allocate frame\n");
        return AVERROR(ENOMEM);
    }
    
    //for yuv420p 12 bytes is OK
    byte_buffer_size = av_image_get_buffer_size(origin_ctx->pix_fmt, origin_ctx->width, origin_ctx->height, 12);
    byte_buffer = av_malloc(byte_buffer_size);
    if (!byte_buffer) {
        av_log(NULL, AV_LOG_ERROR, "Can't allocate buffer\n");
        return AVERROR(ENOMEM);
    }

    i = 0;
    av_init_packet(&pkt);
    out_fp = fopen(output_filename, "w+");
    if(out_fp == NULL) {
        av_log(NULL, AV_LOG_ERROR, "Can't open output file\n");
        return AVERROR(ENOMEM);
    }
    do {
        if (!end_of_stream)
            if (av_read_frame(fmt_ctx, &pkt) < 0)
                end_of_stream = 1;
        if (end_of_stream) {
            pkt.data = NULL;
            pkt.size = 0;
        }
        if (pkt.stream_index == video_stream || end_of_stream) {
            got_frame = 0;
            if (pkt.pts == AV_NOPTS_VALUE)
                pkt.pts = pkt.dts = i;
            result = avcodec_decode_video2(origin_ctx, fr, &got_frame, &pkt);
            if (result < 0) {
                av_log(NULL, AV_LOG_ERROR, "Error decoding frame\n");
                return result;
            }
            if (got_frame) {
                number_of_written_bytes = av_image_copy_to_buffer(byte_buffer, byte_buffer_size,
                                        (const uint8_t* const *)fr->data, (const int*) fr->linesize,
                                        origin_ctx->pix_fmt, origin_ctx->width, origin_ctx->height, 1);
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
            av_init_packet(&pkt);
        }
        i++;
    } while (!end_of_stream || got_frame);

    fclose(out_fp);
    out_fp = NULL; 
    av_free_packet(&pkt);
    av_frame_free(&fr);
    avformat_close_input(&fmt_ctx);
    av_freep(&byte_buffer);
    
    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        av_log(NULL, AV_LOG_ERROR, "Incorrect input\n");
        return 1;
    }

    av_register_all();

    if (video_decode_example(argv[1], argv[2]) != 0)
        return 1;

    return 0;
}
