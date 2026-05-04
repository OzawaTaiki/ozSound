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

    if (!jsonData.contains("effects"))
        return;

    auto host = VST3Host::GetInstance();

    for (const auto& entry : jsonData["effects"])
    {
        AudioEffectDef def = entry.get<AudioEffectDef>(); // from_json(AudioEffectDef) を使用
        effectDefs_[def.name] = def;
        auto module = host->LoadModule(def.path);
        if (!module)
        {
            ozSound::Log("Failed to load VST3 module for effect: " + def.name + "\n");
            continue;
        }
        auto classes = module->GetAudioEffectClasses();
        if (!classes.empty())
        {
            if (def.className.empty())
            {
                def.className = classes[0].name(); // クラス名が指定されていない場合は最初のクラスを使用
            }
            for (const auto& cls : classes)
            {
                if (cls.name() == def.className)
                {
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
            }
        }

    }
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
            effectChain.AddEffect(AudioEffect(xapo, 2, false));
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
