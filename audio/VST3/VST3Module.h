#pragma once

#include <string>
#include <vector>
#include <memory>

#include <public.sdk/source/vst/hosting/module.h>
#include <optional>

namespace ozSound
{

class VST3Plugin;

// .vst3ファイル（DLL）のローダー
// DLLを開いてプラグインのクラス一覧を取得し、VST3Pluginを生成する
// 1つのDLLに複数のプラグインが含まれている場合もある
class VST3Module
{
public:
    VST3Module() = default;
    ~VST3Module() = default;

    // .vst3ファイルを開く
    bool Load(const std::string& path);

    // エフェクト系プラグインのクラス情報一覧を返す
    std::vector<VST3::Hosting::ClassInfo> GetAudioEffectClasses() const;

    // ClassInfoからVST3Pluginを生成する
    std::unique_ptr<VST3Plugin> CreatePlugin(const VST3::Hosting::ClassInfo& classInfo);


    const VST3::Hosting::PluginFactory& GetFactory(){ return module_->getFactory(); }

private:
    // ロードした.vst3ファイル（DLL）の管理オブジェクト
    // PluginFactoryの取得もこいつを通して行う
    VST3::Hosting::Module::Ptr module_;
};

} // namespace ozSound