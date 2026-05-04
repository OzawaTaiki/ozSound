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
    SetVolume
};

struct SoundEventAction
{
    SoundEventType type;
    std::string soundId; // Play の場合に使用
    float volume;       // SetVolume の場合に使用
    bool loop;          // Play の場合に使用
    std::vector<std::string> effects;// 適用するエフェクト
};


struct SoundEventDef
{
    std::string name;   // イベント名
    std::vector<SoundEventAction> actions; // イベントに紐づくアクションのリスト
};

NLOHMANN_JSON_SERIALIZE_ENUM(SoundEventType, {
    {SoundEventType::Play,      "Play"     },
    {SoundEventType::Stop,      "Stop"     },
    {SoundEventType::Pause,     "Pause"    },
    {SoundEventType::Resume,    "Resume"   },
    {SoundEventType::SetVolume, "SetVolume"},
                             })


inline void to_json(json& j, const SoundEventAction& v)
{
    j = json{
        {"type", v.type},
        {"soundId", v.soundId},
        {"volume", v.volume},
        {"loop", v.loop},
        {"effects", v.effects}
    };
}

inline void from_json(const json& j, SoundEventAction& v)
{
    if (j.contains("type"))
        v.type = j["type"].get<ozSound::SoundEventType>();
    if (j.contains("soundId"))
        v.soundId = j["soundId"].get<std::string>();
    v.volume = j.value("volume", 1.0f);
    v.loop = j.value("loop", false);
    if (j.contains("effects"))
        v.effects = j["effects"].get<std::vector<std::string>>();
}

inline void to_json(json& j, const SoundEventDef& v)
{
    j = json{
        {"name",    v.name   },
        {"actions", v.actions},
    };
}

inline void from_json(const json& j, SoundEventDef& v)
{
    v.name = j.value("name", std::string(""));
    if (j.contains("actions"))
        v.actions = j["actions"].get<std::vector<SoundEventAction>>();
}

} // namespace ozSound

