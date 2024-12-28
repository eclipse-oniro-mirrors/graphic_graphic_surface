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
#ifdef SUPPORT_ACCESS_TOKEN
#include <chrono>
#include <thread>
#include <vector>
#include <sys/wait.h>
#include <unistd.h>
#include <gtest/gtest.h>

#include <iservice_registry.h>
#include <surface.h>
#include <buffer_extra_data_impl.h>
#include <buffer_client_producer.h>
#include <buffer_queue_producer.h>
#include "buffer_consumer_listener.h"
#include "sync_fence.h"
#include "accesstoken_kit.h"
#include "nativetoken_kit.h"
#include "token_setproc.h"
#include <buffer_producer_listener.h>

using namespace testing;
using namespace testing::ext;

namespace OHOS::Rosen {
class BufferClientProducerRemoteTest : public testing::Test {
public:
    static void SetUpTestCase();
    static void TearDownTestCase();

    static inline BufferRequestConfig requestConfig = {
        .width = 0x100,
        .height = 0x100,
        .strideAlignment = 0x8,
        .format = GRAPHIC_PIXEL_FMT_RGBA_8888,
        .usage = BUFFER_USAGE_CPU_READ | BUFFER_USAGE_CPU_WRITE | BUFFER_USAGE_MEM_DMA,
        .timeout = 0,
        .transform = GraphicTransformType::GRAPHIC_ROTATE_90,
        .colorGamut = GraphicColorGamut::GRAPHIC_COLOR_GAMUT_NATIVE,
    };
    static inline BufferFlushConfigWithDamages flushConfig = {
        .damages = {
            {
                .w = 0x100,
                .h = 0x100,
            }
        },
    };
    static inline sptr<IRemoteObject> robj = nullptr;
    static inline sptr<IBufferProducer> bp = nullptr;
    static inline std::vector<uint32_t> deletingBuffers;
    static inline pid_t pid = 0;
    static inline int pipeFd[2] = {};
    static inline int pipe1Fd[2] = {};
    static inline int32_t systemAbilityID = 345135;
    static inline sptr<BufferExtraData> bedata = new BufferExtraDataImpl;
    static inline uint32_t firstSeqnum = 0;
};

static void InitNativeTokenInfo()
{
    uint64_t tokenId;
    const char *perms[2];
    perms[0] = "ohos.permission.DISTRIBUTED_DATASYNC";
    perms[1] = "ohos.permission.CAMERA";
    NativeTokenInfoParams infoInstance = {
        .dcapsNum = 0,
        .permsNum = 2,
        .aclsNum = 0,
        .dcaps = NULL,
        .perms = perms,
        .acls = NULL,
        .processName = "dcamera_client_demo",
        .aplStr = "system_basic",
    };
    tokenId = GetAccessTokenId(&infoInstance);
    SetSelfTokenID(tokenId);
    int32_t ret = Security::AccessToken::AccessTokenKit::ReloadNativeTokenInfo();
    ASSERT_EQ(ret, Security::AccessToken::RET_SUCCESS);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));  // wait 50ms
}

void BufferClientProducerRemoteTest::SetUpTestCase()
{
    if (pipe(pipeFd) < 0) {
        exit(1);
    }
    if (pipe(pipe1Fd) < 0) {
        exit(0);
    }
    pid = fork();
    if (pid < 0) {
        exit(1);
    }
    if (pid == 0) {
        InitNativeTokenInfo();
        sptr<BufferQueue> bq = new BufferQueue("test");
        ASSERT_NE(bq, nullptr);
        sptr<BufferQueueProducer> bqp = new BufferQueueProducer(bq);
        ASSERT_NE(bqp, nullptr);
        sptr<IBufferConsumerListener> listener = new BufferConsumerListener();
        bq->RegisterConsumerListener(listener);
        auto sam = SystemAbilityManagerClient::GetInstance().GetSystemAbilityManager();
        sam->AddSystemAbility(systemAbilityID, bqp);
        close(pipeFd[1]);
        close(pipe1Fd[0]);
        char buf[10] = "start";
        write(pipe1Fd[1], buf, sizeof(buf));
        sleep(0);
        read(pipeFd[0], buf, sizeof(buf));
        sam->RemoveSystemAbility(systemAbilityID);
        close(pipeFd[0]);
        close(pipe1Fd[1]);
        exit(0);
    } else {
        close(pipeFd[0]);
        close(pipe1Fd[1]);
        char buf[10];
        read(pipe1Fd[0], buf, sizeof(buf));
        auto sam = SystemAbilityManagerClient::GetInstance().GetSystemAbilityManager();
        robj = sam->GetSystemAbility(systemAbilityID);
        bp = iface_cast<IBufferProducer>(robj);
    }
}

void BufferClientProducerRemoteTest::TearDownTestCase()
{
    bp = nullptr;
    robj = nullptr;

    char buf[10] = "over";
    write(pipeFd[1], buf, sizeof(buf));
    close(pipeFd[1]);
    close(pipe1Fd[0]);

    int32_t ret = 0;
    do {
        waitpid(pid, nullptr, 0);
    } while (ret == -1 && errno == EINTR);
}

/*
* Function: IsProxyObject
* Type: Function
* Rank: Important(2)
* EnvConditions: N/A
* CaseDescription: 1. check ret for IsProxyObject func
 */
HWTEST_F(BufferClientProducerRemoteTest, IsProxy001, Function | MediumTest | Level2)
{
    ASSERT_TRUE(robj->IsProxyObject());
}

/*
* Function: SetQueueSize and GetQueueSize
* Type: Function
* Rank: Important(2)
* EnvConditions: N/A
* CaseDescription: 1. call GetQueueSize for default
*                  2. call SetQueueSize and check the ret of GetQueueSize
 */
HWTEST_F(BufferClientProducerRemoteTest, QueueSize001, Function | MediumTest | Level2)
{
    ASSERT_EQ(bp->GetQueueSize(), (uint32_t)SURFACE_DEFAULT_QUEUE_SIZE);

    GSError ret = bp->SetQueueSize(2);
    ASSERT_EQ(ret, OHOS::GSERROR_OK);

    ret = bp->SetQueueSize(SURFACE_MAX_QUEUE_SIZE + 1);
    ASSERT_NE(ret, OHOS::GSERROR_OK);

    ASSERT_EQ(bp->GetQueueSize(), 2u);
}

/*
* Function: SetQueueSize and GetQueueSize
* Type: Function
* Rank: Important(2)
* EnvConditions: N/A
* CaseDescription: 1. call GetQueueSize for default
*                  2. call SetQueueSize and check the ret of GetQueueSize
 */
HWTEST_F(BufferClientProducerRemoteTest, ReqCan001, Function | MediumTest | Level2)
{
    IBufferProducer::RequestBufferReturnValue retval;
    GSError ret = bp->RequestBuffer(requestConfig, bedata, retval);
    ASSERT_EQ(ret, OHOS::GSERROR_OK);
    ASSERT_NE(retval.buffer, nullptr);
    firstSeqnum = retval.buffer->GetSeqNum();

    ret = bp->CancelBuffer(retval.sequence, bedata);
    ASSERT_EQ(ret, OHOS::GSERROR_OK);
}

/*
* Function: RequestBuffer and CancelBuffer
* Type: Function
* Rank: Important(2)
* EnvConditions: N/A
* CaseDescription: 1. call RequestBuffer
*                  2. call CancelBuffer 2 times
 */
HWTEST_F(BufferClientProducerRemoteTest, ReqCan002, Function | MediumTest | Level2)
{
    IBufferProducer::RequestBufferReturnValue retval;
    GSError ret = bp->RequestBuffer(requestConfig, bedata, retval);
    ASSERT_EQ(ret, OHOS::GSERROR_OK);
    ASSERT_EQ(retval.buffer, nullptr);

    ret = bp->CancelBuffer(retval.sequence, bedata);
    ASSERT_EQ(ret, OHOS::GSERROR_OK);

    ret = bp->CancelBuffer(retval.sequence, bedata);
    ASSERT_NE(ret, OHOS::GSERROR_OK);
}

/*
* Function: RequestBuffer and CancelBuffer
* Type: Function
* Rank: Important(2)
* EnvConditions: N/A
* CaseDescription: 1. call RequestBuffer and CancelBuffer 3 times
 */
HWTEST_F(BufferClientProducerRemoteTest, ReqCan003, Function | MediumTest | Level2)
{
    IBufferProducer::RequestBufferReturnValue retval1;
    IBufferProducer::RequestBufferReturnValue retval2;
    IBufferProducer::RequestBufferReturnValue retval3;
    GSError ret;

    ret = bp->RequestBuffer(requestConfig, bedata, retval1);
    ASSERT_EQ(ret, OHOS::GSERROR_OK);
    ASSERT_EQ(retval1.buffer, nullptr);

    ret = bp->RequestBuffer(requestConfig, bedata, retval2);
    ASSERT_EQ(ret, OHOS::GSERROR_OK);
    ASSERT_NE(retval2.buffer, nullptr);

    ret = bp->RequestBuffer(requestConfig, bedata, retval3);
    ASSERT_NE(ret, OHOS::GSERROR_OK);
    ASSERT_EQ(retval3.buffer, nullptr);

    ret = bp->CancelBuffer(retval1.sequence, bedata);
    ASSERT_EQ(ret, OHOS::GSERROR_OK);

    ret = bp->CancelBuffer(retval2.sequence, bedata);
    ASSERT_EQ(ret, OHOS::GSERROR_OK);

    ret = bp->CancelBuffer(retval3.sequence, bedata);
    ASSERT_NE(ret, OHOS::GSERROR_OK);
}

/*
* Function: SetQueueSize, RequestBuffer and CancelBuffer
* Type: Function
* Rank: Important(2)
* EnvConditions: N/A
* CaseDescription: 1. call SetQueueSize
*                  2. call RequestBuffer and CancelBuffer
*                  3. call SetQueueSize again
 */
HWTEST_F(BufferClientProducerRemoteTest, SetQueueSizeDeleting001, Function | MediumTest | Level2)
{
    GSError ret = bp->SetQueueSize(1);
    ASSERT_EQ(ret, OHOS::GSERROR_OK);

    IBufferProducer::RequestBufferReturnValue retval;
    ret = bp->RequestBuffer(requestConfig, bedata, retval);
    ASSERT_EQ(ret, OHOS::GSERROR_OK);
    ASSERT_EQ(retval.buffer, nullptr);

    ret = bp->CancelBuffer(retval.sequence, bedata);
    ASSERT_EQ(ret, OHOS::GSERROR_OK);

    ret = bp->SetQueueSize(2);
    ASSERT_EQ(ret, OHOS::GSERROR_OK);
}

/*
* Function: RequestBuffer and FlushBuffer
* Type: Function
* Rank: Important(2)
* EnvConditions: N/A
* CaseDescription: 1. call RequestBuffer
*                  2. call FlushBuffer
 */
HWTEST_F(BufferClientProducerRemoteTest, ReqFlu001, Function | MediumTest | Level2)
{
    IBufferProducer::RequestBufferReturnValue retval;
    GSError ret = bp->RequestBuffer(requestConfig, bedata, retval);
    ASSERT_EQ(ret, OHOS::GSERROR_OK);

    sptr<SyncFence> acquireFence = SyncFence::INVALID_FENCE;
    ret = bp->FlushBuffer(retval.sequence, bedata, acquireFence, flushConfig);
    ASSERT_EQ(ret, OHOS::GSERROR_OK);

    sptr<SurfaceBuffer> bufferTmp;
    float matrix[16];
    bool isUseNewMatrix = false;
    ret = bp->GetLastFlushedBuffer(bufferTmp, acquireFence, matrix, isUseNewMatrix);
    ASSERT_EQ(ret, OHOS::GSERROR_OK);
    ASSERT_EQ(bufferTmp->GetSurfaceBufferColorGamut(), GraphicColorGamut::GRAPHIC_COLOR_GAMUT_NATIVE);
    ASSERT_EQ(bufferTmp->GetSurfaceBufferTransform(), GraphicTransformType::GRAPHIC_ROTATE_90);
    ASSERT_EQ(bufferTmp->GetSurfaceBufferWidth(), 0x100);
    ASSERT_EQ(bufferTmp->GetSurfaceBufferHeight(), 0x100);
}

/*
* Function: RequestBuffer and FlushBuffer
* Type: Function
* Rank: Important(2)
* EnvConditions: N/A
* CaseDescription: 1. call RequestBuffer
*                  2. call FlushBuffer 2 times
 */
HWTEST_F(BufferClientProducerRemoteTest, ReqFlu002, Function | MediumTest | Level2)
{
    IBufferProducer::RequestBufferReturnValue retval;
    GSError ret = bp->RequestBuffer(requestConfig, bedata, retval);
    ASSERT_EQ(ret, OHOS::GSERROR_OK);

    sptr<SyncFence> acquireFence = SyncFence::INVALID_FENCE;
    ret = bp->FlushBuffer(retval.sequence, bedata, acquireFence, flushConfig);
    ASSERT_EQ(ret, OHOS::GSERROR_OK);

    ret = bp->FlushBuffer(retval.sequence, bedata, acquireFence, flushConfig);
    ASSERT_NE(ret, OHOS::GSERROR_OK);
}

/*
* Function: AttachBuffer and DetachBuffer
* Type: Function
* Rank: Important(2)
* EnvConditions: N/A
* CaseDescription: 1. call AttachBuffer
*                  2. call DetachBuffer
*/
HWTEST_F(BufferClientProducerRemoteTest, AttachDetach001, Function | MediumTest | Level2)
{
    sptr<OHOS::SurfaceBuffer> buffer = new SurfaceBufferImpl(0);
    GSError ret = bp->AttachBuffer(buffer);
    ASSERT_EQ(ret, OHOS::GSERROR_NOT_SUPPORT);

    ret = bp->DetachBuffer(buffer);
    ASSERT_EQ(ret, OHOS::GSERROR_NOT_SUPPORT);
}

/*
* Function: RegisterReleaseListener
* Type: Function
* Rank: Important(2)
* EnvConditions: N/A
* CaseDescription: 1. call RegisterReleaseListener
*/
HWTEST_F(BufferClientProducerRemoteTest, RegisterReleaseListener001, Function | MediumTest | Level2)
{
    OnReleaseFunc onBufferRelease = nullptr;
    sptr<IProducerListener> listener = new BufferReleaseProducerListener(onBufferRelease);
    GSError ret = bp->RegisterReleaseListener(listener);
    ASSERT_EQ(ret, OHOS::GSERROR_OK);
}

/*
* Function: RegisterReleaseListenerWithFence
* Type: Function
* Rank: Important(2)
* EnvConditions: N/A
* CaseDescription: 1. call RegisterReleaseListenerWithFence
*/
HWTEST_F(BufferClientProducerRemoteTest, RegisterReleaseListenerWithFence001, Function | MediumTest | Level2)
{
    OnReleaseFuncWithFence onBufferReleaseWithFence = nullptr;
    sptr<IProducerListener> listener = new BufferReleaseProducerListener(nullptr, onBufferReleaseWithFence);
    GSError ret = bp->RegisterReleaseListenerWithFence(listener);
    ASSERT_EQ(ret, OHOS::GSERROR_OK);
}

/*
* Function: UnRegisterReleaseListener
* Type: Function
* Rank: Important(2)
* EnvConditions: N/A
* CaseDescription: 1. call UnRegisterReleaseListener
*/
HWTEST_F(BufferClientProducerRemoteTest, UnRegisterReleaseListener001, Function | MediumTest | Level2)
{
    GSError ret = bp->UnRegisterReleaseListener();
    ASSERT_EQ(ret, OHOS::GSERROR_OK);
}

/*
* Function: UnRegisterReleaseListenerWithFence
* Type: Function
* Rank: Important(2)
* EnvConditions: N/A
* CaseDescription: 1. call UnRegisterReleaseListenerWithFence
*/
HWTEST_F(BufferClientProducerRemoteTest, UnRegisterReleaseListenerWithFence001, Function | MediumTest | Level2)
{
    GSError ret = bp->UnRegisterReleaseListenerWithFence();
    ASSERT_EQ(ret, OHOS::GSERROR_OK);
}

/*
* Function: GetName
* Type: Function
* Rank: Important(2)
* EnvConditions: N/A
* CaseDescription: 1. call GetName
*/
HWTEST_F(BufferClientProducerRemoteTest, GetName001, Function | MediumTest | Level2)
{
    std::string name;
    GSError ret = bp->GetName(name);
    ASSERT_EQ(ret, OHOS::GSERROR_OK);
}

/*
* Function: GetUniqueId
* Type: Function
* Rank: Important(2)
* EnvConditions: N/A
* CaseDescription: 1. call GetUniqueId
*/
HWTEST_F(BufferClientProducerRemoteTest, GetUniqueId001, Function | MediumTest | Level2)
{
    uint64_t bpid = bp->GetUniqueId();
    ASSERT_NE(bpid, 0);
    string name;
    GSError ret = bp->GetNameAndUniqueId(name, bpid);
    ASSERT_EQ(ret, OHOS::GSERROR_OK);
    ASSERT_NE(bpid, 0);
    ASSERT_NE(bpid, 0);
}

/*
* Function: GetDefaultUsage
* Type: Function
* Rank: Important(2)
* EnvConditions: N/A
* CaseDescription: 1. call GetDefaultUsage
*/
HWTEST_F(BufferClientProducerRemoteTest, GetDefaultUsage001, Function | MediumTest | Level2)
{
    uint64_t usage = bp->GetDefaultUsage();
    ASSERT_EQ(usage, 0);
}

/*
* Function: SetTransform
* Type: Function
* Rank: Important(2)
* EnvConditions: N/A
* CaseDescription: 1. call SetTransform
*/
HWTEST_F(BufferClientProducerRemoteTest, SetTransform001, Function | MediumTest | Level2)
{
    GraphicTransformType transform = GraphicTransformType::GRAPHIC_ROTATE_90;
    GSError ret = bp->SetTransform(transform);
    ASSERT_EQ(ret, OHOS::GSERROR_OK);
    GraphicTransformType transform2 = GraphicTransformType::GRAPHIC_ROTATE_NONE;
    ASSERT_EQ(bp->GetTransform(transform2), OHOS::GSERROR_OK);
    ASSERT_EQ(transform, transform2);
}

/*
* Function: SetScalingMode
* Type: Function
* Rank: Important(2)
* EnvConditions: N/A
* CaseDescription: 1. call SetScalingMode with abnormal parameters and check ret
*/
HWTEST_F(BufferClientProducerRemoteTest, SetScalingMode001, Function | MediumTest | Level2)
{
    ScalingMode scalingMode = ScalingMode::SCALING_MODE_SCALE_TO_WINDOW;
    GSError ret = bp->SetScalingMode(-1, scalingMode);
    ASSERT_EQ(ret, OHOS::GSERROR_NO_ENTRY);
}

/*
* Function: SetScalingMode002
* Type: Function
* Rank: Important(2)
* EnvConditions: N/A
* CaseDescription: 1. call SetScalingMode with abnormal parameters and check ret
*/
HWTEST_F(BufferClientProducerRemoteTest, SetScalingMode002, Function | MediumTest | Level2)
{
    ScalingMode scalingMode = ScalingMode::SCALING_MODE_SCALE_TO_WINDOW;
    GSError ret = bp->SetScalingMode(scalingMode);
    ASSERT_EQ(ret, OHOS::GSERROR_OK);
}

/*
* Function: SetMetaData
* Type: Function
* Rank: Important(2)
* EnvConditions: N/A
* CaseDescription: 1. call SetMetaData with abnormal parameters and check ret
*/
HWTEST_F(BufferClientProducerRemoteTest, SetMetaData001, Function | MediumTest | Level2)
{
    std::vector<GraphicHDRMetaData> metaData;
    GSError ret = bp->SetMetaData(firstSeqnum, metaData);
    ASSERT_EQ(ret, OHOS::GSERROR_INVALID_ARGUMENTS);
}

/*
* Function: SetMetaDataSet
* Type: Function
* Rank: Important(2)
* EnvConditions: N/A
* CaseDescription: 1. call SetMetaDataSet with abnormal parameters and check ret
*/
HWTEST_F(BufferClientProducerRemoteTest, SetMetaDataSet001, Function | MediumTest | Level2)
{
    GraphicHDRMetadataKey key = GraphicHDRMetadataKey::GRAPHIC_MATAKEY_HDR10_PLUS;
    std::vector<uint8_t> metaData;

    GSError ret = bp->SetMetaDataSet(firstSeqnum, key, metaData);
    ASSERT_EQ(ret, OHOS::GSERROR_INVALID_ARGUMENTS);
}

/*
* Function: GoBackground
* Type: Function
* Rank: Important(2)
* EnvConditions: N/A
* CaseDescription: 1. call GoBackground
*/
HWTEST_F(BufferClientProducerRemoteTest, GoBackground001, Function | MediumTest | Level2)
{
    GSError ret = bp->GoBackground();
    ASSERT_EQ(ret, OHOS::GSERROR_OK);
}

/*
* Function: AttachBuffer
* Type: Function
* Rank: Important(2)
* EnvConditions: N/A
* CaseDescription: 1. call AttachBuffer
*/
HWTEST_F(BufferClientProducerRemoteTest, AttachBuffer001, Function | MediumTest | Level2)
{
    GSError ret = bp->CleanCache(false);
    sptr<SurfaceBuffer> buffer = SurfaceBuffer::Create();
    ASSERT_NE(buffer, nullptr);
    ret = buffer->Alloc(requestConfig);
    ASSERT_EQ(ret, OHOS::GSERROR_OK);
    sptr<SyncFence> fence = SyncFence::INVALID_FENCE;
    int32_t timeOut = 1;
    ret = bp->AttachBuffer(buffer, timeOut);
    ASSERT_EQ(ret, OHOS::GSERROR_OK);
}

/*
* Function: SetSurfaceSourceType and GetSurfaceSourceType
* Type: Function
* Rank: Important(2)
* EnvConditions: N/A
* CaseDescription: 1. call GetSurfaceSourceType for default
*                  2. call SetSurfaceSourceType and check the ret
*/
HWTEST_F(BufferClientProducerRemoteTest, SurfaceSourceType001, Function | MediumTest | Level2)
{
    OHSurfaceSource sourceType;
    bp->GetSurfaceSourceType(sourceType);
    ASSERT_EQ(sourceType, OH_SURFACE_SOURCE_DEFAULT);

    GSError ret = bp->SetSurfaceSourceType(OH_SURFACE_SOURCE_VIDEO);
    ASSERT_EQ(ret, OHOS::GSERROR_OK);
    bp->GetSurfaceSourceType(sourceType);
    ASSERT_EQ(sourceType, OH_SURFACE_SOURCE_VIDEO);
}

/*
* Function: SetSurfaceAppFrameworkType and GetSurfaceAppFrameworkType
* Type: Function
* Rank: Important(2)
* EnvConditions: N/A
* CaseDescription: 1. call GetSurfaceAppFrameworkType for default
*                  2. call SetSurfaceAppFrameworkType and check the ret
*/
HWTEST_F(BufferClientProducerRemoteTest, SurfaceAppFrameworkType001, Function | MediumTest | Level2)
{
    std::string appFrameworkType;
    bp->GetSurfaceAppFrameworkType(appFrameworkType);
    ASSERT_EQ(appFrameworkType, "");

    GSError ret = bp->SetSurfaceAppFrameworkType("test");
    ASSERT_EQ(ret, OHOS::GSERROR_OK);
    bp->GetSurfaceAppFrameworkType(appFrameworkType);
    ASSERT_EQ(appFrameworkType, "test");
}
/*
* Function: RequestBuffersAndFlushBuffers
* Type: Function
* Rank: Important(1)
* EnvConditions: N/A
* CaseDescription: 1. call RequestBuffers and FlushBuffers
* @tc.require: issueI5GMZN issueI5IWHW
 */
HWTEST_F(BufferClientProducerRemoteTest, RequestBuffersAndFlushBuffers, Function | MediumTest | Level2)
{
    constexpr uint32_t size = 12;
    bp->SetQueueSize(size);
    std::vector<IBufferProducer::RequestBufferReturnValue> retvalues;
    std::vector<sptr<BufferExtraData>> bedatas;
    std::vector<BufferFlushConfigWithDamages> flushConfigs;
    retvalues.resize(size);
    GSError ret = bp->RequestBuffers(requestConfig, bedatas, retvalues);
    EXPECT_EQ(ret, OHOS::SURFACE_ERROR_UNKOWN);
    for (uint32_t i = 0; i < size * 10; ++i) {
        sptr<BufferExtraData> data = new BufferExtraDataImpl;
        bedatas.emplace_back(data);
        flushConfigs.emplace_back(flushConfig);
    }
    ret = bp->RequestBuffers(requestConfig, bedatas, retvalues);
    EXPECT_EQ(ret, OHOS::SURFACE_ERROR_UNKOWN);
    bedatas.resize(size);
    ret = bp->RequestBuffers(requestConfig, bedatas, retvalues);
    EXPECT_EQ(ret, OHOS::GSERROR_OK);
    for (const auto &retval : retvalues) {
        EXPECT_NE(retval.buffer, nullptr);
    }
    std::cout << "request buffers ok\n";
    std::vector<sptr<SyncFence>> acquireFences;
    std::vector<uint32_t> sequences;
    ret = bp->FlushBuffers(sequences, bedatas, acquireFences, flushConfigs);
    EXPECT_EQ(ret, OHOS::SURFACE_ERROR_UNKOWN);
    for (const auto &i : retvalues) {
        sequences.emplace_back(i.sequence);
    }
    for (uint32_t i = 0; i < size * 10; ++i) {
        acquireFences.emplace_back(new SyncFence(-1));
        sequences.emplace_back(i);
    }
    ret = bp->FlushBuffers(sequences, bedatas, acquireFences, flushConfigs);
    EXPECT_EQ(ret, OHOS::SURFACE_ERROR_UNKOWN);
    sequences.resize(retvalues.size());
    acquireFences.resize(retvalues.size());
    ret = bp->FlushBuffers(sequences, bedatas, acquireFences, flushConfigs);
    EXPECT_EQ(ret, OHOS::GSERROR_OK);
}

/*
* Function: AcquireAndReleaseLastFlushedBuffer
* Type: Function
* Rank: Important(1)
* EnvConditions: N/A
* CaseDescription: 1. call AcquireLastFlushedBuffer and check the ret
*                  2. call ReleaseLastFlushedBuffer and check the ret
 */
HWTEST_F(BufferClientProducerRemoteTest, AcquireAndReleaseLastFlushedBuffer001, Function | MediumTest | Level2)
{
    sptr<SurfaceBuffer> buffer;
    sptr<SyncFence> fence;
    float matrix[16];
    GSError ret = bp->AcquireLastFlushedBuffer(buffer, fence, matrix, 16, false);
    EXPECT_EQ(ret, OHOS::GSERROR_OK);
    EXPECT_NE(buffer, nullptr);
    ret = bp->ReleaseLastFlushedBuffer(buffer->GetSeqNum());
    EXPECT_EQ(ret, OHOS::GSERROR_OK);
}

/*
* Function: SetBufferhold
* Type: Function
* Rank: Important(1)
* EnvConditions: N/A
* CaseDescription: 1. call SetBufferhold and check ret
* @tc.require: issueI5GMZN issueI5IWHW
 */
HWTEST_F(BufferClientProducerRemoteTest, SetBufferhold001, Function | MediumTest | Level2)
{
    EXPECT_EQ(bp->SetBufferHold(true), GSERROR_OK);
    EXPECT_EQ(bp->SetBufferHold(false), GSERROR_OK);
}

/*
* Function: SetWhitePointBrightness
* Type: Function
* Rank: Important(1)
* EnvConditions: N/A
* CaseDescription: 1. call SetWhitePointBrightness and check ret
* @tc.require: issueI5GMZN issueI5IWHW
 */
HWTEST_F(BufferClientProducerRemoteTest, SetWhitePointBrightness001, Function | MediumTest | Level2)
{
    EXPECT_EQ(bp->SetHdrWhitePointBrightness(1), GSERROR_OK);
    EXPECT_EQ(bp->SetSdrWhitePointBrightness(1), GSERROR_OK);
}

/*
* Function: AttachAndDetachBuffer
* Type: Function
* Rank: Important(1)
* EnvConditions: N/A
* CaseDescription: 1. call AttachBufferFromQueue and check the ret
*                  2. call DetachBufferFromQueue and check the ret
 */
HWTEST_F(BufferClientProducerRemoteTest, AcquireLastFlushedBuffer001, Function | MediumTest | Level2)
{
    sptr<SurfaceBuffer> buffer = SurfaceBuffer::Create();
    GSError ret = bp->AttachBufferToQueue(buffer);
    ASSERT_NE(ret, OHOS::GSERROR_OK);
    ret = bp->DetachBufferFromQueue(buffer);
    ASSERT_NE(ret, OHOS::GSERROR_OK);
}

/*
* Function: SetGlobalAlpha
* Type: Function
* Rank: Important(1)
* EnvConditions: N/A
* CaseDescription: 1. call SetGlobalAlpha and check the ret
 */
HWTEST_F(BufferClientProducerRemoteTest, SetGlobalAlpha001, Function | MediumTest | Level2)
{
    ASSERT_EQ(bp->SetGlobalAlpha(-1), OHOS::GSERROR_OK);
    ASSERT_EQ(bp->SetGlobalAlpha(255), OHOS::GSERROR_OK);
}

/**
 * Function: SetScalingMode and GetScalingMode
 * Type: Function
 * Rank: Important(2)
 * EnvConditions: N/A
 * CaseDescription: 1. preSetUp: na
 *                  2. operation: call RequestAndDetachBuffer and AttachAndFlushBuffer
 *                  3. result: return OK
 */
HWTEST_F(BufferClientProducerRemoteTest, RequestAndDetachBuffer001, Function | MediumTest | Level2)
{
    ASSERT_EQ(bp->CleanCache(true), OHOS::GSERROR_OK);
    BufferRequestConfig requestConfigTmp = {
        .width = 0x100,
        .height = 0x100,
        .strideAlignment = 0x8,
        .format = GRAPHIC_PIXEL_FMT_RGBA_8888,
        .usage = BUFFER_USAGE_CPU_READ | BUFFER_USAGE_CPU_WRITE | BUFFER_USAGE_MEM_DMA,
        .timeout = 0,
    };
    BufferFlushConfig flushConfig = {
        .damage = {
            .w = 0x100,
            .h = 0x100,
        },
    };
    sptr<SurfaceBuffer> buffer = nullptr;
    sptr<SyncFence> fence = SyncFence::INVALID_FENCE;
    GSError ret = bp->RequestAndDetachBuffer(buffer, fence, requestConfigTmp);
    ASSERT_EQ(ret, OHOS::GSERROR_OK);
    ret = bp->AttachAndFlushBuffer(buffer, fence, flushConfig, false);
    ASSERT_EQ(ret, OHOS::GSERROR_OK);
    ASSERT_EQ(bp->CleanCache(true), OHOS::GSERROR_OK);
}
}
#endif
