#pragma once

#include <json.hpp>

#include <fstream>
#include <string>

#include <filesystem>
#include "../Logger/SoundLogger.h"

namespace ozSound
{

using json = nlohmann::json;

inline json LoadJson(const std::string& path)
{
    std::ifstream f(path);

    if (!f.is_open())
        return {};

    return json::parse(f, nullptr, false); // 例外なし
}

inline void SaveJson(const std::string& path, const json& data)
{
    // ディレクトリがなければ作成
    std::filesystem::create_directories(
        std::filesystem::path(path).parent_path());

    std::ofstream f(path);
    if (!f.is_open())
    {
        ozSound::Log("Failed to save JSON data to: " + path + "\n");
        return;
    }

    f << data.dump(4);
}

} // namespace ozSound