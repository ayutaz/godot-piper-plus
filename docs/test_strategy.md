# テスト戦略

godot-piper-plus の品質を担保するためのテスト計画。piper-plus（上流）の Google Test 構成を参考に設計。

---

## 全体構成

```
テスト層                    フレームワーク        実行環境           CI実行
─────────────────────────────────────────────────────────────────────────
Layer 1: C++ ユニットテスト   Google Test 1.14     CTest              全プラットフォーム
Layer 2: GDScript 統合テスト  godot-cpp test方式   Godot --headless   Linux/macOS/Windows
Layer 3: 手動検証             demo/main.gd        Godotエディタ       ローカルのみ
```

### テストピラミッド

| 層 | テスト数目安 | 実行時間 | 目的 |
|----|------------|---------|------|
| Layer 1 | 60-80件 | < 30秒 | 個々の関数・クラスの正確性 |
| Layer 2 | 10-15件 | < 60秒 | GDExtension API の動作検証 |
| Layer 3 | 手動 | - | 音声品質・UX確認 |

---

## Layer 1: C++ ユニットテスト（Google Test）

### CMake 統合方式

```cmake
# CMakeLists.txt に追加
option(BUILD_TESTS "Build unit tests" OFF)

if(BUILD_TESTS)
  enable_testing()
  include(FetchContent)
  FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
  )
  # Windows: 動的CRT競合を防止
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)
  add_subdirectory(tests)
endif()
```

piper-plus と同じ FetchContent 方式を採用。`BUILD_TESTS=OFF` がデフォルトのため、通常ビルドには影響しない。

### ディレクトリ構造

```
tests/
├── CMakeLists.txt                     # テストターゲット定義
├── test_phoneme_mapping.cpp           # 音素マッピング・PUA変換
├── test_custom_dictionary.cpp         # カスタム辞書
├── test_phoneme_parser.cpp            # [[ ]] 表記パーサ
├── test_openjtalk_security.cpp        # セキュリティ検証
├── test_openjtalk_error.cpp           # エラーハンドリング
├── test_audio_queue.cpp               # ロックフリーキュー
├── test_piper_core.cpp                # 推論パイプライン基本検証
├── test_streaming.cpp                 # ストリーミング合成
└── fixtures/                          # テストデータ
    ├── test_dictionary_v1.json
    └── test_dictionary_v2.json
```

### テストスイート詳細

#### 1. test_phoneme_mapping.cpp — 音素マッピング・PUA変換

piper-plus `test_phonemize.cpp`（27テスト）に対応。godot-piper-plus のコア品質を左右する最重要テスト。

| テストケース | 検証内容 | 優先度 |
|------------|---------|-------|
| `BasicPhonemeMapping` | 単一文字音素（a, i, u, e, o）のID変換 | 高 |
| `MultiCharPUA` | 拗音子音の PUA マッピング（ky→0xE006, ch→0xE00E, ts→0xE00F, sh→0xE010） | 高 |
| `NVariantBilabial` | N_m→0xE019（m/b/p 前） | 高 |
| `NVariantAlveolar` | N_n→0xE01A（n/t/d/ts/ch 前） | 高 |
| `NVariantVelar` | N_ng→0xE01B（k/g 前） | 高 |
| `NVariantUvular` | N_uvular→0xE01C（文末/母音前） | 高 |
| `SmallTsuHandling` | 促音（cl/q）の変換 | 高 |
| `LongVowelHandling` | 長音の処理 | 中 |
| `EmptyInput` | 空文字列で空結果を返す | 中 |
| `InvalidUTF8` | 不正UTF-8シーケンスで安全に処理 | 中 |
| `BufferOverflowProtection` | 10,000要素でもオーバーフローしない | 中 |

#### 2. test_custom_dictionary.cpp — カスタム辞書

piper-plus `test_custom_dictionary.cpp`（9テスト）に対応。

| テストケース | 検証内容 | 優先度 |
|------------|---------|-------|
| `AddAndGetWord` | 単語追加→取得の基本動作 | 高 |
| `ApplyToText` | テキスト中の単語置換 | 高 |
| `PriorityOrder` | 高優先度が低優先度を上書き | 高 |
| `LongestMatchFirst` | 長い単語が短い単語より優先（"Docker Compose" > "Docker"） | 高 |
| `CaseSensitivity` | 大小文字区別/非区別の動作 | 中 |
| `LoadV1Format` | JSON v1形式（配列）の読み込み | 中 |
| `LoadV2Format` | JSON v2形式（優先度付き）の読み込み | 中 |
| `SaveAndReload` | 保存→再読み込みの整合性 | 中 |
| `EmptyDictionary` | 空辞書での安全な動作 | 低 |
| `SpecialCharacters` | 特殊文字を含む単語（"C++", "@user"） | 低 |

#### 3. test_phoneme_parser.cpp — 音素パーサ

piper-plus `test_phoneme_parser.cpp`（20テスト）に対応。

| テストケース | 検証内容 | 優先度 |
|------------|---------|-------|
| `ParsePlainText` | `[[ ]]` なしのテキストをそのまま返す | 高 |
| `ParseSingleNotation` | `"test [[ a i u ]] end"` の正しい分割 | 高 |
| `ParseMultipleNotations` | 複数の `[[ ]]` 区間を正しく抽出 | 高 |
| `ParseJapaneseMultiChar` | OpenJTalk形式の PUA マッピング検証 | 高 |
| `QuestionMarkerEmphatic` | `?!` → 0xE016 | 中 |
| `QuestionMarkerNeutral` | `?.` → 0xE017 | 中 |
| `QuestionMarkerTag` | `?~` → 0xE018 | 中 |
| `EmptyNotation` | 空の `[[ ]]` の安全な処理 | 中 |
| `ExtraSpaces` | 余分なスペースのトリミング | 低 |
| `NestedBrackets` | ネストされた括弧の処理 | 低 |

#### 4. test_openjtalk_security.cpp — セキュリティ検証

piper-plus `test_openjtalk_security.cpp`（8テスト）に対応。GDExtensionはユーザー入力を直接受け取るため重要。

| テストケース | 検証内容 | 優先度 |
|------------|---------|-------|
| `SafePathValidation` | 正常パスの受理 | 高 |
| `RejectPathTraversal` | `../` を含むパスの拒否 | 高 |
| `RejectCommandInjection` | `; rm -rf /` 等のコマンド文字の拒否 | 高 |
| `NullInputHandling` | NULL入力で安全に失敗 | 高 |
| `EmptyStringHandling` | 空文字列の安全な処理 | 中 |
| `RejectExtremelyLargeInput` | 1MB超入力の拒否（`OPENJTALK_MAX_INPUT`） | 中 |
| `MalformedUTF8` | 不正UTF-8シーケンスの安全な処理 | 中 |
| `ConcurrentAccess` | 複数スレッドからの同時アクセス | 中 |

#### 5. test_openjtalk_error.cpp — エラーハンドリング

piper-plus `test_openjtalk_error_handling.cpp`（5テスト）に対応。

| テストケース | 検証内容 | 優先度 |
|------------|---------|-------|
| `ErrorToString` | 全13種エラーコード→文字列マッピング | 高 |
| `SetResult` | 結果構造体の書式化（255文字制限） | 中 |
| `InvalidInput` | NULL/空文字列の拒否 | 中 |
| `InputSizeLimits` | 1MB制限の遵守 | 中 |
| `ThreadSafety` | 4スレッド並行処理 | 低 |

#### 6. test_audio_queue.cpp — ロックフリーキュー（M6）

piper-plus にはない godot-piper-plus 固有のテスト。`audio_queue.h` の SPSC リングバッファを検証。

| テストケース | 検証内容 | 優先度 |
|------------|---------|-------|
| `PushAndPop` | 基本的な push→pop 動作 | 高 |
| `FIFO_Order` | 先入先出の順序保証 | 高 |
| `FullQueue` | Capacity=16 でフル時に push が false | 高 |
| `EmptyQueue` | 空キューで pop が false | 高 |
| `Clear` | clear() 後のインデックスリセットと再利用 | 中 |
| `ProducerConsumer` | 1 Producer + 1 Consumer スレッドの並行動作 | 中 |
| `MemoryOrder` | relaxed/acquire/release の正確性 | 低 |

#### 7. test_piper_core.cpp — 推論パイプライン基本検証

piper-plus `test_piper_core.cpp`（8テスト）に対応。ONNX Runtime への依存を最小限にした構造テスト。

| テストケース | 検証内容 | 優先度 |
|------------|---------|-------|
| `SampleRateValidation` | 有効なサンプルレート（16000, 22050, 24000, 44100, 48000） | 高 |
| `Int16Range` | 音声サンプルが -32768 ～ 32767 の範囲内 | 高 |
| `WAVHeaderStructure` | WAV ヘッダが44バイト | 中 |
| `EmptyStringHandling` | 空テキストの安全な処理 | 中 |
| `UTF8Support` | 日本語テキスト（UTF-8）のサポート | 中 |
| `ModelConfigParsing` | JSON設定ファイルの解析（phoneme_id_map, sample_rate） | 中 |

#### 8. test_streaming.cpp — ストリーミング合成（M6）

piper-plus `test_streaming_simple.cpp`（6テスト）に対応。

| テストケース | 検証内容 | 優先度 |
|------------|---------|-------|
| `TextChunkingJapanese` | 日本語句読点での文分割（。！？） | 高 |
| `EmptyTextNoChunks` | 空テキストでチャンク数 = 0 | 中 |
| `SingleSentenceOneChunk` | 単一文で1チャンク | 中 |

### テスト用 CMakeLists.txt（tests/CMakeLists.txt）

```cmake
# テスト実行ファイル
add_executable(piper_plus_tests
  test_phoneme_mapping.cpp
  test_custom_dictionary.cpp
  test_phoneme_parser.cpp
  test_openjtalk_security.cpp
  test_openjtalk_error.cpp
  test_audio_queue.cpp
  test_piper_core.cpp
  test_streaming.cpp
)

target_include_directories(piper_plus_tests PRIVATE
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/src/piper_core
  ${OPENJTALK_DIR}/include
  ${HTS_ENGINE_DIR}/include
  ${ONNXRUNTIME_INCLUDE_DIR}
)

target_link_libraries(piper_plus_tests PRIVATE
  GTest::gtest_main
  # 静的リンクのOpenJTalk/HTSEngineライブラリ（プラットフォーム別）
)

# CTest 登録
include(GoogleTest)
gtest_discover_tests(piper_plus_tests)
```

### 外部依存とモック化方針

| 依存 | モック化方式 | 理由 |
|------|------------|------|
| OpenJTalk C API | **モック不要**（静的リンクで直接テスト） | 辞書ファイルがあれば実動作テスト可能 |
| ONNX Runtime | **条件付きテスト**（`PIPER_TEST_MODEL_PATH` 環境変数） | モデルファイル依存のテストはCIでスキップ可 |
| Godot API | **テスト対象外**（Layer 2 で検証） | Node, AudioStreamWAV 等は GDScript テストで |
| ファイルシステム | **一時ディレクトリ**（`std::filesystem::temp_directory_path`） | テストフィクスチャは SetUp/TearDown で管理 |

---

## Layer 2: GDScript 統合テスト

godot-cpp の `test/` 構成（test_base.gd + run-tests.sh + Godot --headless）を参考に実装。

### ディレクトリ構造

```
test/
├── run-tests.sh                # ヘッドレス実行スクリプト
├── project/
│   ├── project.godot           # テスト用Godotプロジェクト
│   ├── test_base.gd            # テスト基盤クラス（assert_equal等）
│   ├── main.gd                 # テストランナー（全テスト実行）
│   ├── main.tscn               # テストシーン
│   ├── test_piper_tts.gd       # PiperTTS API テスト
│   └── bin/                    # ビルド済バイナリ配置先
│       └── .gdextension
```

### テストスイート

#### test_piper_tts.gd — PiperTTS GDExtension API テスト

| テストケース | 検証内容 | モデル依存 |
|------------|---------|----------|
| `test_node_creation` | PiperTTS ノードが作成できる | 不要 |
| `test_properties` | model_path, speaker_id 等のプロパティ設定/取得 | 不要 |
| `test_speech_rate_range` | speech_rate が 0.1-5.0 にクランプされる | 不要 |
| `test_execution_provider_enum` | EP_CPU, EP_AUTO 等のenum値 | 不要 |
| `test_initialize_without_model` | モデル未設定で initialize() がエラーを返す | 不要 |
| `test_synthesize_without_init` | 未初期化で synthesize() がエラーを返す | 不要 |
| `test_is_ready_default` | 初期状態で is_ready() == false | 不要 |
| `test_is_processing_default` | 初期状態で is_processing() == false | 不要 |
| `test_initialize_with_model` | 有効モデルで initialize() 成功 | 必要 |
| `test_synthesize_basic` | synthesize() で AudioStreamWAV が返る | 必要 |
| `test_synthesize_async` | synthesize_async() + synthesis_completed シグナル | 必要 |
| `test_audio_stream_format` | 出力が 22050Hz, 16bit, mono | 必要 |

### 実行方式

godot-cpp と同じパターン:

```bash
#!/bin/bash
# test/run-tests.sh
GODOT=${GODOT:-godot}
END_STRING="==== TESTS FINISHED ===="
FAILURE_STRING="******** FAILED ********"

OUTPUT=$($GODOT --path test/project --debug --headless --quit 2>&1)
ERRCODE=$?

echo "$OUTPUT"

if ! echo "$OUTPUT" | grep -e "$END_STRING" >/dev/null; then
    echo "ERROR: Tests failed to complete"
    exit 1
fi

if echo "$OUTPUT" | grep -e "$FAILURE_STRING" >/dev/null; then
    exit 1
fi

exit 0
```

### モデル依存テストの扱い

モデル依存テストは**テストモデルの有無で自動スキップ**する:

```gdscript
# test_piper_tts.gd
const TEST_MODEL_PATH = "res://models/ja_JP-test-medium.onnx"

func _has_test_model() -> bool:
    return FileAccess.file_exists(TEST_MODEL_PATH)

func test_synthesize_basic():
    if not _has_test_model():
        print("  SKIP: test model not found")
        return
    # ... テスト実行
```

---

## CI/CD 統合

### build.yml への追加ステップ

```yaml
# ---- 既存のビルドステップ後に追加 ----

# C++ ユニットテスト（デスクトッププラットフォームのみ）
- name: Configure with tests
  if: matrix.platform != 'android' && matrix.platform != 'ios'
  run: cmake -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON

- name: Build with tests
  if: matrix.platform != 'android' && matrix.platform != 'ios'
  run: cmake --build build --target piper_plus_tests -j$(nproc)

- name: Run C++ unit tests
  if: matrix.platform != 'android' && matrix.platform != 'ios'
  run: ctest --test-dir build --output-on-failure -V
```

### プラットフォーム別テスト実行可否

| プラットフォーム | Layer 1 (C++ Unit) | Layer 2 (GDScript) | 理由 |
|----------------|-------------------|-------------------|------|
| Windows x86_64 | o | o（将来） | デスクトップ、Godot headless 可能 |
| Linux x86_64 | o | o（将来） | デスクトップ、Godot headless 可能 |
| macOS arm64 | o | o（将来） | デスクトップ、Godot headless 可能 |
| Android arm64 | x | x | クロスコンパイルのためホスト実行不可 |
| iOS arm64 | x | x | クロスコンパイルのためホスト実行不可 |

### 段階的導入計画

| フェーズ | 内容 | 追加マイルストーン |
|---------|------|--------------|
| Phase 1 | Google Test 統合 + Layer 1 テスト実装 | テスト基盤 |
| Phase 2 | CI/CD に CTest ステップ追加 | CI テスト自動化 |
| Phase 3 | GDScript 統合テスト（Layer 2） | GDExtension API 検証 |
| Phase 4 | テストモデルによる E2E テスト | 音声出力検証 |

---

## TTS 固有のテスト観点

### 音素変換の正確性評価

| 指標 | 説明 | 計測方法 |
|------|------|---------|
| Phoneme Error Rate (PER) | (置換+削除+挿入) / 参照音素数 | 期待音素列との比較 |
| PUA マッピング一致率 | 多文字音素→PUA 変換の正確性 | 既知の入力-出力ペア |

**テストデータ例（日本語）:**

| 入力テキスト | 期待音素列 | 検証ポイント |
|------------|----------|------------|
| "こんにちは" | k o N n i ch i w a | 基本変換 |
| "がっこう" | g a cl k o: | 促音 + 長音 |
| "きょうと" | ky o: t o | 拗音 PUA |
| "しんぶん" | sh i N_m b u N | N音素文脈依存 |

### 音声出力の検証

| 検証項目 | 期待値 | テスト方法 |
|---------|-------|----------|
| サンプルレート | 22050 Hz | `AudioStreamWAV.mix_rate` 確認 |
| ビット深度 | 16-bit PCM | `AudioStreamWAV.format` 確認 |
| チャンネル | モノラル | `AudioStreamWAV.stereo == false` |
| データサイズ | > 0 bytes | `AudioStreamWAV.data.size() > 0` |
| 無音でないこと | max amplitude > 閾値 | サンプル値の最大振幅確認 |

### 辞書テスト

| 検証項目 | テスト方法 |
|---------|----------|
| JSON v1/v2 読み込み | フィクスチャファイルからの読み込み |
| 置換の正確性 | 入力テキスト → 期待出力の比較 |
| 優先度の動作 | 同一単語の異なる優先度での上書き |
| 特殊文字対応 | "C++", "@user" 等の正規表現エスケープ |

---

## piper-plus テスト構成との対応表

| piper-plus テストファイル | テスト数 | godot-piper-plus 対応 | 備考 |
|-------------------------|---------|---------------------|------|
| `test_phonemize.cpp` | 27 | `test_phoneme_mapping.cpp` | PUA マッピング中心 |
| `test_piper_core.cpp` | 8 | `test_piper_core.cpp` | 構造テスト |
| `test_phoneme_parser.cpp` | 20 | `test_phoneme_parser.cpp` | `[[ ]]` 表記 |
| `test_openjtalk_security.cpp` | 8 | `test_openjtalk_security.cpp` | セキュリティ |
| `test_openjtalk_error_handling.cpp` | 5 | `test_openjtalk_error.cpp` | エラー処理 |
| `test_streaming_simple.cpp` | 6 | `test_streaming.cpp` | ストリーミング |
| `test_dictionary_manager.cpp` | 9 | 不要 | 辞書自動DL削除済み |
| `test_gpu_device_id.cpp` | 7 | 不要 | EP選択はenum方式 |
| `test_prosody_inference.cpp` | 6 | `test_piper_core.cpp` に統合 | Prosody基本検証 |
| `test_streaming_raw_phonemes.cpp` | 5 | 将来（E2E テスト） | モデル依存 |
| `test_phoneme_timing.cpp` | 3 | 不要 | タイミング未実装 |
| `test_model_speaker_detection.cpp` | 6 | `test_piper_core.cpp` に統合 | 入力テンソル検証 |
| `test_openjtalk_optimized.cpp` | 8 | 不要 | キャッシュ未実装 |
| — | — | `test_audio_queue.cpp` (新規) | ロックフリーキュー |
| — | — | `test_piper_tts.gd` (新規) | GDExtension API |

---

## 品質メトリクス目標

| メトリクス | 目標値 | 備考 |
|----------|-------|------|
| Layer 1 テスト数 | 60+ | piper-plus の 100+ テストから必要分を移植 |
| Layer 1 パス率 | 100% | CI で全テスト通過が必須 |
| セキュリティテスト | 8+ | パストラバーサル、コマンドインジェクション等 |
| ビルド成功率 | 100% | 5プラットフォーム全て |
| テスト実行時間 | < 30秒 | Layer 1 の CI 実行時間 |
