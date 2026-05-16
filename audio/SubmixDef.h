#pragma once

#include <string>
#include <vector>

#include "JsonUtils/JsonUtils.h"

namespace ozSound
{

struct InsertSlot
{
    std::string effectName; // 挿入するエフェクトの名前
    bool enabled = true;     // エフェクトの有効/無効
};

struct SendSlot
{
    std::string targetSubmix; // 送信先のサブミックス名
    float sendLevel = 1.0f;   // 送信レベル（0.0～1.0）
    bool enabled = true;      // 送信の有効/無効

};

struct SubmixDef
{
    std::string name; // サブミックスの名前（ルーティングのために一意である必要がある）
    std::vector<InsertSlot> inserts; // 挿入エフェクトスロットのリスト
    std::vector<SendSlot> sends;     // 送信スロットのリスト
    float volume = 1.0f; // サブミックスの音量（0.0～1.0）
    float pan = 0.0f;    // サブミックスのパン（-1.0が左、0がセンター、1.0が右）
    bool mute = false;   // サブミックスのミュート状態
    bool solo = false;   // サブミックスのソロ状態
};

/// -------------------------------
/// JSONシリアライズ/デシリアライズ

inline void to_json(json& j, const InsertSlot& v)
{
    j = json{
        {"effect", v.effectName},
        {"enabled", v.enabled   },
    };
}

inline void from_json(const json& j, InsertSlot& v)
{
    v.effectName = j.value("effect", std::string(""));
    v.enabled    = j.value("enabled", true);
}

inline void to_json(json& j, const SendSlot& v)
{
    j = json{
        {"targetSubmix", v.targetSubmix},
        {"sendLevel",   v.sendLevel   },
        {"enabled",     v.enabled     },
    };
}

inline void from_json(const json& j, SendSlot& v)
{
    v.targetSubmix = j.value("targetSubmix", std::string(""));
    v.sendLevel    = j.value("sendLevel", 1.0f);
    v.enabled      = j.value("enabled", true);
}

inline void to_json(json& j, const SubmixDef& v)
{
    j = json{
        {"name",    v.name   },
        {"inserts", v.inserts},
        {"sends",   v.sends  },
        {"volume",  v.volume },
        {"pan",     v.pan    },
        {"mute",    v.mute   },
        {"solo",    v.solo   },
    };
}

inline void from_json(const json& j, SubmixDef& v)
{
    v.name      = j.value("name",    std::string("")     );
    v.inserts   = j.value("inserts", std::vector<InsertSlot>{});
    v.sends     = j.value("sends",   std::vector<SendSlot>{}  );
    v.volume    = j.value("volume",  1.0f                );
    v.pan       = j.value("pan",     0.0f                );
    v.mute      = j.value("mute",    false               );
    v.solo      = j.value("solo",    false               );

}// namespace ozSound