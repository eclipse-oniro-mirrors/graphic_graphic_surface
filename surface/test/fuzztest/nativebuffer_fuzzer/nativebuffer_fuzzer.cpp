/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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

#include "nativebuffer_fuzzer.h"

#include <securec.h>
#include <string>

#include "data_generate.h"
#include "native_buffer.h"
#include "native_window.h"

using namespace g_fuzzCommon;

namespace OHOS {
    bool DoSomethingInterestingWithMyAPI(const uint8_t* data, size_t size)
    {
        if (data == nullptr) {
            return false;
        }

        // initialize
        g_data = data;
        g_size = size;
        g_pos = 0;

        // get data
        OH_NativeBuffer_Config config = GetData<OH_NativeBuffer_Config>();
        config.width = 1920; // 1920 pixels
        config.height = 1080; // 1080 pixels
        OH_NativeBuffer_Config checkConfig = GetData<OH_NativeBuffer_Config>();
        void *virAddr = static_cast<void*>(GetStringFromData(STR_LEN).data());

        // test
        OH_NativeBuffer* buffer = OH_NativeBuffer_Alloc(&config);
        CreateNativeWindowBufferFromNativeBuffer(buffer);
        OH_NativeBuffer_GetSeqNum(buffer);
        OH_NativeBuffer_GetConfig(buffer, &checkConfig);
        OH_NativeBuffer_Reference(buffer);
        OH_NativeBuffer_Unreference(buffer);
        OH_NativeBuffer_Map(buffer, &virAddr);
        OH_NativeBuffer_Unmap(buffer);
        OH_NativeBuffer_Unreference(buffer);
        OH_NativeBuffer_Unreference(buffer);

        return true;
    }
} // namespace OHOS

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    /* Run your code on data */
    OHOS::DoSomethingInterestingWithMyAPI(data, size);
    return 0;
}

