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

#ifndef FRAMEWORKS_SURFACE_INCLUDE_CONSUMER_SURFACE_H
#define FRAMEWORKS_SURFACE_INCLUDE_CONSUMER_SURFACE_H

#include <map>
#include <string>

#include <iconsumer_surface.h>

#include "buffer_queue.h"
#include "buffer_queue_producer.h"
#include "buffer_queue_consumer.h"

namespace OHOS {
class ConsumerSurface : public IConsumerSurface {
public:
    ConsumerSurface(const std::string &name);
    virtual ~ConsumerSurface();
    GSError Init();

    bool IsConsumer() const override;
    sptr<IBufferProducer> GetProducer() const override;

    GSError AcquireBuffer(sptr<SurfaceBuffer>& buffer, int32_t &fence,
                          int64_t &timestamp, Rect &damage) override;

    GSError ReleaseBuffer(sptr<SurfaceBuffer>& buffer, int32_t fence) override;

    GSError AcquireBuffer(sptr<SurfaceBuffer>& buffer, sptr<SyncFence>& fence,
                          int64_t &timestamp, Rect &damage) override;
    GSError AcquireBuffer(sptr<SurfaceBuffer>& buffer, sptr<SyncFence>& fence,
                          int64_t &timestamp, std::vector<Rect> &damages) override;
    GSError AcquireBuffer(AcquireBufferReturnValue &returnValue, int64_t expectPresentTimestamp,
                          bool isUsingAutoTimestamp) override;
    GSError ReleaseBuffer(sptr<SurfaceBuffer>& buffer, const sptr<SyncFence>& fence) override;

    GSError AttachBuffer(sptr<SurfaceBuffer>& buffer) override;
    GSError DetachBuffer(sptr<SurfaceBuffer>& buffer) override;

    bool QueryIfBufferAvailable() override;

    uint32_t GetQueueSize() override;
    GSError SetQueueSize(uint32_t queueSize) override;

    const std::string& GetName() override;

    GSError SetDefaultWidthAndHeight(int32_t width, int32_t height) override;
    int32_t GetDefaultWidth() override;
    int32_t GetDefaultHeight() override;
    GSError SetDefaultUsage(uint64_t usage) override;
    uint64_t GetDefaultUsage() override;

    GSError SetUserData(const std::string &key, const std::string &val) override;
    std::string GetUserData(const std::string &key) override;

    GSError RegisterConsumerListener(sptr<IBufferConsumerListener>& listener) override;
    GSError RegisterConsumerListener(IBufferConsumerListenerClazz *listener) override;
    GSError RegisterReleaseListener(OnReleaseFunc func) override;
    GSError UnRegisterReleaseListener() override
    {
        return GSERROR_OK;
    }
    GSError RegisterDeleteBufferListener(OnDeleteBufferFunc func, bool isForUniRedraw = false) override;
    GSError UnregisterConsumerListener() override;

    uint64_t GetUniqueId() const override;

    void Dump(std::string &result) const override;

    void DumpCurrentFrameLayer() const override;

    GSError GoBackground() override;

    GSError SetTransform(GraphicTransformType transform) override;
    GraphicTransformType GetTransform() const override;
    GSError SetScalingMode(uint32_t sequence, ScalingMode scalingMode) override;
    GSError GetScalingMode(uint32_t sequence, ScalingMode &scalingMode) override;
    GSError SetMetaData(uint32_t sequence, const std::vector<GraphicHDRMetaData> &metaData) override;
    GSError SetMetaDataSet(uint32_t sequence, GraphicHDRMetadataKey key, const std::vector<uint8_t> &metaData) override;
    GSError QueryMetaDataType(uint32_t sequence, HDRMetaDataType &type) const override;
    GSError GetMetaData(uint32_t sequence, std::vector<GraphicHDRMetaData> &metaData) const override;
    GSError GetMetaDataSet(uint32_t sequence, GraphicHDRMetadataKey &key,
                           std::vector<uint8_t> &metaData) const override;
    GSError SetTunnelHandle(const GraphicExtDataHandle *handle) override;
    sptr<SurfaceTunnelHandle> GetTunnelHandle() const override;
    GSError SetPresentTimestamp(uint32_t sequence, const GraphicPresentTimestamp &timestamp) override;

    GSError AttachBuffer(sptr<SurfaceBuffer>& buffer, int32_t timeOut) override;
    GSError RegisterSurfaceDelegator(sptr<IRemoteObject> client) override;
    GSError RegisterReleaseListener(OnReleaseFuncWithFence func) override
    {
        return GSERROR_NOT_SUPPORT;
    }
    GSError RegisterUserDataChangeListener(const std::string &funcName, OnUserDataChangeFunc func) override;
    GSError UnRegisterUserDataChangeListener(const std::string &funcName) override;
    GSError ClearUserDataChangeListener() override;
    void ConsumerRequestCpuAccess(bool on) override;
    GSError AttachBufferToQueue(sptr<SurfaceBuffer> buffer) override;
    /**
     * @brief if isReserveSlot is true, a slot in the bufferqueue will be kept
     * empty until attachbuffer is used to fill the slot.
     * if isReserveSlot is true, it must used with AttachBufferToQueue together.
     */
    GSError DetachBufferFromQueue(sptr<SurfaceBuffer> buffer, bool isReserveSlot = false) override;
    GraphicTransformType GetTransformHint() const override;
    GSError SetTransformHint(GraphicTransformType transformHint) override;
    inline bool IsBufferHold() override
    {
        if (consumer_ == nullptr) {
            return false;
        }
        return consumer_->IsBufferHold();
    }
    void SetBufferHold(bool hold) override;
    GSError SetScalingMode(ScalingMode scalingMode) override;
    GSError SetSurfaceSourceType(OHSurfaceSource sourceType) override;
    OHSurfaceSource GetSurfaceSourceType() const override;
    GSError SetSurfaceAppFrameworkType(std::string appFrameworkType) override;
    std::string GetSurfaceAppFrameworkType() const override;
    float GetHdrWhitePointBrightness() const override;
    float GetSdrWhitePointBrightness() const override;

    GSError GetSurfaceBufferTransformType(sptr<SurfaceBuffer> buffer, GraphicTransformType *transformType) override;
    GSError IsSurfaceBufferInCache(uint32_t seqNum, bool &isInCache) override;
    GSError GetGlobalAlpha(int32_t &alpha) override;
    uint32_t GetAvailableBufferCount() const override;
    GSError GetLastFlushedDesiredPresentTimeStamp(int64_t &lastFlushedDesiredPresentTimeStamp) const override;
    GSError GetFrontDesiredPresentTimeStamp(int64_t &desiredPresentTimeStamp, bool &isAutoTimeStamp) const override;
    GSError GetBufferSupportFastCompose(bool &bufferSupportFastCompose) override;
    GSError GetBufferCacheConfig(const sptr<SurfaceBuffer>& buffer, BufferRequestConfig& config) override;
    GSError GetCycleBuffersNumber(uint32_t& cycleBuffersNumber) override;
    GSError SetCycleBuffersNumber(uint32_t cycleBuffersNumber) override
    {
        (void)cycleBuffersNumber;
        return GSERROR_NOT_SUPPORT;
    }
    GSError GetFrameGravity(int32_t &frameGravity) override;
    GSError GetFixedRotation(int32_t &fixedRotation) override;
    GSError GetLastConsumeTime(int64_t &lastConsumeTime) const override;
    GSError SetMaxQueueSize(uint32_t queueSize) override;
    GSError GetMaxQueueSize(uint32_t &queueSize) const override;
private:
    std::map<std::string, std::string> userData_;
    sptr<BufferQueueProducer> producer_ = nullptr;
    sptr<BufferQueueConsumer> consumer_ = nullptr;
    std::string name_ = "not init";
    std::map<std::string, OnUserDataChangeFunc> onUserDataChange_;
    std::mutex lockMutex_;
    uint64_t uniqueId_ = 0;
    std::atomic<bool> hasRegistercallBackForRT_ = false;
    std::atomic<bool> hasRegistercallBackForRedraw_ = false;
    std::atomic<bool> isFirstBuffer_ = true;
    std::atomic<bool> supportFastCompose_ = false;
};
} // namespace OHOS

#endif // FRAMEWORKS_SURFACE_INCLUDE_CONSUMER_SURFACE_H
