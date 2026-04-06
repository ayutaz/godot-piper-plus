# Web Platform 技術調査

更新日: 2026-04-06

関連文書:

- [TKT-002 Web Platform 対応](./tickets/TKT-002-web-platform.md)
- [要求定義](./requirements.md)
- [マイルストーン管理](./milestones.md)

## 結論

- Godot 4.4 系で Web 向け GDExtension addon を成立させること自体は可能です。
- ただし既定の Web export template では GDExtension を読み込めないため、`dlink_enabled=yes` で作った custom Web export template が前提です。
- `godot-cpp` 側は CMake でも Emscripten / `web` target を持っているため、addon 本体の wasm side module 化は現行 build に接続できます。
- 現在の repo がそのまま Web で動かない主因は、ONNX Runtime が native shared library 前提、model/config/dictionary 読み込みが実パス + `std::filesystem` / `std::ifstream` 前提、browser smoke が未整備、の 3 点です。
- `TKT-002` の第 1 段階は、`preview` 扱いの Web 対応として `CPU-only`、custom template、`web.*` manifest、browser smoke を閉じるのが妥当です。
- desktop/mobile と同じ完全 parity を最初から狙うより、まず `export できる` `addon が load できる` `最小モデルで synthesize できる` を固め、その後に Japanese text frontend などの重い項目を広げる方が安全です。

## 一次ソースで確認できた前提

### Godot / GDExtension

- Godot 公式の Web compile 文書では、既定の Web export template は GDExtension support を含まず、`dlink_enabled=yes` 付きで custom template を build する必要があります。
  - Source: <https://docs.godotengine.org/en/stable/engine_details/development/compiling/compiling_for_web.html>
- Godot 公式の Web export 文書では、thread 有効 export は `SharedArrayBuffer` 前提で、`Cross-Origin-Opener-Policy: same-origin` と `Cross-Origin-Embedder-Policy: require-corp` を付けた secure context が必要です。
  - Source: <https://docs.godotengine.org/en/4.4/tutorials/export/exporting_for_web.html>
- repo に含まれる公式 `godot-cpp` の test project は、`.gdextension` に次の 4 entry を持っています。
  - `web.debug.threads.wasm32`
  - `web.release.threads.wasm32`
  - `web.debug.wasm32`
  - `web.release.wasm32`
  - Source: [thirdparty/godot-cpp/test/project/example.gdextension](../thirdparty/godot-cpp/test/project/example.gdextension)
- repo に含まれる公式 `godot-cpp` CMake は `PLATFORM_ID:Emscripten` を `web` として扱い、`cmake/web.cmake` で `-sSIDE_MODULE=1` を付ける構成を持っています。
  - Source: [thirdparty/godot-cpp/cmake/godotcpp.cmake](../thirdparty/godot-cpp/cmake/godotcpp.cmake)
  - Source: [thirdparty/godot-cpp/cmake/web.cmake](../thirdparty/godot-cpp/cmake/web.cmake)

### Emscripten

- Emscripten の pthread 文書では、thread ありと thread なしを 1 binary で兼用できず、2 build を作って runtime で選ぶのが上限とされています。
  - Source: <https://emscripten.org/docs/porting/pthreads.html>
- Emscripten の dynamic linking 文書では、main module + side module の構成が前提で、side module は pure WebAssembly module です。
  - Source: <https://emscripten.org/docs/compiling/Dynamic-Linking.html>
- 同文書では dynamic linking + pthreads は experimental とされています。Web では runtime `dlopen()` に依存せず、load-time linking 前提で設計した方が安全です。
  - Source: <https://emscripten.org/docs/compiling/Dynamic-Linking.html>

### ONNX Runtime

- ONNX Runtime 公式は Web 向けに 2 系統を案内しています。
  - `--build_wasm`: JS / npm package 向けの wasm artifacts
  - `--build_wasm_static_lib`: C/C++ project が link できる `libonnxruntime_webassembly.a`
  - Source: <https://onnxruntime.ai/docs/build/web.html>
- 同文書では C++ project 向けに `libonnxruntime_webassembly.a` と C/C++ headers を使う形が明示されています。現行 C++ backend を維持したまま Web 対応する道はあります。
  - Source: <https://onnxruntime.ai/docs/build/web.html>

## repo 現状との差分

### build / toolchain

- [CMakeLists.txt](../CMakeLists.txt) は `iOS` 以外を `SHARED` library で作るため、`godot-cpp` の Emscripten hack が効けば `piper_plus` 自体を side module にする方向は取れます。
- ただし `EXTERNAL_CMAKE_ARGS` は `iOS` / `Apple` / `Android` しか明示しておらず、Web 用の toolchain 受け渡し分岐がありません。
- [cmake/HTSEngine.cmake](../cmake/HTSEngine.cmake) は `Windows / Android / iOS / Linux ARM64` を CMake build、`macOS / Linux x86_64` を autotools build に分けています。Web を入れるなら CMake build 側へ明示的に含める必要があります。
- [cmake/OpenJTalk.cmake](../cmake/OpenJTalk.cmake) は `EXTERNAL_CMAKE_ARGS` 伝播前提なので、Web 側 toolchain を渡せれば筋はあります。

### ONNX Runtime

- [cmake/FindOnnxRuntime.cmake](../cmake/FindOnnxRuntime.cmake) は `Windows / macOS / Linux / Android / iOS` しか扱っておらず、Web path がありません。
- 同ファイルは native package の `lib/` と `include/` を探索する前提です。Web では `libonnxruntime_webassembly.a` を明示的に受ける分岐が必要です。

### runtime I/O

- [src/piper_core/piper.cpp](../src/piper_core/piper.cpp) は `std::ifstream` で model config を読み、`Ort::Session(env, modelPathStr, options)` で model path から session を作っています。
- [src/piper_tts.cpp](../src/piper_tts.cpp) は `resolve_model_path()` と `resolve_config_path()` で `std::filesystem` に依存しています。
- Web export では real path 前提が弱いため、model / config / CMU dict は `FileAccess` 由来の byte/string loader に寄せ、ONNX Runtime も memory session へ切り替える方が安全です。

### frontend / optional backend

- [src/piper_core/openjtalk_wrapper.c](../src/piper_core/openjtalk_wrapper.c) は optional `openjtalk-native` backend のために `dlopen()` / `LoadLibrary()` を使います。
- Web では optional native backend は対象外にし、builtin OpenJTalk のみ、または phase 1 では Japanese text frontend 自体を scope 外にする判断が必要です。

### package / manifest / test

- [addons/piper_plus/piper_plus.gdextension](../addons/piper_plus/piper_plus.gdextension) と [test/project/addons/piper_plus/piper_plus.gdextension](../test/project/addons/piper_plus/piper_plus.gdextension) に `web.*` entry がありません。
- [scripts/ci/package-addon.sh](../scripts/ci/package-addon.sh) と [scripts/ci/validate-addon-package.sh](../scripts/ci/validate-addon-package.sh) は manifest-driven なので、`web.*` を足せば package/validator の骨格は再利用できます。
- [test/project/export_presets.cfg](../test/project/export_presets.cfg) に Web preset がありません。
- [.github/workflows/build.yml](../.github/workflows/build.yml) に Web build / export / browser smoke job がありません。
- 現状の smoke は headless Godot か export artifact 確認までで、browser runtime を見ていません。

## 実装方式の比較

### 方式 A: C++ backend を維持して Web side module 化する

内容:

- custom Web export template を `dlink_enabled=yes` で作る
- `piper_plus` を Emscripten toolchain で side module build する
- ONNX Runtime は `libonnxruntime_webassembly.a` を静的 link する
- Godot resource は `FileAccess` 経由で byte/string に読み、core へ渡す

利点:

- `PiperTTS` の public API と core 実装を最大限再利用できる
- `NFR-2` の offline runtime 要件と整合しやすい
- Web 専用 JS backend を増やさずに済む

欠点:

- model/config/dictionary 読み込みの refactor が必要
- ORT / addon binary のサイズと初期化時間が増える
- Japanese text frontend は dictionary FS 戦略まで含めると一気に重くなる

評価:

- `TKT-002` の主線として推奨

### 方式 B: Web だけ `onnxruntime-web` を JavaScriptBridge 経由で使う

内容:

- Web export では native ORT を使わず、JS 側の `onnxruntime-web` を呼ぶ
- GDExtension は制御層だけに寄せるか、Web では別 backend を持つ

利点:

- ORT WebGPU / WebNN へ将来的に乗りやすい
- ORT の公式 Web path に近い

欠点:

- backend が platform ごとに二重化する
- `PiperTTS` の同期 / 非同期 / streaming と error contract を JS 側と揃える追加設計が必要
- Web だけ JavaScriptBridge 依存が強くなり、テスト系も分岐する

評価:

- static-lib 案がサイズ・安定性・toolchain のどれかで破綻した場合の fallback

## 推奨スコープ

### Phase 1: preview support

第 1 段階で閉じる対象:

- custom Web export template を前提に export が通る
- `web.*` manifest と package validator が揃う
- browser 上で addon が load され、最小モデルで synthesize できる
- CI で browser smoke が再現できる

第 1 段階で明示的に制約とするもの:

- inference backend は `CPU-only`
- `execution_provider` は Web では `EP_CPU` 固定、他 provider は machine-readable に unsupported を返す
- `openjtalk-native` は unsupported
- thread あり / なしは別 binary として扱う

### Phase 2: parity expansion

第 2 段階で扱う候補:

- Japanese text input の dictionary bootstrap
- multilingual capability matrix の Web 列追加
- binary size 最適化
- browser audio policy を含む streaming 体験の調整

## 推奨実装順

1. custom Web export template と Emscripten build bootstrap を作る
2. `web.*` manifest と package validator を通す
3. `FindOnnxRuntime.cmake` に Web static-lib 分岐を入れる
4. model/config/CMU dict 読み込みを path-based から byte/string-based に分離する
5. browser smoke を CI に追加する
6. その後に Japanese frontend と broader multilingual scope を広げる

## 具体的な変更対象

- [CMakeLists.txt](../CMakeLists.txt)
  - Web toolchain と external dependency 伝播を追加する
- [cmake/FindOnnxRuntime.cmake](../cmake/FindOnnxRuntime.cmake)
  - `libonnxruntime_webassembly.a` を扱う Web 分岐を追加する
- [cmake/HTSEngine.cmake](../cmake/HTSEngine.cmake)
  - Web を CMake build 分岐に含める
- [addons/piper_plus/piper_plus.gdextension](../addons/piper_plus/piper_plus.gdextension)
  - `web.*` / `web.*.threads` entry を追加する
- [test/project/addons/piper_plus/piper_plus.gdextension](../test/project/addons/piper_plus/piper_plus.gdextension)
  - test project 側の manifest を同期する
- [test/project/export_presets.cfg](../test/project/export_presets.cfg)
  - Web preset を追加する
- [scripts/ci/package-addon.sh](../scripts/ci/package-addon.sh)
  - Web side module artifacts を assemble 対象に含める
- [scripts/ci/validate-addon-package.sh](../scripts/ci/validate-addon-package.sh)
  - Web manifest entry を required binary として検証する
- [src/piper_core/piper.cpp](../src/piper_core/piper.cpp)
  - config/model load を memory-friendly な形へ分離する
- [src/piper_tts.cpp](../src/piper_tts.cpp)
  - Web での resource load と unsupported provider/error handling を追加する
- [.github/workflows/build.yml](../.github/workflows/build.yml)
  - `build-web` / `export-web` / `browser-smoke` job を追加する

## 検証計画

### unit / integration

- `web.*` manifest entry を validator が見落とさないこと
- Web build artifact 名が `.gdextension` と一致していること
- Web では unsupported provider が `get_last_error()` に反映されること

### e2e

- packaged addon を使った Web export が通ること
- local static server で COOP / COEP を付けて exported page を起動できること
- headless browser から console log または DOM 経由で smoke 成功を判定できること

推奨ツール:

- export: Godot headless + custom Web templates
- serving: COOP / COEP を付けられる最小 HTTP server
- browser automation: Playwright
  - Source: <https://playwright.dev/docs/ci>

## 未確定事項

- Web を release 上 `正式サポート` と呼ぶか `preview` と呼ぶか
- Phase 1 に Japanese text input を含めるか
- thread build を必須にするか、thread / no-thread の両 build を package に入れるか
- ORT static-lib route の binary size が許容範囲に収まるか
- browser smoke を GitHub Actions でどこまで重くしてよいか

## TKT-002 への反映方針

- feasibility は確認できたため、`TKT-002` は `未着手` ではなく `進行中` に上げる
- 完了条件は「custom template + manifest + export smoke + browser smoke + 制約文書化」で固定する
- parity 拡張は `TKT-002` 内でも phase を分けて扱い、phase 1 で desktop/mobile と同等保証を約束しない
