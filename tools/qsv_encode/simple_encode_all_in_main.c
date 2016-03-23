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

int main(int argc, char** argv)
{
    mfxStatus sts = MFX_ERR_NONE;
    bool bEnableInput;
    bool bEnableOutput;
    int i;
    int width;
    int height;

    bEnableInput = true;
    bEnableOutput = true;

    width = 1366;
    height = 768;

    // Open input YV12 YUV file
    FILE* fSource = NULL;
    if (bEnableInput) {
        fSource = fopen("full_screen_video.rgb", "rb");
        MSDK_CHECK_POINTER(fSource, MFX_ERR_NULL_PTR);
    }

    // Create output elementary stream (ES) H.264 file
    FILE* fSink = NULL;
    if (bEnableOutput) {
        fSink = fopen("full_screen_video.264", "wb");
        MSDK_CHECK_POINTER(fSink, MFX_ERR_NULL_PTR);
    }

    // Initialize Intel Media SDK session
    // - MFX_IMPL_AUTO_ANY selects HW acceleration if available (on any adapter)
    mfxIMPL impl = MFX_IMPL_AUTO_ANY;
    mfxVersion ver = { {0, 1} };
    mfxSession session;

    mfxFrameAllocator mfxAllocator;

    sts = Initialize(impl, ver, &session, &mfxAllocator);
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
    sts = MFXVideoENCODE_Query(session, &mfxEncParams, &mfxEncParams);
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
    sts = MFXVideoENCODE_QueryIOSurf(session, &mfxEncParams, &EncRequest);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // Query number of required surfaces for VPP
    mfxFrameAllocRequest VPPRequest[2];     // [0] - in, [1] - out
    memset(&VPPRequest, 0, sizeof(mfxFrameAllocRequest) * 2);
    sts = MFXVideoVPP_QueryIOSurf(session, &VPPParams, VPPRequest);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    EncRequest.Type |= MFX_MEMTYPE_FROM_VPPOUT;     // surfaces are shared between VPP output and encode input

    // Determine the required number of surfaces for VPP input and for VPP output (encoder input)
    mfxU16 nSurfNumVPPIn = VPPRequest[0].NumFrameSuggested;
    mfxU16 nSurfNumVPPOutEnc = EncRequest.NumFrameSuggested + VPPRequest[1].NumFrameSuggested;

    EncRequest.NumFrameSuggested = nSurfNumVPPOutEnc;

    // Allocate required surfaces
    mfxFrameAllocResponse mfxResponseVPPIn;
    mfxFrameAllocResponse mfxResponseVPPOutEnc;
    sts = mfxAllocator.Alloc(mfxAllocator.pthis, &VPPRequest[0], &mfxResponseVPPIn);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    sts = mfxAllocator.Alloc(mfxAllocator.pthis, &EncRequest, &mfxResponseVPPOutEnc);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // Allocate required surfaces
    mfxFrameAllocResponse mfxResponse;
    sts = mfxAllocator.Alloc(mfxAllocator.pthis, &EncRequest, &mfxResponse);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // Allocate surface headers (mfxFrameSurface1) for VPPIn
    mfxFrameSurface1** pmfxSurfacesVPPIn = (mfxFrameSurface1**)malloc(sizeof(mfxFrameSurface1*) * nSurfNumVPPIn);
    MSDK_CHECK_POINTER(pmfxSurfacesVPPIn, MFX_ERR_MEMORY_ALLOC);
    for (i = 0; i < nSurfNumVPPIn; i++) {
        pmfxSurfacesVPPIn[i] = (mfxFrameSurface1*)malloc(sizeof(mfxFrameSurface1));
        MSDK_CHECK_POINTER(pmfxSurfacesVPPIn[i], MFX_ERR_MEMORY_ALLOC);
        memset(pmfxSurfacesVPPIn[i], 0, sizeof(mfxFrameSurface1));
        memcpy(&(pmfxSurfacesVPPIn[i]->Info), &(VPPParams.vpp.In), sizeof(mfxFrameInfo));
        pmfxSurfacesVPPIn[i]->Data.MemId = mfxResponseVPPIn.mids[i];
        if (bEnableInput) {
            ClearRGBSurfaceVMem(pmfxSurfacesVPPIn[i]->Data.MemId);
        }
    }

    mfxFrameSurface1** pVPPSurfacesVPPOutEnc = (mfxFrameSurface1**)malloc(sizeof(mfxFrameSurface1*) * nSurfNumVPPOutEnc);
    MSDK_CHECK_POINTER(pVPPSurfacesVPPOutEnc, MFX_ERR_MEMORY_ALLOC);
    for (i = 0; i < nSurfNumVPPOutEnc; i++) {
        pVPPSurfacesVPPOutEnc[i] = (mfxFrameSurface1*)malloc(sizeof(mfxFrameSurface1));
        MSDK_CHECK_POINTER(pVPPSurfacesVPPOutEnc[i], MFX_ERR_MEMORY_ALLOC);
        memset(pVPPSurfacesVPPOutEnc[i], 0, sizeof(mfxFrameSurface1));
        memcpy(&(pVPPSurfacesVPPOutEnc[i]->Info), &(VPPParams.vpp.Out), sizeof(mfxFrameInfo));
        pVPPSurfacesVPPOutEnc[i]->Data.MemId = mfxResponseVPPOutEnc.mids[i];
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
    sts = MFXVideoENCODE_Init(session, &mfxEncParams);
    MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // Initialize Media SDK VPP
    sts = MFXVideoVPP_Init(session, &VPPParams);
    MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // Retrieve video parameters selected by encoder.
    // - BufferSizeInKB parameter is required to set bit stream buffer size
    mfxVideoParam par;
    memset(&par, 0, sizeof(par));
    sts = MFXVideoENCODE_GetVideoParam(session, &par);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // Prepare Media SDK bit stream buffer
    mfxBitstream mfxBS;
    memset(&mfxBS, 0, sizeof(mfxBS));
    mfxBS.MaxLength = par.mfx.BufferSizeInKB * 1000;
    mfxBS.Data = (mfxU8 *)malloc(sizeof(mfxU8) * mfxBS.MaxLength);
    MSDK_CHECK_POINTER(mfxBS.Data, MFX_ERR_MEMORY_ALLOC);

    // ===================================
    // Start encoding the frames
    //

    int nEncSurfIdx = 0;
    int nVPPSurfIdx = 0;
    mfxSyncPoint syncpVPP, syncpEnc;
    mfxU32 nFrame = 0;

    //
    // Stage 1: Main VPP/encoding loop
    //
    while (MFX_ERR_NONE <= sts || MFX_ERR_MORE_DATA == sts) {
        nVPPSurfIdx = GetFreeSurfaceIndex(pmfxSurfacesVPPIn, nSurfNumVPPIn);    // Find free input frame surface
        MSDK_CHECK_ERROR(MFX_ERR_NOT_FOUND, nVPPSurfIdx, MFX_ERR_MEMORY_ALLOC);

        // Surface locking required when read/write video surfaces
        sts = mfxAllocator.Lock(mfxAllocator.pthis, pmfxSurfacesVPPIn[nVPPSurfIdx]->Data.MemId, &(pmfxSurfacesVPPIn[nVPPSurfIdx]->Data));
        MSDK_BREAK_ON_ERROR(sts);

        sts = LoadRawRGBFrame(pmfxSurfacesVPPIn[nVPPSurfIdx], fSource);  // Load frame from file into surface
        MSDK_BREAK_ON_ERROR(sts);

        sts = mfxAllocator.Unlock(mfxAllocator.pthis, pmfxSurfacesVPPIn[nVPPSurfIdx]->Data.MemId, &(pmfxSurfacesVPPIn[nVPPSurfIdx]->Data));
        MSDK_BREAK_ON_ERROR(sts);

        nEncSurfIdx = GetFreeSurfaceIndex(pVPPSurfacesVPPOutEnc, nSurfNumVPPOutEnc);    // Find free output frame surface
        MSDK_CHECK_ERROR(MFX_ERR_NOT_FOUND, nEncSurfIdx, MFX_ERR_MEMORY_ALLOC);

        for (;;) {
            // Process a frame asychronously (returns immediately)
            sts = MFXVideoVPP_RunFrameVPPAsync(session, pmfxSurfacesVPPIn[nVPPSurfIdx], pVPPSurfacesVPPOutEnc[nEncSurfIdx], NULL, &syncpVPP);
            if (MFX_WRN_DEVICE_BUSY == sts) {
                MSDK_SLEEP(1);  // Wait if device is busy, then repeat the same call
            } else
                break;
        }

        if (MFX_ERR_MORE_DATA == sts)
            continue;

        // MFX_ERR_MORE_SURFACE means output is ready but need more surface (example: Frame Rate Conversion 30->60)
        // * Not handled in this example!

        MSDK_BREAK_ON_ERROR(sts);

        for (;;) {
            // Encode a frame asychronously (returns immediately)
            sts = MFXVideoENCODE_EncodeFrameAsync(session, NULL, pVPPSurfacesVPPOutEnc[nEncSurfIdx], &mfxBS, &syncpEnc);

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
            sts = MFXVideoCORE_SyncOperation(session, syncpEnc, 60000);   // Synchronize. Wait until encoded frame is ready
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

            sts = WriteBitStreamFrame(&mfxBS, fSink);
            MSDK_BREAK_ON_ERROR(sts);

            ++nFrame;
            if (bEnableOutput) {
                fprintf(stderr, "Stage 1: Frame number: %d\n", nFrame);
            }
        }
    }

    // MFX_ERR_MORE_DATA means that the input file has ended, need to go to buffering loop, exit in case of other errors
    MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_DATA);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    //
    // Stage 2: Retrieve the buffered VPP frames
    //
    while (MFX_ERR_NONE <= sts) {
        nEncSurfIdx = GetFreeSurfaceIndex(pVPPSurfacesVPPOutEnc, nSurfNumVPPOutEnc);    // Find free output frame surface
        MSDK_CHECK_ERROR(MFX_ERR_NOT_FOUND, nEncSurfIdx, MFX_ERR_MEMORY_ALLOC);

        for (;;) {
            // Process a frame asychronously (returns immediately)
            sts = MFXVideoVPP_RunFrameVPPAsync(session, NULL, pVPPSurfacesVPPOutEnc[nEncSurfIdx], NULL, &syncpVPP);
            if (MFX_WRN_DEVICE_BUSY == sts) {
                MSDK_SLEEP(1);  // Wait if device is busy, then repeat the same call
            } else
                break;
        }

        MSDK_BREAK_ON_ERROR(sts);

        for (;;) {
            // Encode a frame asychronously (returns immediately)
            sts = MFXVideoENCODE_EncodeFrameAsync(session, NULL, pVPPSurfacesVPPOutEnc[nEncSurfIdx], &mfxBS, &syncpEnc);

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
            sts = MFXVideoCORE_SyncOperation(session, syncpEnc, 60000);   // Synchronize. Wait until encoded frame is ready
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

            sts = WriteBitStreamFrame(&mfxBS, fSink);
            MSDK_BREAK_ON_ERROR(sts);

            ++nFrame;
            if (bEnableOutput) {
                fprintf(stderr, "Stage 2: Frame number: %d\n", nFrame);
            }
        }
    }

    // MFX_ERR_MORE_DATA indicates that there are no more buffered frames, exit in case of other errors
    MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_DATA);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    //
    // Stage 3: Retrieve the buffered encoder frames
    //
    while (MFX_ERR_NONE <= sts) {
        for (;;) {
            // Encode a frame asychronously (returns immediately)
            sts = MFXVideoENCODE_EncodeFrameAsync(session, NULL, NULL, &mfxBS, &syncpEnc);

            if (MFX_ERR_NONE < sts && !syncpEnc) {  // Repeat the call if warning and no output
                if (MFX_WRN_DEVICE_BUSY == sts)
                    MSDK_SLEEP(1);  // Wait if device is busy, then repeat the same call
            } else if (MFX_ERR_NONE < sts && syncpEnc) {
                sts = MFX_ERR_NONE;     // Ignore warnings if output is available
                break;
            } else
                break;
        }

        if (MFX_ERR_NONE == sts) {
            sts = MFXVideoCORE_SyncOperation(session, syncpEnc, 60000);   // Synchronize. Wait until encoded frame is ready
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

            sts = WriteBitStreamFrame(&mfxBS, fSink);
            MSDK_BREAK_ON_ERROR(sts);

            ++nFrame;
            if (bEnableOutput) {
                fprintf(stderr, "Stage 3: Frame number: %d\n", nFrame);
            }
        }
    }

    // MFX_ERR_MORE_DATA indicates that there are no more buffered frames, exit in case of other errors
    MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_DATA);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // ===================================================================
    // Clean up resources
    //  - It is recommended to close Media SDK components first, before releasing allocated surfaces, since
    //    some surfaces may still be locked by internal Media SDK resources.

    MFXVideoENCODE_Close(session);
    MFXVideoVPP_Close(session);
    // session closed automatically on destruction

    for (i = 0; i < nSurfNumVPPIn; i++) {
        free(pmfxSurfacesVPPIn[i]);
        pmfxSurfacesVPPIn[i] = NULL;
    }
    free(pmfxSurfacesVPPIn);
    pmfxSurfacesVPPIn = NULL;
    for (i = 0; i < nSurfNumVPPOutEnc; i++) {
        free(pVPPSurfacesVPPOutEnc[i]);
        pVPPSurfacesVPPOutEnc[i] = NULL;
    }
    free(pVPPSurfacesVPPOutEnc);
    pVPPSurfacesVPPOutEnc = NULL;
    free(mfxBS.Data);
    mfxBS.Data = NULL;
    free(extDoNotUse.AlgList);
    extDoNotUse.AlgList = NULL;

    mfxAllocator.Free(mfxAllocator.pthis, &mfxResponseVPPIn);
    mfxAllocator.Free(mfxAllocator.pthis, &mfxResponseVPPOutEnc);

    if (fSource)
        fclose(fSource);
    if (fSink)
        fclose(fSink);

    Release();

    return 0;
}
