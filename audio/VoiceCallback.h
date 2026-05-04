#pragma once

#include <xaudio2.h>

#include <functional>


namespace ozSound
{

class VoiceCallBack : public IXAudio2VoiceCallback
{
public:
    VoiceCallBack() = default;
    ~VoiceCallBack() = default;

    void OnVoiceProcessingPassStart([[maybe_unused]] UINT32 BytesRequired) override {}
    void OnVoiceProcessingPassEnd() override {}
    void OnStreamEnd() override { if (onStreamEndCallback_) onStreamEndCallback_(); }
    void OnBufferStart(void* pBufferContext) override { if (onBufferStartCallback_) onBufferStartCallback_(pBufferContext); }
    void OnBufferEnd(void* pBufferContext) override { if (onBufferEndCallback_) onBufferEndCallback_(pBufferContext); }
    void OnLoopEnd(void* pBufferContext) override { if (onLoopEndCallback_) onLoopEndCallback_(pBufferContext); }
    void OnVoiceError(void* pBufferContext, HRESULT Error) override { if (onVoiceErrorCallback_) onVoiceErrorCallback_(pBufferContext, Error); }

    void SetOnStreamEndCallback(std::function<void(void)> callback) { onStreamEndCallback_ = callback; }
    void SetOnBufferStartCallback(std::function<void(void*)> callback) { onBufferStartCallback_ = callback; }
    void SetOnBufferEndCallback(std::function<void(void*)> callback) { onBufferEndCallback_ = callback; }
    void SetOnLoopEndCallback(std::function<void(void*)> callback) { onLoopEndCallback_ = callback; }
    void SetOnVoiceErrorCallback(std::function<void(void*, HRESULT)> callback) { onVoiceErrorCallback_ = callback; }


private:

    std::function<void(void)> onStreamEndCallback_ = nullptr; // ストリーム終了時のコールバック
    std::function<void(void*)> onBufferStartCallback_ = nullptr; // バッファ開始時のコールバック
    std::function<void(void*)> onBufferEndCallback_ = nullptr; // バッファ終了時のコールバック
    std::function<void(void*)> onLoopEndCallback_ = nullptr; // ループ終了時のコールバック
    std::function<void(void*, HRESULT)> onVoiceErrorCallback_ = nullptr; // 音声エラー時のコールバック

};


/* At Claude

# XAudio2コールバック関数メモ

## void*引数（pBufferContext）について
- **バッファに関連付けたい任意のユーザーデータ**を渡す
- バッファ管理情報、ストリーミングコンテキスト、ゲームオブジェクトなど
- NULLも有効（データがない場合）
- **メモリ管理はユーザーの責任**
- **別スレッドから呼ばれる可能性あり**（スレッドセーフティ注意）

## 各コールバック関数の呼び出しタイミング

### OnVoiceProcessingPassStart(UINT32 BytesRequired)
- **タイミング**: オーディオエンジンの音声処理開始直前
- **頻度**: 20ms～40ms間隔（バッファサイズ依存）
- **用途**: リアルタイムエフェクト準備、DSP処理準備

### OnVoiceProcessingPassEnd()
- **タイミング**: 音声処理パス完了直後
- **頻度**: OnVoiceProcessingPassStartとペア
- **用途**: 処理後クリーンアップ、統計更新

### OnStreamEnd()
- **タイミング**: **全バッファ**再生完了、ストリーム全体終了時
- **頻度**: ストリーム終了時に1回のみ
- **用途**: BGM終了処理、次曲への切り替え

### OnBufferStart(void* pBufferContext)
- **タイミング**: 個別バッファの再生**開始**時
- **頻度**: キューした各バッファごとに1回
- **用途**: 再生開始ログ、同期処理

### OnBufferEnd(void* pBufferContext) ★最重要
- **タイミング**: 個別バッファの再生**完了**時
- **頻度**: 各バッファごとに1回（最も頻繁に使用）
- **用途**: 次バッファをキュー、ストリーミング継続、メモリ解放

### OnLoopEnd(void* pBufferContext)
- **タイミング**: ループ設定バッファが1周完了時
- **頻度**: LoopCountで指定した回数分
- **用途**: ループカウント管理、ループ終了処理

### OnVoiceError(void* pBufferContext, HRESULT Error)
- **タイミング**: 音声処理中エラー発生時
- **頻度**: エラー発生時のみ
- **用途**: エラーハンドリング、ログ出力、復旧処理

## 実行順序例
1. OnVoiceProcessingPassStart()
2. OnBufferStart() （バッファA開始）
3. OnVoiceProcessingPassEnd()
4. （音声再生中...）
5. OnVoiceProcessingPassStart()
6. OnBufferEnd() （バッファA終了） ← **ここで次バッファをキュー**
7. OnBufferStart() （バッファB開始）
8. OnVoiceProcessingPassEnd()
9. （全バッファ終了後）
10. OnStreamEnd()

## 重要ポイント
- **OnBufferEnd**が最も重要（ストリーミング継続処理）
- pBufferContextはXAUDIO2_BUFFER構造体のpContextに設定した値
- コールバックは音声スレッドから呼ばれるため軽量な処理にする
- メモリ管理（new/delete）はユーザー側で適切に行う

*/

} // namespace ozSound
