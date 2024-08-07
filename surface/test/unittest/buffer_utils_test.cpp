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
#include <gtest/gtest.h>

#include <thread>
#include <chrono>
#include <unistd.h>
#include <fstream>

#include <buffer_utils.h>
#include "surface_buffer_impl.h"
#include "sandbox_utils.h"


using namespace testing;
using namespace testing::ext;

namespace OHOS::Rosen {
class BufferUtilsTest : public testing::Test {
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
    };

    static inline sptr<SurfaceBuffer> buffer = nullptr;
    static inline std::string flagPath = "/data/bq_dump";
    static inline std::string name_ = "test";
};

namespace fs = std::filesystem;

void BufferUtilsTest::SetUpTestCase()
{
    buffer = nullptr;

    // open dump flag
    std::ofstream outfile(flagPath);
    outfile << "touch" << std::endl;
    outfile.close();
}

void BufferUtilsTest::TearDownTestCase()
{
    buffer = nullptr;

    // delete dump flag
    if (fs::exists(flagPath)) {
        fs::remove(flagPath);
    }
}

/*
* Function: WriteToFile
* Type: Function
* Rank: Important(2)
* EnvConditions: N/A
* CaseDescription: 1. call DumpToFileAsync
*                  2. check ret
 */
HWTEST_F(BufferUtilsTest, DumpToFileAsyncTest001, Function | MediumTest | Level2)
{
    const pid_t pid = GetRealPid();

    // Alloc buffer
    buffer = new SurfaceBufferImpl();
    buffer->Alloc(requestConfig);

    // Call DumpToFileAsync
    GSError ret = DumpToFileAsync(true, pid, name_, buffer);
    ASSERT_EQ(ret, OHOS::GSERROR_OK);

    // Expect Buffer Dump to be completed within 20ms.
    std::chrono::milliseconds dura(20);
    std::this_thread::sleep_for(dura);

    const std::string directory = "/data";
    const std::string prefix = "bq_" + std::to_string(pid) + "_" + name_;
    size_t dumpFileSize = 0;
    // Traverse the directory and find the dump file.
    for (const auto& entry : fs::recursive_directory_iterator(directory)) {
        if (entry.is_regular_file() && entry.path().filename().string().find(prefix) == 0) {
            // Open the file to create a stream
            std::ifstream dumpFile(entry.path(), std::ios::binary);
            std::vector<uint8_t> file_data((std::istreambuf_iterator<char>(dumpFile)),
                std::istreambuf_iterator<char>());
            // Get fileSize from the file stream
            dumpFileSize = file_data.size();
            dumpFile.close();
            fs::remove(entry.path());
            break;
        }
    }

    ASSERT_EQ(dumpFileSize, buffer->GetSize());
}
}