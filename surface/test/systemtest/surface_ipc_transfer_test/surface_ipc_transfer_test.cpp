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
constexpr uint32_t PATTERN_SEED = 0x5A5A0000;       // Branch1/2: basics and multi-cycle loop
constexpr uint32_t SMALL_PATTERN_SEED = 0x3C3C0000; // Branch3: small-size buffer after realloc

// Child report convention: a failed API call reports its real GSError; an expectation not met
// (queue identity, attribute or sequence mismatch) reports GSERROR_INVALID_OPERATING, so the
// parent never mistakes a child-side failure for GSERROR_OK.

// Child side: report through the pipe and exit immediately; every child failure path ends here
void ReportAndExit(int32_t pipeFd, int64_t code)
{
    write(pipeFd, &code, sizeof(code));
    exit(0);
}
} // namespace

class SurfaceIPCTransferTest : public testing::Test, public IBufferConsumerListenerClazz {
public:
    static void SetUpTestCase();
    void OnBufferAvailable() override;
    GSError SetData(sptr<SurfaceBuffer> &buffer, const sptr<Surface> &pSurface);
    bool GetData(sptr<SurfaceBuffer> &buffer);
    GSError FillPatternData(sptr<SurfaceBuffer> &buffer, uint32_t seed);
    bool CheckPatternData(sptr<SurfaceBuffer> &buffer, uint32_t seed);
    int64_t ReadChildReport();
    pid_t ChildProcessMain();
    sptr<OHOS::Surface> CreateSurface();
    void ChildBranch1(const sptr<OHOS::Surface> &pSurface);
    void ChildBranch2(const sptr<OHOS::Surface> &pSurface);
    void ChildBranch3(const sptr<OHOS::Surface> &pSurface);
    void SetupNativeToken();
    void ConsumeBranch1Frame();
    void CheckSecondProducerRejected(sptr<IBufferProducer> &producer);
    void ConsumeCycleFrame(uint32_t seed);
    void ConsumeSmallFrame();

    static inline sptr<IConsumerSurface> cSurface = nullptr;
    static inline int32_t pipeMain[2] = {};
    static inline int32_t pipeChild[2] = {};
    static inline int32_t ipcSystemAbilityID = 34156;
    static inline BufferRequestConfig requestConfig = {};
    static inline BufferRequestConfig smallConfig = {};
    static inline BufferFlushConfig flushConfig = {};
    static inline uint32_t cycleTimes = 3;
};

void SurfaceIPCTransferTest::SetUpTestCase()
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

    smallConfig = requestConfig;
    smallConfig.width = 0x40;
    smallConfig.height = 0x40;
    flushConfig = { .damage = {
        .w = 0x100,
        .h = 0x100,
    } };
}

void SurfaceIPCTransferTest::OnBufferAvailable()
{
}

static inline GSError OnBufferRelease(sptr<SurfaceBuffer> &buffer)
{
    return GSERROR_OK;
}

// Child side: write ExtraData and tunnel handle to the buffer obtained via IPC (carried by FlushBuffer)
GSError SurfaceIPCTransferTest::SetData(sptr<SurfaceBuffer> &buffer, const sptr<Surface> &pSurface)
{
    buffer->GetExtraData()->ExtraSet("123", 0x123);
    buffer->GetExtraData()->ExtraSet("345", (int64_t)0x345);
    buffer->GetExtraData()->ExtraSet("567", "567");

    uint32_t reserveInts = 1;
    GraphicExtDataHandle *handle = AllocExtDataHandle(reserveInts);
    handle->reserve[0] = 1;
    GSError ret = pSurface->SetTunnelHandle(handle);
    FreeExtDataHandle(handle);
    handle = nullptr;
    return ret;
}

// Parent side
bool SurfaceIPCTransferTest::GetData(sptr<SurfaceBuffer> &buffer)
{
    int32_t int32;
    int64_t int64;
    std::string str;
    buffer->GetExtraData()->ExtraGet("123", int32);
    buffer->GetExtraData()->ExtraGet("345", int64);
    buffer->GetExtraData()->ExtraGet("567", str);
    if ((int32 != 0x123) || (int64 != 0x345) || (str != "567")) {
        return false;
    }

    sptr<SurfaceTunnelHandle> handleGet = nullptr;
    handleGet = cSurface->GetTunnelHandle();
    if ((handleGet == nullptr) || (handleGet->GetHandle()->fd != -1) ||
        (handleGet->GetHandle()->reserveInts != 1) || (handleGet->GetHandle()->reserve[0] != 1)) {
            return false;
    }
    return true;
}

// Child side: write pattern data into the buffer obtained via IPC. virAddr is an on-demand mmap-ed
// mapping owned by this process; FlushCache after filling flushes the CPU cache so the parent can
// read the new data through its own mapping
GSError SurfaceIPCTransferTest::FillPatternData(sptr<SurfaceBuffer> &buffer, uint32_t seed)
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

bool SurfaceIPCTransferTest::CheckPatternData(sptr<SurfaceBuffer> &buffer, uint32_t seed)
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
int64_t SurfaceIPCTransferTest::ReadChildReport()
{
    int64_t data = -1;
    (void)read(pipeChild[0], &data, sizeof(data));
    return data;
}

// Child side: poll samgr for the producer registered by the parent, then wrap it as a local producer surface
sptr<OHOS::Surface> SurfaceIPCTransferTest::CreateSurface()
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

// Process B main body: fork before any parent-side IPC operation so the child inherits a clean
// binder runtime; after getting the producer via samgr, drive the buffer queue through 3 branches.
// Pipe read/write counts are strictly complementary with the parent (child writes 5 / reads 6,
// including start).
pid_t SurfaceIPCTransferTest::ChildProcessMain()
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
        ReportAndExit(pipeChild[1], GSERROR_INVALID_OPERATING);
    }

    ChildBranch1(pSurface);
    ChildBranch2(pSurface);
    ChildBranch3(pSurface);

    pSurface->UnRegisterReleaseListener();
    close(pipeMain[0]);
    close(pipeMain[1]);
    close(pipeChild[0]);
    close(pipeChild[1]);
    exit(0);
    return 0;
}

// Branch1 child side: request one buffer via IPC, verify its attributes, then submit it with
// ExtraData, tunnel handle and pattern data
void SurfaceIPCTransferTest::ChildBranch1(const sptr<OHOS::Surface> &pSurface)
{
    sptr<SurfaceBuffer> buffer = nullptr;
    int32_t releaseFence = -1;
    auto sRet = pSurface->RequestBuffer(buffer, releaseFence, requestConfig);
    if (sRet != OHOS::GSERROR_OK) {
        ReportAndExit(pipeChild[1], sRet);
    }

    if (buffer == nullptr || buffer->GetFileDescriptor() < 0 ||
        buffer->GetWidth() != requestConfig.width || buffer->GetHeight() != requestConfig.height ||
        buffer->GetFormat() != requestConfig.format) {
        ReportAndExit(pipeChild[1], GSERROR_INVALID_OPERATING);
    }
    sRet = SetData(buffer, pSurface);
    if (sRet != OHOS::GSERROR_OK) {
        ReportAndExit(pipeChild[1], sRet);
    }
    sRet = FillPatternData(buffer, PATTERN_SEED);
    if (sRet != OHOS::GSERROR_OK) {
        ReportAndExit(pipeChild[1], sRet);
    }
    sRet = pSurface->FlushBuffer(buffer, -1, flushConfig);
    int64_t data = sRet;
    write(pipeChild[1], &data, sizeof(data));
    read(pipeMain[0], &data, sizeof(data)); // wait for the parent to finish Branch1 consumption verification
}

// Branch2 child side: request and flush the buffer in lockstep cycles; the sequence number stays
// unchanged, proving the server reuses the buffer instead of reallocating per cycle
void SurfaceIPCTransferTest::ChildBranch2(const sptr<OHOS::Surface> &pSurface)
{
    uint32_t firstSequence = 0;
    for (uint32_t i = 0; i < cycleTimes; i++) {
        sptr<SurfaceBuffer> buffer = nullptr;
        int32_t releaseFence = -1;
        auto sRet = pSurface->RequestBuffer(buffer, releaseFence, requestConfig);
        if (sRet != OHOS::GSERROR_OK) {
            ReportAndExit(pipeChild[1], sRet);
        }
        if (i == 0) {
            firstSequence = buffer->GetSeqNum();
        } else if (buffer->GetSeqNum() != firstSequence) {
            ReportAndExit(pipeChild[1], GSERROR_INVALID_OPERATING);
        }
        // Use a different seed each cycle to verify buffer content is fully overwritten on reuse
        sRet = FillPatternData(buffer, PATTERN_SEED + i);
        if (sRet != OHOS::GSERROR_OK) {
            ReportAndExit(pipeChild[1], sRet);
        }
        sRet = pSurface->FlushBuffer(buffer, -1, flushConfig);
        int64_t data = sRet;
        write(pipeChild[1], &data, sizeof(data));
        read(pipeMain[0], &data, sizeof(data)); 
    }
}

// Branch3 child side: request with a smaller config; the server reallocates the buffer for the new
// size, then the frame is submitted after filling pattern data
void SurfaceIPCTransferTest::ChildBranch3(const sptr<OHOS::Surface> &pSurface)
{
    sptr<SurfaceBuffer> buffer = nullptr;
    int32_t releaseFence = -1;
    auto sRet = pSurface->RequestBuffer(buffer, releaseFence, smallConfig);
    if (sRet != OHOS::GSERROR_OK) {
        ReportAndExit(pipeChild[1], sRet);
    }
    if (buffer->GetWidth() != smallConfig.width || buffer->GetHeight() != smallConfig.height) {
        ReportAndExit(pipeChild[1], GSERROR_INVALID_OPERATING);
    }
    sRet = FillPatternData(buffer, SMALL_PATTERN_SEED);
    if (sRet != OHOS::GSERROR_OK) {
        ReportAndExit(pipeChild[1], sRet);
    }
    sRet = pSurface->FlushBuffer(buffer, -1, flushConfig);
    int64_t data = sRet;
    write(pipeChild[1], &data, sizeof(data));
    read(pipeMain[0], &data, sizeof(data)); // wait for the parent to finish Branch3 consumption verification
}

// Parent side: grant the native token needed by samgr registration, same as surface_ipc_test.cpp
void SurfaceIPCTransferTest::SetupNativeToken()
{
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
    uint64_t tokenId = GetAccessTokenId(&infoInstance);
    SetSelfTokenID(tokenId);
    int32_t ret = Security::AccessToken::AccessTokenKit::ReloadNativeTokenInfo();
    ASSERT_EQ(ret, Security::AccessToken::RET_SUCCESS);
}

// Parent side: consume the Branch1 frame and verify ExtraData, tunnel handle, damage and pattern data
void SurfaceIPCTransferTest::ConsumeBranch1Frame()
{
    sptr<SurfaceBuffer> buffer = nullptr;
    int32_t fence = -1;
    int64_t timestamp = 0;
    Rect damage = {};
    auto sRet = cSurface->AcquireBuffer(buffer, fence, timestamp, damage);
    EXPECT_EQ(sRet, OHOS::GSERROR_OK);
    EXPECT_NE(buffer, nullptr);
    // ExtraData and tunnel handle round-trip across processes unchanged
    EXPECT_EQ(GetData(buffer), true);
    // Pattern data matches word by word: parent and child zero-copy share the same DMA memory
    EXPECT_EQ(CheckPatternData(buffer, PATTERN_SEED), true);
    EXPECT_EQ(damage.w, flushConfig.damage.w);
    EXPECT_EQ(damage.h, flushConfig.damage.h);
    sRet = cSurface->ReleaseBuffer(buffer, -1);
    EXPECT_EQ(sRet, OHOS::GSERROR_OK);
}

// Parent side negative: the child already owns the producer connection; a second local producer
// surface in this process gets its request rejected by PID gating
void SurfaceIPCTransferTest::CheckSecondProducerRejected(sptr<IBufferProducer> &producer)
{
    auto pSurfaceSecond = Surface::CreateSurfaceAsProducer(producer);
    sptr<SurfaceBuffer> buffer = nullptr;
    int32_t releaseFence = -1;
    auto sRet = pSurfaceSecond->RequestBuffer(buffer, releaseFence, requestConfig);
    EXPECT_EQ(sRet, GSERROR_CONSUMER_IS_CONNECTED);
    EXPECT_EQ(buffer, nullptr);
}

// Parent side: consume one lockstep cycle frame and verify its pattern data
void SurfaceIPCTransferTest::ConsumeCycleFrame(uint32_t seed)
{
    sptr<SurfaceBuffer> buffer = nullptr;
    int32_t fence = -1;
    int64_t timestamp = 0;
    Rect damage = {};
    auto sRet = cSurface->AcquireBuffer(buffer, fence, timestamp, damage);
    EXPECT_EQ(sRet, OHOS::GSERROR_OK);
    EXPECT_NE(buffer, nullptr);
    EXPECT_EQ(CheckPatternData(buffer, seed), true);
    sRet = cSurface->ReleaseBuffer(buffer, -1);
    EXPECT_EQ(sRet, OHOS::GSERROR_OK);
}

// Parent side: consume the small-size frame after server realloc, verify size and pattern data
void SurfaceIPCTransferTest::ConsumeSmallFrame()
{
    sptr<SurfaceBuffer> buffer = nullptr;
    int32_t fence = -1;
    int64_t timestamp = 0;
    Rect damage = {};
    auto sRet = cSurface->AcquireBuffer(buffer, fence, timestamp, damage);
    EXPECT_EQ(sRet, OHOS::GSERROR_OK);
    EXPECT_NE(buffer, nullptr);
    EXPECT_EQ(buffer->GetWidth(), smallConfig.width);
    EXPECT_EQ(buffer->GetHeight(), smallConfig.height);
    EXPECT_EQ(CheckPatternData(buffer, SMALL_PATTERN_SEED), true);
    sRet = cSurface->ReleaseBuffer(buffer, -1);
    EXPECT_EQ(sRet, OHOS::GSERROR_OK);
}

/*
* Function: request and flush buffer on producer surface transferred by IPC
* Type: Function
* Rank: Important(1)
* EnvConditions: N/A
* CaseDescription: 1. process A create consumer surface, register producer as system ability,
*                     fork process B before any IPC operation in process A
*                  2. process B get producer from samgr and wrap as local producer surface,
*                     check queue unique id is the same one created by process A
*                  3. Branch1: process B request/flush buffer, check buffer attribute, ExtraData
*                     and tunnel handle; process A acquire buffer, check pattern data in shared
*                     memory and damage; local second producer in process A is rejected by pid gating
*                  4. Branch2: process B request/flush buffer in 3 cycles with different seeds,
*                     sequence number keeps the same (server reuse buffer, no realloc per cycle),
*                     process A acquire/release and check pattern data per cycle
*                  5. Branch3: process B request buffer with small config, server realloc buffer,
*                     both side check new size and pattern data
*/
HWTEST_F(SurfaceIPCTransferTest, BufferTransferIPC001, TestSize.Level0)
{
    // fork must precede any IPC operation on the parent side (token registration, samgr registration),
    // so the child inherits a clean binder runtime, same ordering as surface_ipc_test.cpp
    auto pid = ChildProcessMain();
    ASSERT_GE(pid, 0);

    SetupNativeToken();
    cSurface = IConsumerSurface::Create("SurfaceIPCTransferTest");
    cSurface->RegisterConsumerListener(this);
    auto producer = cSurface->GetProducer();
    auto sam = SystemAbilityManagerClient::GetInstance().GetSystemAbilityManager();
    sam->AddSystemAbility(ipcSystemAbilityID, producer->AsObject());

    // The start message carries the queue uniqueId for the child to verify it got the same queue
    int64_t data = static_cast<int64_t>(cSurface->GetUniqueId());
    write(pipeMain[1], &data, sizeof(data));

    //  Branch1: parent-side consumption verification 
    data = ReadChildReport();
    EXPECT_EQ(data, static_cast<int64_t>(OHOS::GSERROR_OK));
    ConsumeBranch1Frame();
    CheckSecondProducerRejected(producer);
    write(pipeMain[1], &data, sizeof(data));

    //  Branch2: parent-side consumption verification, 3 lockstep cycles 
    for (uint32_t i = 0; i < cycleTimes; i++) {
        data = ReadChildReport();
        EXPECT_EQ(data, static_cast<int64_t>(OHOS::GSERROR_OK));
        ConsumeCycleFrame(PATTERN_SEED + i);
        write(pipeMain[1], &data, sizeof(data));
    }

    //  Branch3: parent-side consumption verification, small-size frame after realloc 
    data = ReadChildReport();
    EXPECT_EQ(data, static_cast<int64_t>(OHOS::GSERROR_OK));
    ConsumeSmallFrame();
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
