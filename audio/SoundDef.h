#pragma once

#include <string>
#include "JsonUtils/JsonUtils.h"

namespace ozSound
{

/// <summary>
/// SoundEngine に登録するサウンド定義。
/// SoundData.json で id / path / type を記述し、
/// SoundEngine::LoadSoundData() で読み込む。
/// </summary>
struct SoundDef
{
    std::string id;                // サウンドID（一意なキー）
    std::string filePath;          // 音声ファイルパス
    std::string type;              // "BGM" or "SE"（セマンティクス：ループ推奨・非重複等）
    std::string submixName;        // ルーティング先 Submix 名（省略時は type と同名）
    bool        enableOverlap = true; // 重複再生を許可するか（BGM は false 推奨）
};



inline void to_json(json& j, const SoundDef& v)
{
    j = json{
        {"id",            v.id           },
        {"path",          v.filePath     },
        {"type",          v.type         },
        {"submix",        v.submixName   },
        {"enableOverlap", v.enableOverlap},
    };
}

inline void from_json(const json& j, SoundDef& v)
{
    if (j.contains("id"))
        v.id = j["id"].get<std::string>();
    if (j.contains("path"))
        v.filePath = j["path"].get<std::string>();
    v.type          = j.value("type", std::string("SE"));
    // "submix" キーがなければ type と同名にフォールバック
    v.submixName    = j.value("submix", v.type);
    v.enableOverlap = j.value("enableOverlap", true);
}

} // namespace ozSound

