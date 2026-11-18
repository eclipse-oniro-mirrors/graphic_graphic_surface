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
// 各分支特征数据种子：子进程逐字写入 seed+i，父进程 InvalidateCache 后逐字校验，
// 验证跨进程双方零拷贝共享同一块 DMA 内存
constexpr uint32_t PATTERN_SEED = 0x5A5A0000;       // Branch1/2：基础与多轮循环
constexpr uint32_t SMALL_PATTERN_SEED = 0x3C3C0000; // Branch3：realloc 后的小尺寸 buffer

// 子进程校验失败上报的合成码 = 基址 + 步骤号。GSError 本身无法区分是哪一步失败，
// 父进程 EXPECT_EQ(data, GSERROR_OK) 失败时可用上报码减去基址定位子进程内的校验步骤
constexpr int64_t CHILD_CHECK_FAILED_BASE = 100000;
} // namespace

class SurfaceIPCTransferTest : public testing::Test, public IBufferConsumerListenerClazz {
public:
    static void SetUpTestCase();
    void OnBufferAvailable() override;
    GSError SetData(sptr<SurfaceBuffer> &buffer, sptr<Surface> &pSurface);
    bool GetData(sptr<SurfaceBuffer> &buffer);
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
    // Branch3 用小尺寸配置触发服务端 realloc：与上一帧配置不同时 ReuseBuffer 会重新分配
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

// 子进程侧：向跨 IPC 拿到的 buffer 写 ExtraData 与 tunnel handle（FlushBuffer 携带上报）
GSError SurfaceIPCTransferTest::SetData(sptr<SurfaceBuffer> &buffer, sptr<Surface> &pSurface)
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

// 父进程侧：校验消费到的 buffer 携带的 ExtraData 与 tunnel handle 跨进程往返一致
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

// 子进程侧：向跨 IPC 拿到的 buffer 写特征数据。virAddr 是本进程按需 mmap 的独立映射，
// 写满后 FlushCache 刷出 CPU cache，父进程才能透过自己的映射读到新数据
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

// 父进程侧：InvalidateCache 后逐字校验特征数据（与子进程零拷贝共享同一块 DMA 内存）
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

// 父进程侧：读子进程上报。先置 -1 再读，子进程失败提前退出（EOF 读不到数据）时
// 不会被残留值误判为 GSERROR_OK
int64_t SurfaceIPCTransferTest::ReadChildReport()
{
    int64_t data = -1;
    (void)read(pipeChild[0], &data, sizeof(data));
    return data;
}

// 子进程侧：轮询 samgr 拉取父进程注册的 producer，包装成本地生产者 surface
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

// 进程B主体：经 samgr 拿到 producer 后，按 3 个分支依次驱动 buffer 队列。
// 每个分支完成后写一条 report，等父进程完成消费校验并回 ack 再进入下一分支，
// 管道读写次数与父进程严格互补（子写 5 / 子读 6，含 start）。
pid_t SurfaceIPCTransferTest::ChildProcessMain()
{
    pipe(pipeMain);
    pipe(pipeChild);
    // 忽略 SIGPIPE：子进程失败提前退出后，父进程继续写 ack 会得到 EPIPE，
    // 以写失败代替进程被信号杀死，让 gtest 正常记录失败
    (void)signal(SIGPIPE, SIG_IGN);
    pid_t pid = fork();
    if (pid != 0) {
        return pid;
    }

    int64_t data;
    read(pipeMain[0], &data, sizeof(data));
    // start 信号携带队列 uniqueId：校验经 IPC 拿到的确实是父进程创建的同一个队列
    uint64_t uniqueId = static_cast<uint64_t>(data);
    auto pSurface = CreateSurface();
    pSurface->RegisterReleaseListener(OnBufferRelease);
    if (pSurface->GetUniqueId() != uniqueId) {
        data = CHILD_CHECK_FAILED_BASE + 1;
        write(pipeChild[1], &data, sizeof(data));
        exit(0);
    }

    // ---------------- Branch1：跨 IPC RequestBuffer/FlushBuffer 基础能力 ----------------
    sptr<SurfaceBuffer> buffer = nullptr;
    int32_t releaseFence = -1;
    auto sRet = pSurface->RequestBuffer(buffer, releaseFence, requestConfig);
    if (sRet != OHOS::GSERROR_OK) {
        data = sRet;
        write(pipeChild[1], &data, sizeof(data));
        exit(0);
    }
    // 校验跨进程收到的 buffer：fd 已复制到本进程、属性与请求配置一致
    if (buffer == nullptr || buffer->GetFileDescriptor() < 0 ||
        buffer->GetWidth() != requestConfig.width || buffer->GetHeight() != requestConfig.height ||
        buffer->GetFormat() != requestConfig.format) {
        data = CHILD_CHECK_FAILED_BASE + 11;
        write(pipeChild[1], &data, sizeof(data));
        exit(0);
    }
    sRet = SetData(buffer, pSurface);
    if (sRet != OHOS::GSERROR_OK) {
        data = sRet;
        write(pipeChild[1], &data, sizeof(data));
        exit(0);
    }
    sRet = FillPatternData(buffer, PATTERN_SEED);
    if (sRet != OHOS::GSERROR_OK) {
        data = sRet;
        write(pipeChild[1], &data, sizeof(data));
        exit(0);
    }
    sRet = pSurface->FlushBuffer(buffer, -1, flushConfig);
    data = sRet;
    write(pipeChild[1], &data, sizeof(data));
    read(pipeMain[0], &data, sizeof(data)); // 等父进程完成 Branch1 消费校验

    // ---------------- Branch2：多轮 RequestBuffer/FlushBuffer 循环 ----------------
    uint32_t firstSequence = 0;
    for (uint32_t i = 0; i < cycleTimes; i++) {
        buffer = nullptr;
        sRet = pSurface->RequestBuffer(buffer, releaseFence, requestConfig);
        if (sRet != OHOS::GSERROR_OK || buffer == nullptr) {
            data = (sRet != OHOS::GSERROR_OK) ? sRet : CHILD_CHECK_FAILED_BASE + 21;
            write(pipeChild[1], &data, sizeof(data));
            exit(0);
        }
        // lockstep 消费下队列只有 1 个 buffer 轮转：每轮序列号应与首轮一致，
        // 说明服务端复用同一块 buffer 而不是每轮重新分配
        if (i == 0) {
            firstSequence = buffer->GetSeqNum();
        } else if (buffer->GetSeqNum() != firstSequence) {
            data = CHILD_CHECK_FAILED_BASE + 22;
            write(pipeChild[1], &data, sizeof(data));
            exit(0);
        }
        // 每轮使用不同种子，校验 buffer 复用时内容被完整覆盖
        sRet = FillPatternData(buffer, PATTERN_SEED + i);
        if (sRet != OHOS::GSERROR_OK) {
            data = sRet;
            write(pipeChild[1], &data, sizeof(data));
            exit(0);
        }
        sRet = pSurface->FlushBuffer(buffer, -1, flushConfig);
        data = sRet;
        write(pipeChild[1], &data, sizeof(data));
        read(pipeMain[0], &data, sizeof(data)); // 等父进程消费完本轮再进入下一轮
    }

    // ---------------- Branch3：请求配置变更触发服务端 realloc ----------------
    buffer = nullptr;
    sRet = pSurface->RequestBuffer(buffer, releaseFence, smallConfig);
    if (sRet != OHOS::GSERROR_OK || buffer == nullptr ||
        buffer->GetWidth() != smallConfig.width || buffer->GetHeight() != smallConfig.height) {
        data = (sRet != OHOS::GSERROR_OK) ? sRet : CHILD_CHECK_FAILED_BASE + 31;
        write(pipeChild[1], &data, sizeof(data));
        exit(0);
    }
    sRet = FillPatternData(buffer, SMALL_PATTERN_SEED);
    if (sRet != OHOS::GSERROR_OK) {
        data = sRet;
        write(pipeChild[1], &data, sizeof(data));
        exit(0);
    }
    sRet = pSurface->FlushBuffer(buffer, -1, flushConfig);
    data = sRet;
    write(pipeChild[1], &data, sizeof(data));
    read(pipeMain[0], &data, sizeof(data)); // 等父进程完成 Branch3 消费校验

    pSurface->UnRegisterReleaseListener();
    close(pipeMain[0]);
    close(pipeMain[1]);
    close(pipeChild[0]);
    close(pipeChild[1]);
    exit(0);
    return 0;
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
    // fork 必须先于父进程侧任何 IPC 操作（token 注册、samgr 注册），
    // 保证子进程继承到干净的 binder 运行时，与 surface_ipc_test.cpp 同款时序
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
    cSurface = IConsumerSurface::Create("SurfaceIPCTransferTest");
    cSurface->RegisterConsumerListener(this);
    auto producer = cSurface->GetProducer();
    auto sam = SystemAbilityManagerClient::GetInstance().GetSystemAbilityManager();
    sam->AddSystemAbility(ipcSystemAbilityID, producer->AsObject());

    // start 信号携带队列 uniqueId，供子进程校验拿到的是同一个队列
    int64_t data = static_cast<int64_t>(cSurface->GetUniqueId());
    write(pipeMain[1], &data, sizeof(data));

    // ---------------- Branch1 消费校验 ----------------
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
        // ExtraData 与 tunnel handle 跨进程往返一致
        EXPECT_EQ(GetData(buffer), true);
        // 特征数据逐字一致：父子进程零拷贝共享同一块 DMA 内存
        EXPECT_EQ(CheckPatternData(buffer, PATTERN_SEED), true);
        EXPECT_EQ(damage.w, flushConfig.damage.w);
        EXPECT_EQ(damage.h, flushConfig.damage.h);
        sRet = cSurface->ReleaseBuffer(buffer, -1);
        EXPECT_EQ(sRet, OHOS::GSERROR_OK);

        // 负向：子进程已建立生产连接，本进程再建生产者面后 request 被 PID 门控拒绝
        auto pSurfaceSecond = Surface::CreateSurfaceAsProducer(producer);
        sptr<SurfaceBuffer> bufferSecond = nullptr;
        int32_t releaseFence = -1;
        sRet = pSurfaceSecond->RequestBuffer(bufferSecond, releaseFence, requestConfig);
        EXPECT_EQ(sRet, GSERROR_CONSUMER_IS_CONNECTED);
        EXPECT_EQ(bufferSecond, nullptr);
    }
    write(pipeMain[1], &data, sizeof(data));

    // ---------------- Branch2 消费校验：3 轮 lockstep ----------------
    for (uint32_t i = 0; i < cycleTimes; i++) {
        data = ReadChildReport();
        EXPECT_EQ(data, static_cast<int64_t>(OHOS::GSERROR_OK));
        sptr<SurfaceBuffer> buffer = nullptr;
        int32_t fence = -1;
        int64_t timestamp = 0;
        Rect damage = {};
        auto sRet = cSurface->AcquireBuffer(buffer, fence, timestamp, damage);
        EXPECT_EQ(sRet, OHOS::GSERROR_OK);
        EXPECT_NE(buffer, nullptr);
        EXPECT_EQ(CheckPatternData(buffer, PATTERN_SEED + i), true);
        sRet = cSurface->ReleaseBuffer(buffer, -1);
        EXPECT_EQ(sRet, OHOS::GSERROR_OK);
        write(pipeMain[1], &data, sizeof(data));
    }

    // ---------------- Branch3 消费校验：realloc 后的小尺寸帧 ----------------
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
        EXPECT_EQ(buffer->GetWidth(), smallConfig.width);
        EXPECT_EQ(buffer->GetHeight(), smallConfig.height);
        EXPECT_EQ(CheckPatternData(buffer, SMALL_PATTERN_SEED), true);
        sRet = cSurface->ReleaseBuffer(buffer, -1);
        EXPECT_EQ(sRet, OHOS::GSERROR_OK);
    }
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
