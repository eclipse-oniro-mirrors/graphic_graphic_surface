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

#ifndef UTILS_INCLUDE_SYNC_FENCE_H
#define UTILS_INCLUDE_SYNC_FENCE_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <mutex>
#include <vector>

#include <refbase.h>
#include <message_parcel.h>
#include <unique_fd.h>

namespace OHOS {

using ns_sec_t = int64_t;

// same to linux/sync_file define
enum FenceStatus {
    ERROR = -1,
    ACTIVE = 0,
    SIGNALED = 1
};

struct SyncPointInfo {
    uint64_t timestampNs;
    FenceStatus status;
};

class SyncFence : public RefBase {
public:
    explicit SyncFence(int32_t fenceFd);
    /* When the SyncFence is destroyed, the fd will be closed in UniqueFd */
    virtual ~SyncFence();

    SyncFence(const SyncFence& rhs) = delete;
    SyncFence& operator=(const SyncFence& rhs) = delete;
    SyncFence(SyncFence&& rhs) = delete;
    SyncFence& operator=(SyncFence&& rhs) = delete;

    static const sptr<SyncFence> INVALID_FENCE;
    static const ns_sec_t INVALID_TIMESTAMP;
    static const ns_sec_t FENCE_PENDING_TIMESTAMP;
    int32_t Wait(uint32_t timeout);
    static sptr<SyncFence> MergeFence(const std::string &name,
            const sptr<SyncFence>& fence1, const sptr<SyncFence>& fence2);
    ns_sec_t SyncFileReadTimestamp();
    int32_t Dup() const;
    bool IsValid() const;

    /* this is dangerous, when you use it, do not operator the fd */
    int32_t Get() const;

    bool WriteToMessageParcel(MessageParcel &parcel);
    static sptr<SyncFence> ReadFromMessageParcel(MessageParcel &parcel,
        std::function<int(MessageParcel &parcel,
            std::function<int(Parcel &)>readFdDefaultFunc)>readSafeFdFunc = nullptr);
    FenceStatus GetStatus();
    static sptr<SyncFence> InvalidFence();
private:
    std::vector<SyncPointInfo> GetFenceInfo();

    UniqueFd fenceFd_;
    static int32_t SyncMerge(const char *name, int32_t fd1, int32_t fd2, int32_t &newFenceFd);
};

}

#endif // UTILS_INCLUDE_SYNC_FENCE_H