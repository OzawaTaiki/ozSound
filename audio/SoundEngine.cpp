#include "SoundEngine.h"

#include "AudioSystem.h"
#include "SoundInstance.h"
#include "VoiceInstance.h"
#include "SubmixVoice.h"
#include "AudioEffectManager.h"

#include "Logger/SoundLogger.h"

#include <utility>

#ifdef _DEBUG
//#include <Debug/ImGuiDebugManager.h>
//#include <imgui.h>
#endif

#include <fstream>
#include <cassert>
#include <cmath>


namespace ozSound
{

SoundEngine* SoundEngine::GetInstance()
{
    static SoundEngine instance;
    return &instance;
}

void SoundEngine::Initialize()
{
    soundDefs_.clear();
    loadedInstances_.clear();
    playingSounds_.clear();
    nextHandle_ = 0;
}

void SoundEngine::Update(float deltaTime)
{
    for (auto it = runningEvents_.begin(); it != runningEvents_.end(); )
    {
        it->elapsed += deltaTime;

        auto& pending = it->pending;
        while (!pending.empty() && pending.front().fireTime <= it->elapsed)
        {
            ExecuteAction(pending.front().action);
            pending.erase(pending.begin());
        }

        if (pending.empty())
            it = runningEvents_.erase(it);
        else
            ++it;
    }

    CleanupStoppedVoices();
}

void SoundEngine::Finalize()
{
    StopAll();
    playingSounds_.clear();
    loadedInstances_.clear();
    soundDefs_.clear();
}

void SoundEngine::LoadSoundData(const std::string& jsonPath)
{
    soundDataPath_ = jsonPath; // Reload 用にパスを記憶

    json jsonData = LoadJson(jsonPath);
    if (jsonData.empty())
    {
        ozSound::Log("Failed to load sound data from: " + jsonPath + "\n");
        // ファイルがない場合は空の定義で初期化して保存する（サウンドエディタでの編集開始を想定）
        SoundDef sample;
        sample.id = "sample_sound";
        sample.filePath = "path/to/soundfile.wav";
        sample.type = "SE(SE or BGM)";
        sample.submixName = "optional_submix_name";
        sample.enableOverlap = true;

        json sampleJson = sample; // to_json(SoundDef) を使用して JSON オブジェクトに変換

        SaveJson(jsonPath, json{ {"sounds", json::array({ sampleJson })} });
        ozSound::Log("Created new sound data file at: " + jsonPath + "\n");
        return;
    }

    LoadSoundDataFromJson(jsonData);
}

void SoundEngine::LoadSoundDataFromJson(const json& jsonData)
{
    ozSound::Log("Loading Sound Data from JSON\n");
    if (jsonData.empty())
    {
        ozSound::Log("Sound Data JSON is empty. No sounds loaded.\n");
        return;
    }
    if (!jsonData.contains("sounds"))
    {
        ozSound::Log("Sound Data JSON does not contain 'sounds' key. No sounds loaded.\n");
        return;
    }

    for (const auto& entry : jsonData["sounds"])
    {
        SoundDef def = entry.get<SoundDef>(); // from_json(SoundDef) を使用

        if (def.id.empty() || def.filePath.empty())
            continue;

        soundDefs_[def.id] = def;

        auto instance = AudioSystem::GetInstance()->Load(def.filePath);
        if (instance)
        {
            loadedInstances_[def.id] = instance;
        }
    }

    ozSound::Log("Sound Data loaded successfully\n");
}

void SoundEngine::LoadEventData(const std::string& jsonPath)
{
    eventDataPath_ = jsonPath; // Reload 用にパスを記憶

    json jsonData = LoadJson(jsonPath);
    if (jsonData.empty())
    {
        ozSound::Log("Failed to load sound event data from: " + jsonPath + "\n");
        // ファイルがない場合は空の定義で初期化して保存する（サウンドエディタでの編集開始を想定）

        // サンプルイベント定義
        SoundEventDef sampleEvent;
        sampleEvent.name = "sample_event";
        SoundEventAction sampleAction;
        sampleAction.type = SoundEventType::Play;
        sampleAction.soundId = "sample_sound"; // LoadSoundData() で作成されるサンプルサウンドID
        sampleAction.volume = 1.0f;
        sampleAction.loop = false;
        sampleEvent.actions.push_back(sampleAction);

        json sampleEventJson = sampleEvent; // to_json(SoundEventDef) を使用して JSON オブジェクトに変換
        SaveJson(jsonPath, json{ {"events", json::array({ sampleEventJson })} });
        ozSound::Log("Created new sound event data file at: " + jsonPath + "\n");
        return;
    }

    LoadEventDataFromJson(jsonData);
}

void SoundEngine::LoadEventDataFromJson(const json& jsonData)
{
    ozSound::Log("Loading Sound Event Data from JSON\n");
    if (jsonData.empty())
    {
        ozSound::Log("Sound Event Data JSON is empty. No events loaded.\n");
        return;
    }
    if (!jsonData.contains("events"))
    {
        ozSound::Log("Sound Event Data JSON does not contain 'events' key. No events loaded.\n");
        return;
    }

    for (const auto& entry : jsonData["events"])
    {
        SoundEventDef eventDef = entry.get<SoundEventDef>(); // from_json(SoundEventDef) を使用
        if (eventDef.name.empty())
            continue;

        eventDefs_[eventDef.name] = eventDef;
        ozSound::Log("Loaded Sound Event: " + eventDef.name + "\n");
    }

    ozSound::Log("Sound Event Data loaded successfully\n");
}

void SoundEngine::PostEvent(const std::string& eventName)
{
    auto it = eventDefs_.find(eventName);
    if (it == eventDefs_.end())
        return;

    const SoundEventDef& eventDef = it->second;

    RunningEvent runningEvent;
    runningEvent.elapsed = 0.0f;
    for (const auto& action : eventDef.actions)
    {
        PendingAction pending;
        pending.action = action;
        pending.fireTime = action.startTime;
        runningEvent.pending.push_back(pending);
    }

    std::stable_sort(runningEvent.pending.begin(), runningEvent.pending.end(),
                     [](const PendingAction& a, const PendingAction& b)
                     {
                         return a.fireTime < b.fireTime;
                     });

    runningEvents_.push_back(std::move(runningEvent));

    //for (const auto& action : eventDef.actions)
    //{
    //    switch (action.type)
    //    {
    //        case SoundEventType::Play:
    //        {
    //            if (action.effects.empty())
    //                Play(action.soundId, action.volume, action.loop);
    //            else
    //                Play(action.soundId, action.effects, action.volume, action.loop);
    //            break;
    //        }
    //        case SoundEventType::Stop:
    //        {
    //            std::vector<SoundHandle> targets;
    //            for (const auto& [handle, ps] : playingSounds_)
    //                if (ps.soundId == action.soundId)
    //                    targets.push_back(handle);
    //            for (auto h : targets)
    //                Stop(h);
    //            break;
    //        }
    //        case SoundEventType::Pause:
    //        {
    //            std::vector<SoundHandle> targets;
    //            for (const auto& [handle, ps] : playingSounds_)
    //                if (ps.soundId == action.soundId)
    //                    targets.push_back(handle);
    //            for (auto h : targets)
    //                Pause(h);
    //            break;
    //        }
    //        case SoundEventType::Resume:
    //        {
    //            std::vector<SoundHandle> targets;
    //            for (const auto& [handle, ps] : playingSounds_)
    //                if (ps.soundId == action.soundId)
    //                    targets.push_back(handle);
    //            for (auto h : targets)
    //                Resume(h);
    //            break;
    //        }
    //        case SoundEventType::SetVolume:
    //        {
    //            std::vector<SoundHandle> targets;
    //            for (const auto& [handle, ps] : playingSounds_)
    //                if (ps.soundId == action.soundId)
    //                    targets.push_back(handle);
    //            for (auto h : targets)
    //                SetVolume(h, action.volume);
    //            break;
    //        }
    //        case SoundEventType::SetSpeed:
    //        {
    //            std::vector<SoundHandle> targets;
    //            for (const auto& [handle, ps] : playingSounds_)
    //                if (ps.soundId == action.soundId)
    //                    targets.push_back(handle);
    //            for (auto h : targets)
    //                SetSpeed(h, action.speed);
    //        }
    //        default:
    //            break;
    //    }
    //}
}

SoundHandle SoundEngine::Play(const std::string& soundId,
                              float volume,
                              bool  loop,
                              float startTime)
{
    auto instIt = loadedInstances_.find(soundId);
    if (instIt == loadedInstances_.end())
        return kInvalidHandle;

    auto defIt = soundDefs_.find(soundId);
    if (defIt == soundDefs_.end())
        return kInvalidHandle;

    const SoundDef& def         = defIt->second;
    auto& soundInstance         = instIt->second;

    // submixName（省略時は type 名）で Submix を検索、なければ SE にフォールバック
    const std::string& busName = def.submixName.empty() ? def.type : def.submixName;
    SubmixVoice* submix = AudioSystem::GetInstance()->GetSubmix(busName);
    if (!submix)
        submix = AudioSystem::GetInstance()->GetSubmix("SE");

    auto voice = soundInstance->GenerateVoiceInstance(
        volume,
        startTime,
        loop,
        def.enableOverlap,
        nullptr,
        submix
        // エフェクト指定なし版 → 空のチェーン (デフォルト引数)
    );

    if (!voice)
        return kInvalidHandle;

    voice->Play();

    SoundHandle handle = GenerateHandle();
    playingSounds_[handle] = PlayingSound{ soundInstance, voice, soundId, loop };
    return handle;
}

SoundHandle SoundEngine::Play(const std::string& soundId,
                              const std::vector<std::string>& effects,
                              float volume,
                              bool loop,
                              float startTime)
{
    auto instIt = loadedInstances_.find(soundId);
    if (instIt == loadedInstances_.end())
        return kInvalidHandle;

    auto defIt = soundDefs_.find(soundId);
    if (defIt == soundDefs_.end())
        return kInvalidHandle;

    const SoundDef& def         = defIt->second;
    auto& soundInstance         = instIt->second;

    // submixName（省略時は type 名）で Submix を検索、なければ SE にフォールバック
    const std::string& busName = def.submixName.empty() ? def.type : def.submixName;
    SubmixVoice* submix = AudioSystem::GetInstance()->GetSubmix(busName);
    if (!submix)
        submix = AudioSystem::GetInstance()->GetSubmix("SE");

    // effects からエフェクトチェーンを構築
    auto effectChain = AudioEffectManager::GetInstance()->BuildEffectChain(effects);

    auto voice = soundInstance->GenerateVoiceInstance(
        volume,
        startTime,
        loop,
        def.enableOverlap,
        nullptr,
        submix,
        std::move(effectChain)
    );

    if (!voice)
        return kInvalidHandle;

    voice->Play();

    SoundHandle handle = GenerateHandle();
    playingSounds_[handle] = PlayingSound{ soundInstance, voice, soundId, loop };
    return handle;
}

void SoundEngine::Stop(SoundHandle handle)
{
    auto it = playingSounds_.find(handle);
    if (it == playingSounds_.end())
        return;

    if (it->second.voiceInstance)
        it->second.voiceInstance->Stop();

    playingSounds_.erase(it);
}

void SoundEngine::StopAll()
{
    for (auto& [handle, ps] : playingSounds_)
    {
        if (ps.voiceInstance)
            ps.voiceInstance->Stop();
    }
    playingSounds_.clear();
}

void SoundEngine::Pause(SoundHandle handle)
{
    auto it = playingSounds_.find(handle);
    if (it == playingSounds_.end())
        return;

    if (it->second.voiceInstance)
        it->second.voiceInstance->Pause();
}

void SoundEngine::Resume(SoundHandle handle)
{
    auto it = playingSounds_.find(handle);
    if (it == playingSounds_.end())
        return;

    if (it->second.voiceInstance)
        it->second.voiceInstance->Resume();
}

void SoundEngine::SetVolume(SoundHandle handle, float volume)
{
    auto it = playingSounds_.find(handle);
    if (it == playingSounds_.end())
        return;

    if (it->second.voiceInstance)
        it->second.voiceInstance->SetVolume(volume);
}

void SoundEngine::SetSpeed(SoundHandle handle, float speed)
{
    auto it = playingSounds_.find(handle);
    if (it == playingSounds_.end())
        return;

    if (it->second.voiceInstance)
        it->second.voiceInstance->SetPlaySpeed(speed);
}

bool SoundEngine::IsPlaying(SoundHandle handle) const
{
    auto it = playingSounds_.find(handle);
    if (it == playingSounds_.end())
        return false;

    return it->second.voiceInstance && it->second.voiceInstance->IsPlaying();
}

float SoundEngine::GetElapsedTime(SoundHandle handle) const
{
    auto it = playingSounds_.find(handle);
    if (it == playingSounds_.end())
        return 0.0f;

    if (it->second.voiceInstance)
        return it->second.voiceInstance->GetElapsedTime();

    return 0.0f;
}

float SoundEngine::GetDuration(const std::string& soundId) const
{
    auto it = loadedInstances_.find(soundId);
    if (it == loadedInstances_.end())
        return 0.0f;

    return it->second->GetDuration();
}

void SoundEngine::CleanupStoppedVoices()
{
    for (auto it = playingSounds_.begin(); it != playingSounds_.end();)
    {
        if (!it->second.voiceInstance || !it->second.voiceInstance->IsPlaying())
            it = playingSounds_.erase(it);
        else
            ++it;
    }
}

std::shared_ptr<SoundInstance> SoundEngine::GetSoundInstance(const std::string& soundId)
{
    auto it = loadedInstances_.find(soundId);
    if (it == loadedInstances_.end())
        return nullptr;

    return it->second;
}

SoundHandle SoundEngine::GenerateHandle()
{
    if (nextHandle_ == kInvalidHandle)
        nextHandle_ = 0;

    return nextHandle_++;
}

void SoundEngine::ExecuteAction(const SoundEventAction& action)
{
    switch (action.type)
    {
        case SoundEventType::Play:
        {
            if (action.effects.empty())
                Play(action.soundId, action.volume, action.loop);
            else
                Play(action.soundId, action.effects, action.volume, action.loop);
            break;
        }
        case SoundEventType::Stop:
        {
            std::vector<SoundHandle> targets;
            for (const auto& [handle, ps] : playingSounds_)
                if (ps.soundId == action.soundId)
                    targets.push_back(handle);
            for (auto h : targets)
                Stop(h);
            break;
        }
        case SoundEventType::Pause:
        {
            std::vector<SoundHandle> targets;
            for (const auto& [handle, ps] : playingSounds_)
                if (ps.soundId == action.soundId)
                    targets.push_back(handle);
            for (auto h : targets)
                Pause(h);
            break;
        }
        case SoundEventType::Resume:
        {
            std::vector<SoundHandle> targets;
            for (const auto& [handle, ps] : playingSounds_)
                if (ps.soundId == action.soundId)
                    targets.push_back(handle);
            for (auto h : targets)
                Resume(h);
            break;
        }
        case SoundEventType::SetVolume:
        {
            std::vector<SoundHandle> targets;
            for (const auto& [handle, ps] : playingSounds_)
                if (ps.soundId == action.soundId)
                    targets.push_back(handle);
            for (auto h : targets)
                SetVolume(h, action.volume);
            break;
        }
        case SoundEventType::SetSpeed:
        {
            std::vector<SoundHandle> targets;
            for (const auto& [handle, ps] : playingSounds_)
                if (ps.soundId == action.soundId)
                    targets.push_back(handle);
            for (auto h : targets)
                SetSpeed(h, action.speed);
        }
        default:
            break;
    }
}

void SoundEngine::StopSoundsOnSubmix(const std::string& submixName)
{
    std::vector<SoundHandle> targets;
    for (const auto& [handle, ps] : playingSounds_)
    {
        auto defIt = soundDefs_.find(ps.soundId);
        if (defIt == soundDefs_.end()) continue;

        const SoundDef& def = defIt->second;
        const std::string& bus = def.submixName.empty() ? def.type : def.submixName;
        if (bus == submixName)
            targets.push_back(handle);
    }
    for (auto h : targets)
        Stop(h);
}
} // namespace ozSound
