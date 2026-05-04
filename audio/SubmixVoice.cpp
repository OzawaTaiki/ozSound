#include "SubmixVoice.h"

#include "Logger/SoundLogger.h"

#include <cassert>
#include <algorithm>

#include <pluginterfaces/vst/ivstaudioprocessor.h>

using namespace ozSound;

HRESULT SubmixVoice::Initialize(IXAudio2* xAudio2, uint32_t inputChannels, float sampleRate, uint32_t processingStage, SubmixVoice* sendTarget)
{
    XAUDIO2_SEND_DESCRIPTOR sendDesc{};
    sendDesc.Flags = 0;
    sendDesc.pOutputVoice = sendTarget ? sendTarget->GetSubmixVoice() : nullptr;

    XAUDIO2_VOICE_SENDS sendList{};
    sendList.SendCount = 1;
    sendList.pSends = &sendDesc;

    const XAUDIO2_EFFECT_CHAIN* chainPtr = effectChain_.IsEmpty() ? nullptr : effectChain_.BuildChain();

    HRESULT hr = xAudio2->CreateSubmixVoice(
        &submixVoice_,
        inputChannels,
        static_cast<UINT32>(sampleRate),
        XAUDIO2_VOICE_USEFILTER,
        processingStage,
        sendTarget ? &sendList : nullptr,
        chainPtr);

    if (FAILED(hr))
    {
        ozSound::Log("Failed to create submix voice: " + std::to_string(hr) + "\n");
        assert(false && "Failed to create submix voice");
        return hr;
    }

    effectChain_.AttachToVoice(submixVoice_);
    submixVoice_->SetVolume(volume_);
    sampleRate_ = sampleRate;

    sendDesc_ = XAUDIO2_SEND_DESCRIPTOR{};
    sendDesc_.Flags = 0;
    sendDesc_.pOutputVoice = submixVoice_;

    sendList_ = XAUDIO2_VOICE_SENDS{};
    sendList_.SendCount = 1;
    sendList_.pSends = &sendDesc_;

    return hr;
}

void SubmixVoice::Finalize()
{
    effectChain_.DetachFromVoice();
    if (submixVoice_)
    {
        submixVoice_->DestroyVoice();
        submixVoice_ = nullptr;
    }
}

void SubmixVoice::SetVolume(float volume)
{
    volume_ = volume;
    if (submixVoice_)
    {
        submixVoice_->SetVolume(volume_);
        // 第二引数が存在
        // 即時化まとめてかを設定できるらしいが現状は不要なのでセットしない
    }
}

void SubmixVoice::SetFilter(XAUDIO2_FILTER_TYPE type, float cutoffHz, float oneOverQ)
{
    const float kPi = 3.14159265358979f;

    float omega = 2.0f * std::sinf(kPi * cutoffHz / sampleRate_);

    omega = std::clamp(omega, 0.0f, 1.0f);

    XAUDIO2_FILTER_PARAMETERS params{};
    params.Type = type;
    params.Frequency = omega;
    params.OneOverQ = oneOverQ;

    submixVoice_->SetFilterParameters(&params);

}

void SubmixVoice::ClearFilter()
{
    XAUDIO2_FILTER_PARAMETERS params{};
    params.Type = XAUDIO2_FILTER_TYPE::LowPassFilter;
    params.Frequency = 1.0f;
    params.OneOverQ = 1.0f;

    submixVoice_->SetFilterParameters(&params);

}
