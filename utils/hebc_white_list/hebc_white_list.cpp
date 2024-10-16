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

#include "config_policy_utils.h"
#include "hebc_white_list.h"
#include "buffer_log.h"
#include <unistd.h>
#include <fstream>
#include <json/json.h>

namespace OHOS {
namespace {
const std::string CONFIG_FILE_RELATIVE_PATH = "etc/graphics_game/config/graphics_game.json";
const std::string PROCESS_NAME = "/proc/self/cmdline";
constexpr long MAX_FILE_SIZE = 32 * 1024 * 1024;
} // end of anonymous namespace

bool HebcWhiteList::Check(const std::string& name) noexcept
{
    if (!Init()) {
        BLOGE("Init failed");
        return false;
    }
    return (std::find(hebcList_.begin(), hebcList_.end(), name) != hebcList_.end());
}

bool HebcWhiteList::Init() noexcept
{
    if (inited_.load()) {
        return true;
    }

    inited_.store(ParseJson(AcquireConfig(GetConfigAbsolutePath())));
    return inited_.load();
}

void HebcWhiteList::GetApplicationName(std::string& name) noexcept
{
    std::ifstream procfile(PROCESS_NAME);
    if (!procfile.is_open()) {
        return;
    }
    std::getline(procfile, name);
    procfile.close();
    name = name.substr(0, name.find('\0'));
}

bool HebcWhiteList::ParseJson(std::string const &json) noexcept
{
    Json::Value root{};
    Json::Reader reader(Json::Features::all());
    if (!reader.parse(json, root)) {
        BLOGE("parse json failed");
        return false;
    }

    Json::Value const hebc = root.get("HEBC", Json::Value{});
    Json::Value const appNameJson = hebc.get("AppName", Json::Value{});
    for (unsigned int i = 0; i < appNameJson.size(); i++) {
        std::string name = appNameJson[i].asString();
        hebcList_.emplace_back(name);
    }
    return true;
}

std::string HebcWhiteList::AcquireConfig(const std::string& filePath) noexcept
{
    size_t size = 0;
    std::unique_ptr<char[]> buffer = nullptr;
    if (!filePath.empty()) {
        buffer = ReadFile(filePath, size, MAX_FILE_SIZE);
    }
    if (buffer == nullptr) {
        return std::string("{}");
    }
    return std::string(buffer.get());
}

std::string HebcWhiteList::GetConfigAbsolutePath() noexcept
{
    char buf[MAX_PATH_LEN] = { 0 };
    char const *path = GetOneCfgFile(std::string(CONFIG_FILE_RELATIVE_PATH).c_str(), buf, MAX_PATH_LEN);
    if (path == nullptr) {
        BLOGE("failed");
        return std::string("{}");
    }
    return std::string(path);
}

std::unique_ptr<char[]> HebcWhiteList::ReadFile(std::string const &file, size_t &size, size_t maxSize) noexcept
{
    std::ifstream ifs;
    ifs.open(file, std::ifstream::binary);
    if (!ifs.good()) {
        BLOGE("file is bad");
        return nullptr;
    }

    if (!ifs.is_open()) {
        BLOGE("open file failed");
        return nullptr;
    }

    ifs.seekg(0, std::ios::end);
    auto const tellg = static_cast<size_t>(ifs.tellg());
    if (tellg <= 0 || tellg > maxSize) {
        ifs.close();
        BLOGE("read file failed");
        return nullptr;
    }
    size = tellg;
    auto buffer = std::make_unique<char[]>(size);
    ifs.seekg(0, std::ios::beg);
    ifs.read(buffer.get(), size);
    ifs.close();

    return buffer;
}
} // namespace OHOS