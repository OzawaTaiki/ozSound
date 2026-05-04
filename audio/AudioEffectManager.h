#pragma once

#include "AudioEffectDef.h"
#include "VST3/VST3ParameterManager.h"

#include <xaudio2.h>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>
#include <map>
#include <memory>

namespace ozSound
{

class VST3Module;
class VST3Plugin;

class AudioEffectChain;

/// <summary>
/// オーディオエフェクトの定義管理・生成・VST3モジュールキャッシュを担うクラス。
/// </summary>
class AudioEffectManager
{
public:

    static AudioEffectManager* GetInstance();

    void Initialize();
    void Finalize();

    /// <summary>
    /// JSON からエフェクト定義を読み込む。
    /// </summary>
    void LoadEffectData(const std::string& jsonPath);

    /// <summary>
    /// Native エフェクトをファクトリーに登録する。
    /// </summary>
    void RegisterNativeEffect(const std::string& name,
                              std::function<IUnknown* ()> creator);

    /// <summary>
    /// エフェクトIDリストから XAUDIO2_EFFECT_CHAIN を構築して返す。
    /// SoundEngine::Play() 内部で使用。
    /// </summary>
    /// <param name="effectIds">エフェクトIDのリスト</param>
    ozSound::AudioEffectChain BuildEffectChain(const std::vector<std::string>& effectNames);

    /// <summary>
    /// VST3 プラグインのパラメーターマネージャーを取得する。
    /// </summary>
    VST3ParameterManager* GetParameterManager(const std::string& effectName);


private:

    // VST3 プラグイン1つ分のエントリ
    struct VST3PluginEntry
    {
        VST3ParameterManager paramMgr;
        std::unique_ptr<VST3Plugin> plugin   = nullptr;
    };

    // VST3 モジュール（DLL）1つ分のエントリ
    struct VST3ModuleEntry
    {
        VST3Module* module = nullptr;
        std::map<std::string, std::unique_ptr<VST3PluginEntry>> plugins; // プラグイン名 → エントリ
    };

    VST3PluginEntry* GetVST3PluginEntry(const std::string& effectName);

private:

    // エフェクト定義（JSON）
    std::unordered_map<std::string, AudioEffectDef>              effectDefs_;
    // VST3 モジュールキャッシュ（パス → エントリ）
    std::unordered_map<std::string, VST3ModuleEntry>             loadedModules_;
    // Native エフェクトファクトリー（名前 → 生成関数）
    std::unordered_map<std::string, std::function<IUnknown* ()>>  nativeFactories_;

private:
    AudioEffectManager()  = default;
    ~AudioEffectManager() = default;
    AudioEffectManager(const AudioEffectManager&)            = delete;
    AudioEffectManager& operator=(const AudioEffectManager&) = delete;
};

} // namespace ozSound
