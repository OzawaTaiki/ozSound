#include "VoiceInstance.h"

#include "Logger/SoundLogger.h"

#include <algorithm>
#include <stdexcept>
#include <utility>


namespace ozSound{

VoiceInstance::VoiceInstance(IXAudio2SourceVoice* _sourceVoice, float _volume, float _sampleRate, float _startTime) :
    sourceVoice_(_sourceVoice),
    volume_(_volume),
    sampleRate_(_sampleRate),
    startTime_(_startTime),
    isPaused_(false),
    hr_(S_OK)
{
    if (!sourceVoice_)
    {
        ozSound::Log("Error: sourceVoice is null\n");
        throw std::runtime_error("Error: sourceVoice is null");
        return;
    }

    if (volume_ < 0.0f || volume_ > 1.0f)
    {
        ozSound::Log("Error: Volume must be between 0.0 and 1.0\n");
        volume_ = 1.0f;
    }

    ApplyVolume();
    CheckHRESULT();

}

VoiceInstance::~VoiceInstance()
{
    // Voice を壊す前にチェーンの参照を切る (破棄済み Voice を触らせない)
    effectChain_.DetachFromVoice();

    if (sourceVoice_)
    {
        sourceVoice_->DestroyVoice();
        sourceVoice_ = nullptr;
    }
}

void VoiceInstance::SetEffectChain(AudioEffectChain&& _chain)
{
    effectChain_ = std::move(_chain);
    effectChain_.AttachToVoice(sourceVoice_);
}

void VoiceInstance::Play()
{
    if (sourceVoice_)
    {
        if (isPaused_)
        {
            hr_ = sourceVoice_->Start();
            isPaused_ = false;
        }
        else
        {
            hr_ = sourceVoice_->Start();

        }
    }
    CheckHRESULT();
}

void VoiceInstance::Stop()
{
    if (sourceVoice_)
    {
        hr_ = sourceVoice_->Stop();
        hr_ = sourceVoice_->FlushSourceBuffers();
    }
    CheckHRESULT();
}

void VoiceInstance::Pause()
{
    if (sourceVoice_ && !isPaused_)
    {
        hr_ = sourceVoice_->Stop();
        isPaused_ = true;
    }
    CheckHRESULT();
}

void VoiceInstance::Resume()
{
    if (sourceVoice_ && isPaused_)
    {
        hr_ = sourceVoice_->Start();
        isPaused_ = false;
    }
    CheckHRESULT();
}

void VoiceInstance::FadeIn(float _fadeTime)
{
    // 時間指定が無ければ待たずに鳴らし切る
    if (_fadeTime <= 0.0f)
    {
        isFading_ = false;
        stopOnFadeEnd_ = false;
        fadeGain_ = 1.0f;
        ApplyVolume();
        return;
    }

    fadeGainFrom_ = 0.0f;
    fadeGainTo_ = 1.0f;
    fadeDuration_ = _fadeTime;
    fadeElapsed_ = 0.0f;
    isFading_ = true;
    stopOnFadeEnd_ = false;

    // 立ち上がりを待たずに無音から始める（次の UpdateFade から上がっていく）
    fadeGain_ = 0.0f;
    ApplyVolume();
}

void VoiceInstance::FadeOut(float _fadeTime)
{
    // 時間指定が無ければその場で止める
    if (_fadeTime <= 0.0f)
    {
        isFading_ = false;
        stopOnFadeEnd_ = false;
        fadeGain_ = 0.0f;
        ApplyVolume();
        Stop();
        return;
    }

    fadeGainFrom_ = fadeGain_;   // 途中から掛け直しても今の音量から繋がる
    fadeGainTo_ = 0.0f;
    fadeDuration_ = _fadeTime;
    fadeElapsed_ = 0.0f;
    isFading_ = true;
    stopOnFadeEnd_ = true;
}

void VoiceInstance::UpdateFade(float _deltaTime)
{
    if (!isFading_)
        return;

    fadeElapsed_ += _deltaTime;

    const float t = (fadeDuration_ > 0.0f)
        ? (std::min)(1.0f, fadeElapsed_ / fadeDuration_)
        : 1.0f;

    fadeGain_ = fadeGainFrom_ + (fadeGainTo_ - fadeGainFrom_) * t;
    ApplyVolume();

    if (t < 1.0f)
        return;

    isFading_ = false;

    // フェードアウトは下がり切ったところで止める。
    // 停止後は IsPlaying() が false になり、SoundEngine 側の掃除で回収される
    if (stopOnFadeEnd_)
    {
        stopOnFadeEnd_ = false;
        Stop();
    }
}

void VoiceInstance::ApplyVolume()
{
    if (sourceVoice_)
        hr_ = sourceVoice_->SetVolume(volume_ * fadeGain_);
}

void VoiceInstance::SetVolume(float _volume)
{
    // フェード中でも破綻しないよう、指定値は volume_ に置くだけにして
    // 実際に流す音量は ApplyVolume() で fadeGain_ と掛け合わせる
    volume_ = _volume;
    ApplyVolume();
    CheckHRESULT();
}

bool VoiceInstance::IsPlaying() const
{
    if (sourceVoice_)
    {
        XAUDIO2_VOICE_STATE state;
        sourceVoice_->GetState(&state);
        return state.BuffersQueued > 0;
    }
    return false;
}

float VoiceInstance::GetElapsedTime() const
{
    XAUDIO2_VOICE_STATE state;
    sourceVoice_->GetState(&state);
    return static_cast<float>(state.SamplesPlayed) / sampleRate_ + startTime_;
}

void VoiceInstance::SetPlaySpeed(float _speed)
{
    if (sourceVoice_)
    {
        hr_ = sourceVoice_->SetFrequencyRatio(_speed);
        CheckHRESULT();
        playSpeed_ = _speed;
    }
}

void VoiceInstance::CheckHRESULT() const
{
    if (FAILED(hr_))
    {
        ozSound::Log("Error: XAudio2 operation failed with HRESULT: " + std::to_string(hr_) + "\n");
        throw std::runtime_error("XAudio2 operation failed with HRESULT: " + std::to_string(hr_));
    }
}

} // namespace ozSound
