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

#include <unistd.h>
#include <fstream>
#include <sstream>
#include "config_policy_utils.h"
#include "buffer_log.h"
#include "hebc_white_list.h"

#include <cJSON.h>

namespace OHOS {
namespace {
const std::string CONFIG_FILE_RELATIVE_PATH = "etc/graphics_game/config/graphics_game.json";
const std::string PROCESS_NAME = "/proc/self/cmdline";
constexpr long MAX_FILE_SIZE = 32 * 1024 * 1024;
static constexpr uint32_t MAX_HEBC_WHITELIST_NUMBER = 10000; // hebcwhiteList size not exceed 10000
static constexpr uint32_t MAX_APP_NAME_SIZE = 1024; // appname size not exceed 1024
static std::once_flag nameFlag_;
} // end of anonymous namespace

bool HebcWhiteList::Check(const std::string& appName) noexcept
{
    if (!Init()) {
        return false;
    }
    return (std::find(hebcList_.begin(), hebcList_.end(), appName) != hebcList_.end());
}

bool HebcWhiteList::Init() noexcept
{
    if (inited_.load()) {
        return true;
    }

    std::string jsonStr;
    AcquireConfig(GetConfigAbsolutePath(), jsonStr);
    inited_.store(ParseJson(jsonStr));
    return inited_.load();
}

void HebcWhiteList::GetApplicationName(std::string& name) noexcept
{
    std::call_once(nameFlag_, [this, &name]() {
        std::ifstream procfile(PROCESS_NAME);
        if (!procfile.is_open()) {
            return;
        }
        std::getline(procfile, name);
        procfile.close();
        size_t pos = name.find('\0');
        if (pos != std::string::npos) {
            appName_ = name.substr(0, std::min(pos, static_cast<size_t>(MAX_APP_NAME_SIZE)));
        } else {
            appName_ = name.substr(0, MAX_APP_NAME_SIZE);
        }
    });
    name = appName_;
}

bool HebcWhiteList::ParseJson(std::string const &json) noexcept
{
    if (json.empty()) {
        return true;
    }
    cJSON *parsed = cJSON_Parse(json.c_str());
    if (parsed == nullptr) {
        BLOGD("cJSON_Parse failed");
        return false;
    }

    cJSON *hebc = cJSON_GetObjectItem(parsed, "HEBC");
    if (hebc == nullptr) {
        cJSON_Delete(parsed);
        return true;
    }

    cJSON *appNameJson = cJSON_GetObjectItem(hebc, "AppName");
    if (appNameJson == nullptr) {
        cJSON_Delete(parsed);
        return true;
    }

    uint32_t appNameCount = 0;
    if (appNameJson->type == cJSON_Array) {
        cJSON *item;
        cJSON_ArrayForEach(item, appNameJson) {
            if (item == nullptr || !cJSON_IsString(item) || item->valuestring == nullptr ||
                strlen(item->valuestring) > MAX_APP_NAME_SIZE) {
                continue;
            }
            if (++appNameCount > MAX_HEBC_WHITELIST_NUMBER) {
                cJSON_Delete(parsed);
                return true;
            }
            std::string name(item->valuestring);
            hebcList_.emplace_back(name);
        }
    } else {
        if (cJSON_IsString(appNameJson) && appNameJson->valuestring != nullptr &&
            strlen(appNameJson->valuestring) <= MAX_APP_NAME_SIZE) {
            std::string name(appNameJson->valuestring);
            hebcList_.emplace_back(name);
        }
    }
    cJSON_Delete(parsed);
    return true;
}

void HebcWhiteList::AcquireConfig(const std::string& filePath, std::string& jsonStr) noexcept
{
    if (!filePath.empty()) {
        ReadFile(filePath, MAX_FILE_SIZE, jsonStr);
    }
}

std::string HebcWhiteList::GetConfigAbsolutePath() noexcept
{
    char buf[MAX_PATH_LEN] = { 0 };
    char const *path = GetOneCfgFile(std::string(CONFIG_FILE_RELATIVE_PATH).c_str(), buf, MAX_PATH_LEN);
    if (path == nullptr) {
        BLOGD("failed");
        return std::string("{}");
    }
    return std::string(path);
}

void HebcWhiteList::ReadFile(std::string const &file, size_t maxSize, std::string& buffer) noexcept
{
    std::ifstream ifs;
    ifs.open(file, std::ifstream::binary);
    if (!ifs.good()) {
        BLOGD("file is bad");
        return;
    }

    if (!ifs.is_open()) {
        BLOGD("open file failed");
        return;
    }

    ifs.seekg(0, std::ios::end);
    auto const tellg = static_cast<size_t>(ifs.tellg());
    if (tellg <= 0 || tellg > maxSize) {
        ifs.close();
        BLOGD("read file failed");
        return;
    }
    ifs.seekg(0, std::ios::beg);
    std::stringstream data;
    data << ifs.rdbuf();
    ifs.close();
    buffer = data.str();
}
} // namespace OHOS