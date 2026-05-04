#include "AudioEffect.h"

ozSound::AudioEffect::AudioEffect(IUnknown* effect, uint32_t outputChannel, bool initialState)
    : effect_(effect), outputChannel_(outputChannel), initialState_(initialState)
{
}

///===================================================================
/// 以下AudioEffectChain
///===================================================================
void ozSound::AudioEffectChain::AddEffect(AudioEffect&& _effect)
{
    effects_.push_back(std::move(_effect));
}

const XAUDIO2_EFFECT_CHAIN* ozSound::AudioEffectChain::BuildChain()
{
    if (IsEmpty())
    {
        return nullptr;
    }

    descriptors_.clear();
    for (const auto& effect : effects_)
    {
        XAUDIO2_EFFECT_DESCRIPTOR descriptor{};
        descriptor.InitialState = effect.IsInitiallyEnabled();
        descriptor.OutputChannels = effect.GetOutputChannel();
        descriptor.pEffect = effect.GetEffect();
        descriptors_.push_back(descriptor);
    }

    XAUDIO2_EFFECT_CHAIN chain{};
    chain.EffectCount = static_cast<uint32_t>(descriptors_.size());
    chain.pEffectDescriptors = descriptors_.data();
    chain_ = chain;

    return &chain_;

}

void ozSound::AudioEffectChain::AttachToVoice(IXAudio2Voice* _voice)
{
    attachedVoice_ = _voice;
}

void ozSound::AudioEffectChain::DetachFromVoice()
{
    attachedVoice_ = nullptr;
}

HRESULT ozSound::AudioEffectChain::ApplyChain()
{
    if (IsEmpty())
    {
        return attachedVoice_->SetEffectChain(nullptr);
    }

    return attachedVoice_->SetEffectChain(BuildChain());
}

HRESULT ozSound::AudioEffectChain::EnableEffect(uint32_t _index, uint32_t _operationSet)
{
    if(!attachedVoice_)
    {
        return E_FAIL; // ボイスにアタッチされていない
    }

    return attachedVoice_->EnableEffect(_index, _operationSet);

}

HRESULT ozSound::AudioEffectChain::DisableEffect(uint32_t _index, uint32_t _operationSet)
{
    if (!attachedVoice_)
    {
        return E_FAIL; // ボイスにアタッチされていない
    }

    return attachedVoice_->DisableEffect(_index, _operationSet);
}

HRESULT ozSound::AudioEffectChain::SetEffectParameters(uint32_t _index, const void* _pParams, uint32_t _byteSize, uint32_t _operationSet)
{
    if (!attachedVoice_)
    {
        return E_FAIL; // ボイスにアタッチされていない
    }

    return attachedVoice_->SetEffectParameters(_index, _pParams, _byteSize, _operationSet);
}

HRESULT ozSound::AudioEffectChain::GetEffectParameters(uint32_t _index, void* _pParams, uint32_t _byteSize)
{
    if (!attachedVoice_)
    {
        return E_FAIL; // ボイスにアタッチされていない
    }

    return attachedVoice_->GetEffectParameters(_index, _pParams, _byteSize);
}
