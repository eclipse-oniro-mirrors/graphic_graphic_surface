/*
 * Copyright (c) 2022-2023 Huawei Device Co., Ltd.
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
#include <fcntl.h>
#include <hilog/log.h>
#include <unistd.h>
#include "buffer_handle.h"

#include "buffer_handle_utils.h"
#include "buffer_handle_parcel.h"

using namespace testing::ext;

namespace OHOS {
class BufferHandleTest : public testing::Test {
public:
    static constexpr HiviewDFX::HiLogLabel LOG_LABEL = { LOG_CORE, 0xD001400, "graphicutils" };

    static void SetUpTestCase()
    {}

    static void TearDownTestCase()
    {}
};

/*
* Function: BufferHandleTest
* Type: Function
* Rank: Important(2)
* EnvConditions: N/A
* CaseDescription: AllocateBufferHandle
*/
HWTEST_F(BufferHandleTest, AllocateBufferHandle, Function | SmallTest | Level2)
{
    uint32_t fds = 1025, ints = 1025;
    uint32_t buffer_handle_reserve_max_size = 1024;
    ASSERT_EQ(nullptr, AllocateBufferHandle(fds, ints));
    ASSERT_EQ(nullptr, AllocateBufferHandle(fds, buffer_handle_reserve_max_size));
    ASSERT_EQ(nullptr, AllocateBufferHandle(buffer_handle_reserve_max_size, ints));
    BufferHandle *handle = AllocateBufferHandle(buffer_handle_reserve_max_size, buffer_handle_reserve_max_size);
    ASSERT_NE(nullptr, handle);
    ASSERT_EQ(0, FreeBufferHandle(handle));
}

/*
* Function: BufferHandleTest
* Type: Function
* Rank: Important(2)
* EnvConditions: N/A
* CaseDescription: AllocateBufferHandle
*/
HWTEST_F(BufferHandleTest, AllocateBufferHandle001, Function | SmallTest | Level2)
{
    uint32_t fds = 1025, ints = 1025;
    uint32_t testsize = 100;
    auto test1 = AllocateBufferHandle(fds, ints);
    ASSERT_EQ(nullptr, test1);

    auto test2 = AllocateBufferHandle(fds, testsize);
    ASSERT_EQ(nullptr, test2);

    auto test3 = AllocateBufferHandle(testsize, ints);
    ASSERT_EQ(nullptr, test3);

    auto test4 = AllocateBufferHandle(testsize, testsize);
    ASSERT_NE(nullptr, test4);
    ASSERT_EQ(0, FreeBufferHandle(test4));
}

/*
* Function: BufferHandleTest
* Type: Function
* Rank: Important(2)
* EnvConditions: N/A
* CaseDescription: FreeBufferHandle
*/
HWTEST_F(BufferHandleTest, FreeBufferHandle, Function | SmallTest | Level2)
{
    uint32_t buffer_handle_reserve_max_size = 1024;
    ASSERT_EQ(0, FreeBufferHandle(nullptr));
    BufferHandle *handle = AllocateBufferHandle(buffer_handle_reserve_max_size, buffer_handle_reserve_max_size);
    ASSERT_NE(nullptr, handle);
    ASSERT_EQ(0, FreeBufferHandle(handle));
}

/*
* Function: BufferHandleTest
* Type: Function
* Rank: Important(2)
* EnvConditions: N/A
* CaseDescription: FreeBufferHandle
*/
HWTEST_F(BufferHandleTest, FreeBufferHandle001, Function | SmallTest | Level2)
{
    uint32_t testsize = 100;
    auto test1 = FreeBufferHandle(nullptr);
    ASSERT_EQ(0, test1);

    BufferHandle *handle = AllocateBufferHandle(testsize, testsize);
    ASSERT_NE(nullptr, handle);
    handle->fd = 0;
    handle->reserve[0] = 0;
    auto test2 = FreeBufferHandle(handle);
    ASSERT_EQ(0, test2);
}

/*
* Function: BufferHandleTest
* Type: Function
* Rank: Important(2)
* EnvConditions: N/A
* CaseDescription: OHOS::WriteBufferHandle
*/
HWTEST_F(BufferHandleTest, WriteBufferHandle, Function | SmallTest | Level2)
{
    MessageParcel parcel;
    uint32_t buffer_handle_reserve_max_size = 1024;
    BufferHandle *handle = AllocateBufferHandle(buffer_handle_reserve_max_size, buffer_handle_reserve_max_size);
    ASSERT_NE(nullptr, handle);

    ASSERT_EQ(false, WriteBufferHandle(parcel, *handle));

    ASSERT_EQ(0, FreeBufferHandle(handle));
}

/*
* Function: BufferHandleTest
* Type: Function
* Rank: Important(2)
* EnvConditions: N/A
* CaseDescription: OHOS::ReadBufferHandle
*/
HWTEST_F(BufferHandleTest, ReadBufferHandle, Function | SmallTest | Level2)
{
    MessageParcel parcel;

    EXPECT_EQ(nullptr, ReadBufferHandle(parcel));
}

// Serialize a handle with the given geometry and fd = -1 (validFd = false), so that
// ReadBufferHandle exercises the geometry validation without needing a real fd.
static void WriteGeometryParcel(MessageParcel &parcel, int32_t width, int32_t stride, int32_t height, int32_t size)
{
    BufferHandle *handle = AllocateBufferHandle(0, 0);
    handle->width = width;
    handle->stride = stride;
    handle->height = height;
    handle->size = size;
    handle->format = 0;
    handle->usage = 0;
    handle->phyAddr = 0;
    handle->fd = -1;
    WriteBufferHandle(parcel, *handle);
    FreeBufferHandle(handle);
}

/*
* Function: BufferHandleTest
* Type: Function
* Rank: Important(2)
* EnvConditions: N/A
* CaseDescription: ReadBufferHandle rejects width <= 0
*/
HWTEST_F(BufferHandleTest, ReadBufferHandleInvalidWidth, Function | SmallTest | Level2)
{
    MessageParcel parcel;
    WriteGeometryParcel(parcel, 0, 4, 4, 100);
    EXPECT_EQ(nullptr, ReadBufferHandle(parcel));
}

/*
* Function: BufferHandleTest
* Type: Function
* Rank: Important(2)
* EnvConditions: N/A
* CaseDescription: ReadBufferHandle rejects height <= 0
*/
HWTEST_F(BufferHandleTest, ReadBufferHandleInvalidHeight, Function | SmallTest | Level2)
{
    MessageParcel parcel;
    WriteGeometryParcel(parcel, 4, 4, 0, 100);
    EXPECT_EQ(nullptr, ReadBufferHandle(parcel));
}

/*
* Function: BufferHandleTest
* Type: Function
* Rank: Important(2)
* EnvConditions: N/A
* CaseDescription: ReadBufferHandle rejects negative stride
*/
HWTEST_F(BufferHandleTest, ReadBufferHandleNegativeStride, Function | SmallTest | Level2)
{
    MessageParcel parcel;
    WriteGeometryParcel(parcel, 4, -1, 4, 100);
    EXPECT_EQ(nullptr, ReadBufferHandle(parcel));
}

/*
* Function: BufferHandleTest
* Type: Function
* Rank: Important(2)
* EnvConditions: N/A
* CaseDescription: ReadBufferHandle rejects size <= 0
*/
HWTEST_F(BufferHandleTest, ReadBufferHandleInvalidSize, Function | SmallTest | Level2)
{
    MessageParcel parcel;
    WriteGeometryParcel(parcel, 4, 4, 4, 0);
    EXPECT_EQ(nullptr, ReadBufferHandle(parcel));
}

/*
* Function: BufferHandleTest
* Type: Function
* Rank: Important(2)
* EnvConditions: N/A
* CaseDescription: ReadBufferHandle rejects height * stride larger than size
*/
HWTEST_F(BufferHandleTest, ReadBufferHandleGeometryExceedsSize, Function | SmallTest | Level2)
{
    MessageParcel parcel;
    WriteGeometryParcel(parcel, 4, 100, 100, 10);
    EXPECT_EQ(nullptr, ReadBufferHandle(parcel));
}

/*
* Function: BufferHandleTest
* Type: Function
* Rank: Important(2)
* EnvConditions: N/A
* CaseDescription: ReadBufferHandle rejects stride == 0 (narrower than a positive width)
*/
HWTEST_F(BufferHandleTest, ReadBufferHandleZeroStrideInvalid, Function | SmallTest | Level2)
{
    MessageParcel parcel;
    WriteGeometryParcel(parcel, 4, 0, 4, 100);
    EXPECT_EQ(nullptr, ReadBufferHandle(parcel));
}

/*
* Function: BufferHandleTest
* Type: Function
* Rank: Important(2)
* EnvConditions: N/A
* CaseDescription: ReadBufferHandle rejects non-negative stride smaller than width
*/
HWTEST_F(BufferHandleTest, ReadBufferHandleStrideLessThanWidth, Function | SmallTest | Level2)
{
    MessageParcel parcel;
    WriteGeometryParcel(parcel, 4, 2, 4, 100);
    EXPECT_EQ(nullptr, ReadBufferHandle(parcel));
}

/*
* Function: BufferHandleTest
* Type: Function
* Rank: Important(2)
* EnvConditions: N/A
* CaseDescription: ReadBufferHandle accepts stride == width as a valid boundary
*/
HWTEST_F(BufferHandleTest, ReadBufferHandleValidGeometry, Function | SmallTest | Level2)
{
    MessageParcel parcel;
    WriteGeometryParcel(parcel, 4, 4, 4, 100);
    BufferHandle *handle = ReadBufferHandle(parcel);
    ASSERT_NE(nullptr, handle);
    ASSERT_EQ(4, handle->width);
    ASSERT_EQ(4, handle->stride);
    ASSERT_EQ(4, handle->height);
    ASSERT_EQ(100, handle->size);
    ASSERT_EQ(0, FreeBufferHandle(handle));
}

/*
* Function: OHOS::ReadBufferHandle
* Type: Function
* Rank: Important(2)
* EnvConditions: N/A
* CaseDescription: OHOS::ReadBufferHandle accepts stride greater than width
*/
HWTEST_F(BufferHandleTest, ReadBufferHandleStrideGreaterThanWidthValid, Function | SmallTest | Level2)
{
    MessageParcel parcel;
    WriteGeometryParcel(parcel, 4, 8, 4, 100);
    BufferHandle *handle = ReadBufferHandle(parcel);
    ASSERT_NE(nullptr, handle);
    ASSERT_EQ(8, handle->stride);
    ASSERT_EQ(0, FreeBufferHandle(handle));
}

/*
* Function: FreeBufferHandle
* Type: Function
* Rank: Important(2)
* EnvConditions: N/A
* CaseDescription: FreeBufferHandle closes an unowned fd (tag 0) exactly like close():
*                  1. open a real fd with tag 0
*                  2. put it into a BufferHandle and free it
*                  3. the fd must be closed (EBADF) with no fdsan owner mismatch
*/
HWTEST_F(BufferHandleTest, FreeBufferHandleClosesUnownedFd, Function | SmallTest | Level2)
{
    int32_t fd = open("/dev/null", O_RDWR);
    ASSERT_GE(fd, 0);
    BufferHandle *handle = AllocateBufferHandle(0, 0);
    ASSERT_NE(nullptr, handle);
    handle->fd = fd;
    ASSERT_EQ(0, FreeBufferHandle(handle));
    EXPECT_EQ(-1, fcntl(fd, F_GETFD));
    EXPECT_EQ(EBADF, errno);
}

/*
* Function: FreeBufferHandle
* Type: Function
* Rank: Important(2)
* EnvConditions: N/A
* CaseDescription: FreeBufferHandle validates and closes a fd tagged BUFFER_HANDLE_FD_OWNER_TAG:
*                  1. open a real fd and tag it BUFFER_HANDLE_FD_OWNER_TAG
*                  2. free the handle; owner check must pass and fd must be closed
*/
HWTEST_F(BufferHandleTest, FreeBufferHandleClosesOwnerTaggedFd, Function | SmallTest | Level2)
{
    int32_t fd = open("/dev/null", O_RDWR);
    ASSERT_GE(fd, 0);
    fdsan_exchange_owner_tag(fd, 0, BUFFER_HANDLE_FD_OWNER_TAG);
    BufferHandle *handle = AllocateBufferHandle(0, 0);
    ASSERT_NE(nullptr, handle);
    handle->fd = fd;
    ASSERT_EQ(0, FreeBufferHandle(handle));
    EXPECT_EQ(-1, fcntl(fd, F_GETFD));
    EXPECT_EQ(EBADF, errno);
}

/*
* Function: FreeBufferHandle
* Type: Function
* Rank: Important(2)
* EnvConditions: N/A
* CaseDescription: FreeBufferHandle closes reserve fds whatever their owner tag:
*                  1. allocate a handle with two reserve fds, one unowned and one tagged
*                     BUFFER_HANDLE_FD_OWNER_TAG
*                  2. free the handle; both reserve fds must be closed with no fdsan mismatch
*/
HWTEST_F(BufferHandleTest, FreeBufferHandleClosesReserveFds, Function | SmallTest | Level2)
{
    int32_t fd = open("/dev/null", O_RDWR);
    ASSERT_GE(fd, 0);
    int32_t unownedReserveFd = open("/dev/null", O_RDWR);
    ASSERT_GE(unownedReserveFd, 0);
    int32_t ownedReserveFd = open("/dev/null", O_RDWR);
    ASSERT_GE(ownedReserveFd, 0);
    fdsan_exchange_owner_tag(ownedReserveFd, 0, BUFFER_HANDLE_FD_OWNER_TAG);

    BufferHandle *handle = AllocateBufferHandle(2, 0);
    ASSERT_NE(nullptr, handle);
    ASSERT_EQ(2u, handle->reserveFds);
    handle->fd = fd;
    handle->reserve[0] = unownedReserveFd;
    handle->reserve[1] = ownedReserveFd;
    ASSERT_EQ(0, FreeBufferHandle(handle));

    EXPECT_EQ(-1, fcntl(fd, F_GETFD));
    EXPECT_EQ(EBADF, errno);
    EXPECT_EQ(-1, fcntl(unownedReserveFd, F_GETFD));
    EXPECT_EQ(EBADF, errno);
    EXPECT_EQ(-1, fcntl(ownedReserveFd, F_GETFD));
    EXPECT_EQ(EBADF, errno);
}

/*
* Function: OHOS::ReadBufferHandle
* Type: Function
* Rank: Important(2)
* EnvConditions: N/A
* CaseDescription: ReadBufferHandle is source neutral, it must not tag the fd:
*                  1. write a handle carrying a real fd (tag 0)
*                  2. read it back; WriteFileDescriptor/ReadFileDescriptor each dup once,
*                     so the fd read out is fresh and must stay unowned
*                  3. both handles free their own fds independently
*/
HWTEST_F(BufferHandleTest, ReadBufferHandleLeavesFdUnowned, Function | SmallTest | Level2)
{
    int32_t fd = open("/dev/null", O_RDWR);
    ASSERT_GE(fd, 0);
    BufferHandle *src = AllocateBufferHandle(0, 0);
    ASSERT_NE(nullptr, src);
    src->width = 4;
    src->stride = 4;
    src->height = 4;
    src->size = 100;
    src->fd = fd;

    MessageParcel parcel;
    ASSERT_EQ(true, WriteBufferHandle(parcel, *src));
    BufferHandle *dst = ReadBufferHandle(parcel);
    ASSERT_NE(nullptr, dst);
    EXPECT_NE(dst->fd, fd); // parcel fd transfer dups, never reuses the source fd
    EXPECT_EQ(0u, fdsan_get_owner_tag(dst->fd)); // ReadBufferHandle must not tag
    ASSERT_EQ(0, FreeBufferHandle(dst));
    ASSERT_EQ(0, FreeBufferHandle(src));
    EXPECT_EQ(-1, fcntl(fd, F_GETFD));
    EXPECT_EQ(EBADF, errno);
}
}