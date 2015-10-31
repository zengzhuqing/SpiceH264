#include <stdio.h>
#include <stdint.h> /* uint8_t for x264.h */
#include <x264.h>

#define FAIL_IF_ERROR( cond, ... )\
do\
{\
    if( cond )\
    {\
        fprintf( stderr, __VA_ARGS__ );\
        goto fail;\
    }\
} while( 0 )

int main(int argc, char **argv){
    int width, height;
    x264_param_t param;
    x264_picture_t pic;
    x264_picture_t pic_out;
    x264_t *h;
    int i_frame = 0;
    int i_frame_size;
    x264_nal_t *nal;
    int i_nal;

    FAIL_IF_ERROR(!(argc > 1), "Example usage: example 352x288 <input.yuv >output.h264\n");
    FAIL_IF_ERROR(2 != sscanf( argv[1], "%dx%d", &width, &height ), "resolution not specified or incorrect\n");

    /* Get default params for preset/tuning */
    if(x264_param_default_preset(&param, "medium", "zerolatency") < 0)
        goto fail;

    /* Configure non-default params */
    param.i_csp = X264_CSP_I420;
    param.i_width  = width;
    param.i_height = height;
    param.b_vfr_input = 0;
    param.b_repeat_headers = 1;
    param.b_annexb = 1;

    /* Apply profile restrictions. */
    /* the same as x264 command line parameter --profile baseline */
    if(x264_param_apply_profile(&param, "baseline") < 0)
        goto fail;

    if(x264_picture_alloc(&pic, param.i_csp, param.i_width, param.i_height) < 0)
        goto fail;
#undef fail
#define fail fail2

    h = x264_encoder_open(&param);
    if(!h)
        goto fail;
#undef fail
#define fail fail3

    int luma_size = width * height;
    int chroma_size = luma_size / 4;
    /* Encode frames */
    for(; ; i_frame++)
    {
        /* Read input frame */
        if(fread(pic.img.plane[0], 1, luma_size, stdin) != luma_size)
            break;
        if(fread(pic.img.plane[1], 1, chroma_size, stdin) != chroma_size)
            break;
        if(fread(pic.img.plane[2], 1, chroma_size, stdin) != chroma_size)
            break;

        pic.i_pts = i_frame;
        i_frame_size = x264_encoder_encode(h, &nal, &i_nal, &pic, &pic_out);
        if(i_frame_size < 0)
            goto fail;
        else if(i_frame_size)
        {
            if(!fwrite(nal->p_payload, i_frame_size, 1, stdout))
                goto fail;
        }
        fprintf(stderr, "frame size : %d\n", i_frame_size);
    }
    /* Flush delayed frames */
    while(x264_encoder_delayed_frames(h))
    {
        i_frame_size = x264_encoder_encode(h, &nal, &i_nal, NULL, &pic_out);
        if(i_frame_size < 0)
            goto fail;
        else if(i_frame_size)
        {
            if(!fwrite(nal->p_payload, i_frame_size, 1, stdout))
                goto fail;
        }
        fprintf(stderr, "Delay frame size : %d\n", i_frame_size);
    }

    x264_encoder_close(h);
    x264_picture_clean(&pic);
    return 0;

#undef fail
fail3:
    x264_encoder_close(h);
fail2:
    x264_picture_clean(&pic);
fail:
    return -1;
}
