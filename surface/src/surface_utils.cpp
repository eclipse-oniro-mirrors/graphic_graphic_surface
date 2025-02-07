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

#include <surface_utils.h>
#include <cinttypes>
#include "securec.h"
#include "buffer_log.h"

namespace OHOS {
using namespace HiviewDFX;
static SurfaceUtils* instance = nullptr;
static std::once_flag createFlag_;
constexpr uint32_t MATRIX_ARRAY_SIZE = 16;

SurfaceUtils* SurfaceUtils::GetInstance()
{
    std::call_once(createFlag_, [&]() {
        instance = new SurfaceUtils();
    });

    return instance;
}

SurfaceUtils::~SurfaceUtils()
{
    instance = nullptr;
    surfaceCache_.clear();
    nativeWindowCache_.clear();
}

sptr<Surface> SurfaceUtils::GetSurface(uint64_t uniqueId)
{
    std::lock_guard<std::mutex> lockGuard(mutex_);
    auto iter = surfaceCache_.find(uniqueId);
    if (iter == surfaceCache_.end()) {
        BLOGE("Cannot find surface, uniqueId: %{public}" PRIu64 ".", uniqueId);
        return nullptr;
    }
    sptr<Surface> surface = iter->second.promote();
    if (surface == nullptr) {
        BLOGE("surface is nullptr, uniqueId: %{public}" PRIu64 ".", uniqueId);
        return nullptr;
    }
    return surface;
}

SurfaceError SurfaceUtils::Add(uint64_t uniqueId, const wptr<Surface> &surface)
{
    if (surface == nullptr) {
        return GSERROR_INVALID_ARGUMENTS;
    }
    std::lock_guard<std::mutex> lockGuard(mutex_);
    if (surfaceCache_.count(uniqueId) == 0) {
        surfaceCache_[uniqueId] = surface;
        return GSERROR_OK;
    }
    BLOGD("the surface already existed, uniqueId: %{public}" PRIu64, uniqueId);
    return GSERROR_OK;
}

SurfaceError SurfaceUtils::Remove(uint64_t uniqueId)
{
    std::lock_guard<std::mutex> lockGuard(mutex_);
    auto iter = surfaceCache_.find(uniqueId);
    if (iter == surfaceCache_.end()) {
        BLOGD("Cannot find surface, uniqueId: %{public}" PRIu64 ".", uniqueId);
        return GSERROR_INVALID_OPERATING;
    }
    surfaceCache_.erase(iter);
    return GSERROR_OK;
}

std::array<float, MATRIX_ARRAY_SIZE> SurfaceUtils::MatrixProduct(const std::array<float, MATRIX_ARRAY_SIZE>& lMat,
    const std::array<float, MATRIX_ARRAY_SIZE>& rMat)
{
    // Product matrix 4 * 4 = 16
    return std::array<float, MATRIX_ARRAY_SIZE> {
        lMat[0] * rMat[0] + lMat[4] * rMat[1] + lMat[8] * rMat[2] + lMat[12] * rMat[3],
        lMat[1] * rMat[0] + lMat[5] * rMat[1] + lMat[9] * rMat[2] + lMat[13] * rMat[3],
        lMat[2] * rMat[0] + lMat[6] * rMat[1] + lMat[10] * rMat[2] + lMat[14] * rMat[3],
        lMat[3] * rMat[0] + lMat[7] * rMat[1] + lMat[11] * rMat[2] + lMat[15] * rMat[3],

        lMat[0] * rMat[4] + lMat[4] * rMat[5] + lMat[8] * rMat[6] + lMat[12] * rMat[7],
        lMat[1] * rMat[4] + lMat[5] * rMat[5] + lMat[9] * rMat[6] + lMat[13] * rMat[7],
        lMat[2] * rMat[4] + lMat[6] * rMat[5] + lMat[10] * rMat[6] + lMat[14] * rMat[7],
        lMat[3] * rMat[4] + lMat[7] * rMat[5] + lMat[11] * rMat[6] + lMat[15] * rMat[7],

        lMat[0] * rMat[8] + lMat[4] * rMat[9] + lMat[8] * rMat[10] + lMat[12] * rMat[11],
        lMat[1] * rMat[8] + lMat[5] * rMat[9] + lMat[9] * rMat[10] + lMat[13] * rMat[11],
        lMat[2] * rMat[8] + lMat[6] * rMat[9] + lMat[10] * rMat[10] + lMat[14] * rMat[11],
        lMat[3] * rMat[8] + lMat[7] * rMat[9] + lMat[11] * rMat[10] + lMat[15] * rMat[11],

        lMat[0] * rMat[12] + lMat[4] * rMat[13] + lMat[8] * rMat[14] + lMat[12] * rMat[15],
        lMat[1] * rMat[12] + lMat[5] * rMat[13] + lMat[9] * rMat[14] + lMat[13] * rMat[15],
        lMat[2] * rMat[12] + lMat[6] * rMat[13] + lMat[10] * rMat[14] + lMat[14] * rMat[15],
        lMat[3] * rMat[12] + lMat[7] * rMat[13] + lMat[11] * rMat[14] + lMat[15] * rMat[15]
    };
}

void SurfaceUtils::ComputeTransformByMatrix(GraphicTransformType& transform,
    std::array<float, TRANSFORM_MATRIX_ELE_COUNT> *transformMatrix)
{
    const std::array<float, TRANSFORM_MATRIX_ELE_COUNT> rotate90 = {0, -1, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    const std::array<float, TRANSFORM_MATRIX_ELE_COUNT> rotate180 = {-1, 0, 0, 0, 0, -1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    const std::array<float, TRANSFORM_MATRIX_ELE_COUNT> rotate270 = {0, 1, 0, 0, -1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    const std::array<float, TRANSFORM_MATRIX_ELE_COUNT> flipH = {-1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    const std::array<float, TRANSFORM_MATRIX_ELE_COUNT> flipV = {1, 0, 0, 0, 0, -1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};

    switch (transform) {
        case GraphicTransformType::GRAPHIC_ROTATE_NONE:
            break;
        case GraphicTransformType::GRAPHIC_ROTATE_90:
            *transformMatrix = MatrixProduct(*transformMatrix, rotate90);
            break;
        case GraphicTransformType::GRAPHIC_ROTATE_180:
            *transformMatrix = MatrixProduct(*transformMatrix, rotate180);
            break;
        case GraphicTransformType::GRAPHIC_ROTATE_270:
            *transformMatrix = MatrixProduct(*transformMatrix, rotate270);
            break;
        case GraphicTransformType::GRAPHIC_FLIP_H:
            *transformMatrix = MatrixProduct(*transformMatrix, flipH);
            break;
        case GraphicTransformType::GRAPHIC_FLIP_V:
            *transformMatrix = MatrixProduct(*transformMatrix, flipV);
            break;
        case GraphicTransformType::GRAPHIC_FLIP_H_ROT90:
            *transformMatrix = MatrixProduct(flipH, rotate90);
            break;
        case GraphicTransformType::GRAPHIC_FLIP_V_ROT90:
            *transformMatrix = MatrixProduct(flipV, rotate90);
            break;
        case GraphicTransformType::GRAPHIC_FLIP_H_ROT180:
            *transformMatrix = MatrixProduct(flipH, rotate180);
            break;
        case GraphicTransformType::GRAPHIC_FLIP_V_ROT180:
            *transformMatrix = MatrixProduct(flipV, rotate180);
            break;
        case GraphicTransformType::GRAPHIC_FLIP_H_ROT270:
            *transformMatrix = MatrixProduct(flipH, rotate270);
            break;
        case GraphicTransformType::GRAPHIC_FLIP_V_ROT270:
            *transformMatrix = MatrixProduct(flipV, rotate270);
            break;
        default:
            break;
    }
}

void SurfaceUtils::ComputeTransformMatrix(float matrix[MATRIX_ARRAY_SIZE], uint32_t matrixSize,
    sptr<SurfaceBuffer>& buffer, GraphicTransformType& transform, const Rect& crop)
{
    if (buffer == nullptr) {
        return;
    }
    float tx = 0.f;
    float ty = 0.f;
    float sx = 1.f;
    float sy = 1.f;
    std::array<float, TRANSFORM_MATRIX_ELE_COUNT> transformMatrix = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    ComputeTransformByMatrix(transform, &transformMatrix);

    float bufferWidth = buffer->GetWidth();
    float bufferHeight = buffer->GetHeight();
    bool changeFlag = false;
    if (crop.w < bufferWidth && bufferWidth != 0) {
        tx = (float(crop.x) / bufferWidth);
        sx = (float(crop.w) / bufferWidth);
        changeFlag = true;
    }
    if (crop.h < bufferHeight && bufferHeight != 0) {
        ty = (float(bufferHeight - crop.y) / bufferHeight);
        sy = (float(crop.h) / bufferHeight);
        changeFlag = true;
    }
    if (changeFlag) {
        std::array<float, MATRIX_ARRAY_SIZE> cropMatrix = {sx, 0, 0, 0, 0, sy, 0, 0, 0, 0, 1, 0, tx, ty, 0, 1};
        transformMatrix = MatrixProduct(cropMatrix, transformMatrix);
    }

    auto ret = memcpy_s(matrix, matrixSize * sizeof(float),
                        transformMatrix.data(), sizeof(transformMatrix));
    if (ret != EOK) {
        BLOGE("memcpy_s failed, ret: %{public}d", ret);
    }
}

void SurfaceUtils::ComputeTransformByMatrixV2(GraphicTransformType& transform,
    std::array<float, TRANSFORM_MATRIX_ELE_COUNT> *transformMatrix)
{
    const std::array<float, TRANSFORM_MATRIX_ELE_COUNT> rotate90 = {0, 1, 0, 0, -1, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 1};
    const std::array<float, TRANSFORM_MATRIX_ELE_COUNT> rotate180 = {-1, 0, 0, 0, 0, -1, 0, 0, 0, 0, 1, 0, 1, 1, 0, 1};
    const std::array<float, TRANSFORM_MATRIX_ELE_COUNT> rotate270 = {0, -1, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 1};
    const std::array<float, TRANSFORM_MATRIX_ELE_COUNT> flipH = {-1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 1, 0, 0, 1};
    const std::array<float, TRANSFORM_MATRIX_ELE_COUNT> flipV = {1, 0, 0, 0, 0, -1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 1};

    switch (transform) {
        case GraphicTransformType::GRAPHIC_ROTATE_NONE:
            break;
        case GraphicTransformType::GRAPHIC_ROTATE_90:
            *transformMatrix = rotate90;
            break;
        case GraphicTransformType::GRAPHIC_ROTATE_180:
            *transformMatrix = rotate180;
            break;
        case GraphicTransformType::GRAPHIC_ROTATE_270:
            *transformMatrix = rotate270;
            break;
        case GraphicTransformType::GRAPHIC_FLIP_H:
            *transformMatrix = flipH;
            break;
        case GraphicTransformType::GRAPHIC_FLIP_V:
            *transformMatrix = flipV;
            break;
        case GraphicTransformType::GRAPHIC_FLIP_H_ROT90:
            *transformMatrix = MatrixProduct(flipV, rotate90);
            break;
        case GraphicTransformType::GRAPHIC_FLIP_V_ROT90:
            *transformMatrix = MatrixProduct(flipH, rotate90);
            break;
        case GraphicTransformType::GRAPHIC_FLIP_H_ROT180:
            *transformMatrix = flipV;
            break;
        case GraphicTransformType::GRAPHIC_FLIP_V_ROT180:
            *transformMatrix = flipH;
            break;
        case GraphicTransformType::GRAPHIC_FLIP_H_ROT270:
            *transformMatrix = MatrixProduct(flipH, rotate90);
            break;
        case GraphicTransformType::GRAPHIC_FLIP_V_ROT270:
            *transformMatrix = MatrixProduct(flipV, rotate90);
            break;
        default:
            break;
    }
}

/*
 * Computes a transformation matrix for buffer rendering with crop and coordinate system conversion.
 *
 * The transformation process involves texture coordinate translation, coordinate system
 * conversion, and scaling operations to achieve proper buffer rendering.
 *
 * Texture Coordinate Translation involves converting device coordinates to normalized
 * texture coordinates [0,1], where:
 * - tx = crop.x / bufferWidth represents X-axis translation ratio
 * - ty = (bufferHeight - crop.y - crop.h) / bufferHeight handles Y-axis translation with flip
 *
 * Coordinate System Conversion handles the following:
 * - Device coordinates with origin at top-left and Y-axis down
 * - OpenGL coordinates with origin at bottom-left and Y-axis up
 * The Y-axis calculation requires:
 * - Moving origin from top to bottom using bufferHeight - crop.y
 * - Accounting for crop region height by subtracting crop.h
 * - Normalizing to [0,1] range by dividing with bufferHeight
 *
 * Scale calculation uses the following factors:
 * - sx = crop.w / bufferWidth for X-axis scaling
 * - sy = crop.h / bufferHeight for Y-axis scaling
 *
 * The final transformation matrix has the structure:
 * | sx  0   0   0 |  (X-axis scaling)
 * | 0   sy  0   0 |  (Y-axis scaling)
 * | 0   0   1   0 |  (Z-axis unchanged)
 * | tx  ty  0   1 |  (Translation)
 *
 * @param matrix [out] Output array where the transformation matrix will be stored
 * @param matrixSize Size of the provided matrix array
 * @param buffer Source surface buffer containing the image data
 * @param transform Rotation transformation type to be applied
 * @param crop Rectangle defining the region of interest for cropping
 */
void SurfaceUtils::ComputeTransformMatrixV2(float matrix[MATRIX_ARRAY_SIZE], uint32_t matrixSize,
    sptr<SurfaceBuffer>& buffer, GraphicTransformType& transform, const Rect& crop)
{
    if (buffer == nullptr) {
        return;
    }
    float tx = 0.f;
    float ty = 0.f;
    float sx = 1.f;
    float sy = 1.f;
    switch (transform) {
        case GraphicTransformType::GRAPHIC_ROTATE_90:
            transform = GraphicTransformType::GRAPHIC_ROTATE_270;
            break;
        case GraphicTransformType::GRAPHIC_ROTATE_270:
            transform = GraphicTransformType::GRAPHIC_ROTATE_90;
            break;
        default:
            break;
    }
    std::array<float, TRANSFORM_MATRIX_ELE_COUNT> transformMatrix = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    ComputeTransformByMatrixV2(transform, &transformMatrix);

    float bufferWidth = buffer->GetWidth();
    float bufferHeight = buffer->GetHeight();
    bool changeFlag = false;
    if (crop.w < bufferWidth && bufferWidth != 0) {
        tx = (float(crop.x) / bufferWidth);
        sx = (float(crop.w) / bufferWidth);
        changeFlag = true;
    }
    if (crop.h < bufferHeight && bufferHeight != 0) {
        ty = (float(bufferHeight - crop.y - crop.h) / bufferHeight);
        sy = (float(crop.h) / bufferHeight);
        changeFlag = true;
    }
    if (changeFlag) {
        std::array<float, MATRIX_ARRAY_SIZE> cropMatrix = {sx, 0, 0, 0, 0, sy, 0, 0, 0, 0, 1, 0, tx, ty, 0, 1};
        transformMatrix = MatrixProduct(cropMatrix, transformMatrix);
    }

    const std::array<float, TRANSFORM_MATRIX_ELE_COUNT> flipV = {1, 0, 0, 0, 0, -1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 1};
    transformMatrix = MatrixProduct(flipV, transformMatrix);

    auto ret = memcpy_s(matrix, matrixSize * sizeof(float),
                        transformMatrix.data(), sizeof(transformMatrix));
    if (ret != EOK) {
        BLOGE("memcpy_s failed, ret: %{public}d", ret);
    }
}

void SurfaceUtils::ComputeBufferMatrix(float matrix[MATRIX_ARRAY_SIZE], uint32_t matrixSize,
    sptr<SurfaceBuffer>& buffer, GraphicTransformType& transform, const Rect& crop)
{
    ComputeTransformMatrixV2(matrix, matrixSize, buffer, transform, crop);
}

void* SurfaceUtils::GetNativeWindow(uint64_t uniqueId)
{
    std::lock_guard<std::mutex> lockGuard(mutex_);
    auto iter = nativeWindowCache_.find(uniqueId);
    if (iter == nativeWindowCache_.end()) {
        BLOGE("Cannot find nativeWindow, uniqueId %{public}" PRIu64 ".", uniqueId);
        return nullptr;
    }
    return iter->second;
}

SurfaceError SurfaceUtils::AddNativeWindow(uint64_t uniqueId, void *nativeWidow)
{
    if (nativeWidow == nullptr) {
        return GSERROR_INVALID_ARGUMENTS;
    }
    std::lock_guard<std::mutex> lockGuard(mutex_);
    if (nativeWindowCache_.count(uniqueId) == 0) {
        nativeWindowCache_[uniqueId] = nativeWidow;
        return GSERROR_OK;
    }
    BLOGD("the nativeWidow already existed, uniqueId %" PRIu64, uniqueId);
    return GSERROR_OK;
}

SurfaceError SurfaceUtils::RemoveNativeWindow(uint64_t uniqueId)
{
    std::lock_guard<std::mutex> lockGuard(mutex_);
    nativeWindowCache_.erase(uniqueId);
    return GSERROR_OK;
}
} // namespace OHOS
