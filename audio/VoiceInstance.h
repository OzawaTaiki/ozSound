#pragma once

#include <xaudio2.h>

#include "AudioEffect.h"

namespace ozSound
{

class VoiceInstance
{
public:
    VoiceInstance(IXAudio2SourceVoice* _sourceVoice, float _volume = 1.0f, float _sampleRate = 44100.0f, float _startTime = 0.0f);
    ~VoiceInstance();

    /// <summary>
    /// 始めから再生
    /// </summary>
    void Play();

    /// <summary>
    /// 再生を停止
    /// </summary>
    void Stop();

    /// <summary>
    /// 一時停止
    /// </summary>
    void Pause();

    /// <summary>
    /// 一時停止から再開
    /// あるいは始めから再生
    /// </summary>
    void Resume();

    /// <summary>
    /// 無音から本来の音量まで上げる。
    /// 呼んだ時点で即座に無音になり、以降 UpdateFade() を回すぶんだけ上がっていく。
    /// _fadeTime <= 0 なら即座に本来の音量へ。
    /// </summary>
    /// <param name="_fadeTime">フェードインにかかる時間(秒)</param>
    void FadeIn(float _fadeTime);

    /// <summary>
    /// 現在の音量から無音まで下げ、下がり切ったら Stop() する。
    /// _fadeTime <= 0 なら即座に停止する。
    /// </summary>
    /// <param name="_fadeTime">フェードアウトにかかる時間(秒)</param>
    void FadeOut(float _fadeTime);

    /// <summary>
    /// フェードを 1 フレーム進める。SoundEngine::Update() が毎フレーム呼ぶ。
    /// フェード中でなければ何もしない。
    /// </summary>
    /// <param name="_deltaTime">前フレームからの経過時間(秒)</param>
    void UpdateFade(float _deltaTime);

    /// <summary>フェード中か</summary>
    bool IsFading() const { return isFading_; }

    /// <summary>
    /// 音量を設定
    /// </summary>
    /// <param name="_volume">0.0f から 1.0f の範囲で設定</param>
    void SetVolume(float _volume);

    /// <summary>
    /// 音量を取得
    /// </summary>
    /// <returns>音量</returns>
    float GetVolume() const { return volume_; }

    /// <summary>
    /// 再生中かどうかを確認
    /// </summary>
    /// <returns>再生中なら true</returns>
    bool IsPlaying() const;

    /// <summary>
    /// 一時停止中かどうかを確認
    /// </summary>
    /// <returns>一時停止中なら true</returns>
    bool IsPaused() const { return isPaused_; }

    /// <summary>
    /// 経過時間を取得
    /// </summary>
    /// <returns>経過時間(秒)</returns>
    float GetElapsedTime() const;

    /// <summary>
    /// 再生開始時間を取得
    /// </summary>
    /// <returns>再生開始時間(秒)</returns>
    float GetStartTime() const { return startTime_; }


    /// <summary>
    /// 再生速度を設定
    /// </summary>
    /// <param name="_speed">1.0f で通常速度</param>
    void SetPlaySpeed(float _speed);

    /// <summary>
    /// 再生速度を取得
    /// </summary>
    /// <returns>再生速度</returns>
    float GetPlaySpeed() const { return playSpeed_; }

    /// <summary>
    /// 音声ソースボイスを取得
    /// </summary>
    IXAudio2SourceVoice* GetSourceVoice() const { return sourceVoice_; }

    /// <summary>
    /// エフェクトチェーンの所有権を受け取り、このボイスにアタッチする。
    /// CreateSourceVoice 時にチェーンを渡しただけだと、後から
    /// EnableEffect / SetEffectParameters を呼ぶ手段が無くなるため、
    /// チェーンはボイスと同じ寿命でここに保持する。
    /// </summary>
    void SetEffectChain(AudioEffectChain&& _chain);

    /// <summary>
    /// このボイスに適用されているエフェクトチェーン。
    /// エフェクト無しで生成された場合は空のチェーンが返る (IsEmpty() が true)。
    /// </summary>
    AudioEffectChain& GetEffectChain() { return effectChain_; }
    const AudioEffectChain& GetEffectChain() const { return effectChain_; }

public:// エフェクトチェーンを所有するためコピー不可 (ムーブも shared_ptr 前提で行わない)
    VoiceInstance(const VoiceInstance&) = delete;
    VoiceInstance& operator=(const VoiceInstance&) = delete;

private:
    /// <summary>
    /// HRESULTをチェックし、エラーがあれば例外を投げる
    /// </summary>
    void CheckHRESULT() const ;

    /// <summary>
    /// 実際にボイスへ流す音量 (volume_ * fadeGain_) を適用する。
    /// 音量指定とフェードを独立して扱うため、必ずこの掛け算を通す。
    /// </summary>
    void ApplyVolume();

    // 一時停止
    bool isPaused_ = false;

    // 音量。フェードとは独立した「本来の音量」で、SetVolume() が書き換える
    float volume_ = 1.0f;

    // ── フェード ──────────────────────────────────────────────
    // 音量に掛ける係数 0..1。実際にボイスへ渡すのは volume_ * fadeGain_。
    // こうしておくとフェード中に SetVolume() されても破綻しない
    float fadeGain_ = 1.0f;

    bool  isFading_ = false;
    bool  stopOnFadeEnd_ = false;   // フェードアウトなら下がり切ったところで Stop する
    float fadeElapsed_ = 0.0f;
    float fadeDuration_ = 0.0f;
    float fadeGainFrom_ = 0.0f;
    float fadeGainTo_ = 1.0f;

    // 再生開始時間
    float startTime_ = 0.0f;

    // サンプルレート
    float sampleRate_ = 44100.0f;

    float playSpeed_ = 1.0f; // 再生速度

    // 音声ソースボイス
    IXAudio2SourceVoice* sourceVoice_ = nullptr;

    // このボイスに適用されているエフェクトチェーン (所有)
    AudioEffectChain effectChain_ = {};


    HRESULT hr_ = S_OK;

};

} // namespace ozSound
