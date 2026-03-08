# マイルストーン計画

godot-piper-plus の実装マイルストーン。技術選定（`docs/technical_selection.md`）に基づく。

---

## 全体概要

```
M1 CMakeビルドシステム ──→ M2 基本TTS実装 ──→ M3 非同期合成
       │                        │                    │
       └──→ M4 CI/CD ───────────┘                    │
                                 │                    │
                            M5 モデル管理        M6 ストリーミング
                            + ドキュメント            │
                                 │              M7 GPU推論EP
                                 │                    │
                                 └────→ M8 Asset Library + エディタUI
```

### フェーズサマリー

| マイルストーン | 内容 | 依存 | 完了基準 | 状態 |
|-------------|------|------|---------|------|
| **M1** | CMakeビルドシステム構築 | - | GDExtensionがGodotにロードされる | ✅ 完了 |
| **M2** | PiperTTSノード基本実装 | M1 | GDScriptから同期合成でき、音声が再生される | ✅ 完了 |
| **M3** | 非同期合成 + シグナル | M2 | synthesize_async()でUIをブロックせず音声生成 | ✅ 完了 |
| **M4** | CI/CD（全プラットフォーム） | M1 | GitHub Actionsで5プラットフォームの自動ビルド | ✅ 完了 |
| **M5** | モデル/辞書管理 + ドキュメント | M2 | XMLドキュメント表示、モデルダウンロード機能 | ✅ 完了 |
| **M6** | ストリーミング合成 | M3 | AudioStreamGeneratorで逐次再生 | ✅ 完了 |
| **M7** | GPU推論EP | M3 | CoreML/DirectML/NNAPIでの推論動作 | 未着手 |
| **M8** | Asset Library + エディタUI | M4,M5 | Godot Asset Libraryから導入可能 | 未着手 |

---

## M1: CMakeビルドシステム構築

**目標:** godot-cpp + piper-plus依存ライブラリを統合し、空のGDExtensionがGodotにロードされる状態にする。

### タスク

| # | タスク | 成果物 | 詳細 |
|---|-------|--------|------|
| 1.1 | godot-cppサブモジュール追加 | `.gitmodules`, `thirdparty/godot-cpp/` | v10.xタグでピン留め、compatibility_minimum=4.4 |
| 1.2 | ルートCMakeLists.txt作成 | `CMakeLists.txt` | cmake_minimum_required(3.20), C++17, MSVC `/MD` 設定 |
| 1.3 | godot-cpp CMake統合 | `CMakeLists.txt` 内 | `add_subdirectory(thirdparty/godot-cpp)`, `godot::cpp` ターゲット |
| 1.4 | HTSEngine ExternalProject | `cmake/HTSEngine.cmake` | piper-plus CMakeLists.txt パターン流用、v1.10 |
| 1.5 | OpenJTalk ExternalProject | `cmake/OpenJTalk.cmake` | HTSEngine依存、v1.11、静的リンク |
| ~~1.6~~ | ~~piper-phonemize ExternalProject~~ | ~~`cmake/PiperPhonemize.cmake`~~ | **削除: GPL-3.0ライセンス汚染のため不使用。代わりに`phoneme_ids.hpp`を自前実装** |
| 1.7 | ONNX Runtime検索モジュール | `cmake/FindOnnxRuntime.cmake` | プリビルトバイナリ検索、v1.24.x |
| 1.8 | 最小GDExtension実装 | `src/register_types.cpp` | 空のGDExtensionエントリポイント |
| 1.9 | .gdextensionファイル | `addons/piper_plus/piper_plus.gdextension` | 全プラットフォーム定義、ONNX Runtime依存設定 |
| 1.10 | ローカルビルド検証 | - | macOS arm64でビルド→Godotにロード確認 |

### piper-plus既存CMakeからの流用箇所

| piper-plus CMakeLists.txt | 流用先 | 変更点 |
|--------------------------|--------|--------|
| `ExternalProject_Add(hts_engine_api ...)` | `cmake/HTSEngine.cmake` | そのまま流用 |
| `ExternalProject_Add(open_jtalk ...)` | `cmake/OpenJTalk.cmake` | そのまま流用 |
| ~~`ExternalProject_Add(piper_phonemize ...)`~~ | ~~`cmake/PiperPhonemize.cmake`~~ | **削除: GPL汚染のため不使用** |
| ONNX Runtime検出ロジック | `cmake/FindOnnxRuntime.cmake` | find_packageパターンに整理 |
| MSVC `/utf-8`, `/MD` 設定 | `CMakeLists.txt` | そのまま流用 |

### 完了基準

- [x] `cmake -B build && cmake --build build` がエラーなく完了する
- [x] OpenJTalk, HTSEngine が静的リンクされている（piper-phonemizeはGPL汚染のため除外）
- [x] ONNX Runtime が動的リンクされている
- [x] Godotエディタで `.gdextension` ファイルが認識される
- [x] `GDExtensionManager` にプラグインが表示される

---

## M2: PiperTTSノード基本実装

**目標:** GDScriptから `PiperTTS.synthesize(text)` で音声を生成し、`AudioStreamPlayer` で再生できる。

### タスク

| # | タスク | 成果物 | 詳細 |
|---|-------|--------|------|
| 2.1 | piper-plus C++コア統合 | `src/piper_core/` | piper.cpp/hpp, json.hpp をプロジェクトに配置・ビルド統合 |
| 2.2 | OpenJTalk音素化統合 | `src/piper_core/` | openjtalk_phonemize.cpp/hpp, openjtalk_wrapper.c/h |
| 2.3 | カスタム辞書統合 | `src/piper_core/` | custom_dictionary.cpp/hpp |
| 2.4 | 音素パーサ統合 | `src/piper_core/` | phoneme_parser.cpp/hpp |
| 2.5 | PiperTTSノード実装 | `src/piper_tts.cpp/h` | Nodeを継承、プロパティ/メソッド/シグナル定義 |
| 2.6 | AudioStreamWAV生成 | `src/piper_tts.cpp` 内 | int16_t配列 → PackedByteArray → AudioStreamWAV |
| 2.7 | GDExtension登録更新 | `src/register_types.cpp` | PiperTTSクラスのClassDB登録 |
| 2.8 | Godotパス解決 | `src/piper_tts.cpp` 内 | `res://` → OS絶対パス変換（モデル/辞書ロード用） |
| 2.9 | デモプロジェクト | `demo/` | project.godot, 最小限のGDScriptデモシーン |
| 2.10 | 動作検証 | - | 日本語テキスト→音声再生の確認 |

### piper-plus C++コード統合方式

piper-plusの `src/cpp/` からGDExtensionに必要なファイルを直接コピーし、`src/piper_core/` に配置する。piper-plusのmain.cppやCLI固有コードは不要。

**統合対象ファイル:**

| piper-plus ファイル | 行数 | GDExtension側の変更 |
|-------------------|------|-------------------|
| `piper.cpp` | 998 | espeak-ng/tashkeel除去、spdlog→no-opシムヘッダ、phoneme_ids.hpp自前実装に差替 |
| `piper.hpp` | 修正 | piper-phonemize/phoneme_ids.hpp→自前phoneme_ids.hpp、eSpeakConfig/tashkeel除去、PhonemeType再番号付け |
| `openjtalk_phonemize.cpp/hpp` | 244 | spdlogインクルードパスをローカルシムに変更 |
| `custom_dictionary.cpp/hpp` | 536 | そのまま |
| `phoneme_parser.cpp/hpp` | 219 | spdlogインクルードパスをローカルシムに変更 |
| `openjtalk_wrapper.c/h` | 1,095 | そのまま |
| `openjtalk_error.c/h` | - | そのまま |
| `openjtalk_dictionary_manager.c/h` | - | そのまま |
| `openjtalk_security.c/h` | - | そのまま |
| `json.hpp` | - | header-only、そのまま |
| `utf8.h` + `utf8/` | - | そのまま |
| `wavfile.hpp` | - | 不要（Godot AudioStreamWAVを使用） |
| `main.cpp` | - | 不要（CLI用） |
| `test.cpp` | - | 不要 |

**GDExtension新規コード（追加ファイル含む）:**

| ファイル | 行数 | 内容 |
|---------|------|------|
| `src/piper_tts.h` | 103 | PiperTTSクラス定義（Node継承、同期+非同期API） |
| `src/piper_tts.cpp` | 403 | 初期化、同期/非同期合成、スレッド管理、プロパティバインド |
| `src/register_types.cpp` | 39 | ClassDB登録 |
| `src/piper_core/phoneme_ids.hpp` | 新規 | piper-phonemize代替（MIT互換、型定義+phonemes_to_ids） |
| `src/piper_core/spdlog/spdlog.h` | 新規 | spdlog no-opシム（83箇所のspdlog呼び出しを無改変で対応） |

### PiperTTSノード API（M2スコープ）

```cpp
// プロパティ
String model_path;         // ONNX モデルファイル
String config_path;        // .onnx.json 設定ファイル
String dictionary_path;    // OpenJTalk辞書ディレクトリ
int speaker_id = 0;
float speech_rate = 1.0f;  // lengthScale
float noise_scale = 0.667f;
float noise_w = 0.8f;

// メソッド（M2: 同期のみ）
Error initialize();
Ref<AudioStreamWAV> synthesize(const String &text);
bool is_ready() const;

// シグナル（M2: initialized のみ）
signal initialized(success: bool)
```

### 完了基準

- [x] `PiperTTS` ノードがGodotエディタのノード追加ダイアログに表示される
- [x] Inspectorでmodel_path等のプロパティが設定可能
- [x] GDScriptから `tts.initialize()` → `tts.synthesize("こんにちは")` で `AudioStreamWAV` が返る
- [x] 返された `AudioStreamWAV` を `AudioStreamPlayer` で再生し、音声が聞こえる
- [x] 日本語モデルの動作確認
- ~~英語モデルの動作確認~~ → espeak-ng(GPL)除去によりTextPhonemes無効化。英語G2PはFlite LTS実装（将来M）で対応予定

---

## M3: 非同期合成 + シグナル

**目標:** 推論をワーカースレッドで実行し、メインスレッドをブロックしない音声合成。

### タスク

| # | タスク | 成果物 | 詳細 |
|---|-------|--------|------|
| 3.1 | ワーカースレッド実装 | `src/piper_tts.cpp` | `std::thread` で推論実行 |
| 3.2 | call_deferred連携 | `src/piper_tts.cpp` | 推論完了後メインスレッドへ結果転送 |
| 3.3 | シグナル実装 | `src/piper_tts.cpp` | `synthesis_completed`, `synthesis_failed` |
| 3.4 | synthesize_async() | `src/piper_tts.cpp` | 非同期APIメソッド |
| 3.5 | stop()実装 | `src/piper_tts.cpp` | 合成中止（std::atomic フラグ） |
| 3.6 | is_processing() | `src/piper_tts.cpp` | 合成中状態問い合わせ |
| 3.7 | スレッド安全性テスト | `demo/` | 連続呼び出し、UI操作中の合成テスト |

### スレッドモデル詳細

```
メインスレッド                              ワーカースレッド
    │                                          │
    ├─ synthesize_async("text")                │
    │   ├─ if processing → return ERR_BUSY     │
    │   ├─ _join_worker_thread() (前回分join)    │
    │   ├─ processing = true                   │
    │   ├─ stop_requested = false              │
    │   └─ std::make_unique<std::thread> ──→  ├─ _synthesis_thread_func()
    │                                          │   ├─ piper::textToAudio()
    │   (UIは自由に操作可能)                     │   │   (数百ms～数秒)
    │                                          │   ├─ stop_requested チェック
    │                                          │   ├─ create_audio_stream()
    │   ← call_deferred("_on_synthesis_done") ─┤   └─ processing = false
    ├─ _on_synthesis_done()                    │
    │   └─ emit_signal("synthesis_completed")  │
    │                                          │
    ├─ stop()                                  │
    │   ├─ stop_requested = true               │
    │   └─ worker_thread->join() ─────────→  (終了待ち)
    │
    ├─ ~PiperTTS()
    │   ├─ stop_requested = true
    │   └─ _join_worker_thread()
```

**スレッド安全性の設計:**
- `std::unique_ptr<std::thread>`: detachせずjoinで確実にスレッド終了を管理
- `std::atomic<bool> processing`: 二重起動を `ERR_BUSY` で防止
- `std::atomic<bool> stop_requested`: 合成中止フラグ（例外発生時もチェック）
- `call_deferred()`: ワーカースレッドからメインスレッドへの安全な結果転送
- デストラクタ: stop_requested設定 + join で安全な破棄を保証

### 完了基準

- [x] `synthesize_async()` 呼び出し中にGodot UIがフリーズしない（std::thread + call_deferred実装済み）
- [x] `synthesis_completed` シグナルが正しく発火する（call_deferredでメインスレッドから発火）
- [x] `synthesis_failed` シグナルが正しく発火する
- [x] `stop()` で合成が中断できる（std::atomic<bool> stop_requestedフラグ + thread join）
- [x] 連続して `synthesize_async()` を呼んでもクラッシュしない（processing中はERR_BUSY返却）
- [x] 実機テスト: Godotエディタでの非同期合成動作確認

---

## M4: CI/CD（全プラットフォーム自動ビルド）

**目標:** GitHub Actionsで5プラットフォームの自動ビルドとリリースパッケージング。

### タスク

| # | タスク | 成果物 | 詳細 |
|---|-------|--------|------|
| 4.1 | ビルドワークフロー | `.github/workflows/build.yml` | matrix: windows/linux/macos/android/ios |
| 4.2 | ONNX Runtimeキャッシュ | `build.yml` 内 | actions/cacheでプリビルトバイナリキャッシュ |
| 4.3 | Windows x86_64ビルド | `build.yml` 内 | MSVC + CMake |
| 4.4 | Linux x86_64ビルド | `build.yml` 内 | GCC/Clang + CMake |
| 4.5 | macOS arm64ビルド | `build.yml` 内 | Clang + CMake |
| 4.6 | Android arm64ビルド | `build.yml` 内 | NDK + CMake ツールチェーン |
| 4.7 | iOS arm64ビルド | `build.yml` 内 | Xcode + CMake ツールチェーン |
| 4.8 | リリースワークフロー | `.github/workflows/release.yml` | tag push → artifacts収集 → GitHub Release |
| 4.9 | アーティファクトパッケージ | `release.yml` 内 | addons/piper_plus/bin/ に全バイナリ配置 → zip |

### ビルドマトリクス

```yaml
strategy:
  matrix:
    include:
      - {os: windows-latest, platform: windows, arch: x86_64, cmake_args: ""}
      - {os: ubuntu-22.04,   platform: linux,   arch: x86_64, cmake_args: ""}
      - {os: macos-latest,   platform: macos,   arch: arm64,  cmake_args: ""}
      - {os: ubuntu-22.04,   platform: android, arch: arm64,  cmake_args: "-DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake -DANDROID_ABI=arm64-v8a"}
      - {os: macos-latest,   platform: ios,     arch: arm64,  cmake_args: "-DCMAKE_TOOLCHAIN_FILE=cmake/ios.toolchain.cmake"}
```

### 完了基準

- [x] 5プラットフォーム全てのビルドワークフローが作成済み（Windows/Linux/macOS/Android/iOS）
- [x] タグpush時にGitHub Releaseが自動作成されるワークフロー（release.yml）
- [x] Release zipにaddon構成のバイナリが含まれる（artifacts収集 + zip）
- [x] ExternalProject依存キャッシュとONNX Runtimeキャッシュ設定済み

**実装詳細:**
- `.github/workflows/build.yml`: 5プラットフォームmatrixビルド、ExternalProject/ONNX Runtimeキャッシュ
- `.github/workflows/release.yml`: tag push → build.yml呼び出し → artifacts収集 → GitHub Release作成
- `CMakeLists.txt`: iOS静的ライブラリ、Android NDKツールチェーン伝播、EXTERNAL_CMAKE_ARGS
- `cmake/HTSEngine.cmake`: Android/iOSのCMakeビルドパス追加

---

## M5: モデル/辞書管理 + ドキュメント

**目標:** エディタ内からモデル・辞書のダウンロードができ、XMLクラスドキュメントが表示される。

### タスク

| # | タスク | 成果物 | 詳細 |
|---|-------|--------|------|
| 5.1 | XMLクラスドキュメント | `doc_classes/PiperTTS.xml` | 全プロパティ/メソッド/シグナルの説明 |
| 5.2 | GodotCPPドキュメント統合 | `CMakeLists.txt` | `target_doc_sources()`でXML→C++自動生成・登録 |
| 5.3 | plugin.cfg | `addons/piper_plus/plugin.cfg` | EditorPlugin設定 |
| 5.4 | モデルダウンローダ | `addons/piper_plus/model_downloader.gd` | HTTPRequest + ZIPReaderでモデル/辞書ダウンロード・展開 |
| 5.5 | EditorPlugin | `addons/piper_plus/piper_plus_plugin.gd` | ツールメニュー統合（ダウンローダ/辞書エディタ） |
| 5.6 | カスタム辞書エディタ | `addons/piper_plus/dictionary_editor.gd` | JSON辞書の読み込み/編集/保存UI |

### 完了基準

- [x] Godotエディタの「検索ヘルプ」でPiperTTSクラスのドキュメントが表示される（`target_doc_sources()`統合）
- [x] エディタメニューからモデル/辞書をダウンロードできる（model_downloader.gd）
- [x] カスタム辞書のJSON編集が可能（dictionary_editor.gd）

---

## M6: ストリーミング合成

**目標:** 推論途中のチャンク音声を逐次再生し、発話開始までのレイテンシを削減。

### タスク

| # | タスク | 成果物 | 詳細 |
|---|-------|--------|------|
| 6.1 | AudioStreamGenerator統合 | `src/piper_tts.cpp` | mix_rate=22050, buffer設定 |
| 6.2 | チャンクコールバック接続 | `src/piper_tts.cpp` | piper::textToAudioStreaming のchunkCallback活用 |
| 6.3 | ロックフリーキュー | `src/audio_queue.h` | ワーカースレッド→メインスレッドのチャンク転送 |
| 6.4 | _process()バッファ投入 | `src/piper_tts.cpp` | メインスレッドの_process()でpush_buffer() |
| 6.5 | synthesize_streaming() | `src/piper_tts.cpp` | ストリーミング合成APIメソッド |

### データフロー

```
ワーカースレッド                    メインスレッド
    │                                │
    ├─ textToAudioStreaming() ─→     │
    │   chunkCallback(chunk1) ─→  ロックフリーキュー
    │   chunkCallback(chunk2) ─→  ロックフリーキュー
    │   ...                          │
    │                           _process():
    │                             queue.pop() → push_buffer()
    │                             queue.pop() → push_buffer()
```

### 完了基準

- [x] テキスト入力から最初の音声が聞こえるまでの時間がAudioStreamWAV方式より短い
- [x] 長文テキストで途切れなくストリーミング再生される
- [x] メインスレッドがブロックされない

**実装詳細:**
- `src/audio_queue.h`: ロックフリーSPSCリングバッファ（Capacity=16、sentence単位チャンク転送）
- `src/piper_tts.cpp`: `synthesize_streaming(text, playback)` API、`_process()`でpush_buffer()
- piper-plusの`audioCallback`（sentence単位コールバック）を活用し、sentence合成完了ごとにチャンクをキューに投入
- `_process()`でキューからpop → int16→float変換 → `AudioStreamGeneratorPlayback::push_buffer()`
- `streaming_ended`シグナルで完了通知

---

## M7: GPU推論EP対応

**目標:** プラットフォーム別のGPU Execution Providerで推論を高速化。

### タスク

| # | タスク | 成果物 | 詳細 |
|---|-------|--------|------|
| 7.1 | EP選択API | `src/piper_tts.h` | execution_provider プロパティ (CPU/CoreML/DirectML/NNAPI) |
| 7.2 | CoreML EP | `src/piper_tts.cpp` | iOS/macOS向け |
| 7.3 | DirectML EP | `src/piper_tts.cpp` | Windows向け |
| 7.4 | NNAPI EP | `src/piper_tts.cpp` | Android向け |
| 7.5 | 自動選択ロジック | `src/piper_tts.cpp` | プラットフォーム判定で最適EP自動選択 |
| 7.6 | フォールバック | `src/piper_tts.cpp` | GPU EP失敗時にCPUへ自動フォールバック |

### EP対応マトリクス

| プラットフォーム | 優先EP | フォールバック |
|----------------|--------|-------------|
| iOS/macOS | CoreML | CPU |
| Windows | DirectML | CPU |
| Android | NNAPI | CPU |
| Linux | CPU | - |

### 完了基準

- [ ] 対応プラットフォームでGPU EPが利用可能
- [ ] GPU EP失敗時にCPUに自動フォールバックする
- [ ] CPU比で推論時間が改善されている（ベンチマーク計測）

---

## M8: Asset Library + エディタUI

**目標:** Godot Asset Libraryから導入でき、エディタ内で完結した使用体験を提供。

### タスク

| # | タスク | 成果物 | 詳細 |
|---|-------|--------|------|
| 8.1 | Asset Library登録 | asset-library 設定 | Custom URL方式（GitHub Release） |
| 8.2 | エディタアイコン | `addons/piper_plus/icon.svg` | PiperTTSノード用アイコン |
| 8.3 | Inspectorカスタマイズ | EditorPlugin (GDScript) | モデル選択ドロップダウン、テスト発話ボタン |
| 8.4 | README_EN.md | `README_EN.md` | 英語版ドキュメント |
| 8.5 | CHANGELOG | `CHANGELOG.md` | リリースノート |

### 完了基準

- [ ] Godot Asset Libraryの検索結果にgodot-piper-plusが表示される
- [ ] Asset Libraryからインストール → デモシーン実行まで5分以内で完了
- [ ] エディタ内でモデル選択・テスト発話が可能

---

## マイルストーン依存関係と推奨実行順序

```
必須パス:  M1 → M2 → M3
並行可能:  M4 は M1完了後いつでも開始可能
           M5 は M2完了後いつでも開始可能
後続:      M6 は M3完了後
           M7 は M3完了後
最終:      M8 は M4,M5完了後
```

### 推奨実行順序

1. **M1** → **M2** → **M3** （クリティカルパス、コア機能）
2. **M4** （M1完了後に並行開始、CIの早期整備）
3. **M5** （M2完了後に並行開始）
4. **M6**, **M7** （M3完了後、優先度に応じて）
5. **M8** （最終仕上げ）

---

## リスクと対策

| リスク | 影響 | 対策 |
|-------|------|------|
| ~~OpenJTalk/HTSEngineのクロスコンパイル失敗~~ | ~~M1, M4 ブロック~~ | **解決済み（M1）: macOS arm64でExternalProjectビルド成功。他プラットフォームはM4で対応** |
| ~~ONNX Runtime動的リンクのパス解決~~ | ~~M2 ブロック~~ | **解決済み（M2）: .gdextension dependencies + RPATH設定で動作確認済み** |
| ~~piper.cpp の spdlog 依存除去の複雑さ~~ | ~~M2 遅延~~ | **解決済み（M2）: no-opシムヘッダで83箇所を無改変で対応** |
| Godot AudioStreamGeneratorのスレッドセーフ問題 | M6 遅延 | M3段階でstd::thread + call_deferredのスレッドモデルを検証済み |
| iOS静的リンクのONNX Runtimeサイズ | M4 遅延 | ort-builderでカスタムビルド、不要EPを除外 |
| Android NDK + CMakeツールチェーン互換性 | M4 遅延 | piper-plusのAndroidビルド設定を参照 |
