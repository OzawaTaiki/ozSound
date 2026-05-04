#include "VoiceInstance.h"

#include "Logger/SoundLogger.h"

#include <stdexcept>


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

    hr_ = sourceVoice_->SetVolume(volume_);
    CheckHRESULT();

}

VoiceInstance::~VoiceInstance()
{
    if (sourceVoice_)
    {
        sourceVoice_->DestroyVoice();
        sourceVoice_ = nullptr;
    }
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
    // todo : 実装
    _fadeTime;
}

void VoiceInstance::FadeOut(float _fadeTime)
{
    // todo : 実装
    _fadeTime;
}

void VoiceInstance::SetVolume(float _volume)
{
    if (sourceVoice_)
    {
        hr_ = sourceVoice_->SetVolume(_volume);
        volume_ = _volume;
    }
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
