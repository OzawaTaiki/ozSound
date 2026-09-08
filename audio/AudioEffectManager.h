#pragma once

#include "AudioEffectDef.h"
#include "JsonUtils/JsonUtils.h"   // ozSound::json
#include "VST3/VST3ParameterManager.h"

#include <xaudio2.h>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>
#include <map>
#include <memory>

namespace ozSound
{

class VST3Module;
class VST3Plugin;

class AudioEffectChain;

/// <summary>
/// オーディオエフェクトの定義管理・生成・VST3モジュールキャッシュを担うクラス。
/// </summary>
class AudioEffectManager
{
public:

    static AudioEffectManager* GetInstance();

    void Initialize();
    void Finalize();

    /// <summary>
    /// JSON からエフェクト定義を読み込む。
    /// </summary>
    void LoadEffectData(const std::string& jsonPath);

    /// <summary>
    /// 既にパース済みの JSON からエフェクト定義を読み込む。
    /// LoadProject 統合経路で 1 度の parse を共有するための入口。
    /// </summary>
    void LoadEffectDataFromJson(const json& data);

    /// <summary>
    /// エフェクト定義を直接登録する（エディタ用: JSON を経由しない入口）。
    /// 既に同じ内容 (name/path/className/type) で登録済みのものは何もしないので、
    /// 毎フレーム呼んでも VST3 モジュールを読み直さない。
    /// path は呼び出し側で絶対パスに解決してから渡すこと。
    /// </summary>
    void LoadEffectDefs(const std::vector<AudioEffectDef>& defs);

    /// <summary>定義が登録済みか。</summary>
    bool HasEffect(const std::string& effectName) const;

    /// <summary>
    /// パラメータを名前で設定する (VST3 のみ)。値は正規化値 0.0〜1.0。
    /// 注意: パラメータは「エフェクト定義」単位で共有されるため、同じエフェクトを
    /// 複数のボイスで鳴らしている場合は最後に生成されたインスタンスにのみ効く。
    /// </summary>
    bool SetEffectParameterByName(const std::string& effectName,
                                  const std::string& paramName,
                                  double normalizedValue);

    /// <summary>
    /// パラメータの現在値 (正規化 0.0〜1.0) を名前で取得する (VST3 のみ)。
    /// 取得できたら true。
    /// </summary>
    bool GetEffectParameterByName(const std::string& effectName,
                                  const std::string& paramName,
                                  double* outNormalizedValue);

    /// <summary>
    /// エフェクトのパラメータ名一覧を返す (VST3 のみ)。
    /// プラグイン未ロード / Native の場合は空の配列。
    /// エディタでパラメータ名を選ばせるために使う。
    /// </summary>
    std::vector<std::string> GetParameterNames(const std::string& effectName);

    /// <summary>
    /// .vst3 モジュールに含まれるエフェクトクラス名の一覧を返す。
    /// エフェクト定義の登録状態とは無関係に、パスさえ開ければ取得できる。
    /// (className の指定が間違っていてプラグイン生成に失敗している状態でも
    ///  候補を列挙できるので、エディタでの指定ミス検出に使える)
    /// path は絶対パスで渡すこと。
    /// </summary>
    std::vector<std::string> GetModuleClassNames(const std::string& vst3Path);

    /// <summary>
    /// Native エフェクトをファクトリーに登録する。
    /// </summary>
    void RegisterNativeEffect(const std::string& name,
                              std::function<IUnknown* ()> creator);

    /// <summary>
    /// エフェクトIDリストから XAUDIO2_EFFECT_CHAIN を構築して返す。
    /// SoundEngine::Play() 内部で使用。
    /// </summary>
    /// <param name="effectIds">エフェクトIDのリスト</param>
    ozSound::AudioEffectChain BuildEffectChain(const std::vector<std::string>& effectNames);

    /// <summary>
    /// VST3 プラグインのパラメーターマネージャーを取得する。
    /// </summary>
    VST3ParameterManager* GetParameterManager(const std::string& effectName);


private:

    // VST3 プラグイン1つ分のエントリ
    struct VST3PluginEntry
    {
        VST3ParameterManager paramMgr;
        std::unique_ptr<VST3Plugin> plugin   = nullptr;
    };

    // VST3 モジュール（DLL）1つ分のエントリ
    struct VST3ModuleEntry
    {
        VST3Module* module = nullptr;
        std::map<std::string, std::unique_ptr<VST3PluginEntry>> plugins; // プラグイン名 → エントリ
    };

    VST3PluginEntry* GetVST3PluginEntry(const std::string& effectName);

    /// <summary>
    /// 定義を1件登録し、VST3 ならモジュール/プラグインを生成する。
    /// LoadEffectDataFromJson / LoadEffectDefs の共通実装。
    /// </summary>
    void RegisterEffectDef(const AudioEffectDef& def);

private:

    // エフェクト定義（JSON）
    std::unordered_map<std::string, AudioEffectDef>              effectDefs_;
    // VST3 モジュールキャッシュ（パス → エントリ）
    std::unordered_map<std::string, VST3ModuleEntry>             loadedModules_;
    // Native エフェクトファクトリー（名前 → 生成関数）
    std::unordered_map<std::string, std::function<IUnknown* ()>>  nativeFactories_;

private:
    AudioEffectManager()  = default;
    ~AudioEffectManager() = default;
    AudioEffectManager(const AudioEffectManager&)            = delete;
    AudioEffectManager& operator=(const AudioEffectManager&) = delete;
};

} // namespace ozSound
