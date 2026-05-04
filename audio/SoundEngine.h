#pragma once

#include "SoundDef.h"
#include "SoundEventDef.h"

#include <string>
#include <memory>
#include <unordered_map>
#include <vector>
#include <cstdint>

namespace ozSound
{

class SoundInstance;
class VoiceInstance;


/// <summary>
/// 再生中のサウンドを識別するハンドル型。
/// </summary>
using SoundHandle = uint32_t;
static constexpr SoundHandle kInvalidHandle = UINT32_MAX;

/// <summary>
/// サウンドをデータ（JSON）で一元管理するエンジン。
/// SoundInstance / VoiceInstance を隠蔽し、
/// 呼び出し元はサウンドID とハンドルのみで操作する。
/// </summary>
class SoundEngine
{
public:

    static SoundEngine* GetInstance();

    void Initialize();
    void Finalize();

    /// <summary>
    /// JSON ファイルからサウンド定義を読み込み、全音声を即時ロードする。
    /// AudioSystem::Initialize() の後に呼ぶこと。
    /// </summary>
    /// <param name="jsonPath">SoundData.json のパス</param>
    void LoadSoundData(const std::string& jsonPath);

    /// <summary>
    /// JSON ファイルからサウンドイベント定義を読み込む。
    /// </summary>
    /// <param name="jsonPath"></param>
    void LoadEventData(const std::string& jsonPath);

    /// <summary>
    /// イベント名に紐づくサウンドイベントを実行する。
    /// </summary>
    /// <param name="eventName">SoundEventDef.name と一致するイベント名</param>
    void PostEvent(const std::string& eventName);

    /// <summary>
    /// サウンドを再生し、操作用ハンドルを返す。
    /// type が "BGM" なら BGM submix、それ以外は SE submix に自動振り分け。
    /// 失敗時は kInvalidHandle を返す。
    /// </summary>
    SoundHandle Play(const std::string& soundId,
                     float volume    = 1.0f,
                     bool  loop      = false,
                     float startTime = 0.0f);

    SoundHandle Play(const std::string& soundId,
                     const std::vector<std::string>& effects,
                     float volume = 1.0f,
                     bool loop = false,
                     float startTime = 0.0f);

    void Stop(SoundHandle handle);
    void StopAll();
    void Pause(SoundHandle handle);
    void Resume(SoundHandle handle);
    void SetVolume(SoundHandle handle, float volume);

    bool  IsPlaying(SoundHandle handle) const;
    float GetElapsedTime(SoundHandle handle) const;
    float GetDuration(const std::string& soundId) const;

    /// <summary>
    /// 再生終了済みエントリをクリーンアップする。毎フレーム呼ぶことを推奨。
    /// </summary>
    void CleanupStoppedVoices();

    /// <summary>
    /// 指定 Submix にルーティングされている再生中サウンドをすべて停止する。
    /// AudioSystem::RemoveSubmix() から呼ばれる。
    /// </summary>
    void StopSoundsOnSubmix(const std::string& submixName);

    /// <summary>
    /// 段階的移行用エスケープハッチ。
    /// 既存クラス（GameMusic 等）が SoundInstance に直接アクセスする必要がある場合に使う。
    /// </summary>
    std::shared_ptr<SoundInstance> GetSoundInstance(const std::string& soundId);

#ifdef _DEBUG
    static void ImGui(bool* _open);
#endif

private:

    struct PlayingSound
    {
        std::shared_ptr<SoundInstance> soundInstance;
        std::shared_ptr<VoiceInstance> voiceInstance;
        std::string soundId;
        bool        loop = false; // ループ再生中かどうか（Now Playing 表示用）
    };

    SoundHandle GenerateHandle();

private:

    std::unordered_map<std::string, SoundDef>                       soundDefs_;
    std::unordered_map<std::string, SoundEventDef>                  eventDefs_;
    std::unordered_map<std::string, std::shared_ptr<SoundInstance>> loadedInstances_;
    std::unordered_map<SoundHandle, PlayingSound>                   playingSounds_;

    SoundHandle nextHandle_ = 0;

    std::string soundDataPath_; // LoadSoundData() で渡されたパスを記憶（Reload 用）
    std::string eventDataPath_; // LoadEventData() で渡されたパスを記憶（Reload 用）

private:
    SoundEngine()  = default;
    ~SoundEngine() = default;
    SoundEngine(const SoundEngine&)            = delete;
    SoundEngine& operator=(const SoundEngine&) = delete;
};

} // namespace ozSound
