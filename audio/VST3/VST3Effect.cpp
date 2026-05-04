#include "VST3Effect.h"

#include "VST3Plugin.h"
#include <cstring>

namespace
{
const XAPO_REGISTRATION_PROPERTIES kRegProps = {
    __uuidof(VST3EffectTag),
    L"VST3Effect",
    L"Engine",
    1,0,
    XAPO_FLAG_INPLACE_SUPPORTED | XAPO_FLAG_INPLACE_REQUIRED,
    1,1,1,1
};
}

using namespace ozSound;

VST3Effect::VST3Effect(VST3Plugin* plugin)
    : AudioEffectBase(&kRegProps), plugin_(plugin)
{
}

HRESULT VST3Effect::Create(VST3Plugin* plugin, IUnknown** effect)
{
    if (!plugin || !effect)
        return E_INVALIDARG;

    auto* newEffect = new VST3Effect(plugin);
    *effect = static_cast<IXAPO*>(newEffect);

    return S_OK;
}

void STDMETHODCALLTYPE VST3Effect::Process(UINT32 , const XAPO_PROCESS_BUFFER_PARAMETERS* input, UINT32 , XAPO_PROCESS_BUFFER_PARAMETERS* output, BOOL isEnabled)
{
    if (!isEnabled || !plugin_ || !plugin_->IsActive())
    {
        PassThrough(input, output);
        return;
    }

    if (!input || !output)
        return;

    const float*    src     = static_cast<const float*>(input[0].pBuffer);
    float*          dst     = static_cast<float*>(output[0].pBuffer);
    uint32_t        frames  = input[0].ValidFrameCount;

    // バッファサイズを確保
    inputBuf_ .resize(frames * channels_);
    outputBuf_.resize(frames * channels_);

    // interleaved -> planar変換
    for (uint32_t ch = 0; ch < channels_; ++ch)
    {
        for (uint32_t i = 0; i < frames; ++i)
        {
            inputBuf_[ch * frames + i] = src[i * channels_ + ch];
        }
    }

    // outputBufをinputで初期化（in-place処理するプラグイン対策）
    std::memcpy(outputBuf_.data(), inputBuf_.data(), frames * channels_ * sizeof(float));

    // VST3 processDataの構築
    std::vector<float*> inputPtrs(channels_);
    std::vector<float*> outputPtrs(channels_);
    for (uint32_t ch = 0; ch < channels_; ++ch)
    {
        inputPtrs[ch] = &inputBuf_[ch * frames];
        outputPtrs[ch] = &outputBuf_[ch * frames];
    }

    inputBusBuffers_.numChannels        = channels_;
    outputBusBuffers_.numChannels       = channels_;
    inputBusBuffers_.channelBuffers32   = inputPtrs.data();
    outputBusBuffers_.channelBuffers32  = outputPtrs.data();

    processData_.numSamples       = static_cast<Steinberg::int32>(frames);
    processData_.numInputs       = 1;
    processData_.numOutputs      = 1;
    processData_.inputs          = &inputBusBuffers_;
    processData_.outputs         = &outputBusBuffers_;
    processData_.symbolicSampleSize = Steinberg::Vst::kSample32;

    // パラメータ変更をキューに積む（clearしてから追加）
    paramChanges_.clearQueue();

    int32_t index = 0;

    for (auto& p : pendingParams_)
    {
        auto queue = paramChanges_.addParameterData(p.id, index);

        if (queue)
            queue->addPoint(0, p.value, index);
    }

    pendingParams_.clear();

    processData_.inputParameterChanges = &paramChanges_;
    processData_.outputParameterChanges = nullptr;

    plugin_->Process(processData_);

    // planar -> interleaved変換
    for (uint32_t ch = 0; ch < channels_; ++ch)
    {
        for (uint32_t i = 0; i < frames; ++i)
        {
            dst[i * channels_ + ch] = outputBuf_[ch * frames + i];
        }
    }
}
