#pragma once

#include <fstream>
#include <array>
#include <cstdint>
#include <vector>
#include <map>
#include <wrl.h>

#include <xaudio2.h>

#pragma comment (lib,"xaudio2.lib")




namespace Engine {

class Audio
{

public:
    static Audio* GetInstance();

private:

    struct ChunkHeader
    {
        char id[4];             // チャンク毎のID
        int32_t size;           // チャンクサイズ
    };

    struct RiffHeader
    {
        ChunkHeader chunk;      // "RIFF"
        char type[4];             // "WAVE"
    };

    struct FormatChunk
    {
        ChunkHeader chunk;      // ”fmt"
        WAVEFORMATEX fmt;       // 波形フォーマット
    };

    struct SoundData
    {
        WAVEFORMATEX wfex;
        BYTE* pBuffer;
        unsigned int bufferSize;
    };
public:

    ~Audio();

    void Initialize();

    uint32_t SoundLoadWave(const std::string& _filename);
    uint32_t SoundPlay(uint32_t _soundHandle, float _volume, bool _loop = false, bool _enableOverlap = true);
    bool IsPlaying(uint32_t _voiceHandle);
    void SetVolume(uint32_t _voiceHandle, float _volume);
    void SoundStop(uint32_t _voiceHandle);

    void SoundUnLoad(SoundData* _soundData);

private:

    Microsoft::WRL::ComPtr<IXAudio2> xAudio2_;
    IXAudio2MasteringVoice* masterVoice_;

    std::map <uint32_t, uint32_t> map_;

    std::vector<SoundData> sounds_;
    std::map<uint32_t, IXAudio2SourceVoice*> sourceVoice_;
};

} // namespace Engine
