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

#ifndef FRAMEWORKS_SURFACE_INCLUDE_BUFFER_CLIENT_PRODUCER_H
#define FRAMEWORKS_SURFACE_INCLUDE_BUFFER_CLIENT_PRODUCER_H

#include <map>
#include <vector>
#include <mutex>

#include <iremote_proxy.h>
#include <iremote_object.h>

#include <ibuffer_producer.h>

#include "surface_buffer_impl.h"

namespace OHOS {
class BufferClientProducer : public IRemoteProxy<IBufferProducer> {
public:
    BufferClientProducer(const sptr<IRemoteObject>& impl);
    virtual ~BufferClientProducer();

    GSError RequestBuffer(const BufferRequestConfig &config, sptr<BufferExtraData> &bedata,
                          RequestBufferReturnValue &retval) override;

    GSError CancelBuffer(uint32_t sequence, sptr<BufferExtraData> bedata) override;

    GSError FlushBuffer(uint32_t sequence, sptr<BufferExtraData> bedata,
                        sptr<SyncFence> fence, BufferFlushConfigWithDamages &config) override;
    GSError GetLastFlushedBuffer(sptr<SurfaceBuffer>& buffer,
        sptr<SyncFence>& fence, float matrix[16], bool isUseNewMatrix) override;
    uint32_t GetQueueSize() override;
    GSError SetQueueSize(uint32_t queueSize) override;

    GSError GetName(std::string &name) override;
    uint64_t GetUniqueId() override;
    GSError GetNameAndUniqueId(std::string& name, uint64_t& uniqueId) override;

    int32_t GetDefaultWidth() override;
    int32_t GetDefaultHeight() override;
    GSError SetDefaultUsage(uint64_t usage) override;
    uint64_t GetDefaultUsage() override;
    GSError SetTransform(GraphicTransformType transform) override;

    GSError AttachBuffer(sptr<SurfaceBuffer>& buffer) override;
    GSError AttachBuffer(sptr<SurfaceBuffer>& buffer, int32_t timeOut) override;
    GSError DetachBuffer(sptr<SurfaceBuffer>& buffer) override;
    GSError RegisterReleaseListener(sptr<IProducerListener> listener) override;
    GSError UnRegisterReleaseListener() override;

    GSError IsSupportedAlloc(const std::vector<BufferVerifyAllocInfo> &infos, std::vector<bool> &supporteds) override;

    // Call carefully. This interface will empty all caches of the current process
    GSError CleanCache() override;
    GSError Disconnect() override;
    GSError GoBackground() override;

    GSError SetScalingMode(uint32_t sequence, ScalingMode scalingMode) override;
    GSError SetBufferHold(bool hold) override;
    GSError SetMetaData(uint32_t sequence, const std::vector<GraphicHDRMetaData> &metaData) override;
    GSError SetMetaDataSet(uint32_t sequence, GraphicHDRMetadataKey key,
                           const std::vector<uint8_t> &metaData) override;
    GSError SetTunnelHandle(const GraphicExtDataHandle *handle) override;
    GSError GetPresentTimestamp(uint32_t sequence, GraphicPresentTimestampType type, int64_t &time) override;

    sptr<NativeSurface> GetNativeSurface() override;

    GSError SendAddDeathRecipientObject() override;

    GSError GetTransform(GraphicTransformType &transform) override;
    GSError AttachBufferToQueue(sptr<SurfaceBuffer> buffer) override;
    GSError DetachBufferFromQueue(sptr<SurfaceBuffer> buffer) override;

    GSError GetTransformHint(GraphicTransformType &transformHint) override;
    GSError SetTransformHint(GraphicTransformType transformHint) override;
    GSError SetScalingMode(ScalingMode scalingMode) override;

    GSError SetSurfaceSourceType(OHSurfaceSource sourceType) override;
    GSError GetSurfaceSourceType(OHSurfaceSource &sourceType) override;

    GSError SetSurfaceAppFrameworkType(std::string appFrameworkType) override;
    GSError GetSurfaceAppFrameworkType(std::string &appFrameworkType) override;
private:
    static inline BrokerDelegator<BufferClientProducer> delegator_;
    std::string name_ = "not init";
    uint64_t uniqueId_ = 0;
    std::mutex mutex_;
    sptr<IBufferProducerToken> token_;
    GraphicTransformType lastSetTransformType_ = GraphicTransformType::GRAPHIC_ROTATE_BUTT;
};
}; // namespace OHOS

#endif // FRAMEWORKS_SURFACE_INCLUDE_BUFFER_CLIENT_PRODUCER_H
