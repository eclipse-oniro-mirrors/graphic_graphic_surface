/*
 * Copyright (c) 2021-2022 Huawei Device Co., Ltd.
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

#include "native_window.h"

#include <cstdint>
#include <map>
#include <cinttypes>
#include "buffer_log.h"
#include "window.h"
#include "surface_type.h"
#include "sync_fence.h"

#ifndef WEAK_ALIAS
    #define WEAK_ALIAS(old, new) \
        extern __typeof(old) new __attribute__((__weak__, __alias__(#old)))
#endif

using namespace OHOS;

OHNativeWindow* CreateNativeWindowFromSurface(void* pSurface)
{
    if (pSurface == nullptr) {
        BLOGE("parameter error, please check input parameter");
        return nullptr;
    }
    OHNativeWindow* nativeWindow = new OHNativeWindow();
    nativeWindow->surface =
                *reinterpret_cast<OHOS::sptr<OHOS::Surface> *>(pSurface);
    nativeWindow->config.width = nativeWindow->surface->GetDefaultWidth();
    nativeWindow->config.height = nativeWindow->surface->GetDefaultHeight();
    nativeWindow->config.usage = BUFFER_USAGE_CPU_READ | BUFFER_USAGE_MEM_DMA;
    nativeWindow->config.format = GRAPHIC_PIXEL_FMT_RGBA_8888;
    nativeWindow->config.strideAlignment = 8;   // default stride is 8
    nativeWindow->config.timeout = 3000;        // default timeout is 3000 ms
    nativeWindow->config.colorGamut = GraphicColorGamut::GRAPHIC_COLOR_GAMUT_SRGB;
    nativeWindow->config.transform = GraphicTransformType::GRAPHIC_ROTATE_NONE;

    NativeObjectReference(nativeWindow);
    nativeWindow->surface->SetWptrNativeWindowToPSurface(nativeWindow);
    return nativeWindow;
}

void DestoryNativeWindow(OHNativeWindow *window)
{
    if (window == nullptr) {
        BLOGE("parameter error, please check input parameter");
        return;
    }
    // unreference nativewindow object
    NativeObjectUnreference(window);
}

OHNativeWindowBuffer* CreateNativeWindowBufferFromSurfaceBuffer(void* pSurfaceBuffer)
{
    if (pSurfaceBuffer == nullptr) {
        BLOGE("parameter error, please check input parameter");
        return nullptr;
    }
    OHNativeWindowBuffer *nwBuffer = new OHNativeWindowBuffer();
    nwBuffer->sfbuffer = *reinterpret_cast<OHOS::sptr<OHOS::SurfaceBuffer> *>(pSurfaceBuffer);
    NativeObjectReference(nwBuffer);
    return nwBuffer;
}

OHNativeWindowBuffer* CreateNativeWindowBufferFromNativeBuffer(OH_NativeBuffer* nativeBuffer)
{
    if (nativeBuffer == nullptr) {
        BLOGE("parameter error, please check input parameter");
        return nullptr;
    }
    OHNativeWindowBuffer *nwBuffer = new OHNativeWindowBuffer();
    OHOS::sptr<OHOS::SurfaceBuffer> surfaceBuffer(reinterpret_cast<OHOS::SurfaceBuffer *>(nativeBuffer));
    nwBuffer->sfbuffer = surfaceBuffer;

    NativeObjectReference(nwBuffer);
    return nwBuffer;
}

void DestroyNativeWindowBuffer(OHNativeWindowBuffer* buffer)
{
    if (buffer == nullptr) {
        BLOGE("parameter error, please check input parameter");
        return;
    }
    NativeObjectUnreference(buffer);
}

int32_t NativeWindowRequestBuffer(OHNativeWindow *window,
    OHNativeWindowBuffer **buffer, int *fenceFd)
{
    if (window == nullptr || buffer == nullptr || fenceFd == nullptr) {
        BLOGE("parameter error, please check input parameter");
        return OHOS::GSERROR_INVALID_ARGUMENTS;
    }
    OHOS::sptr<OHOS::SurfaceBuffer> sfbuffer;
    OHOS::sptr<OHOS::SyncFence> releaseFence = OHOS::SyncFence::INVALID_FENCE;
    int32_t ret = window->surface->RequestBuffer(sfbuffer, releaseFence, window->config);
    if (ret != OHOS::GSError::GSERROR_OK || sfbuffer == nullptr) {
        BLOGE("API failed, please check RequestBuffer function ret:%{public}d, Queue Id:%{public}" PRIu64,
                ret, window->surface->GetUniqueId());
        return OHOS::GSERROR_NO_BUFFER;
    }
    uint32_t seqNum = sfbuffer->GetSeqNum();
    if (window->bufferCache_.find(seqNum) == window->bufferCache_.end()) {
        OHNativeWindowBuffer *nwBuffer = new OHNativeWindowBuffer();
        nwBuffer->sfbuffer = sfbuffer;
        nwBuffer->uiTimestamp = window->uiTimestamp;
        *buffer = nwBuffer;
        // Add to cache
        NativeObjectReference(nwBuffer);
        window->bufferCache_[seqNum] = nwBuffer;
    } else {
        *buffer = window->bufferCache_[seqNum];
        (*buffer)->uiTimestamp = window->uiTimestamp;
    }
    *fenceFd = releaseFence->Dup();
    return OHOS::GSERROR_OK;
}

int32_t NativeWindowFlushBuffer(OHNativeWindow *window, OHNativeWindowBuffer *buffer,
    int fenceFd, struct Region region)
{
    if (window == nullptr || buffer == nullptr || window->surface == nullptr) {
        BLOGE("parameter error, please check input parameter");
        return OHOS::GSERROR_INVALID_ARGUMENTS;
    }

    OHOS::BufferFlushConfigWithDamages config;
    if ((region.rectNumber != 0) && (region.rects != nullptr)) {
        config.damages.reserve(region.rectNumber);
        for (int32_t i = 0; i < region.rectNumber; i++) {
            OHOS::Rect damage = {
                .x = region.rects[i].x,
                .y = region.rects[i].y,
                .w = static_cast<int32_t>(region.rects[i].w),
                .h = static_cast<int32_t>(region.rects[i].h),
            };
            config.damages.emplace_back(damage);
        }
        config.timestamp = buffer->uiTimestamp;
    } else {
        config.damages.reserve(1);
        OHOS::Rect damage = {
            .x = 0,
            .y = 0,
            .w = window->config.width,
            .h = window->config.height,
        };
        config.damages.emplace_back(damage);
        config.timestamp = buffer->uiTimestamp;
    }
    OHOS::sptr<OHOS::SyncFence> acquireFence = new OHOS::SyncFence(fenceFd);
    window->surface->FlushBuffer(buffer->sfbuffer, acquireFence, config);

    for (auto &[seqNum, buf] : window->bufferCache_) {
        if (buf == buffer) {
            window->lastBufferSeqNum = seqNum;
            break;
        }
    }

    return OHOS::GSERROR_OK;
}

int32_t GetLastFlushedBuffer(OHNativeWindow *window, OHNativeWindowBuffer **buffer, int *fenceFd, float matrix[16])
{
    if (window == nullptr || buffer == nullptr) {
        BLOGE("parameter error, please check input parameter");
        return OHOS::GSERROR_INVALID_ARGUMENTS;
    }
    OHNativeWindowBuffer *nwBuffer = new OHNativeWindowBuffer();
    OHOS::sptr<OHOS::SyncFence> acquireFence = OHOS::SyncFence::INVALID_FENCE;
    int32_t ret = window->surface->GetLastFlushedBuffer(nwBuffer->sfbuffer, acquireFence, matrix);
    if (ret != OHOS::GSError::GSERROR_OK || nwBuffer->sfbuffer == nullptr) {
        BLOGE("GetLastFlushedBuffer fail");
        return ret;
    }
    *buffer = nwBuffer;
    NativeObjectReference(nwBuffer);
    *fenceFd = acquireFence->Dup();
    return OHOS::GSERROR_OK;
}

int32_t NativeWindowCancelBuffer(OHNativeWindow *window, OHNativeWindowBuffer *buffer)
{
    if (window == nullptr || buffer == nullptr) {
        BLOGE("parameter error, please check input parameter");
        return OHOS::GSERROR_INVALID_ARGUMENTS;
    }

    window->surface->CancelBuffer(buffer->sfbuffer);
    return OHOS::GSERROR_OK;
}

static int32_t InternalHandleNativeWindowOpt(OHNativeWindow *window, int code, va_list args)
{
    switch (code) {
        case SET_USAGE: {
            uint64_t usage = va_arg(args, uint64_t);
            window->config.usage = usage;
            break;
        }
        case SET_BUFFER_GEOMETRY: {
            int32_t width = va_arg(args, int32_t);
            int32_t height = va_arg(args, int32_t);
            window->config.height = height;
            window->config.width = width;
            break;
        }
        case SET_FORMAT: {
            int32_t format = va_arg(args, int32_t);
            window->config.format = format;
            break;
        }
        case SET_STRIDE: {
            int32_t stride = va_arg(args, int32_t);
            window->config.strideAlignment = stride;
            break;
        }
        case SET_TIMEOUT: {
            int32_t timeout = va_arg(args, int32_t);
            window->config.timeout = timeout;
            break;
        }
        case SET_COLOR_GAMUT: {
            int32_t colorGamut = va_arg(args, int32_t);
            window->config.colorGamut = static_cast<GraphicColorGamut>(colorGamut);
            break;
        }
        case SET_TRANSFORM : {
            int32_t transform = va_arg(args, int32_t);
            window->config.transform = static_cast<GraphicTransformType>(transform);
            break;
        }
        case SET_UI_TIMESTAMP : {
            uint64_t uiTimestamp = va_arg(args, uint64_t);
            window->uiTimestamp = static_cast<int64_t>(uiTimestamp);
            break;
        }
        case GET_USAGE: {
            uint64_t *value = va_arg(args, uint64_t*);
            uint64_t usage = window->config.usage;
            *value = usage;
            break;
        }
        case GET_BUFFER_GEOMETRY: {
            int32_t *height = va_arg(args, int32_t*);
            int32_t *width = va_arg(args, int32_t*);
            *height = window->config.height;
            *width = window->config.width;
            break;
        }
        case GET_FORMAT: {
            int32_t *format = va_arg(args, int32_t*);
            *format = window->config.format;
            break;
        }
        case GET_STRIDE: {
            int32_t *stride = va_arg(args, int32_t*);
            *stride = window->config.strideAlignment;
            break;
        }
        case GET_TIMEOUT : {
            int32_t *timeout = va_arg(args, int32_t*);
            *timeout = window->config.timeout;
            break;
        }
        case GET_COLOR_GAMUT: {
            int32_t *colorGamut = va_arg(args, int32_t*);
            *colorGamut = static_cast<int32_t>(window->config.colorGamut);
            break;
        }
        case GET_TRANSFORM: {
            int32_t *transform = va_arg(args, int32_t*);
            *transform = static_cast<int32_t>(window->config.transform);
            break;
        }
        default:
            break;
    }
    return OHOS::GSERROR_OK;
}

int32_t NativeWindowHandleOpt(OHNativeWindow *window, int code, ...)
{
    if (window == nullptr) {
        BLOGE("parameter error, please check input parameter");
        return OHOS::GSERROR_INVALID_ARGUMENTS;
    }
    va_list args;
    va_start(args, code);
    InternalHandleNativeWindowOpt(window, code, args);
    va_end(args);
    return OHOS::GSERROR_OK;
}

BufferHandle *GetBufferHandleFromNative(OHNativeWindowBuffer *buffer)
{
    if (buffer == nullptr) {
        BLOGE("parameter error, please check input parameter");
        return nullptr;
    }
    return buffer->sfbuffer->GetBufferHandle();
}

int32_t GetNativeObjectMagic(void *obj)
{
    if (obj == nullptr) {
        BLOGE("parameter error, please check input parameter");
        return OHOS::GSERROR_INVALID_ARGUMENTS;
    }
    NativeWindowMagic* nativeWindowMagic = reinterpret_cast<NativeWindowMagic *>(obj);
    return nativeWindowMagic->magic;
}

int32_t NativeObjectReference(void *obj)
{
    if (obj == nullptr) {
        BLOGE("parameter error, please check input parameter");
        return OHOS::GSERROR_INVALID_ARGUMENTS;
    }
    switch (GetNativeObjectMagic(obj)) {
        case NATIVE_OBJECT_MAGIC_WINDOW:
        case NATIVE_OBJECT_MAGIC_WINDOW_BUFFER:
            break;
        default:
            return OHOS::GSERROR_TYPE_ERROR;
    }
    OHOS::RefBase *ref = reinterpret_cast<OHOS::RefBase *>(obj);
    ref->IncStrongRef(ref);
    return OHOS::GSERROR_OK;
}

int32_t NativeObjectUnreference(void *obj)
{
    if (obj == nullptr) {
        BLOGE("parameter error, please check input parameter");
        return OHOS::GSERROR_INVALID_ARGUMENTS;
    }
    switch (GetNativeObjectMagic(obj)) {
        case NATIVE_OBJECT_MAGIC_WINDOW:
        case NATIVE_OBJECT_MAGIC_WINDOW_BUFFER:
            break;
        default:
            return OHOS::GSERROR_TYPE_ERROR;
    }
    OHOS::RefBase *ref = reinterpret_cast<OHOS::RefBase *>(obj);
    ref->DecStrongRef(ref);
    return OHOS::GSERROR_OK;
}

int32_t NativeWindowSetScalingMode(OHNativeWindow *window, uint32_t sequence, OHScalingMode scalingMode)
{
    if (window == nullptr || window->surface == nullptr ||
        scalingMode < OHScalingMode::OH_SCALING_MODE_FREEZE ||
        scalingMode > OHScalingMode::OH_SCALING_MODE_NO_SCALE_CROP) {
        BLOGE("parameter error, please check input parameter");
        return OHOS::GSERROR_INVALID_ARGUMENTS;
    }
    return window->surface->SetScalingMode(sequence, static_cast<ScalingMode>(scalingMode));
}

int32_t NativeWindowSetMetaData(OHNativeWindow *window, uint32_t sequence, int32_t size,
                                const OHHDRMetaData *metaData)
{
    if (window == nullptr || window->surface == nullptr || size <= 0 || metaData == nullptr) {
        BLOGE("parameter error, please check input parameter");
        return OHOS::GSERROR_INVALID_ARGUMENTS;
    }

    std::vector<GraphicHDRMetaData> data(reinterpret_cast<const GraphicHDRMetaData *>(metaData),
                                         reinterpret_cast<const GraphicHDRMetaData *>(metaData) + size);
    return window->surface->SetMetaData(sequence, data);
}

int32_t NativeWindowSetMetaDataSet(OHNativeWindow *window, uint32_t sequence, OHHDRMetadataKey key,
                                   int32_t size, const uint8_t *metaData)
{
    if (window == nullptr || window->surface == nullptr ||
        key < OHHDRMetadataKey::OH_METAKEY_RED_PRIMARY_X || key > OHHDRMetadataKey::OH_METAKEY_HDR_VIVID ||
        size <= 0 || metaData == nullptr) {
        BLOGE("parameter error, please check input parameter");
        return OHOS::GSERROR_INVALID_ARGUMENTS;
    }
    std::vector<uint8_t> data(metaData, metaData + size);
    return window->surface->SetMetaDataSet(sequence, static_cast<GraphicHDRMetadataKey>(key), data);
}

int32_t NativeWindowSetTunnelHandle(OHNativeWindow *window, const OHExtDataHandle *handle)
{
    if (window == nullptr || window->surface == nullptr || handle == nullptr) {
        BLOGE("parameter error, please check input parameter");
        return OHOS::GSERROR_INVALID_ARGUMENTS;
    }
    return window->surface->SetTunnelHandle(reinterpret_cast<const OHOS::GraphicExtDataHandle*>(handle));
}

NativeWindow::NativeWindow() : NativeWindowMagic(NATIVE_OBJECT_MAGIC_WINDOW), surface(nullptr)
{
}

NativeWindow::~NativeWindow()
{
    for (auto &[seqNum, buffer] : bufferCache_) {
        NativeObjectUnreference(buffer);
    }
}

NativeWindowBuffer::~NativeWindowBuffer()
{
}

NativeWindowBuffer::NativeWindowBuffer() : NativeWindowMagic(NATIVE_OBJECT_MAGIC_WINDOW_BUFFER), sfbuffer(nullptr)
{
}

WEAK_ALIAS(CreateNativeWindowFromSurface, OH_NativeWindow_CreateNativeWindow);
WEAK_ALIAS(DestoryNativeWindow, OH_NativeWindow_DestroyNativeWindow);
WEAK_ALIAS(CreateNativeWindowBufferFromSurfaceBuffer, OH_NativeWindow_CreateNativeWindowBufferFromSurfaceBuffer);
WEAK_ALIAS(CreateNativeWindowBufferFromNativeBuffer, OH_NativeWindow_CreateNativeWindowBufferFromNativeBuffer);
WEAK_ALIAS(DestroyNativeWindowBuffer, OH_NativeWindow_DestroyNativeWindowBuffer);
WEAK_ALIAS(NativeWindowRequestBuffer, OH_NativeWindow_NativeWindowRequestBuffer);
WEAK_ALIAS(NativeWindowFlushBuffer, OH_NativeWindow_NativeWindowFlushBuffer);
WEAK_ALIAS(GetLastFlushedBuffer, OH_NativeWindow_GetLastFlushedBuffer);
WEAK_ALIAS(NativeWindowCancelBuffer, OH_NativeWindow_NativeWindowAbortBuffer);
WEAK_ALIAS(NativeWindowHandleOpt, OH_NativeWindow_NativeWindowHandleOpt);
WEAK_ALIAS(GetBufferHandleFromNative, OH_NativeWindow_GetBufferHandleFromNative);
WEAK_ALIAS(NativeObjectReference, OH_NativeWindow_NativeObjectReference);
WEAK_ALIAS(NativeObjectUnreference, OH_NativeWindow_NativeObjectUnreference);
WEAK_ALIAS(GetNativeObjectMagic, OH_NativeWindow_GetNativeObjectMagic);
WEAK_ALIAS(NativeWindowSetScalingMode, OH_NativeWindow_NativeWindowSetScalingMode);
WEAK_ALIAS(NativeWindowSetMetaData, OH_NativeWindow_NativeWindowSetMetaData);
WEAK_ALIAS(NativeWindowSetMetaDataSet, OH_NativeWindow_NativeWindowSetMetaDataSet);
WEAK_ALIAS(NativeWindowSetTunnelHandle, OH_NativeWindow_NativeWindowSetTunnelHandle);