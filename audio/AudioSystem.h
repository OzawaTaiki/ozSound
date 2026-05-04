#pragma once


#include <fstream>
#include <array>
#include <string>
#include <cstdint>
#include <vector>
#include <map>
#include <memory>
#include <wrl.h>

#include <xaudio2.h>
#include "SubmixVoice.h"

#pragma comment (lib,"xaudio2.lib")

#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>

#pragma comment(lib, "Mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "Mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")



namespace ozSound
{

class SoundInstance;

class AudioSystem
{
public:

    static AudioSystem* GetInstance();

    void Initialize();
    void Finalize();

    std::shared_ptr<SoundInstance> Load(const std::string& _filename);

    Microsoft::WRL::ComPtr<IXAudio2> GetXAudio2() { return xAudio2_; }

    void SetMasterVolume(float _volume);
    float GetMasterVolume() const { return masterVolume_; }

    // 後方互換 accessor
    SubmixVoice* GetBGMSubmix() { return GetSubmix("BGM"); }
    SubmixVoice* GetSESubmix() { return GetSubmix("SE"); }

    // --- Submix 動的管理 ---

    /// <summary>
    /// JSON ファイルからサブミックス定義を読み込み構築する。
    /// ファイルが存在しない場合はデフォルト（BGM/SE）を生成してファイルに保存する。
    /// </summary>
    void LoadSubmixConfig(const std::string& jsonPath);

    /// <summary>
    /// 現在のサブミックス設定を JSON ファイルに保存する。
    /// </summary>
    void SaveSubmixConfig(const std::string& jsonPath);

    /// <summary>
    /// 名前でサブミックスを取得する。存在しない場合は nullptr を返す。
    /// </summary>
    SubmixVoice* GetSubmix(const std::string& name);

    /// <summary>
    /// 新しいサブミックスを追加する。既に同名が存在する場合は false を返す。
    /// </summary>
    bool AddSubmix(const std::string& name,
                   uint32_t channels   = 2,
                   float sampleRate    = 48000.0f,
                   uint32_t stage      = 1);

    /// <summary>
    /// サブミックスを削除する。BGM / SE は削除不可。
    /// 削除前に接続中の Voice を SoundEngine 経由で停止する。
    /// </summary>
    void RemoveSubmix(const std::string& name);

    /// <summary>
    /// 全サブミックスを取得する（Mixer UI 等での列挙用）。
    /// </summary>
    const std::map<std::string, std::unique_ptr<SubmixVoice>>& GetAllSubmixes() const { return namedSubmixes_; }

    const WAVEFORMATEX& GetSoundFormat(uint32_t _soundID) const { return sounds_[_soundID].wfex; }
    const BYTE* GetBuffer(uint32_t _soundID) const { return sounds_[_soundID].mediaData.data(); }
    size_t GetBufferSize(uint32_t _soundID) const { return sounds_[_soundID].mediaData.size(); }

    XAUDIO2_VOICE_DETAILS GetMasterVoiceDetails();

private:

    struct SoundData
    {
        WAVEFORMATEX wfex;
        std::vector<BYTE> mediaData;
        std::string path;
    };

    std::shared_ptr<SoundInstance> CreateSoundInstance(const WAVEFORMATEX& _wfex, std::vector<BYTE> _mediaData, const std::string& _path);

private:

    Microsoft::WRL::ComPtr<IXAudio2> xAudio2_;
    IXAudio2MasteringVoice* masterVoice_;

    XAUDIO2_VOICE_DETAILS masterDetails_{};


    std::vector<SoundData> sounds_;
    std::map<std::string, uint32_t> pathToid_;

    std::map<uint32_t, std::weak_ptr<SoundInstance>> soundInstances_;

    float masterVolume_ = 1.0f;

    std::map<std::string, std::unique_ptr<SubmixVoice>> namedSubmixes_;


private:
    // 構造体たち
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

private:

    AudioSystem() = default;
    ~AudioSystem();
    AudioSystem(const AudioSystem&) = delete;
    AudioSystem& operator=(const AudioSystem&) = delete;
};

} // namespace ozSound
