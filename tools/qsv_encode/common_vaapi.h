/*****************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or
nondisclosure agreement with Intel Corporation and may not be copied
or disclosed except in accordance with the terms of that agreement.
Copyright(c) 2005-2014 Intel Corporation. All Rights Reserved.

*****************************************************************************/

#pragma once

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <new>
#include <stdlib.h>
#include <assert.h>

#include "mfxvideo++.h"

#include "va/va.h"
#include "va/va_drm.h"

// =================================================================
// VAAPI functionality required to manage VA surfaces
mfxStatus CreateVAEnvDRM(mfxHDL* displayHandle);
void CleanupVAEnvDRM();
void ClearYUVSurfaceVAAPI(mfxMemId memId);
void ClearRGBSurfaceVAAPI(mfxMemId memId);

// utility
mfxStatus va_to_mfx_status(VAStatus va_res);
