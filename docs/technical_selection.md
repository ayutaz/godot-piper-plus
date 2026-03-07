# 技術選定ドキュメント

godot-piper-plus の実装に向けた技術調査結果と選定方針。

---

## 1. ビルドシステム: CMake

### 選定: CMake単独（SConsは不使用）

| 評価項目 | SCons | CMake |
|---------|-------|-------|
| godot-cpp統合 | 公式プライマリ | 公式セカンダリ（4.3+で安定） |
| OpenJTalk/HTSEngineビルド | 事前ビルド必要（高難度） | ExternalProject_Addで自動（piper-plus実績あり） |
| piper-phonemize/espeak-ng | autotools統合が極めて困難 | ExternalProject_Addで自動 |
| ONNX Runtimeリンク | 手動パス指定 | find_package / プリビルト検索 |
| IDE連携 | 弱い | CLion/VSCode CMake Tools/VS |
| MSVCランタイム | `/MT`（ONNX Runtimeと不整合） | `/MD`（ONNX Runtimeと一致） |

**決定理由:**
- piper-plusのCMakeLists.txt（855行）にOpenJTalk/HTSEngine/piper-phonemize/espeak-ng/ONNX Runtimeの全依存統合パターンが検証済みで存在し、流用可能
- 5つの外部C/C++ライブラリの自動ビルド・リンクはSConsでは現実的でない
- godot-cppのCMakeサポートは公式ドキュメント化済み、`godot::cpp`ターゲットとして利用可能

### 参考テンプレート

| テンプレート | URL |
|-------------|-----|
| asmaloney/GDExtensionTemplate (CMake) | https://github.com/asmaloney/GDExtensionTemplate |
| godot-cpp-template (SCons, CI参考) | https://github.com/godotengine/godot-cpp-template |
| joemarshall/godot_onnx_extension | https://github.com/joemarshall/godot_onnx_extension |

### CMakeLists.txt骨格

```cmake
cmake_minimum_required(VERSION 3.20)
project(godot_piper_plus CXX C)
set(CMAKE_CXX_STANDARD 17)

# MSVC runtime (ONNX Runtime互換)
if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL$<$<CONFIG:Debug>:Debug>")
endif()

# godot-cpp (submodule)
add_subdirectory(thirdparty/godot-cpp)

# 外部依存 (piper-plus CMakeパターン流用)
include(cmake/HTSEngine.cmake)       # ExternalProject_Add
include(cmake/OpenJTalk.cmake)        # ExternalProject_Add
include(cmake/PiperPhonemize.cmake)   # ExternalProject_Add
include(cmake/FindOnnxRuntime.cmake)  # プリビルト検索

# GDExtension
add_library(piper_plus SHARED
  src/register_types.cpp
  src/piper_tts.cpp
  src/phoneme_encoder.cpp
)
target_link_libraries(piper_plus PUBLIC godot::cpp onnxruntime)
# OpenJTalk/HTSEngine/piper-phonemize は静的リンク
```

---

## 2. Godotバージョン: 4.4 (compatibility_minimum)

### Godot 4.x リリース・サポート状況（2026年3月調査）

| バージョン | リリース日 | EOL日 | 状態 |
|-----------|----------|-------|------|
| 4.0 | 2023-03-01 | 2023-11-29 | EOL |
| 4.1 | 2023-07-05 | 2025-03-03 | EOL |
| 4.2 | 2023-11-29 | 2025-03-03 | EOL |
| 4.3 | 2024-08-15 | 2025-10-09 | EOL |
| 4.4 | 2025-03-03 | 2025-10-23 | EOL |
| **4.5** | 2025-09-15 | - | **Active** |
| **4.6** | 2026-01-26 | - | **Active（最新安定: 4.6.1）** |

出典: [endoflife.date/godot](https://endoflife.date/godot)

### GDExtension API互換性の調査

| バージョン境界 | GDExtension影響 |
|-------------|---------------|
| 4.2 → 4.3 | **重大**: `GDExtension`クラスの3メソッド削除、`GDExtensionManager`に移行 |
| 4.3 → 4.4 | 軽微: `FileAccess`戻り値型変更等（GDExtensionロード機構は変更なし） |
| 4.4 → 4.5 | 軽微 |
| 4.5 → 4.6 | 軽微 |

出典: [Godot 4.x Breaking Changes](https://gist.github.com/raulsntos/06ac5dd10ebccc3a4f1e7e3ad30dc876)

### 主要GDExtensionプラグインの対応状況

| プラグイン | compatibility_minimum | 備考 |
|-----------|---------------------|------|
| GodotSteam | **4.4** | 4.1-4.3版はDeprecated |
| godot-rust | 4.2 | 「保守的に低いバージョンで始める」方針 |
| spine-godot | 4.3/4.4 | バージョン別ビルド |

出典: [GodotSteam GDExtension 4.4+](https://godotassetlibrary.com/asset/j4zWKK/godotsteam-gdextension-4.4+), [godot-rust: Selecting a Godot version](https://godot-rust.github.io/book/toolchain/godot-version.html)

### 選定: 4.4

**決定理由:**
- GDExtensionは下位互換: 4.4でビルドすれば4.5/4.6以降全てで動作
- GodotSteam（最も利用されているGDExtensionプラグイン）が4.4+を現行ターゲットにしている実績
- 4.4→4.5→4.6間のGDExtension API変更は軽微（4.2→4.3の重大変更以降は安定）
- 4.3以前はGDExtension API破壊的変更があり、対応コストが高い
- godot-cpp v10.xの`api_version`パラメータで柔軟にターゲット可能
- XMLドキュメントシステム（`GodotCPPDocData`）が4.3+で利用可能

### godot-cpp管理

```bash
git submodule add https://github.com/godotengine/godot-cpp.git thirdparty/godot-cpp
cd thirdparty/godot-cpp && git checkout <v10.x tag>  # タグでピン留め
```

---

## 3. ONNX Runtime: v1.24.x / 動的リンク

### バージョン選定: 1.24.x（最新安定）

- piper-plusのopset 15は完全対応（ONNX Runtime 1.24はopset 7-21+サポート）
- VITSモデルの演算子は全て標準セット（特殊オペレータ不要）
- Android 15+ の16KBページサイズ要件に対応済み（1.23.2以降）

### リンク方式

| プラットフォーム | リンク方式 | 配布形式 |
|----------------|-----------|---------|
| Windows x86_64 | 動的 | `onnxruntime.dll` (.gdextension dependencies) |
| Linux x86_64 | 動的 | `libonnxruntime.so` |
| macOS arm64 | 動的 | `onnxruntime.framework` |
| Android arm64 | 動的 | `libonnxruntime.so` |
| iOS arm64 | **静的** | GDExtension .a に組込（Apple制約） |

### プリビルトバイナリの入手

| ソース | 用途 |
|-------|------|
| GitHub Releases (`microsoft/onnxruntime`) | デスクトップ向け |
| NuGet (`Microsoft.ML.OnnxRuntime`) | Windows向け |
| Maven Central (`onnxruntime-android`) | Android向け |
| 自前ビルド / olilarkin/ort-builder | iOS向け静的 / サイズ最適化 |

### サイズ推定

| プラットフォーム | ONNX Runtime | GDExtension本体 | 合計 |
|----------------|-------------|----------------|------|
| Windows | ~15MB | ~10MB | ~25MB |
| Linux | ~14MB | ~8MB | ~22MB |
| Android | ~12MB | ~8MB | ~20MB |
| iOS | 静的リンクで一体 | ~20MB | ~20MB |

### GPU推論（Phase 2以降のオプション）

CPU推論でRTF ~0.2（1秒の音声を0.2秒で生成）のため、GPU EPはオプション扱い。

| プラットフォーム | Execution Provider | 優先度 |
|----------------|-------------------|-------|
| 全プラットフォーム | CPU | Phase 1（必須） |
| iOS/macOS | CoreML | Phase 2 |
| Windows | DirectML | Phase 2 |
| Android | NNAPI | Phase 2 |
| Linux | CUDA | Phase 3（オプション） |

### piper-plusの推論設定（そのまま流用）

```cpp
session.options.SetGraphOptimizationLevel(ORT_DISABLE_ALL);  // 有効化するとロード時間2倍、速度改善なし
session.options.DisableCpuMemArena();
session.options.DisableMemPattern();
```

---

## 4. 音声出力: AudioStreamWAV（Phase 1）

### 方式比較

| 項目 | AudioStreamWAV | AudioStreamGenerator |
|------|---------------|---------------------|
| 実装複雑度 | 低（5-10行） | 高（バッファ管理必要） |
| レイテンシ | 推論完了まで待つ | 逐次再生可能 |
| メモリ | 全データ保持 | バッファ分のみ |
| スレッドセーフティ | 問題なし | RingBufferが非スレッドセーフ |
| サンプルレート変換 | Godot自動（22050→44100） | mix_rate設定で対応 |
| uPiperとの対応 | AudioClipBuilder直接対応 | - |

### 選定: 二段階実装

**Phase 1: AudioStreamWAV + 非ストリーミング**
```cpp
Ref<AudioStreamWAV> create_audio_stream(const std::vector<int16_t>& buf, int rate) {
    Ref<AudioStreamWAV> stream;
    stream.instantiate();
    stream->set_format(AudioStreamWAV::FORMAT_16_BITS);
    stream->set_mix_rate(rate);  // 22050
    stream->set_stereo(false);
    PackedByteArray data;
    data.resize(buf.size() * sizeof(int16_t));
    memcpy(data.ptrw(), buf.data(), data.size());
    stream->set_data(data);
    return stream;
}
```

**Phase 2: AudioStreamGenerator + 文単位ストリーミング**
- piper-plusの`textToAudioStreaming()`のchunkCallbackを活用
- ワーカースレッド → ロックフリーキュー → メインスレッド `_process()` でpush_buffer()

---

## 5. スレッドモデル: std::thread + call_deferred

### 推論スレッド

VITS推論は数百ms～数秒かかるため、**必ず別スレッドで実行**する。

| 方式 | 評価 |
|------|------|
| `std::thread` / `std::async` | 推奨。piper-plus既存コードと自然に統合 |
| Godot `Thread` クラス | 可能だがCallableラッパーの複雑さ |
| `WorkerThreadPool` | 長時間タスクにはプール枯渇リスク |

### シグナル送信パターン

```cpp
void PiperTTS::synthesize_async(const String &text) {
    // WorkerThreadPool or std::thread
    std::thread([this, text]() {
        std::vector<int16_t> audioBuffer;
        SynthesisResult result;
        piper::textToAudio(config, voice, text.utf8().get_data(), audioBuffer, result, nullptr);
        Ref<AudioStreamWAV> stream = create_audio_stream(audioBuffer, voice.synthesisConfig.sampleRate);
        call_deferred("_on_synthesis_done", stream);  // メインスレッドへ
    }).detach();
}

void PiperTTS::_on_synthesis_done(Ref<AudioStreamWAV> stream) {
    emit_signal("synthesis_completed", stream);  // メインスレッドで安全に発火
}
```

**注意:** Godot 4.4以前では`emit_signal()`をワーカースレッドから直接呼ぶのは安全ではない。必ず`call_deferred()`を経由する。

---

## 6. GDScript API設計

### PiperTTSノード

```gdscript
# 基本使用例
@onready var tts = $PiperTTS

func _ready():
    tts.synthesis_completed.connect(_on_done)
    tts.model_path = "res://addons/piper_plus/models/tsukuyomi-chan.onnx"
    tts.config_path = "res://addons/piper_plus/models/tsukuyomi-chan.onnx.json"
    tts.dictionary_path = "res://addons/piper_plus/dictionaries/naist-jdic"
    tts.initialize()

func speak(text: String):
    tts.synthesize_async(text)

func _on_done(audio: AudioStreamWAV):
    $AudioStreamPlayer.stream = audio
    $AudioStreamPlayer.play()
```

### プロパティ（Inspector UI対応）

| プロパティ | 型 | Inspector Hint | デフォルト |
|-----------|---|---------------|----------|
| `model_path` | String | PROPERTY_HINT_FILE, "*.onnx" | "" |
| `config_path` | String | PROPERTY_HINT_FILE, "*.json" | "" |
| `dictionary_path` | String | PROPERTY_HINT_DIR | "" |
| `speaker_id` | int | - | 0 |
| `speech_rate` | float | PROPERTY_HINT_RANGE, "0.5,2.0,0.1" | 1.0 |
| `noise_scale` | float | PROPERTY_HINT_RANGE, "0.0,1.0,0.01" | 0.667 |
| `noise_w` | float | PROPERTY_HINT_RANGE, "0.0,1.0,0.01" | 0.8 |

### メソッド

| メソッド | 戻り値 | 説明 |
|---------|-------|------|
| `initialize()` | Error | モデル・辞書ロード |
| `synthesize(text)` | AudioStreamWAV | 同期合成 |
| `synthesize_async(text)` | void | 非同期合成（シグナル通知） |
| `get_phonemes(text)` | PackedStringArray | 音素取得 |
| `stop()` | void | 合成中止 |
| `is_ready()` | bool | 初期化済みか |
| `is_processing()` | bool | 合成中か |

### シグナル

| シグナル | パラメータ | 説明 |
|---------|----------|------|
| `synthesis_completed` | audio: AudioStreamWAV | 合成完了 |
| `synthesis_failed` | error: String | エラー |
| `initialized` | success: bool | 初期化完了 |

---

## 7. 配布戦略

### プラグイン構成

```
addons/piper_plus/
├── piper_plus.gdextension           # GDExtension設定
├── plugin.cfg                       # EditorPlugin設定
├── icon.svg                         # エディタアイコン
├── bin/                             # プラットフォーム別バイナリ
│   ├── windows/
│   │   ├── libpiper_plus.windows.template_release.x86_64.dll
│   │   └── onnxruntime.dll
│   ├── linux/
│   │   ├── libpiper_plus.linux.template_release.x86_64.so
│   │   └── libonnxruntime.so
│   ├── macos/
│   │   └── libpiper_plus.macos.template_release.framework/
│   ├── android/
│   │   └── libpiper_plus.android.template_release.arm64.so
│   └── ios/
│       └── libpiper_plus.ios.template_release.arm64.a
├── models/                          # ONNXモデル（別途配布）
├── dictionaries/                    # OpenJTalk辞書（別途配布）
└── doc_classes/                     # XMLドキュメント
```

### モデル・辞書の配布

**プラグイン本体（bin/ + ONNX Runtime）とモデル/辞書は分離配布:**

| 配布物 | 配布方式 | 推定サイズ |
|-------|---------|----------|
| プラグイン本体 | GitHub Release zip / Asset Library | ~100-200MB（全プラットフォーム） |
| 日本語モデル (tsukuyomi-chan) | GitHub Release 別zip | ~数十MB |
| 英語モデル (ljspeech) | GitHub Release 別zip | ~数十MB |
| OpenJTalk辞書 (naist-jdic) | GitHub Release 別zip | ~23MB（圧縮） |

### Godot Asset Library登録

- Repository host: **Custom**（GitHub Release URLを指定）
- バイナリを含むGDExtensionはCustom URL方式が標準（GodotSteam等で実績あり）

---

## 8. CI/CD構成

### GitHub Actions ワークフロー

```yaml
# .github/workflows/build.yml
strategy:
  matrix:
    include:
      - {os: windows-latest, platform: windows, arch: x86_64}
      - {os: ubuntu-latest,  platform: linux,   arch: x86_64}
      - {os: macos-latest,   platform: macos,   arch: arm64}
      - {os: ubuntu-latest,  platform: android, arch: arm64}
      - {os: macos-latest,   platform: ios,     arch: arm64}

steps:
  1. checkout with submodules
  2. setup build tools (cmake, NDK, Xcode)
  3. download/cache prebuilt ONNX Runtime
  4. cmake configure + build (ExternalProject_Add で OpenJTalk等自動ビルド)
  5. upload artifacts

# .github/workflows/release.yml (tag push時)
  - 全プラットフォームのartifactsを収集
  - addons/piper_plus/ にパッケージ
  - GitHub Release作成、zipをassetとして添付
```

---

## 9. 実装フェーズ

| Phase | 内容 | 依存 |
|-------|------|------|
| **1** | CMakeビルドシステム構築（godot-cpp + piper-plus依存統合） | - |
| **2** | PiperTTSノード基本実装（同期合成 + AudioStreamWAV） | Phase 1 |
| **3** | 非同期合成（std::thread + call_deferred + シグナル） | Phase 2 |
| **4** | GitHub Actions CI/CD（全プラットフォーム自動ビルド） | Phase 1 |
| **5** | モデル/辞書ダウンロード機能 + XMLドキュメント | Phase 2 |
| **6** | ストリーミング合成（AudioStreamGenerator） | Phase 3 |
| **7** | GPU推論EP対応（CoreML/DirectML/NNAPI） | Phase 3 |
| **8** | Asset Library登録 + エディタUI | Phase 4,5 |

---

## 参考プロジェクト一覧

| プロジェクト | URL | 参考ポイント |
|-------------|-----|------------|
| godot-kokoro (TTS GDExtension) | https://github.com/PhilNikitin/godot-kokoro | TTS API設計、モデル配布パターン |
| godot-whisper (STT GDExtension) | https://github.com/V-Sekai/godot-whisper | エディタ内ダウンロード、スレッドモデル |
| godot_onnx_extension | https://github.com/joemarshall/godot_onnx_extension | ONNX Runtime + GDExtension統合 |
| GDExtensionTemplate (CMake) | https://github.com/asmaloney/GDExtensionTemplate | CMakeビルドテンプレート |
| godot-cpp-template (SCons, CI) | https://github.com/godotengine/godot-cpp-template | CI/CDワークフロー参考 |
| nathanfranke/gdextension | https://github.com/nathanfranke/gdextension | Asset Library配布パターン |
| oparisy/gdextension-module | https://github.com/oparisy/gdextension-module | カスタムAudioStream実装 |
| GodotSteam | https://godotengine.org/asset-library/asset/2445 | Custom URL配布の実績 |
| VOICEVOX/onnxruntime-builder | https://github.com/VOICEVOX/onnxruntime-builder | TTS向けONNX Runtimeカスタムビルド |
