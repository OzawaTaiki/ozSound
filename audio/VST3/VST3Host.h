#pragma once

#include <string>
#include <map>
#include <memory>

#include <public.sdk/source/vst/hosting/hostclasses.h>

namespace ozSound
{

class VST3Module;

// VST3環境全体の管理者。シングルトン。
// HostApplicationの管理とモジュール（DLL）の生存管理を担う
class VST3Host
{
public:
    static VST3Host* GetInstance();

    // 初期化
    bool Initialize(const std::string& hostName = "GameEngine");

    // 終了処理
    void Finalize();

    // .vst3ファイルをロードしてVST3Moduleを返す
    // 同じパスのモジュールは再利用する
    VST3Module* LoadModule(const std::string& path);

    // モジュールをアンロードする
    void UnloadModule(const std::string& path);

    Steinberg::Vst::HostApplication* GetHostApp();

private:

    // ホスト情報をプラグインに提供するオブジェクト
    std::unique_ptr<Steinberg::Vst::HostApplication> hostApp_;

    // ロード済みモジュールの管理（パス→モジュール）
    std::map<std::string, std::unique_ptr<VST3Module>> modules_;

private:
    VST3Host() = default;
    ~VST3Host() = default;

    VST3Host(const VST3Host&) = delete;
    VST3Host& operator=(const VST3Host&) = delete;
};

} // namespace ozSound