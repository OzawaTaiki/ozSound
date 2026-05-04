#include "AudioSystem.h"

#include "Logger/SoundLogger.h"
#include "SoundInstance.h"
#include "SoundEngine.h"

#include <chrono>

#define CHECK_HR(hr, msg)      if (FAILED(hr)) { ozSound::Log(msg); return nullptr; }
#define CHECK_HR_VOID(hr, msg) if (FAILED(hr)) { ozSound::Log(msg); return; }

namespace ozSound
{

AudioSystem* AudioSystem::GetInstance()
{
    static AudioSystem instance;
    return &instance;
}

void AudioSystem::Initialize()
{
    HRESULT hresult = S_FALSE;

    // エンジンのインスタンスを生成
    hresult = XAudio2Create(&xAudio2_, 0, XAUDIO2_DEFAULT_PROCESSOR);
    CHECK_HR_VOID(hresult, "Failed to create XAudio2 instance\n");

    // マスターボイスを生成
    hresult = xAudio2_->CreateMasteringVoice(&masterVoice_);
    CHECK_HR_VOID(hresult, "Failed to create mastering voice\n");

    XAUDIO2_VOICE_DETAILS masterDetails{};
    masterVoice_->GetVoiceDetails(&masterDetails);

    // Submix は LoadSubmixConfig() で生成する（呼ばれなかった場合のフォールバック）
    AddSubmix("BGM", 2, 48000.0f, 1);
    AddSubmix("SE", 2, 48000.0f, 1);

    hresult = MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET);
    CHECK_HR_VOID(hresult, "Failed to startup Media Foundation\n");
}

void AudioSystem::Finalize()
{
    for (auto& [name, sv] : namedSubmixes_)
        sv->Finalize();
    namedSubmixes_.clear();

    masterVoice_->DestroyVoice();
    masterVoice_ = nullptr;
    xAudio2_.Reset();

    sounds_.clear();
    pathToid_.clear();
    soundInstances_.clear();

    HRESULT hr = MFShutdown();
    CHECK_HR_VOID(hr, "Failed to shutdown Media Foundation\n");
}

std::shared_ptr<SoundInstance> AudioSystem::Load(const std::string& _filename)
{
    auto begin = std::chrono::system_clock::now();

    HRESULT hr = S_OK;

    ozSound::Log("Loading sound file: " + _filename +"\n");

    // ワイド文字列に変換
    std::wstring wFilename(_filename.begin(), _filename.end());

    // ソースリーダーの生成
    Microsoft::WRL::ComPtr<IMFSourceReader> pMFSourceReader{ nullptr };
    hr = MFCreateSourceReaderFromURL(wFilename.c_str(), NULL, &pMFSourceReader);
    CHECK_HR(hr, "Failed to create source reader from URL\n");

    // 出力メディアタイプの設定
    Microsoft::WRL::ComPtr<IMFMediaType> pMFMediaType{ nullptr };
    hr = MFCreateMediaType(&pMFMediaType);
    CHECK_HR(hr, "Failed to create media type\n");
    hr = pMFMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    CHECK_HR(hr, "Failed to set major type\n");
    hr = pMFMediaType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_Float);
    CHECK_HR(hr, "Failed to set subtype\n");
    hr = pMFSourceReader->SetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM), nullptr, pMFMediaType.Get());
    CHECK_HR(hr, "Failed to set current media type\n");

    // 現在のメディアタイプを取得
    hr = pMFSourceReader->GetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM), &pMFMediaType);
    CHECK_HR(hr, "Failed to get current media type\n");

    // WAVEFORMATEXの取得
    WAVEFORMATEX* waveFormat{ nullptr };
    hr = MFCreateWaveFormatExFromMFMediaType(pMFMediaType.Get(), &waveFormat, nullptr);
    CHECK_HR(hr, "Failed to create WAVEFORMATEX from media type\n");

    // メディアデータの読み込み
    std::vector<BYTE> mediaData;
    while (true)
    {
        Microsoft::WRL::ComPtr<IMFSample> pMFSample;
        DWORD dwStreamFlags{ 0 };

        hr = pMFSourceReader->ReadSample(
            static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM),
            0,
            nullptr,
            &dwStreamFlags,
            nullptr,
            &pMFSample);
        CHECK_HR(hr, "Failed to read sample\n");

        if (dwStreamFlags & MF_SOURCE_READERF_ENDOFSTREAM)
            break;

        Microsoft::WRL::ComPtr<IMFMediaBuffer> pMFMediaBuffer;
        hr = pMFSample->ConvertToContiguousBuffer(&pMFMediaBuffer);
        CHECK_HR(hr, "Failed to convert sample to contiguous buffer\n");

        BYTE* pBuffer{ nullptr };
        DWORD cbCurrentLength{ 0 };
        hr = pMFMediaBuffer->Lock(&pBuffer, nullptr, &cbCurrentLength);
        CHECK_HR(hr, "Failed to lock media buffer\n");

        mediaData.resize(mediaData.size() + cbCurrentLength);
        memcpy(mediaData.data() + mediaData.size() - cbCurrentLength, pBuffer, cbCurrentLength);

        pMFMediaBuffer->Unlock();
    }

    auto soundInstance = CreateSoundInstance(*waveFormat, std::move(mediaData), _filename);

    CoTaskMemFree(waveFormat);

    auto end = std::chrono::system_clock::now();
    ozSound::Log(std::format("Sound Load Time: {} ms\n", std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count()) + "\n");

    return soundInstance;
}

void AudioSystem::SetMasterVolume(float _volume)
{
    if (masterVoice_)
    {
        masterVoice_->SetVolume(_volume);
        masterVolume_ = _volume;
    }
}

// -----------------------------------------------------------------------
// Submix 動的管理
// -----------------------------------------------------------------------

void AudioSystem::LoadSubmixConfig(const std::string& jsonPath)
{
    json data = LoadJson(jsonPath);

    if (data.empty())
    {
        // ファイルが存在しない → デフォルト（BGM/SE）をそのまま使い自動保存
        SaveSubmixConfig(jsonPath);
        ozSound::Log("[AudioSystem] AudioConfig.json not found. Created default.\n");
        return;
    }

    // 既存 Submix をすべてクリアして再構築
    SoundEngine::GetInstance()->StopAll();
    for (auto& [name, sv] : namedSubmixes_)
        sv->Finalize();
    namedSubmixes_.clear();

    if (data.contains("masterVolume"))
        SetMasterVolume(data["masterVolume"].get<float>());

    if (data.contains("submixes"))
    {
        for (const auto& entry : data["submixes"])
        {
            std::string name  = entry.value("name", std::string(""));
            uint32_t    ch    = entry.value("channels", 2u);
            float       sr    = entry.value("sampleRate", 48000.0f);
            uint32_t    stage = entry.value("stage", 1u);
            float       vol   = entry.value("volume", 1.0f);

            if (name.empty()) continue;
            if (AddSubmix(name, ch, sr, stage))
                namedSubmixes_[name]->SetVolume(vol);
        }
    }

    // BGM / SE は必ず存在させる
    if (!namedSubmixes_.count("BGM")) AddSubmix("BGM", 2, 48000.0f, 1);
    if (!namedSubmixes_.count("SE"))  AddSubmix("SE", 2, 48000.0f, 1);
}

void AudioSystem::SaveSubmixConfig(const std::string& jsonPath)
{
    json data;
    data["masterVolume"] = masterVolume_;

    json submixArray = json::array();
    for (const auto& [name, sv] : namedSubmixes_)
    {
        json entry;
        entry["name"]       = name;
        entry["channels"]   = 2;
        entry["sampleRate"] = 48000.0f;
        entry["stage"]      = 1;
        entry["volume"]     = sv->GetVolume();
        submixArray.push_back(entry);
    }
    data["submixes"] = submixArray;

    SaveJson(jsonPath, data);
}

SubmixVoice* AudioSystem::GetSubmix(const std::string& name)
{
    auto it = namedSubmixes_.find(name);
    if (it == namedSubmixes_.end()) return nullptr;
    return it->second.get();
}

bool AudioSystem::AddSubmix(const std::string& name, uint32_t channels,
                            float sampleRate, uint32_t stage)
{
    if (namedSubmixes_.count(name)) return false;

    auto sv = std::make_unique<SubmixVoice>();
    HRESULT hr = sv->Initialize(xAudio2_.Get(), channels, sampleRate, stage);
    if (FAILED(hr))
    {
        ozSound::Log("[AudioSystem] Failed to create submix: " + name + "\n");
        return false;
    }

    namedSubmixes_.emplace(name, std::move(sv));
    ozSound::Log("[AudioSystem] Submix added: " + name + "\n");
    return true;
}

void AudioSystem::RemoveSubmix(const std::string& name)
{
    if (name == "BGM" || name == "SE") return;

    SoundEngine::GetInstance()->StopSoundsOnSubmix(name);

    auto it = namedSubmixes_.find(name);
    if (it == namedSubmixes_.end()) return;

    it->second->Finalize();
    namedSubmixes_.erase(it);
    ozSound::Log("[AudioSystem] Submix removed: " + name + "\n");
}

std::shared_ptr<SoundInstance> AudioSystem::CreateSoundInstance(const WAVEFORMATEX& _wfex, std::vector<BYTE> _mediaData, const std::string& _path)
{
    SoundData soundData{};
    soundData.wfex = _wfex;
    soundData.mediaData = std::move(_mediaData);
    soundData.path = _path;

    sounds_.push_back(std::move(soundData));

    uint32_t soundID = static_cast<uint32_t>(sounds_.size() - 1);
    pathToid_.emplace(_path, soundID);

    auto soundInstance = std::make_shared<SoundInstance>(soundID, this, static_cast<float>(_wfex.nSamplesPerSec));
    soundInstances_.emplace(soundID, soundInstance);

    ozSound::Log("Sound Loaded : " + _path + " (ID " + std::to_string(soundID) + ")\n");

    return soundInstance;
}

XAUDIO2_VOICE_DETAILS AudioSystem::GetMasterVoiceDetails()
{
    if (masterVoice_)
    {
        masterVoice_->GetVoiceDetails(&masterDetails_);
    }

    return masterDetails_;
}

AudioSystem::~AudioSystem()
{
    Finalize();
}

} // namespace ozSound
