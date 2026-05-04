#pragma once

#include <string>

#include <pluginterfaces/vst/ivsteditcontroller.h>

namespace ozSound
{

class VST3Effect;

class VST3ParameterManager
{
public:
    // IEditControllerを設定する
    void Initialize(Steinberg::Vst::IEditController* controller);

    // VST3Effectを設定する（オーディオ処理側にパラメータ変更を通知するため）
    void SetEffect(VST3Effect* effect) { effect_ = effect; }

    // パラメータの数を返す
    int32_t GetParameterCount() const;

    // indexで指定したパラメータのIDを返す
    std::string GetParameterName(int32_t index) const;

    // パラメータの値を取得する(0.0～1.0の正規化された値)
    double GetParameter(Steinberg::Vst::ParamID id) const;

    // パラメータの値を設定する(0.0～1.0の正規化された値)
    void SetParameter(Steinberg::Vst::ParamID id, double normalizedValue);

private:
    // IEditControllerはプラグインのパラメータの読み書きを行うためのインターフェース
    // 所有権はVST3Plugin
    Steinberg::Vst::IEditController* controller_ = nullptr;
    VST3Effect* effect_ = nullptr;
};

}// namespace ozSound