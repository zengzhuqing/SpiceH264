#include <stdbool.h>
#include <stdio.h>
#include <fcntl.h> /* O_RDWR */
#include <memory.h>
#include <stdlib.h>

#include <mfx/mfxvideo.h>

#define MSDK_SLEEP(X)                   { usleep(1000*(X)); }

#define MSDK_PRINT_RET_MSG(ERR)         {PrintErrString(ERR, __FILE__, __LINE__);}
#define MSDK_CHECK_RESULT(P, X, ERR)    {if ((X) > (P)) {MSDK_PRINT_RET_MSG(ERR); return ERR;}}
#define MSDK_CHECK_POINTER(P, ERR)      {if (!(P)) {MSDK_PRINT_RET_MSG(ERR); return ERR;}}
#define MSDK_CHECK_ERROR(P, X, ERR)     {if ((X) == (P)) {MSDK_PRINT_RET_MSG(ERR); return ERR;}}
#define MSDK_IGNORE_MFX_STS(P, X)       {if ((X) == (P)) {P = MFX_ERR_NONE;}}
#define MSDK_BREAK_ON_ERROR(P)          {if (MFX_ERR_NONE != (P)) break;}

#define MSDK_ALIGN32(X)                 (((mfxU32)((X)+31)) & (~ (mfxU32)31))
#define MSDK_ALIGN16(value)             (((value + 15) >> 4) << 4)

typedef struct {
    mfxSession session;

    mfxFrameAllocator mfxAllocator;

    mfxU16 nSurfNumVPPIn;
    mfxU16 nSurfNumVPPOutEnc;

    mfxFrameAllocResponse mfxResponseVPPIn;
    mfxFrameAllocResponse mfxResponseVPPOutEnc;

    mfxFrameSurface1** pmfxSurfacesVPPIn;
    mfxFrameSurface1** pVPPSurfacesVPPOutEnc;

    mfxBitstream mfxBS;
} h264_qsv_ctx;

int h264_qsv_encoder_encode(bool *first, h264_qsv_ctx *pCtx, const int width, const int height, const unsigned char *rgb, unsigned char **pFrame, int *pFrameSize)
{
    mfxStatus sts;
    int nEncSurfIdx;
    int nVPPSurfIdx;
    mfxSyncPoint syncpVPP, syncpEnc;

    sts = MFX_ERR_NONE;
    nEncSurfIdx = 0;
    nVPPSurfIdx = 0;

    if (*first) {
        sts = h264_qsv_encoder_init(pCtx, width, height);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

        *first = false;
    }

    nVPPSurfIdx = GetFreeSurfaceIndex(pCtx->pmfxSurfacesVPPIn, pCtx->nSurfNumVPPIn);    // Find free input frame surface
    MSDK_CHECK_ERROR(MFX_ERR_NOT_FOUND, nVPPSurfIdx, MFX_ERR_MEMORY_ALLOC);

        // Surface locking required when read/write video surfaces
    sts = pCtx->mfxAllocator.Lock(pCtx->mfxAllocator.pthis, pCtx->pmfxSurfacesVPPIn[nVPPSurfIdx]->Data.MemId, &(pCtx->pmfxSurfacesVPPIn[nVPPSurfIdx]->Data));
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    sts = LoadRawRGBFrameFromRGB(pCtx->pmfxSurfacesVPPIn[nVPPSurfIdx], rgb, 4 * width * height);  // Load frame from file into surface
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    sts = pCtx->mfxAllocator.Unlock(pCtx->mfxAllocator.pthis, pCtx->pmfxSurfacesVPPIn[nVPPSurfIdx]->Data.MemId, &(pCtx->pmfxSurfacesVPPIn[nVPPSurfIdx]->Data));
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    nEncSurfIdx = GetFreeSurfaceIndex(pCtx->pVPPSurfacesVPPOutEnc, pCtx->nSurfNumVPPOutEnc);    // Find free output frame surface
    MSDK_CHECK_ERROR(MFX_ERR_NOT_FOUND, nEncSurfIdx, MFX_ERR_MEMORY_ALLOC);

    for (;;) {
        // Process a frame asychronously (returns immediately)
        sts = MFXVideoVPP_RunFrameVPPAsync(pCtx->session, pCtx->pmfxSurfacesVPPIn[nVPPSurfIdx], pCtx->pVPPSurfacesVPPOutEnc[nEncSurfIdx], NULL, &syncpVPP);
        if (MFX_WRN_DEVICE_BUSY == sts) {
            MSDK_SLEEP(1);  // Wait if device is busy, then repeat the same call
        } else
            break;
    }

    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    for (;;) {
        // Encode a frame asychronously (returns immediately)
        sts = MFXVideoENCODE_EncodeFrameAsync(pCtx->session, NULL, pCtx->pVPPSurfacesVPPOutEnc[nEncSurfIdx], &pCtx->mfxBS, &syncpEnc);

        if (MFX_ERR_NONE < sts && !syncpEnc) {  // Repeat the call if warning and no output
            if (MFX_WRN_DEVICE_BUSY == sts)
                MSDK_SLEEP(1);  // Wait if device is busy, then repeat the same call
        } else if (MFX_ERR_NONE < sts && syncpEnc) {
            sts = MFX_ERR_NONE;     // Ignore warnings if output is available
            break;
        } else if (MFX_ERR_NOT_ENOUGH_BUFFER == sts) {
            // Allocate more bitstream buffer memory here if needed...
            break;
        } else
            break;
    }

    if (MFX_ERR_NONE == sts) {
        sts = MFXVideoCORE_SyncOperation(pCtx->session, syncpEnc, 60000);   // Synchronize. Wait until encoded frame is ready
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

        *pFrame = pCtx->mfxBS.Data + pCtx->mfxBS.DataOffset;
        *pFrameSize = pCtx->mfxBS.DataLength;
        pCtx->mfxBS.DataLength = 0;  //FIXME ZZQ must reset length here
    }

    return MFX_ERR_NONE;
}

int h264_qsv_encoder_init(h264_qsv_ctx *pCtx, const int width, const int height)
{
    mfxStatus sts;
    int i;

    sts = MFX_ERR_NONE;

    // Initialize Intel Media SDK pCtx->session
    // - MFX_IMPL_AUTO_ANY selects HW acceleration if available (on any adapter)
    mfxIMPL impl = MFX_IMPL_AUTO_ANY;
    mfxVersion ver = { {0, 1} };
    sts = Initialize(impl, ver, &pCtx->session, &pCtx->mfxAllocator);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // Initialize encoder parameters
    // - In this example we are encoding an AVC (H.264) stream
    mfxVideoParam mfxEncParams;
    memset(&mfxEncParams, 0, sizeof(mfxEncParams));
    mfxEncParams.mfx.CodecId = MFX_CODEC_AVC;
    mfxEncParams.mfx.TargetUsage = MFX_TARGETUSAGE_BEST_SPEED;
    mfxEncParams.mfx.TargetKbps = 2000;
    mfxEncParams.mfx.RateControlMethod = MFX_RATECONTROL_VBR;
    mfxEncParams.mfx.FrameInfo.FrameRateExtN = 30;
    mfxEncParams.mfx.FrameInfo.FrameRateExtD = 1;
    mfxEncParams.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
    mfxEncParams.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
    mfxEncParams.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
    mfxEncParams.mfx.FrameInfo.CropX = 0;
    mfxEncParams.mfx.FrameInfo.CropY = 0;
    mfxEncParams.mfx.FrameInfo.CropW = width;
    mfxEncParams.mfx.FrameInfo.CropH = height;
    // Width must be a multiple of 16
    // Height must be a multiple of 16 in case of frame picture and a multiple of 32 in case of field picture
    mfxEncParams.mfx.FrameInfo.Width = MSDK_ALIGN16(width);
    mfxEncParams.mfx.FrameInfo.Height =
        (MFX_PICSTRUCT_PROGRESSIVE == mfxEncParams.mfx.FrameInfo.PicStruct) ?
        MSDK_ALIGN16(height) :
        MSDK_ALIGN32(height);

    mfxEncParams.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY;

    // Configuration for low latency
    mfxEncParams.AsyncDepth = 1;    //1 is best for low latency
    mfxEncParams.mfx.GopRefDist = 1;        //1 is best for low latency, I and P frames only

    mfxExtCodingOption extendedCodingOptions;
    memset(&extendedCodingOptions, 0, sizeof(extendedCodingOptions));
    extendedCodingOptions.Header.BufferId = MFX_EXTBUFF_CODING_OPTION;
    extendedCodingOptions.Header.BufferSz = sizeof(extendedCodingOptions);
    extendedCodingOptions.MaxDecFrameBuffering = 1;
    mfxExtBuffer* extendedBuffers[1];
    extendedBuffers[0] = (mfxExtBuffer*) & extendedCodingOptions;
    mfxEncParams.ExtParam = extendedBuffers;
    mfxEncParams.NumExtParam = 1;

    // Validate video encode parameters (optional)
    // - In this example the validation result is written to same structure
    // - MFX_WRN_INCOMPATIBLE_VIDEO_PARAM is returned if some of the video parameters are not supported,
    //   instead the encoder will select suitable parameters closest matching the requested configuration
    sts = MFXVideoENCODE_Query(pCtx->session, &mfxEncParams, &mfxEncParams);
    MSDK_IGNORE_MFX_STS(sts, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // Initialize VPP parameters
    mfxVideoParam VPPParams;
    memset(&VPPParams, 0, sizeof(VPPParams));
    // Input data
    VPPParams.vpp.In.FourCC = MFX_FOURCC_RGB4;
    VPPParams.vpp.In.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
    VPPParams.vpp.In.CropX = 0;
    VPPParams.vpp.In.CropY = 0;
    VPPParams.vpp.In.CropW = width;
    VPPParams.vpp.In.CropH = height;
    VPPParams.vpp.In.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
    VPPParams.vpp.In.FrameRateExtN = 30;
    VPPParams.vpp.In.FrameRateExtD = 1;
    // width must be a multiple of 16
    // height must be a multiple of 16 in case of frame picture and a multiple of 32 in case of field picture
    VPPParams.vpp.In.Width = MSDK_ALIGN16(width);
    VPPParams.vpp.In.Height =
        (MFX_PICSTRUCT_PROGRESSIVE == VPPParams.vpp.In.PicStruct) ?
        MSDK_ALIGN16(height) :
        MSDK_ALIGN32(height);
    // Output data
    VPPParams.vpp.Out.FourCC = MFX_FOURCC_NV12;
    VPPParams.vpp.Out.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
    VPPParams.vpp.Out.CropX = 0;
    VPPParams.vpp.Out.CropY = 0;
    VPPParams.vpp.Out.CropW = width;
    VPPParams.vpp.Out.CropH = height;
    VPPParams.vpp.Out.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
    VPPParams.vpp.Out.FrameRateExtN = 30;
    VPPParams.vpp.Out.FrameRateExtD = 1;
    // width must be a multiple of 16
    // height must be a multiple of 16 in case of frame picture and a multiple of 32 in case of field picture
    VPPParams.vpp.Out.Width = MSDK_ALIGN16(VPPParams.vpp.Out.CropW);
    VPPParams.vpp.Out.Height =
        (MFX_PICSTRUCT_PROGRESSIVE == VPPParams.vpp.Out.PicStruct) ?
        MSDK_ALIGN16(VPPParams.vpp.Out.CropH) :
        MSDK_ALIGN32(VPPParams.vpp.Out.CropH);

    VPPParams.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY | MFX_IOPATTERN_OUT_VIDEO_MEMORY;

    // Query number of required surfaces for encoder
    mfxFrameAllocRequest EncRequest;
    memset(&EncRequest, 0, sizeof(EncRequest));
    sts = MFXVideoENCODE_QueryIOSurf(pCtx->session, &mfxEncParams, &EncRequest);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // Query number of required surfaces for VPP
    mfxFrameAllocRequest VPPRequest[2];     // [0] - in, [1] - out
    memset(&VPPRequest, 0, sizeof(mfxFrameAllocRequest) * 2);
    sts = MFXVideoVPP_QueryIOSurf(pCtx->session, &VPPParams, VPPRequest);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    EncRequest.Type |= MFX_MEMTYPE_FROM_VPPOUT;     // surfaces are shared between VPP output and encode input

    // Determine the required number of surfaces for VPP input and for VPP output (encoder input)
    pCtx->nSurfNumVPPIn = VPPRequest[0].NumFrameSuggested;
    pCtx->nSurfNumVPPOutEnc = EncRequest.NumFrameSuggested + VPPRequest[1].NumFrameSuggested;

    EncRequest.NumFrameSuggested = pCtx->nSurfNumVPPOutEnc;

    // Allocate required surfaces
    sts = pCtx->mfxAllocator.Alloc(pCtx->mfxAllocator.pthis, &VPPRequest[0], &pCtx->mfxResponseVPPIn);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    sts = pCtx->mfxAllocator.Alloc(pCtx->mfxAllocator.pthis, &EncRequest, &pCtx->mfxResponseVPPOutEnc);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // Allocate required surfaces
    mfxFrameAllocResponse mfxResponse ;
    sts = pCtx->mfxAllocator.Alloc(pCtx->mfxAllocator.pthis, &EncRequest, &mfxResponse);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // Allocate surface headers (mfxFrameSurface1) for VPPIn
    pCtx->pmfxSurfacesVPPIn = (mfxFrameSurface1**)malloc(sizeof(mfxFrameSurface1*) * pCtx->nSurfNumVPPIn);
    MSDK_CHECK_POINTER(pCtx->pmfxSurfacesVPPIn, MFX_ERR_MEMORY_ALLOC);
    for (i = 0; i < pCtx->nSurfNumVPPIn; i++) {
        pCtx->pmfxSurfacesVPPIn[i] = (mfxFrameSurface1*)malloc(sizeof(mfxFrameSurface1));
        MSDK_CHECK_POINTER(pCtx->pmfxSurfacesVPPIn[i], MFX_ERR_MEMORY_ALLOC);
        memset(pCtx->pmfxSurfacesVPPIn[i], 0, sizeof(mfxFrameSurface1));
        memcpy(&(pCtx->pmfxSurfacesVPPIn[i]->Info), &(VPPParams.vpp.In), sizeof(mfxFrameInfo));
        pCtx->pmfxSurfacesVPPIn[i]->Data.MemId = pCtx->mfxResponseVPPIn.mids[i];
        ClearRGBSurfaceVMem(pCtx->pmfxSurfacesVPPIn[i]->Data.MemId);
    }

    pCtx->pVPPSurfacesVPPOutEnc = (mfxFrameSurface1**)malloc(sizeof(mfxFrameSurface1*) * pCtx->nSurfNumVPPOutEnc);
    MSDK_CHECK_POINTER(pCtx->pVPPSurfacesVPPOutEnc, MFX_ERR_MEMORY_ALLOC);
    for (i = 0; i < pCtx->nSurfNumVPPOutEnc; i++) {
        pCtx->pVPPSurfacesVPPOutEnc[i] = (mfxFrameSurface1*)malloc(sizeof(mfxFrameSurface1));
        MSDK_CHECK_POINTER(pCtx->pVPPSurfacesVPPOutEnc[i], MFX_ERR_MEMORY_ALLOC);
        memset(pCtx->pVPPSurfacesVPPOutEnc[i], 0, sizeof(mfxFrameSurface1));
        memcpy(&(pCtx->pVPPSurfacesVPPOutEnc[i]->Info), &(VPPParams.vpp.Out), sizeof(mfxFrameInfo));
        pCtx->pVPPSurfacesVPPOutEnc[i]->Data.MemId = pCtx->mfxResponseVPPOutEnc.mids[i];
    }

    // Disable default VPP operations
    mfxExtVPPDoNotUse extDoNotUse;
    memset(&extDoNotUse, 0, sizeof(mfxExtVPPDoNotUse));
    extDoNotUse.Header.BufferId = MFX_EXTBUFF_VPP_DONOTUSE;
    extDoNotUse.Header.BufferSz = sizeof(mfxExtVPPDoNotUse);
    extDoNotUse.NumAlg = 4;
    extDoNotUse.AlgList = malloc(sizeof(mfxU32) * extDoNotUse.NumAlg);
    MSDK_CHECK_POINTER(extDoNotUse.AlgList, MFX_ERR_MEMORY_ALLOC);
    extDoNotUse.AlgList[0] = MFX_EXTBUFF_VPP_DENOISE;       // turn off denoising (on by default)
    extDoNotUse.AlgList[1] = MFX_EXTBUFF_VPP_SCENE_ANALYSIS;        // turn off scene analysis (on by default)
    extDoNotUse.AlgList[2] = MFX_EXTBUFF_VPP_DETAIL;        // turn off detail enhancement (on by default)
    extDoNotUse.AlgList[3] = MFX_EXTBUFF_VPP_PROCAMP;       // turn off processing amplified (on by default)

    // Add extended VPP buffers
    mfxExtBuffer* extBuffers[1];
    extBuffers[0] = (mfxExtBuffer*) & extDoNotUse;
    VPPParams.ExtParam = extBuffers;
    VPPParams.NumExtParam = 1;

    // Initialize the Media SDK encoder
    sts = MFXVideoENCODE_Init(pCtx->session, &mfxEncParams);
    MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // Initialize Media SDK VPP
    sts = MFXVideoVPP_Init(pCtx->session, &VPPParams);
    MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // Retrieve video parameters selected by encoder.
    // - BufferSizeInKB parameter is required to set bit stream buffer size
    mfxVideoParam par;
    memset(&par, 0, sizeof(par));
    sts = MFXVideoENCODE_GetVideoParam(pCtx->session, &par);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // Prepare Media SDK bit stream buffer
    memset(&pCtx->mfxBS, 0, sizeof(pCtx->mfxBS));
    pCtx->mfxBS.MaxLength = par.mfx.BufferSizeInKB * 1000;
    pCtx->mfxBS.Data = (mfxU8 *)malloc(sizeof(mfxU8) * pCtx->mfxBS.MaxLength);
    MSDK_CHECK_POINTER(pCtx->mfxBS.Data, MFX_ERR_MEMORY_ALLOC);

    free(extDoNotUse.AlgList);
    extDoNotUse.AlgList = NULL;

    return MFX_ERR_NONE;
}

int h264_qsv_encoder_fini(h264_qsv_ctx *pCtx)
{
    int i;
    // ===================================================================
    // Clean up resources
    //  - It is recommended to close Media SDK components first, before releasing allocated surfaces, since
    //    some surfaces may still be locked by internal Media SDK resources.

    MFXVideoENCODE_Close(pCtx->session);
    MFXVideoVPP_Close(pCtx->session);
    // pCtx->session closed automatically on destruction

    for (i = 0; i < pCtx->nSurfNumVPPIn; i++) {
        free(pCtx->pmfxSurfacesVPPIn[i]);
        pCtx->pmfxSurfacesVPPIn[i] = NULL;
    }
    free(pCtx->pmfxSurfacesVPPIn);
    pCtx->pmfxSurfacesVPPIn = NULL;
    for (i = 0; i < pCtx->nSurfNumVPPOutEnc; i++) {
        free(pCtx->pVPPSurfacesVPPOutEnc[i]);
        pCtx->pVPPSurfacesVPPOutEnc[i] = NULL;
    }
    free(pCtx->pVPPSurfacesVPPOutEnc);
    pCtx->pVPPSurfacesVPPOutEnc = NULL;
    free(pCtx->mfxBS.Data);
    pCtx->mfxBS.Data = NULL;

    pCtx->mfxAllocator.Free(pCtx->mfxAllocator.pthis, &pCtx->mfxResponseVPPIn);
    pCtx->mfxAllocator.Free(pCtx->mfxAllocator.pthis, &pCtx->mfxResponseVPPOutEnc);

    Release();

    return 0;
}

int main(int argc, char *argv[])
{
    const char *bitmap_file, *out_h264;
    int width, height;
    FILE *fSource, *fSink;
    unsigned char *rgb;
    int size;
    bool first;
    unsigned char *frame;
    int frame_size;
    int ret;
    h264_qsv_ctx ctx;


    fSource = NULL;
    fSink = NULL;
    rgb = NULL;
    first = true;

    if (argc != 4) {
        fprintf(stderr, "Example Usage: example 1366x768 bitmap_file h264_file\n");
        return -1;
    }

    if (sscanf(argv[1], "%dx%d", &width, &height) != 2) {
        fprintf(stderr, "Get Resolution error\n");
        fprintf(stderr, "Example Usage: example 1366x768 bitmap_file h264_file\n");
        return -1;
    }

    if (width <= 0 || height <= 0) {
        fprintf(stderr, "Bad image size: %d x %d\n", width, height);
        return -1;
    }

    bitmap_file = argv[2];
    out_h264 = argv[3];
    size = 4 * width * height;

    rgb = malloc(size);
    if (rgb == NULL) {
        fprintf(stderr, "Failed to allocate RGB buffer\n");
        goto fail;
    }

    fSource = fopen(bitmap_file, "rb");
    if (fSource == NULL) {
        fprintf(stderr, "Failed to open %s\n", bitmap_file);
        goto fail;
    }

    fSink = fopen(out_h264, "wb");
    if (fSink == NULL) {
        fprintf(stderr, "Failed to open %s\n", out_h264);
        goto fail;
    }

    while(1) {
        if (fread(rgb, size, 1, fSource) != 1)
            break;
        // call h264_encoder_encode
        int ret = h264_qsv_encoder_encode(&first, &ctx, width, height, rgb, &frame, &frame_size);
        if (ret != MFX_ERR_NONE) {
            fprintf(stderr, "Failed to encode a frame, ret = %d\n", ret);
            goto fail;
        }
        // write encoded frame to fSink
        if (fwrite(frame, frame_size, 1, fSink) != 1) {
            fprintf(stderr, "Failed to frame to out_h264\n");
            goto fail;
        }
    }

    h264_qsv_encoder_fini(&ctx);

    (void)fclose(fSource);
    fSource = NULL;
    (void)fclose(fSink);
    fSink = NULL;

    return 0;

fail:
    if (rgb != NULL) {
        free(rgb);
        rgb = NULL;
    }
    if (fSource != NULL) {
        (void)fclose(fSource);
        fSource = NULL;
    }
    if (fSink != NULL) {
        (void)fclose(fSink);
        fSink = NULL;
    }

    return -1;
}
