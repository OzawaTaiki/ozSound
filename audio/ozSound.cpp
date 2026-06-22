#include "ozSound.h"

#include "JsonUtils/JsonUtils.h"
#include "Logger/SoundLogger.h"

namespace ozSound
{

namespace
{
    /// <summary>
    /// Initialize で何を立ち上げたかを記録する。Finalize は記録に基づいて
    /// 逆順で Finalize を呼び、未初期化サブシステムを触らないようにする。
    /// </summary>
    struct InitState
    {
        bool audioSystem  = false;
        bool vst3Host     = false;
        bool effectMgr    = false;
        bool soundEngine  = false;

        bool Any() const { return audioSystem || vst3Host || effectMgr || soundEngine; }
    };

    InitState g_state;
} // anonymous

bool Initialize(const InitOptions& opts)
{
    if (g_state.Any())
    {
        ozSound::Log("[ozSound] Initialize: already initialized — skipping.\n");
        return true;
    }

    // ── 1. AudioSystem (必須) ────────────────────────────────────────────────
    AudioSystem::GetInstance()->Initialize();
    g_state.audioSystem = true;

    // ── 2. VST3Host + AudioEffectManager (opts.enableVST3) ──────────────────
    if (opts.enableVST3)
    {
        VST3Host::GetInstance()->Initialize(opts.hostName);
        g_state.vst3Host = true;

        AudioEffectManager::GetInstance()->Initialize();
        g_state.effectMgr = true;
    }

    // ── 3. SoundEngine (opts.enableEngine) ──────────────────────────────────
    if (opts.enableEngine)
    {
        SoundEngine::GetInstance()->Initialize();
        g_state.soundEngine = true;
    }

    return true;
}

void Finalize()
{
    if (!g_state.Any()) return;

    // 逆順
    if (g_state.soundEngine)
    {
        SoundEngine::GetInstance()->Finalize();
        g_state.soundEngine = false;
    }
    if (g_state.effectMgr)
    {
        AudioEffectManager::GetInstance()->Finalize();
        g_state.effectMgr = false;
    }
    if (g_state.vst3Host)
    {
        VST3Host::GetInstance()->Finalize();
        g_state.vst3Host = false;
    }
    if (g_state.audioSystem)
    {
        AudioSystem::GetInstance()->Finalize();
        g_state.audioSystem = false;
    }
}

bool LoadProject(const std::string& ozprojPath)
{
    json data = LoadJson(ozprojPath);
    if (data.empty())
    {
        ozSound::Log("[ozSound] LoadProject failed: " + ozprojPath + "\n");
        return false;
    }

    // 順序: submixes → effects → sounds → events
    // (Sound 再生時に Submix と Effect 名を解決する。Event は Sound ID を解決する)

    if (g_state.audioSystem)
        AudioSystem::GetInstance()->LoadSubmixConfigFromJson(data);

    if (g_state.effectMgr)
        AudioEffectManager::GetInstance()->LoadEffectDataFromJson(data);

    if (g_state.soundEngine)
    {
        SoundEngine::GetInstance()->LoadSoundDataFromJson(data);
        SoundEngine::GetInstance()->LoadEventDataFromJson(data);
    }

    return true;
}

} // namespace ozSound
