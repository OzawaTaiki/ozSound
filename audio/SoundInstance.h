#pragma once

#include <cstdint>
#include <string>
#include <memory>
#include <vector>

#include "VoiceInstance.h"
#include "VoiceCallback.h"


namespace ozSound
{

class AudioSystem;
class SubmixVoice;

class SoundInstance
{
public:

    SoundInstance(uint32_t _soundID, AudioSystem* _audioSystem, float _sampleRate);
    ~SoundInstance();

    std::shared_ptr<VoiceInstance> GenerateVoiceInstance(float _volume = 1.0f, float _startTime = 0.0f, bool _loop = false, bool _enableOverlap = true, VoiceCallBack* _callback = nullptr, SubmixVoice* _submix = nullptr, const XAUDIO2_EFFECT_CHAIN* _effectChain = nullptr);

    std::shared_ptr<VoiceInstance> Play(float _volume, bool _loop = false, bool _enableOverlap = true, VoiceCallBack* _callback = nullptr, SubmixVoice* _submix = nullptr);
    std::shared_ptr<VoiceInstance> Play(float _volume, float _startTime, bool _loop = false, bool _enableOverlap = true, VoiceCallBack* _callback = nullptr, SubmixVoice* _submix = nullptr);

    std::vector<float> GetAudioData() const;

    /// <summary>
    /// 音声IDを取得
    /// </summary>
    /// <returns>音声ID</returns>
    uint32_t GetSoundID() const { return soundID_; }

    /// <summary>
    /// 音声ファイルの再生時間を取得
    /// </summary>
    /// <returns>再生時間(秒)</returns>
    float GetDuration() const;

    /// <summary>
    /// サンプルレートを取得
    /// </summary>
    /// <returns> サンプルレート(Hz)</returns>
    float GetSampleRate() const { return sampleRate_; }

    

private:


private:

    AudioSystem* audioSystem_;
    uint32_t soundID_;

    float sampleRate_;

    std::vector<std::weak_ptr<VoiceInstance>> voiceInstance_;

};

} // namespace ozSound
