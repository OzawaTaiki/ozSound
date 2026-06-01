#pragma once

#include <string>
#include <vector>

#include "JsonUtils/JsonUtils.h"

namespace ozSound
{

enum class SoundEventType
{
    Play,
    Stop,
    Pause,
    Resume,
    SetVolume,
    SetSpeed,
    Delay,
    DuckingStart,
    DuckingEnd,
};

struct SoundEventAction
{
    SoundEventType type = SoundEventType::Play;
    std::string soundId = ""; // Play の場合に使用
    float volume = 0.5f;       // SetVolume の場合に使用
    float speed = 1.0f;         // SetSpeed
    bool loop = false;          // Play の場合に使用
    std::vector<std::string> effects = {};// 適用するエフェクト

    float startTime = 0.0f; // イベント開始からの遅延時間（秒）
    float duration= 1.0f;  // イベントの持続時間（秒）。0の場合は無限

    std::string targetSubmix = ""; // 対象のサブミックス（空の場合は全体）
    float fadeTime = 0.2f;
    float releaseTime = 1.0f;

};


struct SoundEventDef
{
    std::string name;   // イベント名
    std::vector<SoundEventAction> actions; // イベントに紐づくアクションのリスト
    float totalDuration = 0.0f; // イベントの総持続時間（秒）。全アクションの開始時間と持続時間から計算される
};

NLOHMANN_JSON_SERIALIZE_ENUM(SoundEventType, {
    {SoundEventType::Play,          "Play"          },
    {SoundEventType::Stop,          "Stop"          },
    {SoundEventType::Pause,         "Pause"         },
    {SoundEventType::Resume,        "Resume"        },
    {SoundEventType::SetVolume,     "SetVolume"     },
    {SoundEventType::SetSpeed,      "SetSpeed"      },
    {SoundEventType::Delay,         "Delay"         },
    {SoundEventType::DuckingStart,  "DuckingStart"  },
    {SoundEventType::DuckingEnd,    "DuckingEnd"    },
})


inline void to_json(json& j, const SoundEventAction& v)
{
    j = json{
        {"type",            v.type          },
        {"soundId",         v.soundId       },
        {"volume",          v.volume        },
        {"speed",           v.speed         },
        {"loop",            v.loop          },
        {"effects",         v.effects       },
        {"startTime",       v.startTime     },
        {"duration",        v.duration      },
        {"targetSubmix",    v.targetSubmix  },
        {"fadeTime",        v.fadeTime      },
        {"releaseTime",     v.releaseTime   },
    };
}

inline void from_json(const json& j, SoundEventAction& v)
{
    v.type          = j.value("type",           SoundEventType::Play        );
    v.soundId       = j.value("soundId",        std::string("")             );
    v.volume        = j.value("volume",         1.0f                        );
    v.speed         = j.value("speed",          1.0f                        );
    v.loop          = j.value("loop",           false                       );
    v.effects       = j.value("effects",        std::vector<std::string>{}  );
    v.startTime     = j.value("startTime",      0.0f                        );
    v.duration      = j.value("duration",       0.0f                        );
    v.targetSubmix  = j.value("targetSubmix",   std::string("")             );
    v.fadeTime      = j.value("fadeTime",       0.2f                        );
    v.releaseTime   = j.value("releaseTime",    1.0f                        );
}

inline void to_json(json& j, const SoundEventDef& v)
{
    j = json{
        {"name",    v.name   },
        {"actions", v.actions},
        {"totalDuration", v.totalDuration},
    };
}

inline void from_json(const json& j, SoundEventDef& v)
{
    v.name          = j.value("name",           std::string("")                 );
    v.actions       = j.value("actions",        std::vector<SoundEventAction>{} );
    v.totalDuration = j.value("totalDuration",  0.0f                            );
}

} // namespace ozSound

