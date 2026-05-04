#include "VST3Module.h"
#include "VST3Plugin.h"

bool ozSound::VST3Module::Load(const std::string& path)
{
    std::string errorMessage;
    // 指定パスのDLLからモジュールを作成する
    module_ = VST3::Hosting::Module::create(path, errorMessage);
    if (!module_)
        return false;

    return true;
}

std::vector<VST3::Hosting::ClassInfo> ozSound::VST3Module::GetAudioEffectClasses() const
{
    std::vector<VST3::Hosting::ClassInfo> audioEffectClasses;

    if (!module_)
        return audioEffectClasses;

    // DLLの中にあるプラグインのクラス情報をすべて取得する
    for (auto& classInfo : module_->getFactory().classInfos())
    {
        // クラス情報のカテゴリがエフェクト系プラグインのものであれば、audioEffectClassesに追加する
        if (classInfo.category() == kVstAudioEffectClass)
        {
            audioEffectClasses.push_back(classInfo);
        }
        // kVstComponentControllerClass もある
        // これはGUIを提供するクラス。プラグイン独自のGUIを提供する場合にはこれも必要になる。
    }

    return audioEffectClasses;
}

std::unique_ptr<ozSound::VST3Plugin> ozSound::VST3Module::CreatePlugin(const VST3::Hosting::ClassInfo& classInfo)
{
    if (!module_)
        return nullptr;

    auto plugin = std::make_unique<VST3Plugin>();
    // ClassInfoからVST3Pluginを生成する
    bool success = plugin->Load(module_->getFactory(), classInfo);
    if (!success)
        return nullptr;

    return plugin;
}
