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
// 各分支特征数据种子：子进程逐字写入 seed+i，父进程 InvalidateCache 后逐字校验，
// 验证跨进程双方零拷贝共享同一块 DMA 内存
constexpr uint32_t ATTACH_PATTERN_SEED = 0x4A4A0000; // Branch1：attach 归队后提交的一帧
constexpr uint32_t BATCH_PATTERN_SEED = 0x52520000;  // Branch2：批量 attach 的 3 帧（seed+i）

// 子进程校验失败上报的合成码 = 基址 + 步骤号。GSError 本身无法区分是哪一步失败，
// 父进程 EXPECT_EQ(data, GSERROR_OK) 失败时可用上报码减去基址定位子进程内的校验步骤
constexpr int64_t CHILD_CHECK_FAILED_BASE = 100000;
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

// 子进程侧：向跨 IPC 拿到的 buffer 写特征数据。virAddr 是本进程按需 mmap 的独立映射，
// 写满后 FlushCache 刷出 CPU cache，父进程才能透过自己的映射读到新数据
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

// 父进程侧：InvalidateCache 后逐字校验特征数据（与子进程零拷贝共享同一块 DMA 内存）
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

// 父进程侧：读子进程上报。先置 -1 再读，子进程失败提前退出（EOF 读不到数据）时
// 不会被残留值误判为 GSERROR_OK
int64_t SurfaceIPCAttachDetachTest::ReadChildReport()
{
    int64_t data = -1;
    (void)read(pipeChild[0], &data, sizeof(data));
    return data;
}

// 子进程侧：轮询 samgr 拉取父进程注册的 producer，包装成本地生产者 surface
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

// 进程B主体：经 samgr 拿到 producer 后，按 2 个分支驱动 attach/detach 场景。
// 每个分支完成后写一条 report，等父进程完成消费校验并回 ack 再进入下一分支，
// 管道读写次数与父进程严格互补（子写 3 / 子读 4，含 start）。
pid_t SurfaceIPCAttachDetachTest::ChildProcessMain()
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

    // -------- Branch1：AttachBufferToQueue/DetachBufferFromQueue 跨进程状态机 --------
    sptr<SurfaceBuffer> buffer = nullptr;
    int32_t releaseFence = -1;
    auto sRet = pSurface->RequestBuffer(buffer, releaseFence, requestConfig);
    if (sRet != OHOS::GSERROR_OK || buffer == nullptr) {
        data = (sRet != OHOS::GSERROR_OK) ? sRet : CHILD_CHECK_FAILED_BASE + 11;
        write(pipeChild[1], &data, sizeof(data));
        exit(0);
    }
    uint32_t sequence = buffer->GetSeqNum();
    // 负向：对已在服务端队列缓存中的 buffer 重复 attach，必须被拒绝
    sRet = pSurface->AttachBufferToQueue(buffer);
    if (sRet != SURFACE_ERROR_BUFFER_IS_INCACHE) {
        data = CHILD_CHECK_FAILED_BASE + 12;
        write(pipeChild[1], &data, sizeof(data));
        exit(0);
    }
    // REQUESTED 状态 detach 合法，buffer 离开服务端缓存
    sRet = pSurface->DetachBufferFromQueue(buffer);
    if (sRet != OHOS::GSERROR_OK) {
        data = sRet;
        write(pipeChild[1], &data, sizeof(data));
        exit(0);
    }
    // 负向：对已脱队的 buffer flush，必须被拒绝
    sRet = pSurface->FlushBuffer(buffer, -1, flushConfig);
    if (sRet != SURFACE_ERROR_BUFFER_NOT_INCACHE) {
        data = CHILD_CHECK_FAILED_BASE + 13;
        write(pipeChild[1], &data, sizeof(data));
        exit(0);
    }
    // attach 回队列：同序列号，状态回到 REQUESTED，可正常提交一帧
    sRet = pSurface->AttachBufferToQueue(buffer);
    if (sRet != OHOS::GSERROR_OK) {
        data = sRet;
        write(pipeChild[1], &data, sizeof(data));
        exit(0);
    }
    if (buffer->GetSeqNum() != sequence) {
        data = CHILD_CHECK_FAILED_BASE + 14;
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
    read(pipeMain[0], &data, sizeof(data)); // 等父进程完成 Branch1 消费校验

    // -------- Branch2：批量 attach 与服务端 buffer 复用 --------
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
        if (sRet != OHOS::GSERROR_OK || buffer == nullptr) {
            data = (sRet != OHOS::GSERROR_OK) ? sRet : CHILD_CHECK_FAILED_BASE + 21;
            write(pipeChild[1], &data, sizeof(data));
            exit(0);
        }
        buffers.push_back(buffer);
        sequences.push_back(buffer->GetSeqNum());
    }
    // 全部 detach：服务端队列缓存清空
    for (uint32_t i = 0; i < queueSize; i++) {
        sRet = pSurface->DetachBufferFromQueue(buffers[i]);
        if (sRet != OHOS::GSERROR_OK) {
            data = sRet;
            write(pipeChild[1], &data, sizeof(data));
            exit(0);
        }
    }
    // 逐一 attach 回队（序列号不变）并填充提交
    for (uint32_t i = 0; i < queueSize; i++) {
        sRet = pSurface->AttachBufferToQueue(buffers[i]);
        if (sRet != OHOS::GSERROR_OK) {
            data = sRet;
            write(pipeChild[1], &data, sizeof(data));
            exit(0);
        }
        if (buffers[i]->GetSeqNum() != sequences[i]) {
            data = CHILD_CHECK_FAILED_BASE + 22;
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
    read(pipeMain[0], &data, sizeof(data)); // 等父进程完成 3 帧消费

    // 复用验证：buffer 释放回 free 队列后再次 request，应复用原序列号的 buffer（服务端不重新分配）
    for (uint32_t i = 0; i < queueSize; i++) {
        buffer = nullptr;
        sRet = pSurface->RequestBuffer(buffer, releaseFence, requestConfig);
        if (sRet != OHOS::GSERROR_OK || buffer == nullptr) {
            data = (sRet != OHOS::GSERROR_OK) ? sRet : CHILD_CHECK_FAILED_BASE + 23;
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
            data = CHILD_CHECK_FAILED_BASE + 24;
            write(pipeChild[1], &data, sizeof(data));
            exit(0);
        }
        // CancelBuffer 归还，保持队列状态干净
        sRet = pSurface->CancelBuffer(buffer);
        if (sRet != OHOS::GSERROR_OK) {
            data = sRet;
            write(pipeChild[1], &data, sizeof(data));
            exit(0);
        }
    }
    data = OHOS::GSERROR_OK;
    write(pipeChild[1], &data, sizeof(data));
    read(pipeMain[0], &data, sizeof(data)); // 等父进程最终 ack

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
    cSurface = IConsumerSurface::Create("SurfaceIPCAttachDetachTest");
    cSurface->RegisterConsumerListener(this);
    auto producer = cSurface->GetProducer();
    auto sam = SystemAbilityManagerClient::GetInstance().GetSystemAbilityManager();
    sam->AddSystemAbility(ipcSystemAbilityID, producer->AsObject());

    // start 信号携带队列 uniqueId，供子进程校验拿到的是同一个队列
    int64_t data = static_cast<int64_t>(cSurface->GetUniqueId());
    write(pipeMain[1], &data, sizeof(data));

    // ---------------- Branch1 消费校验：attach 归队后提交的一帧 ----------------
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

    // ---------------- Branch2 消费校验：批量 attach 的 3 帧 ----------------
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
        // dirty 队列按提交顺序出队，逐帧校验各自种子
        EXPECT_EQ(CheckPatternData(buffer, BATCH_PATTERN_SEED + i), true);
        sRet = cSurface->ReleaseBuffer(buffer, -1);
        EXPECT_EQ(sRet, OHOS::GSERROR_OK);
    }
    // 负向：3 帧全部消费后队列已排空，再 acquire 应返回 GSERROR_NO_BUFFER
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

    // Branch2 复用轮：子进程只上报结果，无消费动作
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
