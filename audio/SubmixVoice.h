#pragma once

#include <xaudio2.h>

#include "AudioEffect.h"


namespace ozSound
{

class SubmixVoice
{
public:

    SubmixVoice() = default;
    ~SubmixVoice() = default;

    /// <summary>
    /// 初期化
    /// </summary>
    /// <param name="xAudio2">XAudio2インスタンス</param>
    /// <param name="inputChannels">入力チャンネル数</param>
    /// <param name="sampleRate">サンプルレート</param>
    /// <param name="processingStage"> 処理ステージ。0が最初のステージで、数値が大きいほど後のステージになる。デフォルトは0。</param>
    /// <param name="sendTarget"> 送信先のサブミックスボイス。nullptrの場合はマスターボイスに送信される。デフォルトはnullptr。</param>
    HRESULT Initialize(IXAudio2* xAudio2, uint32_t inputChannels, float sampleRate, uint32_t processingStage = 0, SubmixVoice* sendTarget = nullptr);

    void Finalize();

    IXAudio2SubmixVoice* GetSubmixVoice() const { return submixVoice_; }

    XAUDIO2_VOICE_SENDS* GetSendList() { return &sendList_; }

    void SetVolume(float volume);
    float GetVolume() const { return volume_; }

    void SetFilter(XAUDIO2_FILTER_TYPE type, float cutoffHz, float oneOverQ = 1.0f);
    void ClearFilter();

    AudioEffectChain& GetEffectChain() { return effectChain_; }

private:

    XAUDIO2_SEND_DESCRIPTOR sendDesc_ = {};
    XAUDIO2_VOICE_SENDS sendList_ = {};

    IXAudio2SubmixVoice* submixVoice_ = nullptr;
    AudioEffectChain effectChain_ = {};
    float volume_ = 1.0f;
    float sampleRate_ = 44100.0f;
};

};// namespace ozSound