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

#ifndef UTILS_INCLUDE_FRAME_SCHED_H
#define UTILS_INCLUDE_FRAME_SCHED_H

#include <cstdint>
#include <string>

namespace OHOS {
namespace Rosen {

using InitFunc = void(*)();
using MonitorGpuStartFunc = void(*)();
using MonitorGpuEndFunc = void(*)();
using SetFrameParamFunc = void(*)(int, int, int, int);

class FrameSched {
public:
    static FrameSched& GetInstance();
    void SetFrameParam(int requestId, int load, int schedFrameNum, int value);
    void MonitorGpuStart();
    void MonitorGpuEnd();

private:
    FrameSched();
    ~FrameSched();

    void Init();

    bool LoadLibrary();
    void CloseLibrary();
    void* LoadSymbol(const char* symName);

    void* schedHandle_ = nullptr;
    bool schedSoLoaded_ = false;
    InitFunc initFunc_ = nullptr;
    MonitorGpuStartFunc monitorGpuStartFunc_ = nullptr;
    MonitorGpuEndFunc monitorGpuEndFunc_ = nullptr;
    SetFrameParamFunc setFrameParamFunc_ = nullptr;
};

} // namespace Rosen
} // namespace OHOS

#endif // UTILS_INCLUDE_FRAME_SCHED_H