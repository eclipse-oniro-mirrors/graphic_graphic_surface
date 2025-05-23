/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <gtest/gtest.h>
#include "iconsumer_surface.h"
#include <iservice_registry.h>
#include <native_window.h>
#include <securec.h>
#include <ctime>
#include "buffer_log.h"
#include "external_window.h"
#include "surface_utils.h"
#include "sync_fence.h"
#include "ipc_inner_object.h"
#include "ipc_cparcel.h"

using namespace std;
using namespace testing;
using namespace testing::ext;

namespace OHOS::Rosen {
class BufferConsumerListener : public IBufferConsumerListener {
public:
    void OnBufferAvailable() override
    {
    }
};

static OHExtDataHandle *AllocOHExtDataHandle(uint32_t reserveInts)
{
    size_t handleSize = sizeof(OHExtDataHandle) + (sizeof(int32_t) * reserveInts);
    OHExtDataHandle *handle = static_cast<OHExtDataHandle *>(malloc(handleSize));
    if (handle == nullptr) {
        BLOGE("AllocOHExtDataHandle malloc %zu failed", handleSize);
        return nullptr;
    }
    auto ret = memset_s(handle, handleSize, 0, handleSize);
    if (ret != EOK) {
        BLOGE("AllocOHExtDataHandle memset_s failed");
        free(handle);
        return nullptr;
    }
    handle->fd = -1;
    handle->reserveInts = reserveInts;
    for (uint32_t i = 0; i < reserveInts; i++) {
        handle->reserve[i] = -1;
    }
    return handle;
}

static void FreeOHExtDataHandle(OHExtDataHandle *handle)
{
    if (handle == nullptr) {
        BLOGW("FreeOHExtDataHandle with nullptr handle");
        return ;
    }
    if (handle->fd >= 0) {
        close(handle->fd);
        handle->fd = -1;
    }
    free(handle);
}

class NativeWindowTest : public testing::Test {
public:
    static void SetUpTestCase();
    static void TearDownTestCase();

    static inline BufferRequestConfig requestConfig = {};
    static inline BufferFlushConfig flushConfig = {};
    static inline sptr<OHOS::IConsumerSurface> cSurface = nullptr;
    static inline sptr<OHOS::IBufferProducer> producer = nullptr;
    static inline sptr<OHOS::Surface> pSurface = nullptr;
    static inline sptr<OHOS::SurfaceBuffer> sBuffer = nullptr;
    static inline NativeWindow* nativeWindow = nullptr;
    static inline NativeWindowBuffer* nativeWindowBuffer = nullptr;
    static inline uint32_t firstSeqnum = 0;
};

void NativeWindowTest::SetUpTestCase()
{
    requestConfig = {
        .width = 0x100,  // small
        .height = 0x100, // small
        .strideAlignment = 0x8,
        .format = GRAPHIC_PIXEL_FMT_RGBA_8888,
        .usage = BUFFER_USAGE_CPU_READ | BUFFER_USAGE_CPU_WRITE | BUFFER_USAGE_MEM_DMA,
        .timeout = 0,
    };

    cSurface = IConsumerSurface::Create();
    sptr<IBufferConsumerListener> listener = new BufferConsumerListener();
    cSurface->RegisterConsumerListener(listener);
    producer = cSurface->GetProducer();
    pSurface = Surface::CreateSurfaceAsProducer(producer);
    int32_t fence;
    pSurface->RequestBuffer(sBuffer, fence, requestConfig);
    firstSeqnum = sBuffer->GetSeqNum();
}

void NativeWindowTest::TearDownTestCase()
{
    flushConfig = { .damage = {
        .w = 0x100,
        .h = 0x100,
    } };
    pSurface->FlushBuffer(sBuffer, -1, flushConfig);
    sBuffer = nullptr;
    cSurface = nullptr;
    producer = nullptr;
    pSurface = nullptr;
    OH_NativeWindow_DestroyNativeWindow(nativeWindow);
    nativeWindow = nullptr;
    nativeWindowBuffer = nullptr;
}

/*
* Function: OH_NativeWindow_CreateNativeWindow
* Type: Function
* Rank: Important(2)
* EnvConditions: N/A
* CaseDescription: 1. call OH_NativeWindow_CreateNativeWindow by abnormal input
*                  2. check ret
 */
HWTEST_F(NativeWindowTest, CreateNativeWindow001, Function | MediumTest | Level2)
{
    ASSERT_EQ(OH_NativeWindow_CreateNativeWindow(nullptr), nullptr);
}

/*
* Function: OH_NativeWindow_CreateNativeWindow
* Type: Function
* Rank: Important(2)
* EnvConditions: N/A
* CaseDescription: 1. call OH_NativeWindow_CreateNativeWindow
*                  2. check ret
 */
HWTEST_F(NativeWindowTest, CreateNativeWindow002, Function | MediumTest | Level2)
{
    nativeWindow = OH_NativeWindow_CreateNativeWindow(&pSurface);
    ASSERT_NE(nativeWindow, nullptr);
}

/*
* Function: OH_NativeWindow_CreateNativeWindow
* Type: Function
* Rank: Important(2)
* EnvConditions: N/A
* CaseDescription: 1. call OH_NativeWindow_CreateNativeWindow
*                  2. check ret
 */
HWTEST_F(NativeWindowTest, CreateNativeWindow003, Function | MediumTest | Level2)
{
    uint64_t surfaceId = 0;
    int32_t ret = OH_NativeWindow_GetSurfaceId(nativeWindow, &surfaceId);
    ASSERT_EQ(ret, OHOS::GSERROR_OK);
    ASSERT_EQ(surfaceId, pSurface->GetUniqueId());
}

/*
* Function: OH_NativeWindow_CreateNativeWindow
* Type: Function
* Rank: Important(2)
* EnvConditions: N/A
* CaseDescription: 1. call OH_NativeWindow_CreateNativeWindow
*                  2. check ret
 */
HWTEST_F(NativeWindowTest, CreateNativeWindow004, Function | MediumTest | Level2)
{
    sptr<OHOS::Surface> surfaceTmp = nullptr;
    auto nativeWindowTmp = OH_NativeWindow_CreateNativeWindow(&surfaceTmp);
    ASSERT_EQ(nativeWindowTmp, nullptr);
}

/*
* Function: OH_NativeWindow_CreateNativeWindowFromSurfaceId
* Type: Function
* Rank: Important(2)
* EnvConditions: N/A
* CaseDescription: 1. call OH_NativeWindow_CreateNativeWindowFromSurfaceId
*                  2. check ret
 */
HWTEST_F(NativeWindowTest, CreateNativeWindowFromSurfaceId001, Function | MediumTest | Level2)
{
    uint64_t surfaceId = static_cast<uint64_t>(pSurface->GetUniqueId());
    OHNativeWindow *window = nullptr;
    int32_t ret = OH_NativeWindow_CreateNativeWindowFromSurfaceId(surfaceId, &window);
    ASSERT_EQ(ret, OHOS::GSERROR_OK);
    surfaceId = 0;
    ret = OH_NativeWindow_GetSurfaceId(window, &surfaceId);
    ASSERT_EQ(ret, OHOS::GSERROR_OK);
    ASSERT_EQ(surfaceId, pSurface->GetUniqueId());
    OH_NativeWindow_DestroyNativeWindow(window);
}

/*
* Function: OH_NativeWindow_CreateNativeWindowFromSurfaceId
* Type: Function
* Rank: Important(2)
* EnvConditions: N/A
* CaseDescription: 1. call OH_NativeWindow_CreateNativeWindowFromSurfaceId
*                  2. check ret
 */
HWTEST_F(NativeWindowTest, CreateNativeWindowFromSurfaceId002, Function | MediumTest | Level2)
{
    int32_t ret = OH_NativeWindow_CreateNativeWindowFromSurfaceId(0, nullptr);
    ASSERT_EQ(ret, OHOS::GSERROR_INVALID_ARGUMENTS);
    ret = OH_NativeWindow_GetSurfaceId(nullptr, nullptr);
    ASSERT_EQ(ret, OHOS::GSERROR_INVALID_ARGUMENTS);
}

/*
* Function: OH_NativeWindow_CreateNativeWindowFromSurfaceId
* Type: Function
* Rank: Important(2)
* EnvConditions: N/A
* CaseDescription: 1. call OH_NativeWindow_CreateNativeWindowFromSurfaceId
*                  2. check ret
 */
HWTEST_F(NativeWindowTest, CreateNativeWindowFromSurfaceId003, Function | MediumTest | Level2)
{
    sptr<OHOS::IConsumerSurface> cSurfaceTmp = IConsumerSurface::Create();
    sptr<IBufferConsumerListener> listener = new BufferConsumerListener();
    cSurfaceTmp->RegisterConsumerListener(listener);
    sptr<OHOS::IBufferProducer> producerTmp = cSurfaceTmp->GetProducer();
    sptr<OHOS::Surface> pSurfaceTmp = Surface::CreateSurfaceAsProducer(producerTmp);

    uint64_t surfaceId = static_cast<uint64_t>(pSurfaceTmp->GetUniqueId());
    auto utils = SurfaceUtils::GetInstance();
    utils->Add(surfaceId, pSurfaceTmp);
    OHNativeWindow *nativeWindowTmp = nullptr;
    int32_t ret = OH_NativeWindow_CreateNativeWindowFromSurfaceId(0xFFFFFFFF, &nativeWindowTmp);
    ASSERT_EQ(ret, OHOS::GSERROR_INVALID_ARGUMENTS);
    ret = OH_NativeWindow_CreateNativeWindowFromSurfaceId(surfaceId, &nativeWindowTmp);
    ASSERT_EQ(ret, OHOS::GSERROR_OK);
    surfaceId = 0;
    ret = OH_NativeWindow_GetSurfaceId(nativeWindowTmp, &surfaceId);
    ASSERT_EQ(ret, OHOS::GSERROR_OK);
    ASSERT_EQ(surfaceId, pSurfaceTmp->GetUniqueId());

    cSurfaceTmp = nullptr;
    producerTmp = nullptr;
    pSurfaceTmp = nullptr;
    OH_NativeWindow_DestroyNativeWindow(nativeWindowTmp);
}

/*
* Function: OH_NativeWindow_NativeWindowHandleOpt
* Type: Function
* Rank: Important(2)
* EnvConditions: N/A
* CaseDescription: 1. call OH_NativeWindow_NativeWindowHandleOpt by abnormal input
*                  2. check ret
 */
HWTEST_F(NativeWindowTest, HandleOpt001, Function | MediumTest | Level2)
{
    int code = SET_USAGE;
    uint64_t usage = BUFFER_USAGE_CPU_READ;
    ASSERT_EQ(OH_NativeWindow_NativeWindowHandleOpt(nullptr, code, usage), OHOS::GSERROR_INVALID_ARGUMENTS);
}

/*
* Function: OH_NativeWindow_NativeWindowHandleOpt
* Type: Function
* Rank: Important(2)
* EnvConditions: N/A
* CaseDescription: 1. call OH_NativeWindow_NativeWindowHandleOpt by different param
*                  2. check ret
 */
HWTEST_F(NativeWindowTest, HandleOpt002, Function | MediumTest | Level2)
{
    int code = SET_USAGE;
    uint64_t usageSet = BUFFER_USAGE_CPU_READ;
    ASSERT_EQ(OH_NativeWindow_NativeWindowHandleOpt(nativeWindow, code, usageSet), OHOS::GSERROR_OK);

    code = GET_USAGE;
    uint64_t usageGet = BUFFER_USAGE_CPU_WRITE;
    ASSERT_EQ(OH_NativeWindow_NativeWindowHandleOpt(nativeWindow, code, &usageGet), OHOS::GSERROR_OK);
    ASSERT_EQ(usageSet, usageGet);
}

/*
* Function: OH_NativeWindow_NativeWindowHandleOpt
* Type: Function
* Rank: Important(2)
* EnvConditions: N/A
* CaseDescription: 1. call OH_NativeWindow_NativeWindowHandleOpt by different param
*                  2. check ret
 */
HWTEST_F(NativeWindowTest, HandleOpt003, Function | MediumTest | Level2)
{
    int code = SET_BUFFER_GEOMETRY;
    int32_t heightSet = 0x100;
    int32_t widthSet = 0x100;
    ASSERT_EQ(OH_NativeWindow_NativeWindowHandleOpt(nativeWindow, code, heightSet, widthSet), OHOS::GSERROR_OK);

    code = GET_BUFFER_GEOMETRY;
    int32_t heightGet = 0;
    int32_t widthGet = 0;
    ASSERT_EQ(OH_NativeWindow_NativeWindowHandleOpt(nativeWindow, code, &heightGet, &widthGet), OHOS::GSERROR_OK);
    ASSERT_EQ(heightSet, heightGet);
    ASSERT_EQ(widthSet, widthGet);
}

/*
* Function: OH_NativeWindow_NativeWindowHandleOpt
* Type: Function
* Rank: Important(2)
* EnvConditions: N/A
* CaseDescription: 1. call OH_NativeWindow_NativeWindowHandleOpt by different param
*                  2. check ret
 */
HWTEST_F(NativeWindowTest, HandleOpt004, Function | MediumTest | Level2)
{
    int code = SET_FORMAT;
    int32_t formatSet = GRAPHIC_PIXEL_FMT_RGBA_8888;
    ASSERT_EQ(OH_NativeWindow_NativeWindowHandleOpt(nativeWindow, code, formatSet), OHOS::GSERROR_OK);

    code = GET_FORMAT;
    int32_t formatGet = GRAPHIC_PIXEL_FMT_CLUT8;
    ASSERT_EQ(OH_NativeWindow_NativeWindowHandleOpt(nativeWindow, code, &formatGet), OHOS::GSERROR_OK);
    ASSERT_EQ(formatSet, formatGet);
}

/*
* Function: OH_NativeWindow_NativeWindowHandleOpt
* Type: Function
* Rank: Important(2)
* EnvConditions: N/A
* CaseDescription: 1. call OH_NativeWindow_NativeWindowHandleOpt by different param
*                  2. check ret
 */
HWTEST_F(NativeWindowTest, HandleOpt005, Function | MediumTest | Level2)
{
    int code = SET_STRIDE;
    int32_t strideSet = 0x8;
    ASSERT_EQ(OH_NativeWindow_NativeWindowHandleOpt(nativeWindow, code, strideSet), OHOS::GSERROR_OK);

    code = GET_STRIDE;
    int32_t strideGet = 0;
    ASSERT_EQ(OH_NativeWindow_NativeWindowHandleOpt(nativeWindow, code, &strideGet), OHOS::GSERROR_OK);
    ASSERT_EQ(strideSet, strideGet);
}

/*
* Function: OH_NativeWindow_NativeWindowHandleOpt
* Type: Function
* Rank: Important(2)
* EnvConditions: N/A
* CaseDescription: 1. call OH_NativeWindow_NativeWindowHandleOpt by different param
*                  2. check ret
 */
HWTEST_F(NativeWindowTest, HandleOpt006, Function | MediumTest | Level2)
{
    int code = SET_COLOR_GAMUT;
    int32_t colorGamutSet = static_cast<int32_t>(GraphicColorGamut::GRAPHIC_COLOR_GAMUT_DCI_P3);
    ASSERT_EQ(OH_NativeWindow_NativeWindowHandleOpt(nativeWindow, code, colorGamutSet), OHOS::GSERROR_OK);

    code = GET_COLOR_GAMUT;
    int32_t colorGamutGet = 0;
    ASSERT_EQ(OH_NativeWindow_NativeWindowHandleOpt(nativeWindow, code, &colorGamutGet), OHOS::GSERROR_OK);
    ASSERT_EQ(colorGamutSet, colorGamutGet);
}

/*
* Function: OH_NativeWindow_NativeWindowHandleOpt
* Type: Function
* Rank: Important(2)
* EnvConditions: N/A
* CaseDescription: 1. call OH_NativeWindow_NativeWindowHandleOpt by different param
*                  2. check ret
 */
HWTEST_F(NativeWindowTest, HandleOpt007, Function | MediumTest | Level2)
{
    int code = SET_TIMEOUT;
    int32_t timeoutSet = 10;  // 10: for test
    ASSERT_EQ(OH_NativeWindow_NativeWindowHandleOpt(nativeWindow, code, timeoutSet), OHOS::GSERROR_OK);

    code = GET_TIMEOUT;
    int32_t timeoutGet = 0;
    ASSERT_EQ(OH_NativeWindow_NativeWindowHandleOpt(nativeWindow, code, &timeoutGet), OHOS::GSERROR_OK);
    ASSERT_EQ(timeoutSet, timeoutGet);
}

/*
* Function: OH_NativeWindow_NativeWindowHandleOpt
* Type: Function
* Rank: Important(2)
* EnvConditions: N/A
* CaseDescription: 1. call OH_NativeWindow_NativeWindowHandleOpt by different param
*                  2. check ret
 */
HWTEST_F(NativeWindowTest, HandleOpt008, Function | MediumTest | Level1)
{
    int code = GET_TRANSFORM;
    int32_t transform = 0;
    ASSERT_EQ(OH_NativeWindow_NativeWindowHandleOpt(nativeWindow, code, &transform), OHOS::GSERROR_OK);
    transform = GraphicTransformType::GRAPHIC_ROTATE_90;
    code = SET_TRANSFORM;
    ASSERT_EQ(OH_NativeWindow_NativeWindowHandleOpt(nativeWindow, code, transform), OHOS::GSERROR_OK);
    int32_t transformTmp = 0;
    code = GET_TRANSFORM;
    ASSERT_EQ(OH_NativeWindow_NativeWindowHandleOpt(nativeWindow, code, &transformTmp), OHOS::GSERROR_OK);
    ASSERT_EQ(transformTmp, GraphicTransformType::GRAPHIC_ROTATE_90);
    nativeWindow->surface->SetTransform(GraphicTransformType::GRAPHIC_ROTATE_180);
    ASSERT_EQ(OH_NativeWindow_NativeWindowHandleOpt(nativeWindow, code, &transformTmp), OHOS::GSERROR_OK);
    ASSERT_EQ(transformTmp, GraphicTransformType::GRAPHIC_ROTATE_180);
}

/*
* Function: OH_NativeWindow_NativeWindowHandleOpt
* Type: Function
* Rank: Important(2)
* EnvConditions: N/A
* CaseDescription: 1. call OH_NativeWindow_NativeWindowHandleOpt by different param
*                  2. check ret
 */
HWTEST_F(NativeWindowTest, HandleOpt009, Function | MediumTest | Level1)
{
    int code = GET_BUFFERQUEUE_SIZE;
    int32_t queueSize = 0;
    ASSERT_EQ(OH_NativeWindow_NativeWindowHandleOpt(nativeWindow, code, &queueSize), OHOS::GSERROR_OK);
    ASSERT_EQ(queueSize, 3);
    nativeWindow->surface->SetQueueSize(5);
    ASSERT_EQ(OH_NativeWindow_NativeWindowHandleOpt(nativeWindow, code, &queueSize), OHOS::GSERROR_OK);
    ASSERT_EQ(queueSize, 5);
}

/*
* Function: OH_NativeWindow_NativeWindowHandleOpt
* Type: Function
* Rank: Important(2)
* EnvConditions: N/A
* CaseDescription: 1. call OH_NativeWindow_NativeWindowHandleOpt by different param
*                  2. check ret
 */
HWTEST_F(NativeWindowTest, HandleOpt010, Function | MediumTest | Level2)
{
    int code = SET_USAGE;
    uint64_t usageSet = NATIVEBUFFER_USAGE_HW_RENDER | NATIVEBUFFER_USAGE_HW_TEXTURE |
    NATIVEBUFFER_USAGE_CPU_READ_OFTEN | NATIVEBUFFER_USAGE_ALIGNMENT_512;
    ASSERT_EQ(OH_NativeWindow_NativeWindowHandleOpt(nativeWindow, code, usageSet), OHOS::GSERROR_OK);

    code = GET_USAGE;
    uint64_t usageGet = usageSet;
    ASSERT_EQ(OH_NativeWindow_NativeWindowHandleOpt(nativeWindow, code, &usageGet), OHOS::GSERROR_OK);
    ASSERT_EQ(usageSet, usageGet);

    code = SET_FORMAT;
    int32_t formatSet = NATIVEBUFFER_PIXEL_FMT_YCBCR_P010;
    ASSERT_EQ(OH_NativeWindow_NativeWindowHandleOpt(nativeWindow, code, formatSet), OHOS::GSERROR_OK);

    code = GET_FORMAT;
    int32_t formatGet = NATIVEBUFFER_PIXEL_FMT_YCBCR_P010;
    ASSERT_EQ(OH_NativeWindow_NativeWindowHandleOpt(nativeWindow, code, &formatGet), OHOS::GSERROR_OK);
    ASSERT_EQ(formatSet, formatGet);
}

/*
* Function: OH_NativeWindow_NativeWindowHandleOpt
* Type: Function
* Rank: Important(2)
* EnvConditions: N/A
* CaseDescription: 1. call OH_NativeWindow_NativeWindowHandleOpt by different param
*                  2. check ret
 */
HWTEST_F(NativeWindowTest, HandleOpt011, Function | MediumTest | Level1)
{
    int code = SET_SOURCE_TYPE;
    OHSurfaceSource typeSet = OHSurfaceSource::OH_SURFACE_SOURCE_GAME;
    ASSERT_EQ(OH_NativeWindow_NativeWindowHandleOpt(nativeWindow, code, typeSet), OHOS::GSERROR_OK);

    code = GET_SOURCE_TYPE;
    OHSurfaceSource typeGet = OHSurfaceSource::OH_SURFACE_SOURCE_DEFAULT;
    ASSERT_EQ(OH_NativeWindow_NativeWindowHandleOpt(nativeWindow, code, &typeGet), OHOS::GSERROR_OK);
    ASSERT_EQ(typeSet, typeGet);
}

/*
* Function: OH_NativeWindow_NativeWindowHandleOpt
* Type: Function
* Rank: Important(2)
* EnvConditions: N/A
* CaseDescription: 1. call OH_NativeWindow_NativeWindowHandleOpt by different param
*                  2. check ret
 */
HWTEST_F(NativeWindowTest, HandleOpt012, Function | MediumTest | Level1)
{
    int code = SET_APP_FRAMEWORK_TYPE;
    const char* typeSet = "test";
    ASSERT_EQ(OH_NativeWindow_NativeWindowHandleOpt(nativeWindow, code, typeSet), OHOS::GSERROR_OK);

    code = GET_APP_FRAMEWORK_TYPE;
    const char* typeGet;
    ASSERT_EQ(OH_NativeWindow_NativeWindowHandleOpt(nativeWindow, code, &typeGet), OHOS::GSERROR_OK);
    ASSERT_EQ(0, strcmp(typeSet, typeGet));
}

/*
* Function: OH_NativeWindow_NativeWindowHandleOpt
* Type: Function
* Rank: Important(2)
* EnvConditions: N/A
* CaseDescription: 1. call OH_NativeWindow_NativeWindowHandleOpt by different param
*                  2. check ret
 */
HWTEST_F(NativeWindowTest, HandleOpt013, Function | MediumTest | Level1)
{
    int code = SET_HDR_WHITE_POINT_BRIGHTNESS;
    float brightness = 0.8;
    ASSERT_EQ(OH_NativeWindow_NativeWindowHandleOpt(nativeWindow, code, brightness), OHOS::GSERROR_OK);

    code = SET_SDR_WHITE_POINT_BRIGHTNESS;
    brightness = 0.5;
    ASSERT_EQ(OH_NativeWindow_NativeWindowHandleOpt(nativeWindow, code, brightness), OHOS::GSERROR_OK);

    ASSERT_EQ(fabs(cSurface->GetHdrWhitePointBrightness() - 0.8) < 1e-6, true);
    ASSERT_EQ(fabs(cSurface->GetSdrWhitePointBrightness() - 0.5) < 1e-6, true);

    code = SET_HDR_WHITE_POINT_BRIGHTNESS;
    brightness = 1.8;
    ASSERT_EQ(OH_NativeWindow_NativeWindowHandleOpt(nativeWindow, code, brightness), OHOS::GSERROR_OK);
    ASSERT_EQ(fabs(cSurface->GetHdrWhitePointBrightness() - 0.8) < 1e-6, true);
    brightness = -0.5;
    ASSERT_EQ(OH_NativeWindow_NativeWindowHandleOpt(nativeWindow, code, brightness), OHOS::GSERROR_OK);
    ASSERT_EQ(fabs(cSurface->GetHdrWhitePointBrightness() - 0.8) < 1e-6, true);
    brightness = 0.5;
    ASSERT_EQ(OH_NativeWindow_NativeWindowHandleOpt(nativeWindow, code, brightness), OHOS::GSERROR_OK);
    ASSERT_EQ(fabs(cSurface->GetHdrWhitePointBrightness() - 0.5) < 1e-6, true);

    code = SET_SDR_WHITE_POINT_BRIGHTNESS;
    brightness = 1.5;
    ASSERT_EQ(OH_NativeWindow_NativeWindowHandleOpt(nativeWindow, code, brightness), OHOS::GSERROR_OK);
    ASSERT_EQ(fabs(cSurface->GetSdrWhitePointBrightness() - 0.5) < 1e-6, true);
    brightness = -0.1;
    ASSERT_EQ(OH_NativeWindow_NativeWindowHandleOpt(nativeWindow, code, brightness), OHOS::GSERROR_OK);
    ASSERT_EQ(fabs(cSurface->GetSdrWhitePointBrightness() - 0.5) < 1e-6, true);
    brightness = 0.8;
    ASSERT_EQ(OH_NativeWindow_NativeWindowHandleOpt(nativeWindow, code, brightness), OHOS::GSERROR_OK);
    ASSERT_EQ(fabs(cSurface->GetSdrWhitePointBrightness() - 0.8) < 1e-6, true);
}

/*
* Function: OH_NativeWindow_NativeWindowAttachBuffer
* Type: Function
* Rank: Important(2)
* EnvConditions: N/A
* CaseDescription: 1. call OH_NativeWindow_NativeWindowAttachBuffer by abnormal input
*                  2. check ret
 */
HWTEST_F(NativeWindowTest, NativeWindowAttachBuffer001, Function | MediumTest | Level1)
{
    ASSERT_EQ(OH_NativeWindow_NativeWindowAttachBuffer(nullptr, nullptr), OHOS::GSERROR_INVALID_ARGUMENTS);
    ASSERT_EQ(OH_NativeWindow_NativeWindowDetachBuffer(nullptr, nullptr), OHOS::GSERROR_INVALID_ARGUMENTS);
    ASSERT_EQ(OH_NativeWindow_NativeWindowAttachBuffer(nativeWindow, nullptr), OHOS::GSERROR_INVALID_ARGUMENTS);
    ASSERT_EQ(OH_NativeWindow_NativeWindowDetachBuffer(nativeWindow, nullptr), OHOS::GSERROR_INVALID_ARGUMENTS);
}

void SetNativeWindowConfig(NativeWindow *nativeWindow)
{
    int code = SET_USAGE;
    uint64_t usageSet = BUFFER_USAGE_CPU_READ;
    ASSERT_EQ(OH_NativeWindow_NativeWindowHandleOpt(nativeWindow, code, usageSet), OHOS::GSERROR_OK);

    code = SET_BUFFER_GEOMETRY;
    int32_t heightSet = 0x100;
    int32_t widthSet = 0x100;
    ASSERT_EQ(OH_NativeWindow_NativeWindowHandleOpt(nativeWindow, code, heightSet, widthSet), OHOS::GSERROR_OK);

    code = SET_FORMAT;
    int32_t formatSet = GRAPHIC_PIXEL_FMT_RGBA_8888;
    ASSERT_EQ(OH_NativeWindow_NativeWindowHandleOpt(nativeWindow, code, formatSet), OHOS::GSERROR_OK);

    code = SET_STRIDE;
    int32_t strideSet = 0x8;
    ASSERT_EQ(OH_NativeWindow_NativeWindowHandleOpt(nativeWindow, code, strideSet), OHOS::GSERROR_OK);

    code = SET_COLOR_GAMUT;
    int32_t colorGamutSet = static_cast<int32_t>(GraphicColorGamut::GRAPHIC_COLOR_GAMUT_DCI_P3);
    ASSERT_EQ(OH_NativeWindow_NativeWindowHandleOpt(nativeWindow, code, colorGamutSet), OHOS::GSERROR_OK);

    code = SET_TIMEOUT;
    int32_t timeoutSet = 10;  // 10: for test
    ASSERT_EQ(OH_NativeWindow_NativeWindowHandleOpt(nativeWindow, code, timeoutSet), OHOS::GSERROR_OK);
}

/*
* Function: OH_NativeWindow_NativeWindowAttachBuffer
* Type: Function
* Rank: Important(2)
* EnvConditions: N/A
* CaseDescription: 1. call OH_NativeWindow_NativeWindowAttachBuffer by normal input
*                  2. check ret
 */
HWTEST_F(NativeWindowTest, NativeWindowAttachBuffer002, Function | MediumTest | Level1)
{
    sptr<OHOS::IConsumerSurface> cSurfaceTmp = IConsumerSurface::Create();
    sptr<IBufferConsumerListener> listener = new BufferConsumerListener();
    cSurfaceTmp->RegisterConsumerListener(listener);
    sptr<OHOS::IBufferProducer> producerTmp = cSurfaceTmp->GetProducer();
    sptr<OHOS::Surface> pSurfaceTmp = Surface::CreateSurfaceAsProducer(producerTmp);

    NativeWindow *nativeWindowTmp = OH_NativeWindow_CreateNativeWindow(&pSurfaceTmp);
    ASSERT_NE(nativeWindowTmp, nullptr);
    SetNativeWindowConfig(nativeWindowTmp);

    NativeWindowBuffer *nativeWindowBuffer = nullptr;
    int fenceFd = -1;
    int32_t ret = OH_NativeWindow_NativeWindowRequestBuffer(nativeWindowTmp, &nativeWindowBuffer, &fenceFd);
    ASSERT_EQ(ret, GSERROR_OK);

    int code = GET_BUFFERQUEUE_SIZE;
    int32_t queueSize = 0;
    ASSERT_EQ(OH_NativeWindow_NativeWindowHandleOpt(nativeWindowTmp, code, &queueSize), OHOS::GSERROR_OK);
    ASSERT_EQ(queueSize, 3);

    ASSERT_EQ(OH_NativeWindow_NativeWindowDetachBuffer(nativeWindowTmp, nativeWindowBuffer), OHOS::GSERROR_OK);

    ASSERT_EQ(OH_NativeWindow_NativeWindowAttachBuffer(nativeWindow, nativeWindowBuffer), OHOS::GSERROR_OK);

    ASSERT_EQ(OH_NativeWindow_NativeWindowDetachBuffer(nativeWindow, nativeWindowBuffer), OHOS::GSERROR_OK);

    ASSERT_EQ(OH_NativeWindow_NativeWindowAttachBuffer(nativeWindowTmp, nativeWindowBuffer), OHOS::GSERROR_OK);
    ASSERT_EQ(OH_NativeWindow_NativeWindowHandleOpt(nativeWindowTmp, code, &queueSize), OHOS::GSERROR_OK);
    ASSERT_EQ(queueSize, 3);
    ASSERT_EQ(OH_NativeWindow_NativeWindowAttachBuffer(nativeWindowTmp, nativeWindowBuffer),
        OHOS::GSERROR_BUFFER_IS_INCACHE);

    struct Region *region = new Region();
    struct Region::Rect *rect = new Region::Rect();
    rect->x = 0x100;
    rect->y = 0x100;
    rect->w = 0x100;
    rect->h = 0x100;
    region->rects = rect;
    ret = OH_NativeWindow_NativeWindowFlushBuffer(nativeWindowTmp, nativeWindowBuffer, fenceFd, *region);
    ASSERT_EQ(ret, GSERROR_OK);

    OH_NativeWindow_DestroyNativeWindow(nativeWindowTmp);
}

void NativeWindowAttachBuffer003Test(NativeWindow *nativeWindowTmp, NativeWindow *nativeWindowTmp1)
{
    NativeWindowBuffer *nativeWindowBuffer1 = nullptr;
    int fenceFd = -1;
    int32_t ret = OH_NativeWindow_NativeWindowRequestBuffer(nativeWindowTmp, &nativeWindowBuffer1, &fenceFd);
    ASSERT_EQ(ret, GSERROR_OK);

    NativeWindowBuffer *nativeWindowBuffer2 = nullptr;
    fenceFd = -1;
    ret = OH_NativeWindow_NativeWindowRequestBuffer(nativeWindowTmp, &nativeWindowBuffer2, &fenceFd);
    ASSERT_EQ(ret, GSERROR_OK);

    NativeWindowBuffer *nativeWindowBuffer3 = nullptr;
    fenceFd = -1;
    ret = OH_NativeWindow_NativeWindowRequestBuffer(nativeWindowTmp, &nativeWindowBuffer3, &fenceFd);
    ASSERT_EQ(ret, GSERROR_OK);

    int code = GET_BUFFERQUEUE_SIZE;
    int32_t queueSize = 0;
    ASSERT_EQ(OH_NativeWindow_NativeWindowHandleOpt(nativeWindowTmp, code, &queueSize), OHOS::GSERROR_OK);
    ASSERT_EQ(queueSize, 3);

    ASSERT_EQ(OH_NativeWindow_NativeWindowDetachBuffer(nativeWindowTmp, nativeWindowBuffer1), OHOS::GSERROR_OK);
    ASSERT_EQ(OH_NativeWindow_NativeWindowDetachBuffer(nativeWindowTmp, nativeWindowBuffer2), OHOS::GSERROR_OK);
    ASSERT_EQ(OH_NativeWindow_NativeWindowDetachBuffer(nativeWindowTmp, nativeWindowBuffer3), OHOS::GSERROR_OK);

    NativeWindowBuffer *nativeWindowBuffer4 = nullptr;
    fenceFd = -1;
    ret = OH_NativeWindow_NativeWindowRequestBuffer(nativeWindowTmp, &nativeWindowBuffer4, &fenceFd);
    ASSERT_EQ(ret, GSERROR_OK);

    NativeWindowBuffer *nativeWindowBuffer10 = nullptr;
    fenceFd = -1;
    ret = OH_NativeWindow_NativeWindowRequestBuffer(nativeWindowTmp1, &nativeWindowBuffer10, &fenceFd);
    ASSERT_EQ(ret, GSERROR_OK);

    NativeWindowBuffer *nativeWindowBuffer11 = nullptr;
    fenceFd = -1;
    ret = OH_NativeWindow_NativeWindowRequestBuffer(nativeWindowTmp1, &nativeWindowBuffer11, &fenceFd);
    ASSERT_EQ(ret, GSERROR_OK);

    NativeWindowBuffer *nativeWindowBuffer12 = nullptr;
    fenceFd = -1;
    ret = OH_NativeWindow_NativeWindowRequestBuffer(nativeWindowTmp1, &nativeWindowBuffer12, &fenceFd);
    ASSERT_EQ(ret, GSERROR_OK);

    ASSERT_EQ(OH_NativeWindow_NativeWindowAttachBuffer(nativeWindowTmp1, nativeWindowBuffer1),
        OHOS::SURFACE_ERROR_BUFFER_QUEUE_FULL);
}

/*
* Function: OH_NativeWindow_NativeWindowAttachBuffer
* Type: Function
* Rank: Important(2)
* EnvConditions: N/A
* CaseDescription: 1. call OH_NativeWindow_NativeWindowAttachBuffer by normal input
*                  2. check ret
 */
HWTEST_F(NativeWindowTest, NativeWindowAttachBuffer003, Function | MediumTest | Level1)
{
    sptr<OHOS::IConsumerSurface> cSurfaceTmp = IConsumerSurface::Create();
    sptr<IBufferConsumerListener> listener = new BufferConsumerListener();
    cSurfaceTmp->RegisterConsumerListener(listener);
    sptr<OHOS::IBufferProducer> producerTmp = cSurfaceTmp->GetProducer();
    sptr<OHOS::Surface> pSurfaceTmp = Surface::CreateSurfaceAsProducer(producerTmp);

    NativeWindow *nativeWindowTmp = OH_NativeWindow_CreateNativeWindow(&pSurfaceTmp);
    ASSERT_NE(nativeWindowTmp, nullptr);
    SetNativeWindowConfig(nativeWindowTmp);

    sptr<OHOS::IConsumerSurface> cSurfaceTmp1 = IConsumerSurface::Create();
    sptr<IBufferConsumerListener> listener1 = new BufferConsumerListener();
    cSurfaceTmp1->RegisterConsumerListener(listener1);
    sptr<OHOS::IBufferProducer> producerTmp1 = cSurfaceTmp1->GetProducer();
    sptr<OHOS::Surface> pSurfaceTmp1 = Surface::CreateSurfaceAsProducer(producerTmp1);

    NativeWindow *nativeWindowTmp1 = OH_NativeWindow_CreateNativeWindow(&pSurfaceTmp1);
    ASSERT_NE(nativeWindowTmp1, nullptr);
    SetNativeWindowConfig(nativeWindowTmp1);

    NativeWindowAttachBuffer003Test(nativeWindowTmp, nativeWindowTmp1);

    OH_NativeWindow_DestroyNativeWindow(nativeWindowTmp);
    OH_NativeWindow_DestroyNativeWindow(nativeWindowTmp1);
}

/*
* Function: OH_NativeWindow_NativeWindowAttachBuffer
* Type: Function
* Rank: Important(2)
* EnvConditions: N/A
* CaseDescription: 1. call OH_NativeWindow_NativeWindowAttachBuffer by normal input
*                  2. check ret
 */
HWTEST_F(NativeWindowTest, NativeWindowAttachBuffer004, Function | MediumTest | Level1)
{
    sptr<OHOS::IConsumerSurface> cSurfaceTmp = IConsumerSurface::Create();
    sptr<IBufferConsumerListener> listener = new BufferConsumerListener();
    cSurfaceTmp->RegisterConsumerListener(listener);
    sptr<OHOS::IBufferProducer> producerTmp = cSurfaceTmp->GetProducer();
    sptr<OHOS::Surface> pSurfaceTmp = Surface::CreateSurfaceAsProducer(producerTmp);

    NativeWindow *nativeWindowTmp = OH_NativeWindow_CreateNativeWindow(&pSurfaceTmp);
    ASSERT_NE(nativeWindowTmp, nullptr);
    SetNativeWindowConfig(nativeWindowTmp);

    NativeWindowBuffer *nativeWindowBuffer = nullptr;
    int fenceFd = -1;
    int32_t ret = OH_NativeWindow_NativeWindowRequestBuffer(nativeWindowTmp, &nativeWindowBuffer, &fenceFd);
    ASSERT_EQ(ret, GSERROR_OK);

    struct Region *region = new Region();
    struct Region::Rect *rect = new Region::Rect();
    rect->x = 0x100;
    rect->y = 0x100;
    rect->w = 0x100;
    rect->h = 0x100;
    region->rects = rect;
    ret = OH_NativeWindow_NativeWindowFlushBuffer(nativeWindowTmp, nativeWindowBuffer, fenceFd, *region);
    ASSERT_EQ(ret, GSERROR_OK);

    ASSERT_EQ(OH_NativeWindow_NativeWindowDetachBuffer(nativeWindowTmp, nativeWindowBuffer),
        OHOS::SURFACE_ERROR_BUFFER_STATE_INVALID);

    ASSERT_EQ(OH_NativeWindow_NativeWindowDetachBuffer(nativeWindow, nativeWindowBuffer),
        OHOS::SURFACE_ERROR_BUFFER_NOT_INCACHE);

    OH_NativeWindow_DestroyNativeWindow(nativeWindowTmp);
}

/*
* Function: OH_NativeWindow_NativeWindowAttachBuffer
* Type: Function
* Rank: Important(2)
* EnvConditions: N/A
* CaseDescription: 1. call OH_NativeWindow_NativeWindowAttachBuffer by normal input
*                  2. check ret
 */
HWTEST_F(NativeWindowTest, NativeWindowAttachBuffer005, Function | MediumTest | Level1)
{
    sptr<OHOS::IConsumerSurface> cSurfaceTmp = IConsumerSurface::Create();
    sptr<IBufferConsumerListener> listener = new BufferConsumerListener();
    cSurfaceTmp->RegisterConsumerListener(listener);
    sptr<OHOS::IBufferProducer> producerTmp = cSurfaceTmp->GetProducer();
    sptr<OHOS::Surface> pSurfaceTmp = Surface::CreateSurfaceAsProducer(producerTmp);

    NativeWindow *nativeWindowTmp = OH_NativeWindow_CreateNativeWindow(&pSurfaceTmp);
    ASSERT_NE(nativeWindowTmp, nullptr);
    SetNativeWindowConfig(nativeWindowTmp);

    NativeWindowBuffer *nativeWindowBuffer = nullptr;
    int fenceFd = -1;
    int32_t ret = OH_NativeWindow_NativeWindowRequestBuffer(nativeWindowTmp, &nativeWindowBuffer, &fenceFd);
    ASSERT_EQ(ret, GSERROR_OK);

    ASSERT_EQ(cSurface->AttachBufferToQueue(nativeWindowBuffer->sfbuffer), GSERROR_OK);

    ASSERT_EQ(cSurface->DetachBufferFromQueue(nativeWindowBuffer->sfbuffer), GSERROR_OK);

    ASSERT_EQ(cSurface->AttachBufferToQueue(nativeWindowBuffer->sfbuffer), GSERROR_OK);

    sptr<SyncFence> fence = SyncFence::INVALID_FENCE;
    ASSERT_EQ(cSurface->ReleaseBuffer(nativeWindowBuffer->sfbuffer, fence), GSERROR_OK);

    OH_NativeWindow_DestroyNativeWindow(nativeWindowTmp);
}

/*
* Function: OH_NativeWindow_NativeWindowAttachBuffer
* Type: Function
* Rank: Important(2)
* EnvConditions: N/A
* CaseDescription: 1. call OH_NativeWindow_NativeWindowAttachBuffer by normal input
*                  2. check ret
 */
HWTEST_F(NativeWindowTest, NativeWindowAttachBuffer006, Function | MediumTest | Level1)
{
    sptr<OHOS::IConsumerSurface> cSurfaceTmp = IConsumerSurface::Create();
    sptr<IBufferConsumerListener> listener = new BufferConsumerListener();
    cSurfaceTmp->RegisterConsumerListener(listener);
    sptr<OHOS::IBufferProducer> producerTmp = cSurfaceTmp->GetProducer();
    sptr<OHOS::Surface> pSurfaceTmp = Surface::CreateSurfaceAsProducer(producerTmp);

    NativeWindow *nativeWindowTmp = OH_NativeWindow_CreateNativeWindow(&pSurfaceTmp);
    ASSERT_NE(nativeWindowTmp, nullptr);
    SetNativeWindowConfig(nativeWindowTmp);

    NativeWindowBuffer *nativeWindowBuffer1 = nullptr;
    int fenceFd = -1;
    int32_t ret = OH_NativeWindow_NativeWindowRequestBuffer(nativeWindowTmp, &nativeWindowBuffer1, &fenceFd);
    ASSERT_EQ(ret, GSERROR_OK);

    int code = GET_BUFFERQUEUE_SIZE;
    int32_t queueSize = 0;
    ASSERT_EQ(OH_NativeWindow_NativeWindowHandleOpt(nativeWindowTmp, code, &queueSize), OHOS::GSERROR_OK);
    ASSERT_EQ(queueSize, 3);
    clock_t startTime, endTime;
    startTime = clock();
    for (int32_t i = 0; i < 1000; i++) {
        ASSERT_EQ(OH_NativeWindow_NativeWindowDetachBuffer(nativeWindowTmp, nativeWindowBuffer1), OHOS::GSERROR_OK);
        ASSERT_EQ(OH_NativeWindow_NativeWindowAttachBuffer(nativeWindowTmp, nativeWindowBuffer1), OHOS::GSERROR_OK);
    }
    endTime = clock();
    cout << "DetachBuffer and AttachBuffer 1000 times cost time: " << (endTime - startTime) << "ms" << endl;
    ASSERT_EQ(OH_NativeWindow_NativeWindowHandleOpt(nativeWindowTmp, code, &queueSize), OHOS::GSERROR_OK);
    ASSERT_EQ(queueSize, 3);
    OH_NativeWindow_DestroyNativeWindow(nativeWindowTmp);
}

/*
* Function: OH_NativeWindow_CreateNativeWindowBufferFromSurfaceBuffer
* Type: Function
* Rank: Important(2)
* EnvConditions: N/A
* CaseDescription: 1. call OH_NativeWindow_CreateNativeWindowBufferFromSurfaceBuffer by abnormal input
*                  2. check ret
 */
HWTEST_F(NativeWindowTest, CreateNativeWindowBuffer001, Function | MediumTest | Level2)
{
    ASSERT_EQ(OH_NativeWindow_CreateNativeWindowBufferFromSurfaceBuffer(nullptr), nullptr);
}

/*
* Function: OH_NativeWindow_CreateNativeWindowBufferFromSurfaceBuffer
* Type: Function
* Rank: Important(2)
* EnvConditions: N/A
* CaseDescription: 1. call OH_NativeWindow_CreateNativeWindowBufferFromSurfaceBuffer
*                  2. check ret
 */
HWTEST_F(NativeWindowTest, CreateNativeWindowBuffer002, Function | MediumTest | Level2)
{
    nativeWindowBuffer = OH_NativeWindow_CreateNativeWindowBufferFromSurfaceBuffer(&sBuffer);
    ASSERT_NE(nativeWindowBuffer, nullptr);
}

/*
* Function: OH_NativeWindow_CreateNativeWindowBufferFromNativeBuffer
* Type: Function
* Rank: Important(2)
* EnvConditions: N/A
* CaseDescription: 1. call OH_NativeWindow_CreateNativeWindowBufferFromNativeBuffer
*                  2. check ret
*/
HWTEST_F(NativeWindowTest, CreateNativeWindowBuffer003, Function | MediumTest | Level2)
{
    OH_NativeBuffer* nativeBuffer = sBuffer->SurfaceBufferToNativeBuffer();
    ASSERT_NE(nativeBuffer, nullptr);
    NativeWindowBuffer* nwBuffer = OH_NativeWindow_CreateNativeWindowBufferFromNativeBuffer(nativeBuffer);
    ASSERT_NE(nwBuffer, nullptr);
    OH_NativeWindow_DestroyNativeWindowBuffer(nwBuffer);
}

/*
* Function: OH_NativeWindow_NativeWindowRequestBuffer
* Type: Function
* Rank: Important(2)
* EnvConditions: N/A
* CaseDescription: 1. call OH_NativeWindow_NativeWindowRequestBuffer by abnormal input
*                  2. check ret
 */
HWTEST_F(NativeWindowTest, RequestBuffer001, Function | MediumTest | Level2)
{
    ASSERT_EQ(OH_NativeWindow_NativeWindowRequestBuffer(nullptr, &nativeWindowBuffer, nullptr),
              OHOS::GSERROR_INVALID_ARGUMENTS);
}

/*
* Function: OH_NativeWindow_NativeWindowRequestBuffer
* Type: Function
* Rank: Important(2)
* EnvConditions: N/A
* CaseDescription: 1. call OH_NativeWindow_NativeWindowRequestBuffer by abnormal input
*                  2. check ret
 */
HWTEST_F(NativeWindowTest, RequestBuffer002, Function | MediumTest | Level2)
{
    ASSERT_EQ(OH_NativeWindow_NativeWindowRequestBuffer(nativeWindow, nullptr, nullptr),
              OHOS::GSERROR_INVALID_ARGUMENTS);
}

/*
* Function: OH_NativeWindow_GetBufferHandleFromNative
* Type: Function
* Rank: Important(2)
* EnvConditions: N/A
* CaseDescription: 1. call OH_NativeWindow_GetBufferHandleFromNative by abnormal input
*                  2. check ret
 */
HWTEST_F(NativeWindowTest, GetBufferHandle001, Function | MediumTest | Level2)
{
    ASSERT_EQ(OH_NativeWindow_GetBufferHandleFromNative(nullptr), nullptr);
}

/*
* Function: OH_NativeWindow_GetBufferHandleFromNative
* Type: Function
* Rank: Important(2)
* EnvConditions: N/A
* CaseDescription: 1. call OH_NativeWindow_GetBufferHandleFromNative
*                  2. check ret
 */
HWTEST_F(NativeWindowTest, GetBufferHandle002, Function | MediumTest | Level2)
{
    struct NativeWindowBuffer *buffer = new NativeWindowBuffer();
    buffer->sfbuffer = sBuffer;
    ASSERT_NE(OH_NativeWindow_GetBufferHandleFromNative(nativeWindowBuffer), nullptr);
    delete buffer;
}

/*
* Function: OH_NativeWindow_NativeWindowFlushBuffer
* Type: Function
* Rank: Important(2)
* EnvConditions: N/A
* CaseDescription: 1. call OH_NativeWindow_NativeWindowFlushBuffer by abnormal input
*                  2. check ret
 */
HWTEST_F(NativeWindowTest, FlushBuffer001, Function | MediumTest | Level2)
{
    int fenceFd = -1;
    struct Region *region = new Region();
    struct Region::Rect * rect = new Region::Rect();
    rect->x = 0x100;
    rect->y = 0x100;
    rect->w = 0x100;
    rect->h = 0x100;
    region->rects = rect;

    ASSERT_EQ(OH_NativeWindow_NativeWindowFlushBuffer(nullptr, nullptr, fenceFd, *region),
              OHOS::GSERROR_INVALID_ARGUMENTS);
    delete region;
}
