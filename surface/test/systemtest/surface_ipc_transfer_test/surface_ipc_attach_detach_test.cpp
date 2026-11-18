/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
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

#include <csignal>
#include <unistd.h>
#include <vector>
#include <gtest/gtest.h>
#include <iservice_registry.h>
#include <surface.h>
#include "accesstoken_kit.h"
#include "iconsumer_surface.h"
#include "nativetoken_kit.h"
#include "token_setproc.h"

using namespace testing;
using namespace testing::ext;

namespace OHOS::Rosen {
namespace {
// Pattern data seed for each branch: the child writes seed+i word by word and the parent verifies
// word by word after InvalidateCache, proving both processes zero-copy share the same DMA memory
constexpr uint32_t ATTACH_PATTERN_SEED = 0x4A4A0000; // Branch1: frame submitted after attaching back
constexpr uint32_t BATCH_PATTERN_SEED = 0x52520000;  // Branch2: 3 frames attached in batch (seed+i)

// Child report convention: a failed API call reports its real GSError; an expectation not met
// (queue identity or sequence mismatch, a negative case not rejected) reports GSERROR_INVALID_OPERATING,
// so the parent never mistakes a child-side failure for GSERROR_OK.
} // namespace

class SurfaceIPCAttachDetachTest : public testing::Test, public IBufferConsumerListenerClazz {
public:
    static void SetUpTestCase();
    void OnBufferAvailable() override;
    GSError FillPatternData(sptr<SurfaceBuffer> &buffer, uint32_t seed);
    bool CheckPatternData(sptr<SurfaceBuffer> &buffer, uint32_t seed);
    int64_t ReadChildReport();
    pid_t ChildProcessMain();
    sptr<OHOS::Surface> CreateSurface();

    static inline sptr<IConsumerSurface> cSurface = nullptr;
    static inline int32_t pipeMain[2] = {};
    static inline int32_t pipeChild[2] = {};
    static inline int32_t ipcSystemAbilityID = 34156;
    static inline BufferRequestConfig requestConfig = {};
    static inline BufferFlushConfig flushConfig = {};
    static inline uint32_t queueSize = 3;
};

void SurfaceIPCAttachDetachTest::SetUpTestCase()
{
    GTEST_LOG_(INFO) << getpid();
    requestConfig = {
        .width = 0x100,  // small
        .height = 0x100, // small
        .strideAlignment = 0x8,
        .format = GRAPHIC_PIXEL_FMT_RGBA_8888,
        .usage = BUFFER_USAGE_CPU_READ | BUFFER_USAGE_CPU_WRITE | BUFFER_USAGE_MEM_DMA,
        .timeout = 0,
    };
    flushConfig = { .damage = {
        .w = 0x100,
        .h = 0x100,
    } };
}

void SurfaceIPCAttachDetachTest::OnBufferAvailable()
{
}

static inline GSError OnBufferRelease(sptr<SurfaceBuffer> &buffer)
{
    return GSERROR_OK;
}

// Child side: write pattern data into the buffer obtained via IPC. virAddr is an on-demand mmap-ed
// mapping owned by this process; FlushCache after filling flushes the CPU cache so the parent can
// read the new data through its own mapping
GSError SurfaceIPCAttachDetachTest::FillPatternData(sptr<SurfaceBuffer> &buffer, uint32_t seed)
{
    if (buffer == nullptr) {
        return GSERROR_INVALID_ARGUMENTS;
    }
    void *virAddr = buffer->GetVirAddr();
    if (virAddr == nullptr) {
        return GSERROR_NO_MEM;
    }
    uint32_t *addr = static_cast<uint32_t *>(virAddr);
    uint32_t words = buffer->GetSize() / sizeof(uint32_t);
    for (uint32_t i = 0; i < words; ++i) {
        addr[i] = seed + i;
    }
    return buffer->FlushCache();
}

bool SurfaceIPCAttachDetachTest::CheckPatternData(sptr<SurfaceBuffer> &buffer, uint32_t seed)
{
    if (buffer == nullptr) {
        return false;
    }
    void *virAddr = buffer->GetVirAddr();
    if (virAddr == nullptr) {
        return false;
    }
    (void)buffer->InvalidateCache();
    uint32_t *addr = static_cast<uint32_t *>(virAddr);
    uint32_t words = buffer->GetSize() / sizeof(uint32_t);
    for (uint32_t i = 0; i < words; ++i) {
        if (addr[i] != seed + i) {
            return false;
        }
    }
    return true;
}

// Parent side: read the child report. Set to -1 before reading so that when the child exits early
// on failure (EOF, nothing read), a stale value cannot be misjudged as GSERROR_OK
int64_t SurfaceIPCAttachDetachTest::ReadChildReport()
{
    int64_t data = -1;
    (void)read(pipeChild[0], &data, sizeof(data));
    return data;
}

// Child side: poll samgr for the producer registered by the parent, then wrap it as a local producer surface
sptr<OHOS::Surface> SurfaceIPCAttachDetachTest::CreateSurface()
{
    sptr<IRemoteObject> robj = nullptr;
    while (true) {
        auto sam = SystemAbilityManagerClient::GetInstance().GetSystemAbilityManager();
        robj = sam->GetSystemAbility(ipcSystemAbilityID);
        if (robj != nullptr) {
            break;
        }
        sleep(0);
    }

    auto producer = iface_cast<IBufferProducer>(robj);
    return Surface::CreateSurfaceAsProducer(producer);
}

// Process B main body: after getting the producer via samgr, drive the attach/detach scenarios
// through 2 branches in order. Each branch writes a report, then waits for the parent to finish
// consumption verification and reply an ack before entering the next branch; pipe read/write counts
// are strictly complementary with the parent (child writes 3 / reads 4, including start).
pid_t SurfaceIPCAttachDetachTest::ChildProcessMain()
{
    pipe(pipeMain);
    pipe(pipeChild);
    // Ignore SIGPIPE: if the child exits early on failure, the parent's subsequent ack writes get
    // EPIPE; a failed write replaces being killed by the signal, so gtest records the failure normally
    (void)signal(SIGPIPE, SIG_IGN);
    pid_t pid = fork();
    if (pid != 0) {
        return pid;
    }

    int64_t data;
    read(pipeMain[0], &data, sizeof(data));
    uint64_t uniqueId = static_cast<uint64_t>(data);
    auto pSurface = CreateSurface();
    pSurface->RegisterReleaseListener(OnBufferRelease);
    if (pSurface->GetUniqueId() != uniqueId) {
        data = GSERROR_INVALID_OPERATING;
        write(pipeChild[1], &data, sizeof(data));
        exit(0);
    }

    sptr<SurfaceBuffer> buffer = nullptr;
    int32_t releaseFence = -1;
    auto sRet = pSurface->RequestBuffer(buffer, releaseFence, requestConfig);
    if (sRet != OHOS::GSERROR_OK) {
        data = sRet;
        write(pipeChild[1], &data, sizeof(data));
        exit(0);
    }
    uint32_t sequence = buffer->GetSeqNum();
    // Negative: attaching a buffer already cached in the server queue again must be rejected
    sRet = pSurface->AttachBufferToQueue(buffer);
    if (sRet != SURFACE_ERROR_BUFFER_IS_INCACHE) {
        data = GSERROR_INVALID_OPERATING;
        write(pipeChild[1], &data, sizeof(data));
        exit(0);
    }
    // Detaching in REQUESTED state is legal; the buffer leaves the server cache
    sRet = pSurface->DetachBufferFromQueue(buffer);
    if (sRet != OHOS::GSERROR_OK) {
        data = sRet;
        write(pipeChild[1], &data, sizeof(data));
        exit(0);
    }
    // Negative: flushing a detached buffer must be rejected
    sRet = pSurface->FlushBuffer(buffer, -1, flushConfig);
    if (sRet != SURFACE_ERROR_BUFFER_NOT_INCACHE) {
        data = GSERROR_INVALID_OPERATING;
        write(pipeChild[1], &data, sizeof(data));
        exit(0);
    }
    sRet = pSurface->AttachBufferToQueue(buffer);
    if (sRet != OHOS::GSERROR_OK) {
        data = sRet;
        write(pipeChild[1], &data, sizeof(data));
        exit(0);
    }
    if (buffer->GetSeqNum() != sequence) {
        data = GSERROR_INVALID_OPERATING;
        write(pipeChild[1], &data, sizeof(data));
        exit(0);
    }
    sRet = FillPatternData(buffer, ATTACH_PATTERN_SEED);
    if (sRet != OHOS::GSERROR_OK) {
        data = sRet;
        write(pipeChild[1], &data, sizeof(data));
        exit(0);
    }
    sRet = pSurface->FlushBuffer(buffer, -1, flushConfig);
    data = sRet;
    write(pipeChild[1], &data, sizeof(data));
    read(pipeMain[0], &data, sizeof(data)); // wait for the parent to finish Branch1 consumption verification

    // -------- Branch2: batch attach and server-side buffer reuse --------
    sRet = pSurface->SetQueueSize(queueSize);
    if (sRet != OHOS::GSERROR_OK) {
        data = sRet;
        write(pipeChild[1], &data, sizeof(data));
        exit(0);
    }
    std::vector<sptr<SurfaceBuffer>> buffers;
    std::vector<uint32_t> sequences;
    for (uint32_t i = 0; i < queueSize; i++) {
        buffer = nullptr;
        sRet = pSurface->RequestBuffer(buffer, releaseFence, requestConfig);
        if (sRet != OHOS::GSERROR_OK) {
            data = sRet;
            write(pipeChild[1], &data, sizeof(data));
            exit(0);
        }
        buffers.push_back(buffer);
        sequences.push_back(buffer->GetSeqNum());
    }
    // Detach all: the server queue cache is emptied
    for (uint32_t i = 0; i < queueSize; i++) {
        sRet = pSurface->DetachBufferFromQueue(buffers[i]);
        if (sRet != OHOS::GSERROR_OK) {
            data = sRet;
            write(pipeChild[1], &data, sizeof(data));
            exit(0);
        }
    }
    // Attach each one back (sequence number unchanged), fill and submit
    for (uint32_t i = 0; i < queueSize; i++) {
        sRet = pSurface->AttachBufferToQueue(buffers[i]);
        if (sRet != OHOS::GSERROR_OK) {
            data = sRet;
            write(pipeChild[1], &data, sizeof(data));
            exit(0);
        }
        if (buffers[i]->GetSeqNum() != sequences[i]) {
            data = GSERROR_INVALID_OPERATING;
            write(pipeChild[1], &data, sizeof(data));
            exit(0);
        }
        sRet = FillPatternData(buffers[i], BATCH_PATTERN_SEED + i);
        if (sRet != OHOS::GSERROR_OK) {
            data = sRet;
            write(pipeChild[1], &data, sizeof(data));
            exit(0);
        }
        sRet = pSurface->FlushBuffer(buffers[i], -1, flushConfig);
        if (sRet != OHOS::GSERROR_OK) {
            data = sRet;
            write(pipeChild[1], &data, sizeof(data));
            exit(0);
        }
    }
    data = OHOS::GSERROR_OK;
    write(pipeChild[1], &data, sizeof(data));
    read(pipeMain[0], &data, sizeof(data)); // wait for the parent to finish consuming the 3 frames

    // Reuse verification: after buffers return to the free queue, 
    // requesting again should reuse the buffers with the original sequence numbers (no server reallocation)
    for (uint32_t i = 0; i < queueSize; i++) {
        buffer = nullptr;
        sRet = pSurface->RequestBuffer(buffer, releaseFence, requestConfig);
        if (sRet != OHOS::GSERROR_OK) {
            data = sRet;
            write(pipeChild[1], &data, sizeof(data));
            exit(0);
        }
        uint32_t newSequence = buffer->GetSeqNum();
        bool isReused = false;
        for (uint32_t j = 0; j < queueSize; j++) {
            if (newSequence == sequences[j]) {
                isReused = true;
                break;
            }
        }
        if (!isReused) {
            data = GSERROR_INVALID_OPERATING;
            write(pipeChild[1], &data, sizeof(data));
            exit(0);
        }
        // CancelBuffer returns it, keeping the queue state clean
        sRet = pSurface->CancelBuffer(buffer);
        if (sRet != OHOS::GSERROR_OK) {
            data = sRet;
            write(pipeChild[1], &data, sizeof(data));
            exit(0);
        }
    }
    data = OHOS::GSERROR_OK;
    write(pipeChild[1], &data, sizeof(data));
    read(pipeMain[0], &data, sizeof(data)); // wait for the parent's final ack

    pSurface->UnRegisterReleaseListener();
    close(pipeMain[0]);
    close(pipeMain[1]);
    close(pipeChild[0]);
    close(pipeChild[1]);
    exit(0);
    return 0;
}

/*
* Function: attach and detach buffer on producer surface transferred by IPC
* Type: Function
* Rank: Important(1)
* EnvConditions: N/A
* CaseDescription: 1. process A create consumer surface, register producer as system ability,
*                     fork process B before any IPC operation in process A
*                  2. process B get producer from samgr and wrap as local producer surface,
*                     check queue unique id is the same one created by process A
*                  3. Branch1: attach buffer already in queue cache is rejected with
*                     SURFACE_ERROR_BUFFER_IS_INCACHE, detach buffer success, flush detached
*                     buffer is rejected with SURFACE_ERROR_BUFFER_NOT_INCACHE, attach back to
*                     queue with same sequence and flush success, process A acquire and check
*                     pattern data
*                  4. Branch2: process B request 3 buffers, detach all and attach back one by
*                     one with different seeds, process A acquire 3 frames in order and queue is
*                     drained; re-request after release reuse the same sequence buffers
*/
HWTEST_F(SurfaceIPCAttachDetachTest, AttachDetachBufferIPC001, TestSize.Level0)
{
    // fork must precede any IPC operation on the parent side (token registration, samgr registration),
    // so the child inherits a clean binder runtime, same ordering as surface_ipc_test.cpp
    auto pid = ChildProcessMain();
    ASSERT_GE(pid, 0);

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
    int32_t rett = Security::AccessToken::AccessTokenKit::ReloadNativeTokenInfo();
    ASSERT_EQ(rett, Security::AccessToken::RET_SUCCESS);
    cSurface = IConsumerSurface::Create("SurfaceIPCAttachDetachTest");
    cSurface->RegisterConsumerListener(this);
    auto producer = cSurface->GetProducer();
    auto sam = SystemAbilityManagerClient::GetInstance().GetSystemAbilityManager();
    sam->AddSystemAbility(ipcSystemAbilityID, producer->AsObject());

    int64_t data = static_cast<int64_t>(cSurface->GetUniqueId());
    write(pipeMain[1], &data, sizeof(data));

    data = ReadChildReport();
    EXPECT_EQ(data, static_cast<int64_t>(OHOS::GSERROR_OK));
    {
        sptr<SurfaceBuffer> buffer = nullptr;
        int32_t fence = -1;
        int64_t timestamp = 0;
        Rect damage = {};
        auto sRet = cSurface->AcquireBuffer(buffer, fence, timestamp, damage);
        EXPECT_EQ(sRet, OHOS::GSERROR_OK);
        EXPECT_NE(buffer, nullptr);
        EXPECT_EQ(CheckPatternData(buffer, ATTACH_PATTERN_SEED), true);
        sRet = cSurface->ReleaseBuffer(buffer, -1);
        EXPECT_EQ(sRet, OHOS::GSERROR_OK);
    }
    write(pipeMain[1], &data, sizeof(data));

    data = ReadChildReport();
    EXPECT_EQ(data, static_cast<int64_t>(OHOS::GSERROR_OK));
    for (uint32_t i = 0; i < queueSize; i++) {
        sptr<SurfaceBuffer> buffer = nullptr;
        int32_t fence = -1;
        int64_t timestamp = 0;
        Rect damage = {};
        auto sRet = cSurface->AcquireBuffer(buffer, fence, timestamp, damage);
        EXPECT_EQ(sRet, OHOS::GSERROR_OK);
        EXPECT_NE(buffer, nullptr);
        // The dirty queue dequeues in submit order; verify each frame's own seed
        EXPECT_EQ(CheckPatternData(buffer, BATCH_PATTERN_SEED + i), true);
        sRet = cSurface->ReleaseBuffer(buffer, -1);
        EXPECT_EQ(sRet, OHOS::GSERROR_OK);
    }
    {
        sptr<SurfaceBuffer> buffer = nullptr;
        int32_t fence = -1;
        int64_t timestamp = 0;
        Rect damage = {};
        auto sRet = cSurface->AcquireBuffer(buffer, fence, timestamp, damage);
        EXPECT_EQ(sRet, GSERROR_NO_BUFFER);
        EXPECT_EQ(buffer, nullptr);
    }
    write(pipeMain[1], &data, sizeof(data));

    data = ReadChildReport();
    EXPECT_EQ(data, static_cast<int64_t>(OHOS::GSERROR_OK));
    write(pipeMain[1], &data, sizeof(data));

    //close resource
    close(pipeMain[0]);
    close(pipeMain[1]);
    close(pipeChild[0]);
    close(pipeChild[1]);
    sam->RemoveSystemAbility(ipcSystemAbilityID);
    int32_t ret = 0;
    do {
        ret = waitpid(pid, nullptr, 0);
    } while (ret == -1 && errno == EINTR);
    cSurface = nullptr;
}
} // namespace OHOS::Rosen
