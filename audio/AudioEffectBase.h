#pragma once

#include <xapobase.h>
#include <string>

#pragma comment(lib, "xapobase2_8.lib")

namespace ozSound
{

// CXAPOBaseを継承した共通基底クラス
// 派生クラスはProcessだけ実装すればいい
// チャンネル数・サンプルレートの保存とパススルー処理を共通化している
class AudioEffectBase : public CXAPOBase
{
public:

    STDMETHOD(LockForProcess)(UINT32 inputCount,
                              const XAPO_LOCKFORPROCESS_BUFFER_PARAMETERS* input,
                              UINT32 outputCount,
                              const XAPO_LOCKFORPROCESS_BUFFER_PARAMETERS* output)
    {
        HRESULT hr = CXAPOBase::LockForProcess(inputCount, input, outputCount, output);
        if (FAILED(hr))
        {
            return hr;
        }

        channels_ = input[0].pFormat->nChannels;
        sampleRate_ = static_cast<float>(input[0].pFormat->nSamplesPerSec);

        return S_OK;
    }

    STDMETHOD_(void, Process)(
        UINT32 inputCount,
        const XAPO_PROCESS_BUFFER_PARAMETERS* input,
        UINT32 outputCount,
        XAPO_PROCESS_BUFFER_PARAMETERS* output,
        BOOL isEnabled) override = 0;

protected:
    AudioEffectBase(const XAPO_REGISTRATION_PROPERTIES* pRegProps) : CXAPOBase(pRegProps) {}

    void PassThrough(const XAPO_PROCESS_BUFFER_PARAMETERS* input, XAPO_PROCESS_BUFFER_PARAMETERS* output) const
    {
        UINT32 bytes = input[0].ValidFrameCount * channels_ * sizeof(float);
        std::memcpy(output[0].pBuffer, input[0].pBuffer, bytes);
    }

    UINT32 channels_  = 2;
    float sampleRate_ = 48000.0f;

};

}// namespace ozSound