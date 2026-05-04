#include "Audio.h"
#include <cassert>


namespace Engine {

Audio* Audio::GetInstance()
{
    static Audio instance;
    return &instance;
}

Audio::~Audio()
{
    xAudio2_.Reset();

    for (auto sound : sounds_)
        SoundUnLoad(&sound);
    sounds_.clear();
}

void Audio::Initialize()
{
    HRESULT hresult = S_FALSE;

    // エンジンのインスタンスを生成
    hresult = XAudio2Create(&xAudio2_, 0, XAUDIO2_DEFAULT_PROCESSOR);

    // マスターボイスを生成
    hresult = xAudio2_->CreateMasteringVoice(&masterVoice_);
}

uint32_t Audio::SoundLoadWave(const std::string& _filename)
{
    std::ifstream file;
    file.open(_filename, std::ios_base::binary);
    if (!file.is_open())
    {
        throw std::runtime_error("Error: File not found - " + _filename);
    }

    // RIFFヘッダーの読み込み
    RiffHeader riff;
    file.read((char*)&riff, sizeof(riff));
    if (strncmp(riff.chunk.id, "RIFF", 4) != 0)
    {
        throw std::runtime_error("Error: Not a RIFF file - " + _filename);
    }
    if (strncmp(riff.type, "WAVE", 4) != 0)
    {
        throw std::runtime_error("Error: Not a WAVE file - " + _filename);
    }

    // フォーマットチャンクとデータチャンクを見つけるまで繰り返し
    FormatChunk format = {};
    ChunkHeader data = {};
    bool foundFormat = false;
    bool foundData = false;

    while (!foundFormat || !foundData)
    {
        ChunkHeader header;
        file.read((char*)&header, sizeof(ChunkHeader));

        // ファイル終端チェック
        if (file.eof())
        {
            throw std::runtime_error("Error: Unexpected end of file - " + _filename);
        }

        // フォーマットチャンク
        if (strncmp(header.id, "fmt ", 4) == 0)
        {
            format.chunk = header;
            // フォーマットデータのサイズチェック
            if (header.size > sizeof(format.fmt))
            {
                // 追加データがある場合は一時バッファへ読み込む
                std::vector<char> tempBuffer(header.size);
                file.read(tempBuffer.data(), header.size);
                memcpy(&format.fmt, tempBuffer.data(), sizeof(format.fmt));
            }
            else
            {
                file.read((char*)&format.fmt, header.size);
            }
            foundFormat = true;
        }
        // データチャンク
        else if (strncmp(header.id, "data", 4) == 0)
        {
            data = header;
            foundData = true;
            break; // データチャンクが見つかったら終了
        }
        // その他のチャンク（スキップ）
        else
        {
            file.seekg(header.size, std::ios_base::cur);
        }
    }

    if (!foundFormat)
    {
        throw std::runtime_error("Error: No format chunk found - " + _filename);
    }

    if (!foundData)
    {
        throw std::runtime_error("Error: No data chunk found - " + _filename);
    }

    // データの読み込み
    char* pbuffer = new char[data.size];
    file.read(pbuffer, data.size);
    file.close();

    SoundData soundData = {};
    soundData.bufferSize = data.size;
    soundData.pBuffer = reinterpret_cast<BYTE*>(pbuffer);
    soundData.wfex = format.fmt;
    sounds_.push_back(soundData);

    return static_cast<uint32_t>(sounds_.size() - 1);
}


void Audio::SoundUnLoad(SoundData* _soundData)
{
    delete[] _soundData->pBuffer;

    _soundData->pBuffer = 0;
    _soundData->bufferSize = 0;
    _soundData->wfex = {};
}

uint32_t Audio::SoundPlay(uint32_t _soundHandle, float _volume, bool _loop, bool _enableOverlap)
{
    // 重複再生が無効なら
    if (!_enableOverlap)
    {
        // 再生フラグを取得
        uint32_t voicehandle = map_[_soundHandle];
        if (IsPlaying(voicehandle))
        {// 再生されていたらリターン
            return voicehandle;
        }
    }

    HRESULT hresult = S_FALSE;

    SoundData data = sounds_[_soundHandle];

    IXAudio2SourceVoice* pSourceVoice = nullptr;
    hresult = xAudio2_->CreateSourceVoice(&pSourceVoice, &data.wfex);
    assert(SUCCEEDED(hresult));


    XAUDIO2_BUFFER buf{};
    buf.pAudioData = data.pBuffer;
    buf.AudioBytes = data.bufferSize;
    buf.Flags = XAUDIO2_END_OF_STREAM;

    if (_loop)
        buf.LoopCount = XAUDIO2_LOOP_INFINITE;

    hresult = pSourceVoice->SubmitSourceBuffer(&buf);
    assert(SUCCEEDED(hresult));

    hresult = pSourceVoice->Start();
    assert(SUCCEEDED(hresult));

    pSourceVoice->SetVolume(_volume);

    uint32_t index = static_cast<uint32_t>(sourceVoice_.size() - 1);
    map_[_soundHandle] = index;
    sourceVoice_[index] = pSourceVoice;

    return index;
}

bool Audio::IsPlaying(uint32_t _voiceHandle)
{
    if (sourceVoice_.size()==0||
        sourceVoice_.size() < _voiceHandle)
        return false;

    XAUDIO2_VOICE_STATE state;
    sourceVoice_[_voiceHandle]->GetState(&state);

    return state.BuffersQueued > 0;
}

void Audio::SetVolume(uint32_t _voiceHandle, float _volume)
{
    sourceVoice_[_voiceHandle]->SetVolume(_volume);
}

void Audio::SoundStop(uint32_t _voiceHandle)
{
    sourceVoice_[_voiceHandle]->Stop();
}

} // namespace Engine
