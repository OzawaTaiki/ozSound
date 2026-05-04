#pragma once
#include <string>

#include "JsonUtils/JsonUtils.h"

namespace ozSound
{

enum class AudioEffectType
{
    Native, // コードに記述したエフェクト
    VST3 // VST3プラグインエフェクト
};

struct AudioEffectDef
{
    std::string name; // エフェクトの名前
    std::string path; // エフェクトのファイルパス(vst)
    std::string className; // エフェクトのクラス名(vst)
    AudioEffectType type; // エフェクトの種類
};

NLOHMANN_JSON_SERIALIZE_ENUM(AudioEffectType, {
    {AudioEffectType::Native, "Native"},
    {AudioEffectType::VST3,   "VST3"  },
                             })


inline void to_json(json& j, const AudioEffectDef& v)
{
    j = json{
        {"name", v.name},
        {"type", v.type},
        {"path", v.path},
        {"className",v.className}
    };
}

inline void from_json(const json& j, AudioEffectDef& v)
{
    if (j.contains("name"))
        v.name = j["name"].get<std::string>();
    if (j.contains("type"))
        v.type = j["type"].get<ozSound::AudioEffectType>();
    if (j.contains("path"))
        v.path = j["path"].get<std::string>();
    if (j.contains("className"))
        v.className = j["className"].get<std::string>();
}

}// namespace ozSound
