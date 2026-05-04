#include "VST3ParameterManager.h"
#include "VST3Effect.h"
#include "pluginterfaces/base/ustring.h"

#include "../Logger/SoundLogger.h"

void ozSound::VST3ParameterManager::Initialize(Steinberg::Vst::IEditController* controller)
{
    controller_ = controller;
}

int32_t ozSound::VST3ParameterManager::GetParameterCount() const
{
    if (!controller_)
        return 0;

    return controller_->getParameterCount();
}

std::string ozSound::VST3ParameterManager::GetParameterName(int32_t index) const
{
    if (!controller_)
        return "";

    Steinberg::Vst::ParameterInfo info;
    if (controller_->getParameterInfo(index, info) != Steinberg::kResultOk)
        return "";

    // Steinberg::UString128はVST3 SDKで文字列を扱うためのクラス
    // Steinberg::UString128 はwchar_t の文字列を扱うクラスなので、ASCII文字列に変換してからstd::stringにする
    Steinberg::UString128 ustr(info.title);
    char buffer[128];
    ustr.toAscii(buffer, sizeof(buffer));
    return std::string(buffer);
}

double ozSound::VST3ParameterManager::GetParameter(Steinberg::Vst::ParamID id) const
{
    if (!controller_)
        return 0.0;

    Steinberg::Vst::ParameterInfo info;
    if (controller_->getParameterInfo(id, info) == Steinberg::kResultOk)
        return controller_->getParamNormalized(info.id);

    return 0.0;
}

void ozSound::VST3ParameterManager::SetParameter(Steinberg::Vst::ParamID id, double normalizedValue)
{
    if (!controller_)
        return;
    Steinberg::Vst::ParameterInfo info;
    if (controller_->getParameterInfo(id, info) == Steinberg::kResultOk)
        ozSound::Log(std::format("SetParameter: id={},index={}, value={}\n", info.id, id, normalizedValue));

    controller_->setParamNormalized(info.id, normalizedValue);

    // オーディオ処理側にもパラメータ変更を通知
    if (effect_)
        effect_->AddPendingParam(info.id, normalizedValue);
}
