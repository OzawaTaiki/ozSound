#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// ozSound 統合ファサード
//
// ゲーム側はこのヘッダ 1 枚を include すれば、ozSound のライフサイクル制御と
// 主要ランタイム API を全部使える。順序依存のある複数 Initialize / Load* /
// Finalize を内部で解決する。
//
// 典型コード:
//
//   ozSound::Initialize();                            // 順序保証
//   ozSound::LoadProject("Resources/Audio/Game.ozproj");  // parse 1 回
//   // game loop
//   ozSound::PostEvent("BossAppear");
//   ozSound::Tick();                                  // 毎フレーム
//   // shutdown
//   ozSound::Finalize();
// ─────────────────────────────────────────────────────────────────────────────

#include "AudioSystem.h"
#include "AudioEffectManager.h"
#include "SoundEngine.h"
#include "VST3/VST3Host.h"

#include <string>
#include <vector>

namespace ozSound
{

struct InitOptions
{
    /// <summary>VST3Host::Initialize() に渡すホスト名。</summary>
    std::string hostName     = "GameHost";

    /// <summary>false で VST3Host + AudioEffectManager の Initialize/Finalize をスキップ。</summary>
    bool        enableVST3   = true;

    /// <summary>false で SoundEngine の Initialize/Finalize をスキップ。</summary>
    bool        enableEngine = true;
};

// ── Lifecycle ───────────────────────────────────────────────────────────────

/// <summary>
/// ozSound の各サブシステムを依存順で Initialize する。
/// AudioSystem → VST3Host → AudioEffectManager → SoundEngine の順。
/// InitOptions のフラグでスキップ可能。二重呼び出しは無視される。
/// </summary>
/// <returns>AudioSystem の初期化に成功すれば true。</returns>
bool Initialize(const InitOptions& opts = {});

/// <summary>
/// Initialize で立ち上げたサブシステムを逆順で Finalize する。
/// Initialize が呼ばれていない場合は何もしない。
/// </summary>
void Finalize();

/// <summary>
/// .ozproj (sounds/events/submixes/effects 統合 JSON) を 1 度の parse で
/// 4 サブシステムに読み込む。順序: submixes → effects → sounds → events。
/// InitOptions で無効化したサブシステムへのロードはスキップされる。
/// </summary>
/// <returns>ファイル読み込みに成功すれば true。</returns>
bool LoadProject(const std::string& ozprojPath);

// ── Runtime convenience (SoundEngine への薄いラッパ) ────────────────────────
// inline 転送のみ。新規ロジックなし。

inline void PostEvent(const std::string& eventName)
{
    SoundEngine::GetInstance()->PostEvent(eventName);
}

inline SoundHandle Play(const std::string& soundId,
                        float volume    = 1.0f,
                        bool  loop      = false,
                        float startTime = 0.0f)
{
    return SoundEngine::GetInstance()->Play(soundId, volume, loop, startTime);
}

inline SoundHandle Play(const std::string& soundId,
                        const std::vector<std::string>& effects,
                        float volume    = 1.0f,
                        bool  loop      = false,
                        float startTime = 0.0f)
{
    return SoundEngine::GetInstance()->Play(soundId, effects, volume, loop, startTime);
}

inline void  Stop          (SoundHandle h)          { SoundEngine::GetInstance()->Stop(h); }
inline void  StopAll       ()                       { SoundEngine::GetInstance()->StopAll(); }
inline void  Pause         (SoundHandle h)          { SoundEngine::GetInstance()->Pause(h); }
inline void  Resume        (SoundHandle h)          { SoundEngine::GetInstance()->Resume(h); }
inline void  SetVolume     (SoundHandle h, float v) { SoundEngine::GetInstance()->SetVolume(h, v); }
inline bool  IsPlaying     (SoundHandle h)          { return SoundEngine::GetInstance()->IsPlaying(h); }
inline float GetElapsedTime(SoundHandle h)          { return SoundEngine::GetInstance()->GetElapsedTime(h); }
inline float GetDuration   (const std::string& id)  { return SoundEngine::GetInstance()->GetDuration(id); }

/// <summary>毎フレーム呼ぶ。再生終了済みボイスのクリーンアップ。</summary>
inline void Tick(float deltaTime)
{
    SoundEngine::GetInstance()->Update(deltaTime);
}

// ── Master ──────────────────────────────────────────────────────────────────

inline void  SetMasterVolume(float v) { AudioSystem::GetInstance()->SetMasterVolume(v); }
inline float GetMasterVolume()        { return AudioSystem::GetInstance()->GetMasterVolume(); }

} // namespace ozSound
