/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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

#include "consumer_surface.h"

#include <cinttypes>

#include "buffer_log.h"
#include "buffer_queue_producer.h"
#include "sync_fence.h"

namespace OHOS {
sptr<Surface> Surface::CreateSurfaceAsConsumer(std::string name, bool isShared)
{
    sptr<ConsumerSurface> surf = new(std::nothrow) ConsumerSurface(name, isShared);
    if (surf == nullptr || surf->Init() != GSERROR_OK) {
        BLOGE("Failure, Reason: consumer surf init failed");
        return nullptr;
    }
    return surf;
}

sptr<IConsumerSurface> IConsumerSurface::Create(std::string name, bool isShared)
{
    sptr<ConsumerSurface> surf = new(std::nothrow) ConsumerSurface(name, isShared);
    if (surf == nullptr || surf->Init() != GSERROR_OK) {
        BLOGE("Failure, Reason: consumer surf init failed");
        return nullptr;
    }
    return surf;
}

ConsumerSurface::ConsumerSurface(const std::string &name, bool isShared)
    : name_(name), isShared_(isShared)
{
    BLOGND("ctor");
    consumer_ = nullptr;
    producer_ = nullptr;
}

ConsumerSurface::~ConsumerSurface()
{
    if (consumer_ != nullptr) {
        consumer_->OnConsumerDied();
        consumer_->SetStatus(false);
    }
    if (producer_ != nullptr) {
        BLOGNI("~ConsumerSurface queueId:%{public}" PRIu64 "producer_:%{public}d",
            producer_->GetUniqueId(), producer_->GetSptrRefCount());
    }
    consumer_ = nullptr;
    producer_ = nullptr;
}

GSError ConsumerSurface::Init()
{
    sptr<BufferQueue> queue_ = new(std::nothrow) BufferQueue(name_, isShared_);
    if (queue_ == nullptr) {
        BLOGN_FAILURE("Init failed for nullptr queue");
        return GSERROR_NOT_SUPPORT;
    }

    producer_ = new(std::nothrow) BufferQueueProducer(queue_);
    consumer_ = new(std::nothrow) BufferQueueConsumer(queue_);
    return GSERROR_OK;
}

bool ConsumerSurface::IsConsumer() const
{
    return true;
}

sptr<IBufferProducer> ConsumerSurface::GetProducer() const
{
    return producer_;
}

GSError ConsumerSurface::GetProducerInitInfo(ProducerInitInfo &info)
{
    return GSERROR_NOT_SUPPORT;
}

GSError ConsumerSurface::RequestBuffer(sptr<SurfaceBuffer>& buffer,
                                       sptr<SyncFence>& fence, BufferRequestConfig &config)
{
    return GSERROR_NOT_SUPPORT;
}

GSError ConsumerSurface::RequestBuffers(std::vector<sptr<SurfaceBuffer>> &buffers,
    std::vector<sptr<SyncFence>> &fences, BufferRequestConfig &config)
{
    return GSERROR_NOT_SUPPORT;
}

GSError ConsumerSurface::FlushBuffer(sptr<SurfaceBuffer>& buffer,
                                     const sptr<SyncFence>& fence, BufferFlushConfig &config)
{
    return GSERROR_NOT_SUPPORT;
}

GSError ConsumerSurface::FlushBuffer(sptr<SurfaceBuffer>& buffer, const sptr<SyncFence>& fence,
                                     BufferFlushConfigWithDamages &config)
{
    return GSERROR_NOT_SUPPORT;
}

GSError ConsumerSurface::FlushBuffers(const std::vector<sptr<SurfaceBuffer>> &buffers,
    const std::vector<sptr<SyncFence>> &fences, const std::vector<BufferFlushConfigWithDamages> &config)
{
    return GSERROR_NOT_SUPPORT;
}

GSError ConsumerSurface::GetLastFlushedBuffer(sptr<SurfaceBuffer>& buffer,
    sptr<SyncFence>& fence, float matrix[16], bool isUseNewMatrix)
{
    return GSERROR_NOT_SUPPORT;
}

GSError ConsumerSurface::AcquireBuffer(sptr<SurfaceBuffer>& buffer, sptr<SyncFence>& fence,
                                       int64_t &timestamp, Rect &damage)
{
    std::vector<Rect> damages;
    GSError ret = AcquireBuffer(buffer, fence, timestamp, damages);
    if (ret != GSERROR_OK) {
        return ret;
    }
    if (damages.size() == 1) {
        damage = damages[0];
        return GSERROR_OK;
    }
    BLOGN_FAILURE("AcquireBuffer Success but the size of damages is %{public}zu is not 1, should Acquire damages.",
        damages.size());
    return GSERROR_INVALID_ARGUMENTS;
}

GSError ConsumerSurface::AcquireBuffer(sptr<SurfaceBuffer>& buffer, sptr<SyncFence>& fence,
                                       int64_t &timestamp, std::vector<Rect> &damages)
{
    if (consumer_ == nullptr) {
        BLOGFE("AcquireBuffer failed for nullptr consumer.");
        return GSERROR_INVALID_ARGUMENTS;
    }
    return consumer_->AcquireBuffer(buffer, fence, timestamp, damages);
}

GSError ConsumerSurface::ReleaseBuffer(sptr<SurfaceBuffer>& buffer, const sptr<SyncFence>& fence)
{
    if (consumer_ == nullptr) {
        BLOGFE("ReleaseBuffer failed for nullptr consumer.");
        return GSERROR_INVALID_ARGUMENTS;
    }
    return consumer_->ReleaseBuffer(buffer, fence);
}
GSError ConsumerSurface::RequestBuffer(sptr<SurfaceBuffer>& buffer,
                                       int32_t &fence, BufferRequestConfig &config)
{
    return GSERROR_NOT_SUPPORT;
}

GSError ConsumerSurface::CancelBuffer(sptr<SurfaceBuffer>& buffer)
{
    return GSERROR_NOT_SUPPORT;
}

GSError ConsumerSurface::FlushBuffer(sptr<SurfaceBuffer>& buffer,
                                     int32_t fence, BufferFlushConfig &config)
{
    return GSERROR_NOT_SUPPORT;
}

GSError ConsumerSurface::AcquireBuffer(sptr<SurfaceBuffer>& buffer, int32_t &fence,
                                       int64_t &timestamp, Rect &damage)
{
    sptr<SyncFence> syncFence = SyncFence::INVALID_FENCE;
    auto ret = AcquireBuffer(buffer, syncFence, timestamp, damage);
    if (ret != GSERROR_OK) {
        fence = -1;
        return ret;
    }
    fence = syncFence->Dup();
    return GSERROR_OK;
}

GSError ConsumerSurface::ReleaseBuffer(sptr<SurfaceBuffer>& buffer, int32_t fence)
{
    sptr<SyncFence> syncFence = new(std::nothrow) SyncFence(fence);
    if (syncFence == nullptr) {
        BLOGFE("ReleaseBuffer failed for nullptr syncFence.");
        return GSERROR_NOT_SUPPORT;
    }
    return ReleaseBuffer(buffer, syncFence);
}

GSError ConsumerSurface::AttachBufferToQueue(sptr<SurfaceBuffer> buffer)
{
    if (buffer == nullptr || consumer_ == nullptr) {
        return GSERROR_INVALID_ARGUMENTS;
    }
    buffer->SetConsumerAttachBufferFlag(true);
    return consumer_->AttachBufferToQueue(buffer);
}

GSError ConsumerSurface::DetachBufferFromQueue(sptr<SurfaceBuffer> buffer)
{
    if (buffer == nullptr || consumer_ == nullptr) {
        return GSERROR_INVALID_ARGUMENTS;
    }
    buffer->SetConsumerAttachBufferFlag(false);
    return consumer_->DetachBufferFromQueue(buffer);
}

GSError ConsumerSurface::AttachBuffer(sptr<SurfaceBuffer>& buffer)
{
    if (consumer_ == nullptr) {
        BLOGFE("AttachBuffer failed for nullptr consumer.");
        return GSERROR_INVALID_ARGUMENTS;
    }
    return consumer_->AttachBuffer(buffer);
}

GSError ConsumerSurface::AttachBuffer(sptr<SurfaceBuffer>& buffer, int32_t timeOut)
{
    if (consumer_ == nullptr) {
        BLOGFE("AttachBuffer failed for nullptr consumer.");
        return GSERROR_INVALID_ARGUMENTS;
    }
    return consumer_->AttachBuffer(buffer, timeOut);
}

GSError ConsumerSurface::DetachBuffer(sptr<SurfaceBuffer>& buffer)
{
    if (consumer_ == nullptr) {
        BLOGFE("DetachBuffer failed for nullptr consumer.");
        return GSERROR_INVALID_ARGUMENTS;
    }
    return consumer_->DetachBuffer(buffer);
}

GSError ConsumerSurface::RegisterSurfaceDelegator(sptr<IRemoteObject> client)
{
    if (consumer_ == nullptr) {
        BLOGFE("RegisterSurfaceDelegator failed for nullptr consumer.");
        return GSERROR_INVALID_ARGUMENTS;
    }
    return consumer_->RegisterSurfaceDelegator(client, this);
}

bool ConsumerSurface::QueryIfBufferAvailable()
{
    if (consumer_ == nullptr) {
        BLOGFE("QueryIfBufferAvailable failed for nullptr consumer.");
        return false;
    }
    return consumer_->QueryIfBufferAvailable();
}

uint32_t ConsumerSurface::GetQueueSize()
{
    if (producer_ == nullptr) {
        BLOGFE("GetQueueSize failed for nullptr producer.");
        return 0;
    }
    return producer_->GetQueueSize();
}

GSError ConsumerSurface::SetQueueSize(uint32_t queueSize)
{
    if (producer_ == nullptr) {
        BLOGFE("SetQueueSize failed for nullptr producer.");
        return GSERROR_INVALID_ARGUMENTS;
    }
    return producer_->SetQueueSize(queueSize);
}

const std::string& ConsumerSurface::GetName()
{
    return name_;
}

GSError ConsumerSurface::SetDefaultWidthAndHeight(int32_t width, int32_t height)
{
    if (consumer_ == nullptr) {
        BLOGFE("SetDefaultWidthAndHeight failed for nullptr consumer.");
        return GSERROR_INVALID_ARGUMENTS;
    }
    return consumer_->SetDefaultWidthAndHeight(width, height);
}

int32_t ConsumerSurface::GetDefaultWidth()
{
    if (producer_ == nullptr) {
        BLOGFE("GetDefaultWidth failed for nullptr producer.");
        return -1;
    }
    return producer_->GetDefaultWidth();
}

int32_t ConsumerSurface::GetDefaultHeight()
{
    if (producer_ == nullptr) {
        BLOGFE("GetDefaultWidth failed for nullptr producer.");
        return -1;
    }
    return producer_->GetDefaultHeight();
}

GSError ConsumerSurface::SetDefaultUsage(uint64_t usage)
{
    if (consumer_ == nullptr) {
        BLOGFE("SetDefaultUsage failed for nullptr consumer.");
        return GSERROR_INVALID_ARGUMENTS;
    }
    return consumer_->SetDefaultUsage(usage);
}

uint64_t ConsumerSurface::GetDefaultUsage()
{
    if (producer_ == nullptr) {
        BLOGFE("GetDefaultUsage failed for nullptr producer.");
        return 0;
    }
    return producer_->GetDefaultUsage();
}

GSError ConsumerSurface::SetUserData(const std::string &key, const std::string &val)
{
    std::lock_guard<std::mutex> lockGuard(lockMutex_);
    if (userData_.size() >= SURFACE_MAX_USER_DATA_COUNT) {
        BLOGE("SetUserData failed: userData_ size out");
        return GSERROR_OUT_OF_RANGE;
    }

    auto iterUserData = userData_.find(key);
    if (iterUserData != userData_.end() && iterUserData->second == val) {
        BLOGE("SetUserData failed: key:%{public}s, val:%{public}s exist", key.c_str(), val.c_str());
        return GSERROR_API_FAILED;
    }

    userData_[key] = val;
    auto iter = onUserDataChange_.begin();
    while (iter != onUserDataChange_.end()) {
        if (iter->second != nullptr) {
            iter->second(key, val);
        }
        iter++;
    }

    return GSERROR_OK;
}

std::string ConsumerSurface::GetUserData(const std::string &key)
{
    std::lock_guard<std::mutex> lockGuard(lockMutex_);
    if (userData_.find(key) != userData_.end()) {
        return userData_[key];
    }

    return "";
}

GSError ConsumerSurface::RegisterConsumerListener(sptr<IBufferConsumerListener>& listener)
{
    if (consumer_ == nullptr) {
        BLOGFE("RegisterConsumerListener failed for nullptr consumer.");
        return GSERROR_INVALID_ARGUMENTS;
    }
    return consumer_->RegisterConsumerListener(listener);
}

GSError ConsumerSurface::RegisterConsumerListener(IBufferConsumerListenerClazz *listener)
{
    if (consumer_ == nullptr) {
        BLOGFE("RegisterConsumerListener failed for nullptr consumer.");
        return GSERROR_INVALID_ARGUMENTS;
    }
    return consumer_->RegisterConsumerListener(listener);
}

GSError ConsumerSurface::RegisterReleaseListener(OnReleaseFunc func)
{
    if (consumer_ == nullptr) {
        BLOGFE("RegisterReleaseListener failed for nullptr consumer.");
        return GSERROR_INVALID_ARGUMENTS;
    }
    return consumer_->RegisterReleaseListener(func);
}

GSError ConsumerSurface::RegisterReleaseListener(OnReleaseFuncWithFence func)
{
    return GSERROR_NOT_SUPPORT;
}

GSError ConsumerSurface::UnRegisterReleaseListener()
{
    return GSERROR_OK;
}

GSError ConsumerSurface::RegisterDeleteBufferListener(OnDeleteBufferFunc func, bool isForUniRedraw)
{
    if (consumer_ == nullptr) {
        BLOGFE("RegisterDeleteBufferListener failed for nullptr consumer.");
        return GSERROR_INVALID_ARGUMENTS;
    }
    return consumer_->RegisterDeleteBufferListener(func, isForUniRedraw);
}

GSError ConsumerSurface::UnregisterConsumerListener()
{
    if (consumer_ == nullptr) {
        BLOGFE("UnregisterConsumerListener failed for nullptr consumer.");
        return GSERROR_INVALID_ARGUMENTS;
    }
    return consumer_->UnregisterConsumerListener();
}

GSError ConsumerSurface::RegisterUserDataChangeListener(const std::string &funcName, OnUserDataChangeFunc func)
{
    std::lock_guard<std::mutex> lockGuard(lockMutex_);
    if (onUserDataChange_.find(funcName) != onUserDataChange_.end()) {
        BLOGND("func already register");
        return GSERROR_INVALID_ARGUMENTS;
    }
    
    onUserDataChange_[funcName] = func;
    return GSERROR_OK;
}

GSError ConsumerSurface::UnRegisterUserDataChangeListener(const std::string &funcName)
{
    std::lock_guard<std::mutex> lockGuard(lockMutex_);
    if (onUserDataChange_.erase(funcName) == 0) {
        BLOGND("func doesn't register");
        return GSERROR_INVALID_ARGUMENTS;
    }

    return GSERROR_OK;
}

GSError ConsumerSurface::ClearUserDataChangeListener()
{
    std::lock_guard<std::mutex> lockGuard(lockMutex_);
    onUserDataChange_.clear();
    return GSERROR_OK;
}

GSError ConsumerSurface::CleanCache(bool cleanAll)
{
    (void)cleanAll;
    return GSERROR_NOT_SUPPORT;
}

GSError ConsumerSurface::GoBackground()
{
    if (consumer_ == nullptr) {
        BLOGFE("GoBackground failed for nullptr consumer.");
        return GSERROR_INVALID_ARGUMENTS;
    }
    if (producer_ != nullptr) {
        BLOGND("Queue Id:%{public}" PRIu64 "", producer_->GetUniqueId());
    }
    return consumer_->GoBackground();
}

uint64_t ConsumerSurface::GetUniqueId() const
{
    if (producer_ == nullptr) {
        BLOGFE("GetUniqueId failed for nullptr producer.");
        return 0;
    }
    return producer_->GetUniqueId();
}

void ConsumerSurface::Dump(std::string &result) const
{
    if (consumer_ == nullptr) {
        BLOGFE("Dump failed for nullptr consumer.");
        return;
    }
    return consumer_->Dump(result);
}

GSError ConsumerSurface::SetTransform(GraphicTransformType transform)
{
    if (producer_ == nullptr) {
        BLOGFE("SetTransform failed for nullptr producer.");
        return GSERROR_INVALID_ARGUMENTS;
    }
    return producer_->SetTransform(transform);
}

GraphicTransformType ConsumerSurface::GetTransform() const
{
    if (consumer_ == nullptr) {
        BLOGFE("GetTransform failed for nullptr consumer.");
        return GraphicTransformType::GRAPHIC_ROTATE_BUTT;
    }
    return consumer_->GetTransform();
}

GSError ConsumerSurface::IsSupportedAlloc(const std::vector<BufferVerifyAllocInfo> &infos,
                                          std::vector<bool> &supporteds)
{
    return GSERROR_NOT_SUPPORT;
}

GSError ConsumerSurface::Connect()
{
    return GSERROR_NOT_SUPPORT;
}

GSError ConsumerSurface::Disconnect()
{
    return GSERROR_NOT_SUPPORT;
}

GSError ConsumerSurface::SetScalingMode(uint32_t sequence, ScalingMode scalingMode)
{
    if (producer_ == nullptr || scalingMode < ScalingMode::SCALING_MODE_FREEZE ||
        scalingMode > ScalingMode::SCALING_MODE_SCALE_FIT) {
        return GSERROR_INVALID_ARGUMENTS;
    }
    return producer_->SetScalingMode(sequence, scalingMode);
}

GSError ConsumerSurface::SetScalingMode(ScalingMode scalingMode)
{
    if (producer_ == nullptr || scalingMode < ScalingMode::SCALING_MODE_FREEZE ||
        scalingMode > ScalingMode::SCALING_MODE_SCALE_FIT) {
        return GSERROR_INVALID_ARGUMENTS;
    }
    return producer_->SetScalingMode(scalingMode);
}

GSError ConsumerSurface::GetScalingMode(uint32_t sequence, ScalingMode &scalingMode)
{
    if (consumer_ == nullptr) {
        BLOGFE("GetScalingMode failed for nullptr consumer.");
        return GSERROR_INVALID_ARGUMENTS;
    }
    return consumer_->GetScalingMode(sequence, scalingMode);
}

GSError ConsumerSurface::SetMetaData(uint32_t sequence, const std::vector<GraphicHDRMetaData> &metaData)
{
    if (producer_ == nullptr || metaData.size() == 0) {
        return GSERROR_INVALID_ARGUMENTS;
    }
    return producer_->SetMetaData(sequence, metaData);
}

GSError ConsumerSurface::SetMetaDataSet(uint32_t sequence, GraphicHDRMetadataKey key,
                                        const std::vector<uint8_t> &metaData)
{
    if (producer_ == nullptr || key < GraphicHDRMetadataKey::GRAPHIC_MATAKEY_RED_PRIMARY_X ||
        key > GraphicHDRMetadataKey::GRAPHIC_MATAKEY_HDR_VIVID || metaData.size() == 0) {
        return GSERROR_INVALID_ARGUMENTS;
    }
    return producer_->SetMetaDataSet(sequence, key, metaData);
}

GSError ConsumerSurface::QueryMetaDataType(uint32_t sequence, HDRMetaDataType &type) const
{
    if (consumer_ == nullptr) {
        BLOGFE("QueryMetaDataType failed for nullptr consumer.");
        return GSERROR_INVALID_ARGUMENTS;
    }
    return consumer_->QueryMetaDataType(sequence, type);
}

GSError ConsumerSurface::GetMetaData(uint32_t sequence, std::vector<GraphicHDRMetaData> &metaData) const
{
    if (consumer_ == nullptr) {
        BLOGFE("GetMetaData failed for nullptr consumer.");
        return GSERROR_INVALID_ARGUMENTS;
    }
    return consumer_->GetMetaData(sequence, metaData);
}

GSError ConsumerSurface::GetMetaDataSet(uint32_t sequence, GraphicHDRMetadataKey &key,
                                        std::vector<uint8_t> &metaData) const
{
    if (consumer_ == nullptr) {
        BLOGFE("GetMetaDataSet failed for nullptr consumer.");
        return GSERROR_INVALID_ARGUMENTS;
    }
    return consumer_->GetMetaDataSet(sequence, key, metaData);
}

GSError ConsumerSurface::SetTunnelHandle(const GraphicExtDataHandle *handle)
{
    if (producer_ == nullptr || handle == nullptr || handle->reserveInts == 0) {
        return GSERROR_INVALID_ARGUMENTS;
    }
    return producer_->SetTunnelHandle(handle);
}

sptr<SurfaceTunnelHandle> ConsumerSurface::GetTunnelHandle() const
{
    if (consumer_ == nullptr) {
        BLOGFE("GetTunnelHandle failed for nullptr consumer.");
        return nullptr;
    }
    return consumer_->GetTunnelHandle();
}

void ConsumerSurface::SetBufferHold(bool hold)
{
    if (consumer_ == nullptr) {
        BLOGNE("ConsumerSurface::SetBufferHold producer is nullptr.");
        return;
    }
    consumer_->SetBufferHold(hold);
}

GSError ConsumerSurface::SetPresentTimestamp(uint32_t sequence, const GraphicPresentTimestamp &timestamp)
{
    if (consumer_ == nullptr || timestamp.type == GraphicPresentTimestampType::GRAPHIC_DISPLAY_PTS_UNSUPPORTED) {
        return GSERROR_INVALID_ARGUMENTS;
    }
    return consumer_->SetPresentTimestamp(sequence, timestamp);
}

GSError ConsumerSurface::GetPresentTimestamp(uint32_t sequence, GraphicPresentTimestampType type,
                                             int64_t &time) const
{
    return GSERROR_NOT_SUPPORT;
}

int32_t ConsumerSurface::GetDefaultFormat()
{
    BLOGND("ConsumerSurface::GetDefaultFormat not support.");
    return 0;
}

GSError ConsumerSurface::SetDefaultFormat(int32_t format)
{
    BLOGND("ConsumerSurface::SetDefaultFormat not support.");
    return GSERROR_NOT_SUPPORT;
}

int32_t ConsumerSurface::GetDefaultColorGamut()
{
    BLOGND("ConsumerSurface::GetDefaultColorGamut not support.");
    return 0;
}

GSError ConsumerSurface::SetDefaultColorGamut(int32_t colorGamut)
{
    BLOGND("ConsumerSurface::SetDefaultColorGamut not support.");
    return GSERROR_NOT_SUPPORT;
}

sptr<NativeSurface> ConsumerSurface::GetNativeSurface()
{
    BLOGND("ConsumerSurface::GetNativeSurface not support.");
    return nullptr;
}

GSError ConsumerSurface::SetWptrNativeWindowToPSurface(void* nativeWindow)
{
    BLOGND("ConsumerSurface::SetWptrNativeWindowToPSurface not support.");
    return GSERROR_NOT_SUPPORT;
}

void ConsumerSurface::ConsumerRequestCpuAccess(bool on)
{
    if (consumer_ == nullptr) {
        BLOGFE("ConsumerRequestCpuAccess failed for nullptr consumer.");
        return;
    }
    consumer_->ConsumerRequestCpuAccess(on);
}

GraphicTransformType ConsumerSurface::GetTransformHint() const
{
    if (producer_ == nullptr) {
        BLOGFE("GetTransformHint failed for nullptr producer.");
        return GraphicTransformType::GRAPHIC_ROTATE_BUTT;
    }
    GraphicTransformType transformHint = GraphicTransformType::GRAPHIC_ROTATE_BUTT;
    if (producer_->GetTransformHint(transformHint) != GSERROR_OK) {
        BLOGNE("Warning ProducerSurface GetTransformHint failed.");
        return GraphicTransformType::GRAPHIC_ROTATE_BUTT;
    }
    return transformHint;
}

GSError ConsumerSurface::SetTransformHint(GraphicTransformType transformHint)
{
    if (producer_ == nullptr) {
        BLOGFE("SetTransformHint failed for nullptr producer.");
        return GSERROR_INVALID_ARGUMENTS;
    }
    return producer_->SetTransformHint(transformHint);
}

GSError ConsumerSurface::SetSurfaceSourceType(OHSurfaceSource sourceType)
{
    if (producer_ == nullptr) {
        BLOGFE("SetSurfaceSourceType failed for nullptr producer.");
        return GSERROR_INVALID_ARGUMENTS;
    }
    return producer_->SetSurfaceSourceType(sourceType);
}

OHSurfaceSource ConsumerSurface::GetSurfaceSourceType() const
{
    if (producer_ == nullptr) {
        BLOGFE("GetSurfaceSourceType failed for nullptr producer.");
        return OHSurfaceSource::OH_SURFACE_SOURCE_DEFAULT;
    }
    OHSurfaceSource sourceType = OHSurfaceSource::OH_SURFACE_SOURCE_DEFAULT;
    if (producer_->GetSurfaceSourceType(sourceType) != GSERROR_OK) {
        BLOGNE("Warning GetSurfaceSourceType failed.");
        return OHSurfaceSource::OH_SURFACE_SOURCE_DEFAULT;
    }
    return sourceType;
}

GSError ConsumerSurface::SetSurfaceAppFrameworkType(std::string appFrameworkType)
{
    if (producer_ == nullptr) {
        BLOGFE("SetSurfaceAppFrameworkType failed for nullptr producer.");
        return GSERROR_INVALID_ARGUMENTS;
    }
    return producer_->SetSurfaceAppFrameworkType(appFrameworkType);
}

std::string ConsumerSurface::GetSurfaceAppFrameworkType() const
{
    if (producer_ == nullptr) {
        BLOGFE("GetSurfaceAppFrameworkType failed for nullptr producer.");
        return "";
    }
    std::string appFrameworkType = "";
    if (producer_->GetSurfaceAppFrameworkType(appFrameworkType) != GSERROR_OK) {
        BLOGNE("Warning GetSurfaceAppFrameworkType failed.");
        return "";
    }
    return appFrameworkType;
}

void ConsumerSurface::SetRequestWidthAndHeight(int32_t width, int32_t height)
{
    (void)width;
    (void)height;
}

int32_t ConsumerSurface::GetRequestWidth()
{
    return 0;
}

int32_t ConsumerSurface::GetRequestHeight()
{
    return 0;
}

BufferRequestConfig* ConsumerSurface::GetWindowConfig()
{
    return nullptr;
}

GSError ConsumerSurface::SetHdrWhitePointBrightness(float brightness)
{
    (void)brightness;
    return GSERROR_OK;
}

GSError ConsumerSurface::SetSdrWhitePointBrightness(float brightness)
{
    (void)brightness;
    return GSERROR_OK;
}

float ConsumerSurface::GetHdrWhitePointBrightness() const
{
    if (consumer_ == nullptr) {
        BLOGFE("GetHdrWhitePointBrightness failed for nullptr consumer.");
        return 0;
    }
    return consumer_->GetHdrWhitePointBrightness();
}

float ConsumerSurface::GetSdrWhitePointBrightness() const
{
    if (consumer_ == nullptr) {
        BLOGFE("GetSdrWhitePointBrightness failed for nullptr consumer.");
        return 0;
    }
    return consumer_->GetSdrWhitePointBrightness();
}

GSError ConsumerSurface::GetSurfaceBufferTransformType(sptr<SurfaceBuffer> buffer,
    GraphicTransformType *transformType)
{
    if (buffer == nullptr || transformType == nullptr) {
        BLOGNE("input param is nullptr");
        return SURFACE_ERROR_INVALID_PARAM;
    }
    *transformType = buffer->GetSurfaceBufferTransform();
    return GSERROR_OK;
}

GSError ConsumerSurface::IsSurfaceBufferInCache(uint32_t seqNum, bool &isInCache)
{
    if (consumer_ == nullptr) {
        BLOGNE("consumer is nullptr");
        return SURFACE_ERROR_UNKOWN;
    }
    return consumer_->IsSurfaceBufferInCache(seqNum, isInCache);
}
} // namespace OHOS
