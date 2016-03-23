/*****************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or
nondisclosure agreement with Intel Corporation and may not be copied
or disclosed except in accordance with the terms of that agreement.
Copyright(c) 2005-2014 Intel Corporation. All Rights Reserved.

*****************************************************************************/

#include "mfxvideo.h"
#if (MFX_VERSION_MAJOR == 1) && (MFX_VERSION_MINOR < 8)
#include "mfxlinux.h"
#endif

#include "common_utils.h"
#include "common_vaapi.h"

/* =====================================================
 * Linux implementation of OS-specific utility functions
 */

mfxStatus Initialize(mfxIMPL impl, mfxVersion ver, mfxSession *pSession, mfxFrameAllocator* pmfxAllocator, bool bCreateSharedHandles)
{
    mfxStatus sts = MFX_ERR_NONE;

    // Initialize Intel Media SDK Session
    sts = MFXInit(impl, &ver, pSession);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // Create VA display
    mfxHDL displayHandle = { 0 };
    sts = CreateVAEnvDRM(&displayHandle);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // Provide VA display handle to Media SDK
    sts = MFXVideoCORE_SetHandle(*pSession, MFX_HANDLE_VA_DISPLAY, displayHandle);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // If mfxFrameAllocator is provided it means we need to setup  memory allocator
    if (pmfxAllocator) {
        pmfxAllocator->pthis  = *pSession; // We use Media SDK session ID as the allocation identifier
        pmfxAllocator->Alloc  = simple_alloc;
        pmfxAllocator->Free   = simple_free;
        pmfxAllocator->Lock   = simple_lock;
        pmfxAllocator->Unlock = simple_unlock;
        pmfxAllocator->GetHDL = simple_gethdl;

        // Since we are using video memory we must provide Media SDK with an external allocator
        sts = MFXVideoCORE_SetFrameAllocator(*pSession, pmfxAllocator);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }

    return sts;
}

void Release()
{
    CleanupVAEnvDRM();
}

void mfxGetTime(mfxTime* timestamp)
{
    clock_gettime(CLOCK_REALTIME, timestamp);
}

double TimeDiffMsec(mfxTime tfinish, mfxTime tstart)
{
    double result;
    long long elapsed_nsec = tfinish.tv_nsec - tstart.tv_nsec;
    long long elapsed_sec = tfinish.tv_sec - tstart.tv_sec;

    //if (tstart.tv_sec==0) return -1;

    //timespec uses two fields -- check if borrowing necessary
    if (elapsed_nsec < 0) {
        elapsed_sec -= 1;
        elapsed_nsec += 1000000000;
    }
    //return total converted to milliseconds
    result = (double)elapsed_sec *1000.0;
    result += (double)elapsed_nsec / 1000000;

    return result;
}

void ClearYUVSurfaceVMem(mfxMemId memId)
{
    ClearYUVSurfaceVAAPI(memId);
}

void ClearRGBSurfaceVMem(mfxMemId memId)
{
    ClearRGBSurfaceVAAPI(memId);
}
