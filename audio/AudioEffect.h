#pragma once

#include <xaudio2.h>
#include <wrl.h>

#include <cstdint>
#include <vector>

namespace ozSound
{

class AudioEffect
{
public:
    AudioEffect()=default;
    AudioEffect(IUnknown* effect, uint32_t outputChannel, bool initialState = false);

    IUnknown* GetEffect() const { return effect_.Get(); }
    uint32_t GetOutputChannel() const { return outputChannel_; }
    bool IsInitiallyEnabled() const { return initialState_; }


public:// ムーブコンストラクタとムーブ代入演算子をデフォルトで定義
    AudioEffect(AudioEffect&&) = default;
    AudioEffect& operator=(AudioEffect&&) = default;
    AudioEffect(const AudioEffect&) = delete;
    AudioEffect& operator=(const AudioEffect&) = delete;
private:
    //
    Microsoft::WRL::ComPtr<IUnknown> effect_;

    // エフェクトの出力先チャンネル
    uint32_t outputChannel_;

    // エフェクトの初期状態（有効/無効）
    bool initialState_ = false;

};

class AudioEffectChain
{
public:

    AudioEffectChain() = default;
    ~AudioEffectChain() = default;

    void AddEffect(AudioEffect&& _effect);

    // CreateSourceVoice等に渡す構造体を生成
    // エフェクトが0個のときはnullptrを返す
    const XAUDIO2_EFFECT_CHAIN* BuildChain();

    // Voice生成後に呼ぶ
    void AttachToVoice(IXAudio2Voice* _voice);
    void DetachFromVoice();

    HRESULT ApplyChain();

    // ランタイム制御（AttachToVoice後に有効）
    HRESULT EnableEffect(uint32_t _index, uint32_t _operationSet = XAUDIO2_COMMIT_NOW);
    HRESULT DisableEffect(uint32_t _index, uint32_t _operationSet = XAUDIO2_COMMIT_NOW);
    HRESULT SetEffectParameters(uint32_t _index, const void* _pParams, uint32_t _byteSize, uint32_t _operationSet = XAUDIO2_COMMIT_NOW);
    HRESULT GetEffectParameters(uint32_t _index, void* _pParams, uint32_t _byteSize);

    bool IsEmpty() const { return effects_.empty(); }

public:// コピー不可 move可能
    AudioEffectChain(const AudioEffectChain&) = delete;
    AudioEffectChain& operator=(const AudioEffectChain&) = delete;
    AudioEffectChain(AudioEffectChain&&) = default;
    AudioEffectChain& operator=(AudioEffectChain&&) = default;

private:
    std::vector<AudioEffect> effects_;

    // BuildChain()の結果を保持（ポインタ生存期間のため）
    std::vector<XAUDIO2_EFFECT_DESCRIPTOR> descriptors_;
    XAUDIO2_EFFECT_CHAIN chain_{};

    IXAudio2Voice* attachedVoice_ = nullptr;

};

}// namespace ozSound