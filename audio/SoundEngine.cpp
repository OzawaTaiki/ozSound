#include "SoundEngine.h"

#include "AudioSystem.h"
#include "SoundInstance.h"
#include "VoiceInstance.h"
#include "SubmixVoice.h"
#include "AudioEffectManager.h"

#include "Logger/SoundLogger.h"

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

#ifdef _DEBUG
    //ImGuiDebugManager::GetInstance()->RegisterMenuItem(
        //"SoundEngine", [](bool* _open) { ImGui(_open); });
#endif
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

    if (!jsonData.contains("sounds"))
        return;

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

    if (!jsonData.contains("events"))
        return;

    for (const auto& entry : jsonData["events"])
    {
        SoundEventDef eventDef = entry.get<SoundEventDef>(); // from_json(SoundEventDef) を使用
        if (eventDef.name.empty())
            continue;

        eventDefs_[eventDef.name] = eventDef;
    }
}

void SoundEngine::PostEvent(const std::string& eventName)
{
    auto it = eventDefs_.find(eventName);
    if (it == eventDefs_.end())
        return;

    const SoundEventDef& eventDef = it->second;
    for (const auto& action : eventDef.actions)
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
            default:
                break;
        }
    }
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
        submix,
        nullptr
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
        effectChain.BuildChain()
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

// -----------------------------------------------------------------------
// ImGui デバッグパネル
// -----------------------------------------------------------------------
#ifdef _DEBUG

static constexpr const char* kAudioConfigPath = "Resources/Audio/AudioConfig.json";

void SoundEngine::ImGui(bool* _open)
{
//    if (!::ImGui::Begin("SoundEngine", _open))
//    {
//        ::ImGui::End();
//        return;
//    }
//
//    SoundEngine* engine = GetInstance();
//    AudioSystem* audio  = AudioSystem::GetInstance();
//
//    if (!::ImGui::BeginTabBar("SoundEngineTabs"))
//    {
//        ::ImGui::End();
//        return;
//    }
//
//    // ================================================================
//    // Tab 1: Mixer
//    // ================================================================
//    if (::ImGui::BeginTabItem("Mixer"))
//    {
//        float master = audio->GetMasterVolume();
//        if (::ImGui::SliderFloat("Master", &master, 0.0f, 1.5f, "%.2f"))
//            audio->SetMasterVolume(master);
//
//        ::ImGui::Separator();
//
//        static std::unordered_map<std::string, float> preMuteVolumes;
//        std::string pendingRemove;
//
//        for (const auto& [name, sv] : audio->GetAllSubmixes())
//        {
//            float vol = sv->GetVolume();
//            ::ImGui::Text("%-12s", name.c_str());
//            ::ImGui::SameLine();
//            std::string sliderId = "##vol_" + name;
//            ::ImGui::SetNextItemWidth(180.0f);
//            if (::ImGui::SliderFloat(sliderId.c_str(), &vol, 0.0f, 1.5f, "%.2f"))
//                sv->SetVolume(vol);
//
//            ::ImGui::SameLine();
//            bool isMuted = (sv->GetVolume() < 0.001f);
//            std::string muteId = (isMuted ? "Unmute##mu_" : "Mute##mu_") + name;
//            if (::ImGui::SmallButton(muteId.c_str()))
//            {
//                if (isMuted)
//                {
//                    float prev = preMuteVolumes.count(name) ? preMuteVolumes[name] : 1.0f;
//                    sv->SetVolume(prev);
//                }
//                else
//                {
//                    preMuteVolumes[name] = sv->GetVolume();
//                    sv->SetVolume(0.0f);
//                }
//            }
//
//            if (name != "BGM" && name != "SE")
//            {
//                ::ImGui::SameLine();
//                std::string delId = "x##del_" + name;
//                if (::ImGui::SmallButton(delId.c_str()))
//                    pendingRemove = name;
//            }
//        }
//        if (!pendingRemove.empty())
//            audio->RemoveSubmix(pendingRemove);
//
//        ::ImGui::Separator();
//
//        if (::ImGui::Button("+ Add Submix"))
//            ::ImGui::OpenPopup("AddSubmixPopup");
//
//        if (::ImGui::BeginPopup("AddSubmixPopup"))
//        {
//            static char newName[64] = {};
//            static int  newChannels = 2;
//            ::ImGui::InputText("Name",     newName,     sizeof(newName));
//            ::ImGui::InputInt ("Channels", &newChannels);
//            newChannels = std::max(1, newChannels);
//
//            if (::ImGui::Button("OK") && newName[0] != '\0')
//            {
//                audio->AddSubmix(newName, static_cast<uint32_t>(newChannels), 48000.0f, 1);
//                newName[0] = '\0';
//                ::ImGui::CloseCurrentPopup();
//            }
//            ::ImGui::SameLine();
//            if (::ImGui::Button("Cancel"))
//                ::ImGui::CloseCurrentPopup();
//            ::ImGui::EndPopup();
//        }
//
//        ::ImGui::SameLine();
//        if (::ImGui::Button("Save JSON##mix"))
//            audio->SaveSubmixConfig(kAudioConfigPath);
//        ::ImGui::SameLine();
//        if (::ImGui::Button("Load JSON##mix"))
//            audio->LoadSubmixConfig(kAudioConfigPath);
//
//        ::ImGui::EndTabItem();
//    }
//
//    // ================================================================
//    // Tab 2: Sounds
//    // ================================================================
//    if (::ImGui::BeginTabItem("Sounds"))
//    {
//        static char soundFilter[128] = {};
//        static int  submixFilterIdx  = 0;
//
//        ::ImGui::InputText("Filter##sf", soundFilter, sizeof(soundFilter));
//
//        std::vector<std::string> submixNames = {"All"};
//        for (const auto& [n, _] : audio->GetAllSubmixes())
//            submixNames.push_back(n);
//        submixFilterIdx = std::min(submixFilterIdx, static_cast<int>(submixNames.size()) - 1);
//
//        ::ImGui::SameLine();
//        ::ImGui::SetNextItemWidth(120.0f);
//        if (::ImGui::BeginCombo("Submix##sf", submixNames[submixFilterIdx].c_str()))
//        {
//            for (int i = 0; i < static_cast<int>(submixNames.size()); i++)
//            {
//                bool sel = (submixFilterIdx == i);
//                if (::ImGui::Selectable(submixNames[i].c_str(), sel))
//                    submixFilterIdx = i;
//                if (sel) ::ImGui::SetItemDefaultFocus();
//            }
//            ::ImGui::EndCombo();
//        }
//
//        std::vector<const char*> busOptions;
//        for (int i = 1; i < static_cast<int>(submixNames.size()); i++)
//            busOptions.push_back(submixNames[i].c_str());
//
//        if (::ImGui::BeginTable("SoundsTable", 6,
//            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY,
//            ImVec2(0, 280)))
//        {
//            ::ImGui::TableSetupScrollFreeze(0, 1);
//            ::ImGui::TableSetupColumn("ID",      ImGuiTableColumnFlags_WidthStretch);
//            ::ImGui::TableSetupColumn("Path",    ImGuiTableColumnFlags_WidthStretch);
//            ::ImGui::TableSetupColumn("Type",    ImGuiTableColumnFlags_WidthFixed, 50.0f);
//            ::ImGui::TableSetupColumn("Submix",  ImGuiTableColumnFlags_WidthFixed, 110.0f);
//            ::ImGui::TableSetupColumn("Overlap", ImGuiTableColumnFlags_WidthFixed, 60.0f);
//            ::ImGui::TableSetupColumn("Loaded",  ImGuiTableColumnFlags_WidthFixed, 50.0f);
//            ::ImGui::TableHeadersRow();
//
//            for (auto& [id, def] : engine->soundDefs_)
//            {
//                const std::string& bus = def.submixName.empty() ? def.type : def.submixName;
//
//                if (soundFilter[0] != '\0' &&
//                    id.find(soundFilter) == std::string::npos &&
//                    def.filePath.find(soundFilter) == std::string::npos)
//                    continue;
//
//                if (submixFilterIdx > 0 && bus != submixNames[submixFilterIdx])
//                    continue;
//
//                ::ImGui::TableNextRow();
//                ::ImGui::TableSetColumnIndex(0); ::ImGui::TextUnformatted(id.c_str());
//                ::ImGui::TableSetColumnIndex(1); ::ImGui::TextUnformatted(def.filePath.c_str());
//                ::ImGui::TableSetColumnIndex(2); ::ImGui::TextUnformatted(def.type.c_str());
//
//                ::ImGui::TableSetColumnIndex(3);
//                int curIdx = 0;
//                for (int i = 0; i < static_cast<int>(busOptions.size()); i++)
//                    if (bus == busOptions[i]) { curIdx = i; break; }
//                ::ImGui::SetNextItemWidth(-1.0f);
//                std::string comboId = "##sub_" + id;
//                if (!busOptions.empty() &&
//                    ::ImGui::BeginCombo(comboId.c_str(), busOptions[curIdx]))
//                {
//                    for (int i = 0; i < static_cast<int>(busOptions.size()); i++)
//                    {
//                        bool sel = (curIdx == i);
//                        if (::ImGui::Selectable(busOptions[i], sel))
//                            def.submixName = busOptions[i];
//                        if (sel) ::ImGui::SetItemDefaultFocus();
//                    }
//                    ::ImGui::EndCombo();
//                }
//
//                ::ImGui::TableSetColumnIndex(4);
//                std::string cbId = "##ovl_" + id;
//                ::ImGui::Checkbox(cbId.c_str(), &def.enableOverlap);
//
//                ::ImGui::TableSetColumnIndex(5);
//                bool loaded = engine->loadedInstances_.count(id) > 0;
//                ::ImGui::TextUnformatted(loaded ? "OK" : "--");
//            }
//            ::ImGui::EndTable();
//        }
//
//        ::ImGui::Separator();
//
//        // --- 個別サウンド追加 ---
//        static bool s_openAddSound = false;
//        if (::ImGui::Button("+ Add Sound"))
//        {
//            s_openAddSound = true;
//            ::ImGui::OpenPopup("AddSoundPopup");
//        }
//
//        if (::ImGui::BeginPopup("AddSoundPopup"))
//        {
//            static char addId[128]   = {};
//            static char addPath[256] = {};
//            static int  addTypeIdx   = 1; // 0=BGM, 1=SE
//            static int  addSubmixIdx = 0;
//            static bool addOverlap   = true;
//
//            // ボタンで開いた初回フレームにリセット（Escape等で閉じた残骸対策）
//            if (s_openAddSound)
//            {
//                addId[0] = addPath[0] = '\0';
//                addTypeIdx = 1; addSubmixIdx = 0; addOverlap = true;
//                s_openAddSound = false;
//            }
//
//            const char* typeOptions[] = { "BGM", "SE" };
//            ::ImGui::InputText("ID##add",   addId,   sizeof(addId));
//            ::ImGui::InputText("Path##add", addPath, sizeof(addPath));
//            ::ImGui::Combo("Type##add", &addTypeIdx, typeOptions, 2);
//
//            std::vector<std::string> addSubmixNames;
//            for (const auto& [n, _] : audio->GetAllSubmixes())
//                addSubmixNames.push_back(n);
//            addSubmixIdx = std::min(addSubmixIdx, static_cast<int>(addSubmixNames.size()) - 1);
//            if (!addSubmixNames.empty())
//            {
//                if (::ImGui::BeginCombo("Submix##add", addSubmixNames[addSubmixIdx].c_str()))
//                {
//                    for (int i = 0; i < static_cast<int>(addSubmixNames.size()); i++)
//                    {
//                        bool sel = (addSubmixIdx == i);
//                        if (::ImGui::Selectable(addSubmixNames[i].c_str(), sel)) addSubmixIdx = i;
//                        if (sel) ::ImGui::SetItemDefaultFocus();
//                    }
//                    ::ImGui::EndCombo();
//                }
//            }
//            ::ImGui::Checkbox("Overlap##add", &addOverlap);
//
//            bool canAdd = (addId[0] != '\0' && addPath[0] != '\0');
//            if (!canAdd) ::ImGui::BeginDisabled();
//            if (::ImGui::Button("Add##add"))
//            {
//                SoundDef def;
//                def.id            = addId;
//                def.filePath      = addPath;
//                def.type          = typeOptions[addTypeIdx];
//                def.submixName    = addSubmixNames.empty() ? def.type : addSubmixNames[addSubmixIdx];
//                def.enableOverlap = addOverlap;
//
//                engine->soundDefs_[def.id] = def;
//                auto inst = audio->Load(def.filePath);
//                if (inst)
//                    engine->loadedInstances_[def.id] = inst;
//
//                addId[0] = addPath[0] = '\0';
//                ::ImGui::CloseCurrentPopup();
//            }
//            if (!canAdd) ::ImGui::EndDisabled();
//
//            ::ImGui::SameLine();
//            if (::ImGui::Button("Cancel##add"))
//                ::ImGui::CloseCurrentPopup();
//
//            ::ImGui::EndPopup();
//        }
//
//        // --- JSON 保存 / リロード ---
//        ::ImGui::SameLine();
//        if (!engine->soundDataPath_.empty())
//        {
//            if (::ImGui::Button("Save JSON##sounds"))
//            {
//                json arr = json::array();
//                for (const auto& [id, def] : engine->soundDefs_)
//                    arr.push_back(def);
//                JsonFileIO::Save(engine->soundDataPath_, "", json{{"sounds", arr}});
//            }
//            ::ImGui::SameLine();
//            if (::ImGui::Button("Reload JSON##sounds"))
//            {
//                engine->soundDefs_.clear();
//                engine->loadedInstances_.clear();
//                engine->LoadSoundData(engine->soundDataPath_);
//            }
//            ::ImGui::SameLine();
//            ::ImGui::TextDisabled("%s", engine->soundDataPath_.c_str());
//        }
//        else
//        {
//            static char newSoundPath[256] = {};
//            ::ImGui::SetNextItemWidth(280.0f);
//            ::ImGui::InputText("##newSoundPath", newSoundPath, sizeof(newSoundPath));
//            ::ImGui::SameLine();
//            if (::ImGui::Button("Load JSON##sounds") && newSoundPath[0] != '\0')
//                engine->LoadSoundData(newSoundPath);
//        }
//
//        ::ImGui::EndTabItem();
//    }
//
//    // ================================================================
//    // Tab 3: Events
//    // ================================================================
//    if (::ImGui::BeginTabItem("Events"))
//    {
//        // イベント一覧
//        if (engine->eventDefs_.empty())
//        {
//            ::ImGui::TextDisabled("（イベントなし）");
//        }
//        else
//        {
//            std::string pendingDeleteEvent;
//            for (const auto& [name, eventDef] : engine->eventDefs_)
//            {
//                bool open = ::ImGui::TreeNodeEx(name.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
//                ::ImGui::SameLine();
//                if (::ImGui::SmallButton(("Test##evt_" + name).c_str()))
//                    engine->PostEvent(name);
//                ::ImGui::SameLine();
//                if (::ImGui::SmallButton(("x##evtdel_" + name).c_str()))
//                    pendingDeleteEvent = name;
//
//                if (open)
//                {
//                    for (const auto& action : eventDef.actions)
//                    {
//                        const char* typeName = "?";
//                        switch (action.type)
//                        {
//                            case SoundEventType::Play:      typeName = "Play";      break;
//                            case SoundEventType::Stop:      typeName = "Stop";      break;
//                            case SoundEventType::Pause:     typeName = "Pause";     break;
//                            case SoundEventType::Resume:    typeName = "Resume";    break;
//                            case SoundEventType::SetVolume: typeName = "SetVolume"; break;
//                        }
//                        ::ImGui::BulletText("%s: %s  vol=%.2f  loop=%s",
//                            typeName, action.soundId.c_str(), action.volume,
//                            action.loop ? "true" : "false");
//                        for (const auto& fx : action.effects)
//                            ::ImGui::BulletText("  fx: %s", fx.c_str());
//                    }
//                    ::ImGui::TreePop();
//                }
//            }
//            if (!pendingDeleteEvent.empty())
//                engine->eventDefs_.erase(pendingDeleteEvent);
//        }
//
//        ::ImGui::Separator();
//
//        // --- イベント追加 ---
//        static const char* kEventTypeNames[] = { "Play", "Stop", "Pause", "Resume", "SetVolume" };
//        static const SoundEventType kEventTypes[] = {
//            SoundEventType::Play, SoundEventType::Stop,
//            SoundEventType::Pause, SoundEventType::Resume, SoundEventType::SetVolume
//        };
//
//        static bool s_openAddEvent = false;
//        if (::ImGui::Button("+ Add Event"))
//        {
//            s_openAddEvent = true;
//            ::ImGui::OpenPopup("AddEventPopup");
//        }
//
//        ::ImGui::SetNextWindowSize(ImVec2(480, 400), ImGuiCond_Appearing);
//        if (::ImGui::BeginPopup("AddEventPopup"))
//        {
//            static char evtName[128] = {};
//            static std::vector<SoundEventAction> evtActions;
//
//            // ボタンで開いた初回フレームにリセット（Escape等で閉じた残骸対策）
//            if (s_openAddEvent)
//            {
//                evtName[0] = '\0';
//                evtActions.clear();
//                s_openAddEvent = false;
//            }
//
//            ::ImGui::InputText("Event Name##evtadd", evtName, sizeof(evtName));
//            ::ImGui::Separator();
//            ::ImGui::Text("Actions (%zu)", evtActions.size());
//
//            int removeIdx = -1;
//            for (int i = 0; i < static_cast<int>(evtActions.size()); i++)
//            {
//                SoundEventAction& a = evtActions[i];
//                ::ImGui::PushID(i);
//
//                int typeIdx = static_cast<int>(a.type);
//                ::ImGui::SetNextItemWidth(100.0f);
//                if (::ImGui::Combo("##atype", &typeIdx, kEventTypeNames, 5))
//                    a.type = kEventTypes[typeIdx];
//
//                ::ImGui::SameLine();
//                ::ImGui::SetNextItemWidth(130.0f);
//                if (::ImGui::BeginCombo("##asound", a.soundId.empty() ? "--" : a.soundId.c_str()))
//                {
//                    for (const auto& [sid, _] : engine->soundDefs_)
//                    {
//                        bool sel = (a.soundId == sid);
//                        if (::ImGui::Selectable(sid.c_str(), sel)) a.soundId = sid;
//                        if (sel) ::ImGui::SetItemDefaultFocus();
//                    }
//                    ::ImGui::EndCombo();
//                }
//
//                if (a.type == SoundEventType::Play || a.type == SoundEventType::SetVolume)
//                {
//                    ::ImGui::SameLine();
//                    ::ImGui::SetNextItemWidth(70.0f);
//                    ::ImGui::SliderFloat("##avol", &a.volume, 0.0f, 1.5f, "%.2f");
//                }
//
//                if (a.type == SoundEventType::Play)
//                {
//                    ::ImGui::SameLine();
//                    ::ImGui::Checkbox("Loop##al", &a.loop);
//                }
//
//                ::ImGui::SameLine();
//                if (::ImGui::SmallButton("x##adel"))
//                    removeIdx = i;
//
//                ::ImGui::PopID();
//            }
//            if (removeIdx >= 0)
//                evtActions.erase(evtActions.begin() + removeIdx);
//
//            if (::ImGui::Button("+ Add Action##evtadd"))
//            {
//                SoundEventAction action{};
//                action.type   = SoundEventType::Play;
//                action.volume = 1.0f;
//                evtActions.push_back(action);
//            }
//
//            ::ImGui::Separator();
//
//            bool canAddEvt = (evtName[0] != '\0' && !evtActions.empty());
//            if (!canAddEvt) ::ImGui::BeginDisabled();
//            if (::ImGui::Button("Add Event##evtadd"))
//            {
//                SoundEventDef def;
//                def.name    = evtName;
//                def.actions = evtActions;
//                engine->eventDefs_[def.name] = def;
//
//                evtName[0] = '\0';
//                evtActions.clear();
//                ::ImGui::CloseCurrentPopup();
//            }
//            if (!canAddEvt) ::ImGui::EndDisabled();
//
//            ::ImGui::SameLine();
//            if (::ImGui::Button("Cancel##evtadd"))
//            {
//                evtName[0] = '\0';
//                evtActions.clear();
//                ::ImGui::CloseCurrentPopup();
//            }
//
//            ::ImGui::EndPopup();
//        }
//
//        // --- JSON 保存 / リロード ---
//        ::ImGui::SameLine();
//        if (!engine->eventDataPath_.empty())
//        {
//            if (::ImGui::Button("Save JSON##events"))
//            {
//                json arr = json::array();
//                for (const auto& [name, def] : engine->eventDefs_)
//                    arr.push_back(def);
//                JsonFileIO::Save(engine->eventDataPath_, "", json{{"events", arr}});
//            }
//            ::ImGui::SameLine();
//            if (::ImGui::Button("Reload JSON##events"))
//            {
//                engine->eventDefs_.clear();
//                engine->LoadEventData(engine->eventDataPath_);
//            }
//            ::ImGui::SameLine();
//            ::ImGui::TextDisabled("%s", engine->eventDataPath_.c_str());
//        }
//        else
//        {
//            ::ImGui::TextDisabled("（LoadEventData() がまだ呼ばれていません）");
//        }
//
//        ::ImGui::EndTabItem();
//    }
//
//    // ================================================================
//    // Tab 4: Now Playing
//    // ================================================================
//    if (::ImGui::BeginTabItem("Now Playing"))
//    {
//        ::ImGui::Text("Total: %zu", engine->playingSounds_.size());
//        ::ImGui::SameLine();
//        if (::ImGui::Button("Stop All##np"))
//            engine->StopAll();
//
//        if (::ImGui::BeginTable("NowPlayingTable", 5,
//            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY,
//            ImVec2(0, 300)))
//        {
//            ::ImGui::TableSetupScrollFreeze(0, 1);
//            ::ImGui::TableSetupColumn("Handle",   ImGuiTableColumnFlags_WidthFixed,  55.0f);
//            ::ImGui::TableSetupColumn("SoundID",  ImGuiTableColumnFlags_WidthStretch);
//            ::ImGui::TableSetupColumn("Submix",   ImGuiTableColumnFlags_WidthFixed,  80.0f);
//            ::ImGui::TableSetupColumn("Progress", ImGuiTableColumnFlags_WidthStretch);
//            ::ImGui::TableSetupColumn("Stop",     ImGuiTableColumnFlags_WidthFixed,  45.0f);
//            ::ImGui::TableHeadersRow();
//
//            std::vector<SoundHandle> toStop;
//            for (const auto& [handle, ps] : engine->playingSounds_)
//            {
//                ::ImGui::TableNextRow();
//
//                ::ImGui::TableSetColumnIndex(0);
//                ::ImGui::Text("%u", handle);
//
//                ::ImGui::TableSetColumnIndex(1);
//                ::ImGui::TextUnformatted(ps.soundId.c_str());
//
//                ::ImGui::TableSetColumnIndex(2);
//                {
//                    auto defIt = engine->soundDefs_.find(ps.soundId);
//                    if (defIt != engine->soundDefs_.end())
//                    {
//                        const SoundDef& d = defIt->second;
//                        const std::string& bus = d.submixName.empty() ? d.type : d.submixName;
//                        ::ImGui::TextUnformatted(bus.c_str());
//                    }
//                }
//
//                ::ImGui::TableSetColumnIndex(3);
//                {
//                    float elapsed  = engine->GetElapsedTime(handle);
//                    float duration = engine->GetDuration(ps.soundId);
//                    char  overlay[32];
//                    float frac = 0.0f;
//                    if (ps.loop || duration <= 0.0f)
//                    {
//                        frac = (duration > 0.0f)
//                            ? std::fmod(elapsed, duration) / duration : 0.0f;
//                        std::snprintf(overlay, sizeof(overlay), "%.1fs / loop", elapsed);
//                    }
//                    else
//                    {
//                        frac = (duration > 0.0f)
//                            ? std::min(elapsed / duration, 1.0f) : 0.0f;
//                        std::snprintf(overlay, sizeof(overlay), "%.1fs/%.1fs", elapsed, duration);
//                    }
//                    ::ImGui::ProgressBar(frac, ImVec2(-1.0f, 0.0f), overlay);
//                }
//
//                ::ImGui::TableSetColumnIndex(4);
//                {
//                    std::string stopId = "x##stp_" + std::to_string(handle);
//                    if (::ImGui::SmallButton(stopId.c_str()))
//                        toStop.push_back(handle);
//                }
//            }
//            ::ImGui::EndTable();
//
//            for (auto h : toStop)
//                engine->Stop(h);
//        }
//
//        ::ImGui::EndTabItem();
//    }
//
//    // ================================================================
//    // Tab 5: Tester
//    // ================================================================
//    if (::ImGui::BeginTabItem("Tester"))
//    {
//        static int         testerSelectedIdx = 0;
//        static float       testerVolume      = 1.0f;
//        static bool        testerLoop        = false;
//        static float       testerStart       = 0.0f;
//        static SoundHandle testerLastHandle  = kInvalidHandle;
//
//        std::vector<std::string> ids;
//        for (const auto& [id, _] : engine->soundDefs_)
//            ids.push_back(id);
//
//        if (ids.empty())
//        {
//            ::ImGui::TextDisabled("（サウンドが読み込まれていません）");
//        }
//        else
//        {
//            testerSelectedIdx = std::min(testerSelectedIdx, static_cast<int>(ids.size()) - 1);
//
//            ::ImGui::SetNextItemWidth(220.0f);
//            if (::ImGui::BeginCombo("Sound##tst", ids[testerSelectedIdx].c_str()))
//            {
//                for (int i = 0; i < static_cast<int>(ids.size()); i++)
//                {
//                    bool sel = (testerSelectedIdx == i);
//                    if (::ImGui::Selectable(ids[i].c_str(), sel))
//                        testerSelectedIdx = i;
//                    if (sel) ::ImGui::SetItemDefaultFocus();
//                }
//                ::ImGui::EndCombo();
//            }
//
//            ::ImGui::SliderFloat("Volume##tst",  &testerVolume, 0.0f, 1.5f, "%.2f");
//            ::ImGui::Checkbox("Loop##tst",        &testerLoop);
//            ::ImGui::InputFloat("Start (s)##tst", &testerStart, 0.1f, 1.0f, "%.2f");
//            testerStart = std::max(0.0f, testerStart);
//
//            if (::ImGui::Button("Play##tst"))
//                testerLastHandle = engine->Play(
//                    ids[testerSelectedIdx], testerVolume, testerLoop, testerStart);
//
//            ::ImGui::SameLine();
//            if (::ImGui::Button("Stop Last##tst") && testerLastHandle != kInvalidHandle)
//            {
//                engine->Stop(testerLastHandle);
//                testerLastHandle = kInvalidHandle;
//            }
//
//            ::ImGui::SameLine();
//            if (::ImGui::Button("Stop All##tst"))
//                engine->StopAll();
//
//            if (testerLastHandle != kInvalidHandle)
//            {
//                ::ImGui::Text("Handle: %u  Playing: %s",
//                    testerLastHandle,
//                    engine->IsPlaying(testerLastHandle) ? "yes" : "no");
//            }
//        }
//
//        ::ImGui::EndTabItem();
//    }
//
//    ::ImGui::EndTabBar();
//    ::ImGui::End();
}

#endif // _DEBUG

} // namespace ozSound
