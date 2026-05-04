#include "VST3Host.h"

#include "VST3Module.h"

using namespace ozSound;

VST3Host* ozSound::VST3Host::GetInstance()
{
    static VST3Host instance;
    return &instance;
}

/*
hostName:
HostApplicationの初期化時にホスト名を渡すと、プラグイン側からIHostApplication::getName()で取得できる
DAWでいうと「Cubase」「Ableton Live」などのアプリ名に相当する
今の実装ではHostApplicationのコンストラクタにホスト名を渡していないので未使用になっている
厳密に実装するならHostApplicationをカスタムクラスで継承して名前を返すように実装する必要があるらしい
現状は不要だが、形式的に引数で受け取るようにしておく。
*/

bool ozSound::VST3Host::Initialize([[maybe_unused]]const std::string& hostName)
{
    hostApp_ = std::make_unique<Steinberg::Vst::HostApplication>();

    return hostApp_ != nullptr;
}

void ozSound::VST3Host::Finalize()
{
    modules_.clear();
    hostApp_.reset();
}

VST3Module* ozSound::VST3Host::LoadModule(const std::string& path)
{
    // すでにロードされているモジュールがあればそれを返す
    auto it = modules_.find(path);
    if (it != modules_.end())
        return it->second.get();

    // 新たにモジュールをロードする
    auto module = std::make_unique<VST3Module>();
    if (!module->Load(path))
        return nullptr;

    VST3Module* modulePtr = module.get();
    modules_[path] = std::move(module);
    return modulePtr;
}

void ozSound::VST3Host::UnloadModule(const std::string& path)
{
    modules_.erase(path);
}

Steinberg::Vst::HostApplication* ozSound::VST3Host::GetHostApp()
{
    return hostApp_.get();
}
