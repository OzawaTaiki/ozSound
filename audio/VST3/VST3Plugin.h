#pragma once

#include <pluginterfaces/vst/ivstaudioprocessor.h>
#include <pluginterfaces/vst/ivsteditcontroller.h>
#include <public.sdk/source/vst/hosting/module.h>
#include <public.sdk/source/vst/hosting/hostclasses.h>

namespace ozSound
{
// 読み込んだ.vst3プラグインの本体
// プラグインの情報は大体こいつが持っている
// IComponent、IAudioProcessor、IEditControllerの3つのインターフェースをラップしている
// DLLとのやりとりはこのクラスが担当する
class VST3Plugin
{
public:

    VST3Plugin() = default;
    ~VST3Plugin() = default;

    // DLLからプラグインのインスタンスを生成する。
    bool Load(VST3::Hosting::PluginFactory factory, const VST3::Hosting::ClassInfo& classInfo);

    // プラグインを使える状態にする
    bool Initialize(const VST3::Hosting::PluginFactory& factory,
                    Steinberg::Vst::HostApplication* host, float sampleRate,
                    int32_t maxSamplePerBlock, int32_t inputChannels, int32_t outputChannels);

    // プラグインを終了する
    void Terminate();

    // 音を処理する。
    // 毎フレーム呼ばれる
    bool Process(Steinberg::Vst::ProcessData& data);

    bool IsActive() const { return isActive_; }
    const std::string& GetName() const { return name_; }
    Steinberg::Vst::IEditController* GetController() const { return controller_.get(); }

    int32_t GetInputChannels()  const { return inputChannels_; }
    int32_t GetOutputChannels() const { return outputChannels_; }


private:
    // Steinberg::IPtr は comptrのようなもの

    // プラグインの本体
    Steinberg::IPtr<Steinberg::Vst::IComponent> component_ = nullptr;
    // 音を処理する人
    Steinberg::IPtr<Steinberg::Vst::IAudioProcessor> processor_ = nullptr;
    // パラメータの読み書きをおこなう人
    Steinberg::IPtr<Steinberg::Vst::IEditController> controller_ = nullptr;

    int32_t inputChannels_ = 0;
    int32_t outputChannels_ = 0;
    std::string name_;
    bool isActive_ = false;

};

}// namespace ozSound