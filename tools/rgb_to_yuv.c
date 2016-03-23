#include <stdio.h>
#include <assert.h>

#include <libswscale/swscale.h>

#include "bitmap.h"

U8* GetBmpData(U8 *bitCountPerPix, U32 *width, U32 *height, const char* filename)  
{  
    FILE *pf = fopen(filename, "rb");  
    if(!pf)  
    {  
        printf("fopen failed : %s, %d\n", __FILE__, __LINE__);  
        return NULL;  
    }  
  
    BITMAPFILE bmpfile;  
    fread(&(bmpfile.bfHeader), sizeof(BITMAPFILEHEADER), 1, pf);  
    fread(&(bmpfile.biInfo.bmiHeader), sizeof(BITMAPINFOHEADER), 1, pf);  
  
    //print_bmfh(bmpfile.bfHeader);  
    //print_bmih(bmpfile.biInfo.bmiHeader);  
       
    if(bitCountPerPix)  
    {  
        *bitCountPerPix = bmpfile.biInfo.bmiHeader.biBitCount;  
    }  
    if(width)  
    {  
        *width = bmpfile.biInfo.bmiHeader.biWidth;  
    }  
    if(height)  
    {  
        *height = bmpfile.biInfo.bmiHeader.biHeight;  
    }  
  
    U32 bmppicth = (((*width)*(*bitCountPerPix) + 31) >> 5) << 2;  
    U8 *pdata = (U8*)malloc((*height)*bmppicth);  
       
    U8 *pEachLinBuf = (U8*)malloc(bmppicth);  
    memset(pEachLinBuf, 0, bmppicth);  
    U8 BytePerPix = (*bitCountPerPix) >> 3;  
    U32 pitch = (*width) * BytePerPix;  
  
    if(pdata && pEachLinBuf)  
    {  
        int w, h;  
        for(h = (*height) - 1; h >= 0; h--)  
        {  
            fread(pEachLinBuf, bmppicth, 1, pf);  
            for(w = 0; w < (*width); w++)  
            {  
                pdata[h*pitch + w*BytePerPix + 0] = pEachLinBuf[w*BytePerPix+0];  
                pdata[h*pitch + w*BytePerPix + 1] = pEachLinBuf[w*BytePerPix+1];  
                pdata[h*pitch + w*BytePerPix + 2] = pEachLinBuf[w*BytePerPix+2];  
            }  
        }  
        free(pEachLinBuf);  
    }  
    fclose(pf);  
       
    return pdata;  
}  

int main(int argc, char **argv){
    if(argc < 2) {
        printf("Usage: %s rgb_file_name yuv_file_name\n", argv[0]);
        exit(1);  
    } 

    U8 bitCountPerPix;
    U32 width, height;
    U8 *rgb_data;

    rgb_data = GetBmpData(&bitCountPerPix, &width, &height, argv[1]);  
    assert(rgb_data != NULL);    
 
    const uint8_t *rgb_src[3]= {rgb_data, NULL, NULL};
    const int rgb_stride[3]={bitCountPerPix / 8 * width, 0, 0};
   
    printf("width = %d, height = %d\n", width, height); 
    uint8_t *yuv_data = malloc (3 * width * height / 2);
    uint8_t *yuv_src[3]= {yuv_data, yuv_data + width * height, yuv_data + width * height * 5 / 4};
    memset(yuv_data, 0, 3 * width * height / 2);   
 
    const int yuv_stride[3]={width, width / 2, width / 2};
    
    struct SwsContext *sws;

    sws= sws_getContext(width, height, AV_PIX_FMT_RGB32, width, height, AV_PIX_FMT_YUV420P, 1, NULL, NULL, NULL);
    assert(sws != NULL);

    int ret = sws_scale(sws, rgb_src, rgb_stride, 0, height, yuv_src, yuv_stride);
    assert(ret == height);
    
    FILE *out_fp;
    int cnt;
    out_fp = fopen(argv[2], "w+");
    assert(out_fp != NULL);
    cnt = fwrite(yuv_data, sizeof(uint8_t), width * height * 3 / 2, out_fp);
    assert(cnt == width * height * 3 / 2);    
    fclose(out_fp);
    out_fp = NULL;         

    free(rgb_data);

    sws_freeContext(sws); 
    sws = NULL;

    return 0;
}
