/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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

#include <dlfcn.h>
#include <cstring>
#include "buffer_extra_data.h"
#include "ibuffer_producer.h"
#include "surface.h"
#include "surface_buffer.h"
#include "surface_type.h"
#include "sync_fence.h"

namespace OHOS {
namespace Rosen {

// --- Mock function implementations ---
uintptr_t MockProducerSurfaceDelegatorCreate()
{
    return 0x16;
}

void MockProducerSurfaceDelegatorDestroy(uintptr_t delegator) {}

void MockProducerSurfaceDelegatorSetSurface(uintptr_t delegator, sptr<Surface> surface) {}

bool MockProducerSurfaceDelegatorSetClient(uintptr_t delegator, sptr<IRemoteObject> client)
{
    return true;
}

GSError MockProducerSurfaceDelegatorReleaseBuffer(uintptr_t delegator,
    const sptr<SurfaceBuffer>& buffer, const sptr<SyncFence>& fence)
{
    return GSERROR_OK;
}

uintptr_t MockConsumerSurfaceDelegatorCreate()
{
    return 0x11;
}

bool MockConsumerSurfaceDelegatorSetClient(uintptr_t delegator, sptr<IRemoteObject> client)
{
    return true;
}

void MockConsumerSurfaceDelegatorSetSurface(uintptr_t delegator, sptr<Surface> surface) {}

GSError MockConsumerSurfaceDelegatorDequeueBuffer(uintptr_t delegator,
    const BufferRequestConfig& config, sptr<BufferExtraData>& bedata,
    struct IBufferProducer::RequestBufferReturnValue& retval)
{
    return GSERROR_OK;
}

GSError MockConsumerSurfaceDelegatorQueueBuffer(uintptr_t delegator,
    sptr<SurfaceBuffer>& buffer, int32_t fenceFd)
{
    return GSERROR_OK;
}

void MockConsumerSurfaceDelegatorDestroy(uintptr_t delegator) {}

// --- Symbol handler table ---
static void* HandleProducerSurfaceDelegatorCreate()
{
    return reinterpret_cast<void*>(MockProducerSurfaceDelegatorCreate);
}

static void* HandleProducerSurfaceDelegatorDestroy()
{
    return reinterpret_cast<void*>(MockProducerSurfaceDelegatorDestroy);
}

static void* HandleProducerSurfaceDelegatorSetSurface()
{
    return reinterpret_cast<void*>(MockProducerSurfaceDelegatorSetSurface);
}

static void* HandleProducerSurfaceDelegatorSetClient()
{
    return reinterpret_cast<void*>(MockProducerSurfaceDelegatorSetClient);
}

static void* HandleProducerSurfaceDelegatorReleaseBuffer()
{
    return reinterpret_cast<void*>(MockProducerSurfaceDelegatorReleaseBuffer);
}

static void* HandleConsumerSurfaceDelegatorCreate()
{
    return reinterpret_cast<void*>(MockConsumerSurfaceDelegatorCreate);
}

static void* HandleConsumerSurfaceDelegatorSetClient()
{
    return reinterpret_cast<void*>(MockConsumerSurfaceDelegatorSetClient);
}

static void* HandleConsumerSurfaceDelegatorSetSurface()
{
    return reinterpret_cast<void*>(MockConsumerSurfaceDelegatorSetSurface);
}

static void* HandleConsumerSurfaceDelegatorDequeueBuffer()
{
    return reinterpret_cast<void*>(MockConsumerSurfaceDelegatorDequeueBuffer);
}

static void* HandleConsumerSurfaceDelegatorQueueBuffer()
{
    return reinterpret_cast<void*>(MockConsumerSurfaceDelegatorQueueBuffer);
}

static void* HandleConsumerSurfaceDelegatorDestroy()
{
    return reinterpret_cast<void*>(MockConsumerSurfaceDelegatorDestroy);
}

struct SymbolHandler {
    const char* name;
    void* (*handler)();
};

static SymbolHandler g_symbolHandlerTable[] = {
    { "ProducerSurfaceDelegatorCreate",       HandleProducerSurfaceDelegatorCreate },
    { "ProducerSurfaceDelegatorDestroy",      HandleProducerSurfaceDelegatorDestroy },
    { "ProducerSurfaceDelegatorSetSurface",   HandleProducerSurfaceDelegatorSetSurface },
    { "ProducerSurfaceDelegatorSetClient",    HandleProducerSurfaceDelegatorSetClient },
    { "ProducerSurfaceDelegatorReleaseBuffer", HandleProducerSurfaceDelegatorReleaseBuffer },
    { "ConsumerSurfaceDelegatorCreate",       HandleConsumerSurfaceDelegatorCreate },
    { "ConsumerSurfaceDelegatorSetClient",    HandleConsumerSurfaceDelegatorSetClient },
    { "ConsumerSurfaceDelegatorSetSurface",   HandleConsumerSurfaceDelegatorSetSurface },
    { "ConsumerSurfaceDelegatorDequeueBuffer", HandleConsumerSurfaceDelegatorDequeueBuffer },
    { "ConsumerSurfaceDelegatorQueueBuffer",  HandleConsumerSurfaceDelegatorQueueBuffer },
    { "ConsumerSurfaceDelegatorDestroy",      HandleConsumerSurfaceDelegatorDestroy },
};

// Mock handle sentinel for libdelegator.z.so
static constexpr uintptr_t MOCK_HANDLE = 0x1234;

}  // namespace Rosen
}  // namespace OHOS

// --- extern "C" symbol overrides ---
// Only intercept dlopen("libdelegator.z.so") to avoid loading the real library
// (which triggers heap-use-after-free in ASAN). For all other libraries, return
// nullptr — the calling code has existing null-check tolerances.
// This approach avoids RTLD_NEXT which conflicts with ASAN's dlopen interceptor.
// Function names must match libc symbols exactly for dynamic-linker
// interposition; renaming to CamelCase would break the mock. Suppress the
// naming-style rule on these three definitions only.
extern "C" void* dlopen(const char* filename, int flags)  // NOLINT(G.NAM.03-CPP)
{
    if (filename != nullptr && strcmp(filename, "libdelegator.z.so") == 0) {
        return reinterpret_cast<void*>(OHOS::Rosen::MOCK_HANDLE);
    }
    return nullptr;
}

extern "C" void* dlsym(void* handle, const char* symbol)  // NOLINT(G.NAM.03-CPP)
{
    if (handle == reinterpret_cast<void*>(OHOS::Rosen::MOCK_HANDLE)) {
        for (auto& handler : OHOS::Rosen::g_symbolHandlerTable) {
            if (strcmp(symbol, handler.name) == 0) {
                return handler.handler();
            }
        }
        return nullptr;
    }
    return nullptr;
}

extern "C" int dlclose(void* handle)  // NOLINT(G.NAM.03-CPP)
{
    if (handle == reinterpret_cast<void*>(OHOS::Rosen::MOCK_HANDLE)) {
        return 0;
    }
    return -1;
}