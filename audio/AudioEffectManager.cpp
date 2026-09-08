#include "AudioEffectManager.h"

#include "Logger/SoundLogger.h"
#include "AudioEffectDef.h"

#include "VST3/VST3Host.h"
#include "VST3/VST3Module.h"
#include "VST3/VST3Plugin.h"
#include "VST3/VST3Effect.h"
#include "VST3/VST3ParameterManager.h"
#include "AudioEffect.h"

namespace ozSound
{

AudioEffectManager* AudioEffectManager::GetInstance()
{
    static AudioEffectManager instance;
    return &instance;
}

void AudioEffectManager::Initialize()
{
    effectDefs_.clear();
    loadedModules_.clear();
    nativeFactories_.clear();
}

void AudioEffectManager::Finalize()
{
    for (auto& modulePair : loadedModules_)
    {
        auto& plugins = modulePair.second.plugins;
        for (auto& pluginPair : plugins)
        {
            pluginPair.second->plugin->Terminate();
        }
    }
}

void AudioEffectManager::LoadEffectData(const std::string& jsonPath)
{
    json jsonData = LoadJson(jsonPath);
    if (jsonData.empty())
    {
        ozSound::Log("Failed to load effect data from: " + jsonPath + "\n");
        // ファイルがない場合は空の定義で初期化して保存する（サウンドエディタでの編集開始を想定）

        // サンプルエフェクト定義
        AudioEffectDef sampleEffect;
        sampleEffect.name = "sample_effect";
        sampleEffect.type = AudioEffectType::VST3;
        sampleEffect.path = "Resources/Plugins/SamplePlugin.vst3"; // 存在しないパスを指定しておく
        sampleEffect.className = ""; // クラス名は空で、ロード時に最初のクラスを使用する

        json sampleEffectJson = sampleEffect; // to_json(AudioEffectDef) を使用して JSON オブジェクトに変換

        SaveJson(jsonPath, json{ {"effects", json::array({ sampleEffectJson })} });

        ozSound::Log("Created new effect data file at: " + jsonPath + "\n");
        return;
    }

    LoadEffectDataFromJson(jsonData);
}

void AudioEffectManager::LoadEffectDataFromJson(const json& jsonData)
{
    if (jsonData.empty())            return;
    if (!jsonData.contains("effects")) return;

    for (const auto& entry : jsonData["effects"])
    {
        RegisterEffectDef(entry.get<AudioEffectDef>()); // from_json(AudioEffectDef) を使用
    }
}

void AudioEffectManager::LoadEffectDefs(const std::vector<AudioEffectDef>& defs)
{
    for (const auto& def : defs)
    {
        if (def.name.empty())
            continue;

        // 同じ内容で登録済みなら何もしない (毎フレーム呼ばれても再ロードしない)
        auto it = effectDefs_.find(def.name);
        if (it != effectDefs_.end() &&
            it->second.type      == def.type &&
            it->second.path      == def.path &&
            (it->second.className == def.className || def.className.empty()))
        {
            continue;
        }

        RegisterEffectDef(def);
    }
}

bool AudioEffectManager::HasEffect(const std::string& effectName) const
{
    return effectDefs_.find(effectName) != effectDefs_.end();
}

void AudioEffectManager::RegisterEffectDef(const AudioEffectDef& _def)
{
    AudioEffectDef def = _def;

    // Native はファクトリー登録側で解決するので、定義を持つだけでよい
    if (def.type != AudioEffectType::VST3)
    {
        effectDefs_[def.name] = def;
        return;
    }

    if (def.path.empty())
    {
        ozSound::Log("[AudioEffectManager] VST3 path is empty for effect: " + def.name + "\n");
        effectDefs_[def.name] = def;
        return;
    }

    auto host = VST3Host::GetInstance();
    auto module = host->LoadModule(def.path);
    if (!module)
    {
        ozSound::Log("Failed to load VST3 module for effect: " + def.name + "\n");
        effectDefs_[def.name] = def;   // 定義だけは残す (エディタで直せるように)
        return;
    }

    auto classes = module->GetAudioEffectClasses();
    if (classes.empty())
    {
        ozSound::Log("[AudioEffectManager] No audio effect class in module: " + def.path + "\n");
        effectDefs_[def.name] = def;
        return;
    }

    if (def.className.empty())
    {
        def.className = classes[0].name(); // クラス名が指定されていない場合は最初のクラスを使用
    }

    for (const auto& cls : classes)
    {
        if (cls.name() != def.className)
            continue;

        VST3PluginEntry pluginEntry;
        pluginEntry.plugin = module->CreatePlugin(cls);
        // TODO : プラグインの初期化パラメーターは要検討。とりあえず固定値で入れてみる
        pluginEntry.plugin->Initialize(module->GetFactory(),
                                       host->GetHostApp(),
                                       48000.0f,
                                       4096,
                                       2,
                                       2);
        pluginEntry.paramMgr.Initialize(pluginEntry.plugin->GetController());

        VST3ModuleEntry& moduleEntry = loadedModules_[def.path];
        moduleEntry.module = module;
        moduleEntry.plugins[def.name] = std::make_unique<VST3PluginEntry>(std::move(pluginEntry));

        break;
    }

    // 解決後の className を保存しておく (エディタ側の表示・再ロード判定に使う)
    effectDefs_[def.name] = def;
}

bool AudioEffectManager::SetEffectParameterByName(const std::string& effectName,
                                                  const std::string& paramName,
                                                  double normalizedValue)
{
    VST3ParameterManager* paramMgr = GetParameterManager(effectName);
    if (!paramMgr)
        return false;

    const int32_t index = paramMgr->FindParameterIndex(paramName);
    if (index < 0)
        return false;

    paramMgr->SetParameter(static_cast<Steinberg::Vst::ParamID>(index), normalizedValue);
    return true;
}

bool AudioEffectManager::GetEffectParameterByName(const std::string& effectName,
                                                  const std::string& paramName,
                                                  double* outNormalizedValue)
{
    if (!outNormalizedValue)
        return false;

    VST3ParameterManager* paramMgr = GetParameterManager(effectName);
    if (!paramMgr)
        return false;

    const int32_t index = paramMgr->FindParameterIndex(paramName);
    if (index < 0)
        return false;

    *outNormalizedValue = paramMgr->GetParameter(static_cast<Steinberg::Vst::ParamID>(index));
    return true;
}

std::vector<std::string> AudioEffectManager::GetModuleClassNames(const std::string& vst3Path)
{
    std::vector<std::string> names;
    if (vst3Path.empty())
        return names;

    // VST3Host 側でパス単位にキャッシュされるので、同じパスの再呼び出しは軽い
    auto module = VST3Host::GetInstance()->LoadModule(vst3Path);
    if (!module)
        return names;

    for (const auto& cls : module->GetAudioEffectClasses())
        names.push_back(cls.name());

    return names;
}

std::vector<std::string> AudioEffectManager::GetParameterNames(const std::string& effectName)
{
    std::vector<std::string> names;

    VST3ParameterManager* paramMgr = GetParameterManager(effectName);
    if (!paramMgr)
        return names;   // Native / 未ロード

    const int32_t count = paramMgr->GetParameterCount();
    names.reserve(static_cast<size_t>(count > 0 ? count : 0));
    for (int32_t i = 0; i < count; ++i)
    {
        std::string name = paramMgr->GetParameterName(i);
        if (!name.empty())
            names.push_back(std::move(name));
    }
    return names;
}

void AudioEffectManager::RegisterNativeEffect(const std::string& name, std::function<IUnknown* ()> creator)
{
    if (!creator)
    {
        ozSound::Log("Create Function is null for effect: " + name + "\n");
        return;
    }

#ifdef _DEBUG
    // 同名のエフェクトがすでに登録されている場合は警告を出す（上書きはする）
    auto it = nativeFactories_.find(name);
    if (it != nativeFactories_.end())
    {
        ozSound::Log("Warning: Native effect already registered with name: " + name + "\n");
    }

#endif // _DEBUG

    nativeFactories_[name] = creator;
}

AudioEffectChain AudioEffectManager::BuildEffectChain(const std::vector<std::string>& effectNames)
{
    ozSound::AudioEffectChain effectChain = {};

    for (const std::string& effectName : effectNames)
    {
        auto defIt = effectDefs_.find(effectName);
        if (defIt == effectDefs_.end())
            continue;

        IUnknown* xapo = nullptr;
        if(defIt->second.type==AudioEffectType::VST3)
        {

            auto pluginEntry = GetVST3PluginEntry(effectName);
            if (!pluginEntry)
                continue;

            if (SUCCEEDED(VST3Effect::Create(pluginEntry->plugin.get(), &xapo)) && xapo)
            {
                pluginEntry->paramMgr.SetEffect(static_cast<VST3Effect*>(static_cast<IXAPO*>(xapo)));
            }
        }
        else if (defIt->second.type == AudioEffectType::Native)
        {
            auto factoryIt = nativeFactories_.find(effectName);
            if (factoryIt == nativeFactories_.end())
                continue;

            xapo = factoryIt->second();
        }

        if (xapo)
        {
            // 名前を持たせておくと、イベントから SetEffectEnabled で指名できる。
            // InitialState=true: チェーンに載せた時点で有効。無効にしたい場合は
            // SetEffectEnabled action で明示的に落とす。
            effectChain.AddEffect(AudioEffect(xapo, 2, true), effectName);
            xapo->Release();
        }
    }

    return effectChain;
}

VST3ParameterManager* AudioEffectManager::GetParameterManager(const std::string& effectName)
{
    for (auto& def : effectDefs_)
    {
        if (def.first == effectName && def.second.type == AudioEffectType::VST3)
        {
            return GetVST3PluginEntry(effectName) ? &GetVST3PluginEntry(effectName)->paramMgr : nullptr;
        }
    }/*
    for (auto& modulePair : loadedModules_)
    {
        auto& plugins = modulePair.second.plugins;
        auto it = plugins.find(effectName);
        if (it != plugins.end())
        {
            return &it->second->paramMgr;
        }
    }*/
    return nullptr; // エフェクトが見つからない場合はnullptrを返す
}

AudioEffectManager::VST3PluginEntry* AudioEffectManager::GetVST3PluginEntry(const std::string& effectName)
{
    for (auto& modulePair : loadedModules_)
    {
        auto& plugins = modulePair.second.plugins;
        auto it = plugins.find(effectName);
        if (it != plugins.end())
        {
            return it->second.get();
        }
    }
    return nullptr; // エフェクトが見つからない場合はnullptrを返す
}


}// namespace ozSound
