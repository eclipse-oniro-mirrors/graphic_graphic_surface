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
#include <message_option.h>
#include <message_parcel.h>
#include "producer_surface_delegator.h"
#include "buffer_log.h"
#include "sync_fence.h"

namespace OHOS {

std::atomic<int32_t> ProducerSurfaceDelegator::mDisplayRotation_ = 0;

ProducerSurfaceDelegator::~ProducerSurfaceDelegator()
{
    map_.clear();
}
GSError ProducerSurfaceDelegator::DequeueBuffer(int32_t slot, sptr<SurfaceBuffer> buffer)
{
    return GSERROR_OK;
}

GSError ProducerSurfaceDelegator::QueueBuffer(int32_t slot, int32_t acquireFence)
{
    return GSERROR_OK;
}

GSError ProducerSurfaceDelegator::ReleaseBuffer(const sptr<SurfaceBuffer> &buffer, const sptr<SyncFence> &fence)
{
    return GSERROR_OK;
}

GSError ProducerSurfaceDelegator::ClearBufferSlot(int32_t slot)
{
    (void)slot;
    return GSERROR_OK;
}

GSError ProducerSurfaceDelegator::ClearAllBuffers()
{
    return GSERROR_OK;
}

void ProducerSurfaceDelegator::AddBufferLocked(const sptr<SurfaceBuffer>& buffer, int32_t slot)
{
    (void)buffer;
    (void)slot;
}

sptr<SurfaceBuffer> ProducerSurfaceDelegator::GetBufferLocked(int32_t slot)
{
    (void)slot;
    return nullptr;
}

int32_t ProducerSurfaceDelegator::GetSlotLocked(const sptr<SurfaceBuffer>& buffer)
{
    (void)buffer;
    return 0;
}

GSError ProducerSurfaceDelegator::CancelBuffer(int32_t slot, int32_t fenceFd)
{
    return GSERROR_OK;
}

GSError ProducerSurfaceDelegator::DetachBuffer(int32_t slot)
{
    return GSERROR_OK;
}

int ProducerSurfaceDelegator::OnSetBufferQueueSize(MessageParcel &data, MessageParcel &reply)
{
    return ERR_NONE;
}

int ProducerSurfaceDelegator::OnDequeueBuffer(MessageParcel &data, MessageParcel &reply)
{
    return ERR_NONE;
}

int ProducerSurfaceDelegator::OnQueueBuffer(MessageParcel &data, MessageParcel &reply)
{
    (void)data;
    (void)reply;
    return ERR_NONE;
}

int ProducerSurfaceDelegator::OnSetDataspace(MessageParcel& data, MessageParcel& reply)
{
    mAncoDataspace = -1;
    return ERR_NONE;
}

int ProducerSurfaceDelegator::OnRemoteRequest(uint32_t code, MessageParcel &data, MessageParcel &reply,
    MessageOption &option)
{
    return ERR_NONE;
}

int32_t ProducerSurfaceDelegator::OnNdkFlushBuffer(MessageParcel &data, MessageParcel &reply)
{
    (void)data;
    (void)reply;
    return GSERROR_OK;
}

GSError ProducerSurfaceDelegator::RetryFlushBuffer(sptr<SurfaceBuffer>& buffer, int32_t fence,
                                                   BufferFlushConfig& config)
{
    return GSERROR_OK;
}

bool ProducerSurfaceDelegator::HasSlotInSet(int32_t slot)
{
    std::lock_guard<std::mutex> setLock(dequeueFailedSetMutex_);
    return dequeueFailedSet_.find(slot) != dequeueFailedSet_.end();
}

void ProducerSurfaceDelegator::InsertSlotIntoSet(int32_t slot)
{
    std::lock_guard<std::mutex> setLock(dequeueFailedSetMutex_);
    dequeueFailedSet_.insert(slot);
}

void ProducerSurfaceDelegator::EraseSlotFromSet(int32_t slot)
{
    std::lock_guard<std::mutex> setLock(dequeueFailedSetMutex_);
    dequeueFailedSet_.erase(slot);
}

void ProducerSurfaceDelegator::SetDisplayRotation(int32_t rotation)
{
    mDisplayRotation_.store(rotation);
}

void ProducerSurfaceDelegator::UpdateBufferTransform()
{
}

GraphicTransformType ProducerSurfaceDelegator::ConvertTransformToHmos(uint32_t transform)
{
    mTransform_ = transform;
    return mLastTransform_;
}

int32_t ProducerSurfaceDelegator::NdkFlushBuffer(
    sptr<SurfaceBuffer>& buffer, int32_t slot, const sptr<SyncFence>& fence)
{
    (void)buffer;
    (void)slot;
    (void)fence;
    return GSERROR_OK;
}

sptr<SurfaceBuffer> ProducerSurfaceDelegator::NdkConvertBuffer(
    MessageParcel& data, int32_t hasNewBuffer, int32_t slot)
{
    (void)data;
    (void)hasNewBuffer;
    (void)slot;
    return nullptr;
}

void ProducerSurfaceDelegator::NdkClearBuffer(int32_t slot, uint32_t seqNum)
{
    mIsNdk = false;
    (void)slot;
    (void)seqNum;
}
} // namespace OHOS
