#include "SoundInstance.h"

#include "AudioSystem.h"
#include "SubmixVoice.h"

#include "Logger/SoundLogger.h"

#include <cassert>
#include <utility>


namespace ozSound
{

SoundInstance::SoundInstance(uint32_t _soundID, AudioSystem* _audioSystem,float _sampleRate) :
    soundID_(_soundID),
    audioSystem_(_audioSystem),
    sampleRate_(_sampleRate)
{
}

SoundInstance::~SoundInstance()
{
}

std::shared_ptr<VoiceInstance> SoundInstance::GenerateVoiceInstance(float _volume, float _startTime, bool _loop, bool _enableOverlap, VoiceCallBack* _callback, SubmixVoice* _submix, AudioEffectChain _effectChain, float _duration)
{
    if (!_enableOverlap)
    {
        // 既存の再生中ボイスをすべて停止してから新規生成する
        for (auto& weak : voiceInstance_)
        {
            if (auto voice = weak.lock())
            {
                if (voice->IsPlaying())
                    voice->Stop();
            }
        }
        voiceInstance_.clear();
    }

    HRESULT hresult = S_FALSE;

    IXAudio2* xAudio2 = audioSystem_->GetXAudio2().Get();

    XAUDIO2_VOICE_SENDS* sendList = nullptr;
    if (_submix)
    {
        sendList = _submix->GetSendList();
    }

    // 空チェーンなら BuildChain() は nullptr を返す (= エフェクト無しで生成)
    const XAUDIO2_EFFECT_CHAIN* chainDesc = _effectChain.BuildChain();

    IXAudio2SourceVoice* pSourceVoice = nullptr;
    hresult = xAudio2->CreateSourceVoice(
        &pSourceVoice, // Source voice
        &audioSystem_->GetSoundFormat(soundID_), // Wave format
        0, // Flags
        XAUDIO2_DEFAULT_FREQ_RATIO, // Frequency ratio
        _callback,// コールバック関数
        sendList, // Send list
        chainDesc // Effect chain
    );

    if (FAILED(hresult))
    {
        const WAVEFORMATEX& wfex = audioSystem_->GetSoundFormat(soundID_);
        ozSound::Log(std::format(
            "Error: Failed to create source voice (hr=0x{:08X}, tag={}, ch={}, rate={}, bits={}, cbSize={})\n",
            static_cast<uint32_t>(hresult), wfex.wFormatTag, wfex.nChannels,
            wfex.nSamplesPerSec, wfex.wBitsPerSample, wfex.cbSize));
        return nullptr;
    }

    // 鳴らす範囲をサンプル単位で決める。
    // PlayBegin = 開始位置、PlayLength = 鳴らす長さで、PlayLength = 0 が
    // 「末尾まで」を意味するのが XAudio2 の規約。
    // 音源の実サンプル数を超える範囲を渡すと SubmitSourceBuffer が失敗するので、
    // どちらも実データの内側へ丸めてから積む。
    const UINT32 audioBytes = static_cast<UINT32>(audioSystem_->GetBufferSize(soundID_));
    const WAVEFORMATEX& format = audioSystem_->GetSoundFormat(soundID_);
    const UINT32 blockAlign = (format.nBlockAlign > 0) ? format.nBlockAlign : 1;
    const UINT32 totalSamples = audioBytes / blockAlign;

    UINT32 startSample = static_cast<UINT32>(_startTime * sampleRate_);
    if (startSample >= totalSamples)
        startSample = 0;   // 開始位置が尺を超えている指定は頭から鳴らす

    UINT32 playSamples = 0;   // 0 = 末尾まで
    if (_duration > 0.0f)
    {
        playSamples = static_cast<UINT32>(_duration * sampleRate_);
        if (playSamples >= totalSamples - startSample)
            playSamples = 0;   // 残り尺いっぱいなら「末尾まで」と同じ
    }

    XAUDIO2_BUFFER buf{};
    buf.pAudioData = audioSystem_->GetBuffer(soundID_);
    buf.AudioBytes = audioBytes;
    buf.PlayBegin = startSample;
    buf.PlayLength = playSamples;
    buf.Flags = XAUDIO2_END_OF_STREAM;

    if (_loop)
        buf.LoopCount = XAUDIO2_LOOP_INFINITE;

    hresult = pSourceVoice->SubmitSourceBuffer(&buf);
    assert(SUCCEEDED(hresult));

    pSourceVoice->SetVolume(_volume);

    auto voiceInstance = std::make_shared<VoiceInstance>(pSourceVoice, _volume, sampleRate_, _startTime);

    // チェーンをボイスに預ける。以降 Enable/Disable はここ経由で行える。
    voiceInstance->SetEffectChain(std::move(_effectChain));

    voiceInstance_.push_back(voiceInstance);

    return voiceInstance;
}

std::shared_ptr<VoiceInstance> SoundInstance::Play(float _volume, bool _loop, bool _enableOverlap, VoiceCallBack* _callback, SubmixVoice* _submix)
{
    return Play(_volume, 0.0f, _loop, _enableOverlap, _callback, _submix);
}

std::shared_ptr<VoiceInstance> SoundInstance::Play(float _volume, float _startTime, bool _loop, bool _enableOverlap, VoiceCallBack* _callback, SubmixVoice* _submix, float _duration)
{
    auto voiceInstance = GenerateVoiceInstance(_volume, _startTime, _loop, _enableOverlap, _callback, _submix, {}, _duration);
    if (voiceInstance)
    {
        voiceInstance->Play();
        return voiceInstance;
    }
    else
    {
        ozSound::Log("Error: Failed to play sound instance with start time\n");
        return nullptr;
    }
}

std::vector<float> SoundInstance::GetAudioData() const
{
    const BYTE* data= audioSystem_->GetBuffer(soundID_);
    const float* floatData = reinterpret_cast<const float*>(data);

    std::vector<float> audioData;
    size_t dataSize = audioSystem_->GetBufferSize(soundID_);
    audioData.assign(floatData, floatData + dataSize / sizeof(float));
    return audioData;
}
//
//std::vector<float> SoundInstance::GetWaveform() const
//{
//    const WAVEFORMATEX& format = audioSystem_->GetSoundFormat(soundID_);
//    const BYTE* buffer = audioSystem_->GetBuffer(soundID_);
//    size_t bufferSize = audioSystem_->GetBufferSize(soundID_);
//
//    return ConvertToFloatSamples(buffer, bufferSize, format);
//}
//
//std::vector<float> SoundInstance::GetWaveform(float _startTime, float _endTime) const
//{
//    const WAVEFORMATEX& format = audioSystem_->GetSoundFormat(soundID_);
//    BYTE* buffer = audioSystem_->GetBuffer(soundID_);
//    unsigned int bufferSize = audioSystem_->GetBufferSize(soundID_);
//
//    // 秒をサンプル数に変換
//    unsigned int startSample = static_cast<unsigned int>(_startTime * format.nSamplesPerSec);
//    unsigned int endSample = static_cast<unsigned int>(_endTime * format.nSamplesPerSec);
//
//    return ConvertToFloatSamples(buffer, bufferSize, format, _startTime, _endTime - _startTime);
//}

float SoundInstance::GetDuration() const
{
    if (audioSystem_)
    {
        size_t bufSize = audioSystem_->GetBufferSize(soundID_);
        auto& wfex = audioSystem_->GetSoundFormat(soundID_);

        if (wfex.nAvgBytesPerSec > 0)
        {
            return static_cast<float>(bufSize) / wfex.nAvgBytesPerSec;
        }
    }
    return 0.0f;
}

} // namespace ozSound
