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
    float volume = 1.0f;              // 音量（0.0～1.0）
    float pitch = 1.0f;               // ピッチ（0.5～2.0、1.0が原音）
    bool loop = false;                // ループ再生するか
};



inline void to_json(json& j, const SoundDef& v)
{
    j = json{
        {"id",            v.id           },
        {"path",          v.filePath     },
        {"type",          v.type         },
        {"submix",        v.submixName   },
        {"enableOverlap", v.enableOverlap},
        {"volume",        v.volume       },
        {"pitch",         v.pitch        },
        {"loop",          v.loop         },
    };
}

inline void from_json(const json& j, SoundDef& v)
{
    v.id            = j.value("id",             std::string("")     );
    v.filePath      = j.value("path",           std::string("")     );
    v.type          = j.value("type",           std::string("SE")   );
    v.submixName    = j.value("submix",         v.type              );
    v.enableOverlap = j.value("enableOverlap",  true                );
    v.volume        = j.value("volume",         1.0f                );
    v.pitch         = j.value("pitch",          1.0f                );
    v.loop          = j.value("loop",           false               );

}

} // namespace ozSound

