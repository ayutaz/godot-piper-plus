# piper-plus Parity Gap Audit

Date: 2026-03-22

## Scope

- Upstream baseline: `piper-plus` default branch `origin/dev` at `6a32422f` (fetched on 2026-03-22), repository version `1.8.0`.
- Local upstream checkout was on `fix/release-openssl-macos` at `2bd2294b`, but that branch is a Rust/macOS release fix. Functional comparison below uses `origin/dev` / `1.8.0` as the parity baseline.
- Comparison target: what `godot-piper-plus` currently supports, what upstream `piper-plus` supports, and which gaps should be treated as Godot parity work versus upstream-only scope.
- Method: 10-agent parallel review plus direct source inspection in both repositories.

## Executive Summary

`godot-piper-plus` already covers the original Japanese Godot plugin core well: synchronous synthesis, async synthesis, streaming playback, desktop/mobile builds, and non-CUDA execution providers. The largest remaining upstream gaps are no longer in basic playback. They are in multilingual runtime parity, model management, and advanced inference controls.

The most important parity gaps are:

1. `piper-plus` 1.8.0 assumes multilingual inference and language-aware phonemization, while `godot-piper-plus` is still effectively Japanese-first and documents English as not implemented.
2. Upstream has a catalog-driven model manager with alias resolution, default model directories, config fallback, and download workflows. Godot still relies on explicit file paths plus a small hard-coded editor downloader.
3. Upstream runtime features such as `language_id`, raw phoneme input, JSON input, timing sidecar output, sentence/phoneme silence control, and test mode are not exposed in the Godot API.
4. Custom dictionary support is only partial on the Godot side. The editor UI format and runtime integration do not match upstream behavior.
5. Upstream quality coverage is broader. Multilingual G2P, model manager, timing, raw phoneme streaming, question markers, and optimized OpenJTalk paths are not yet represented in Godot tests.

The major gaps that are likely out of scope for a Godot plugin are WebUI, Docker images, PyPI packaging, C# CLI, Rust CLI, WebAssembly demos, and the training stack itself. Those should be tracked separately from runtime parity work.

## Current State Snapshot

| Area | `godot-piper-plus` | `piper-plus` 1.8.0 | Gap Type |
|---|---|---|---|
| Japanese runtime inference | Implemented | Implemented | Parity mostly OK |
| Async synthesis | Implemented | Implemented in CLI/runtime paths | Parity mostly OK |
| Streaming playback | Implemented via `AudioStreamGenerator` | Implemented with chunk/raw streaming modes | Partial |
| GPU execution providers | CoreML / DirectML / NNAPI / Auto | CUDA plus other runtime-specific providers across components | Partial |
| Multilingual phonemizer | Not implemented as runtime feature | 6-language runtime | Missing |
| English GPL-free G2P | Not implemented | Implemented | Missing |
| Model catalog / alias resolution / auto-download | Hard-coded editor downloader only | Implemented | Missing |
| Custom dictionary runtime parity | Partial / mismatched | Implemented | Partial |
| Timing / raw phoneme / JSON input / advanced controls | Not exposed | Implemented in upstream CLI/runtime | Missing |
| WebUI / Docker / WASM / C# / Rust / PyPI | Not present | Implemented | Upstream ecosystem |
| Training stack | Not present | Implemented | Upstream scope |

## Actionable Parity Gaps

### 1. Multilingual phonemizer runtime is missing

Upstream runtime and docs assume multilingual synthesis: Japanese, English, Chinese, Spanish, French, and Portuguese. The current Godot plugin does not expose language selection, does not ship multilingual phonemizers, and still documents English G2P as future work.

- Upstream evidence:
  - `piper-plus/README.md` describes 6-language support and GPL-free English G2P.
  - `piper-plus/src/cpp/piper.cpp` contains language-aware runtime branching.
  - `piper-plus/src/cpp/language_detector.cpp` implements language detection and segmentation.
  - `piper-plus/src/python/piper_train/phonemize/registry.py` registers multilingual phonemizers.
- Godot evidence:
  - `godot-piper-plus/README.md` advertises Japanese and English only, and separately says English is not implemented.
  - `godot-piper-plus/docs/milestones.md` explicitly defers English G2P.
  - `godot-piper-plus/src/piper_tts.h` has no language property or `language_id` API.
  - `godot-piper-plus/src/piper_core/` does not include `english_phonemize`, `chinese_phonemize`, `spanish_phonemize`, `french_phonemize`, `portuguese_phonemize`, `korean_phonemize`, or `language_detector`.

Recommended priority: `P0`

### 2. English GPL-free G2P is still absent in Godot

Upstream already replaced the old GPL-dependent English path with a GPL-free C++ English phonemizer based on dictionary-driven G2P. Godot still treats English support as an unimplemented future task.

- Upstream evidence:
  - `piper-plus/README.md`
  - `piper-plus/CHANGELOG.md` entries for v1.6.0 and v1.7.0
  - `piper-plus/src/cpp/english_phonemize.hpp`
  - `piper-plus/src/cpp/cmudict_data.json`
- Godot evidence:
  - `godot-piper-plus/README.md`
  - `godot-piper-plus/docs/milestones.md`
  - `godot-piper-plus/CLAUDE.md`
  - `godot-piper-plus/addons/piper_plus/model_downloader.gd` marks the English model as not yet functional.

Recommended priority: `P0`

### 3. There is no model catalog / alias resolution / config fallback layer

Upstream has a model manager that can list models, filter by language, resolve aliases like `tsukuyomi`, download missing models, choose a platform-specific model directory, and fall back from `model.onnx.json` to `config.json`. Godot requires explicit `model_path` and `config_path`, and its editor downloader is just a fixed table of URLs.

- Upstream evidence:
  - `piper-plus/src/cpp/model_manager.hpp`
  - `piper-plus/src/cpp/model_manager.cpp`
  - `piper-plus/src/cpp/main.cpp`
  - `piper-plus/src/python/piper_train/model_manager.py`
  - `piper-plus/src/python_run/piper/voices.json`
- Godot evidence:
  - `godot-piper-plus/src/piper_tts.cpp`
  - `godot-piper-plus/src/piper_tts.h`
  - `godot-piper-plus/addons/piper_plus/model_downloader.gd`

Recommended priority: `P0`

### 4. Advanced runtime controls from upstream CLI are not exposed

Upstream runtime accepts features that matter for tooling and inspection, not just playback:

- `language` / `language_id`
- `json-input`
- `raw-phonemes`
- `output-timing`
- `sentence-silence`
- `phoneme-silence`
- `test-mode`
- model name resolution and model directory overrides

Godot currently exposes only:

- `model_path`
- `config_path`
- `dictionary_path`
- `speaker_id`
- `speech_rate`
- `noise_scale`
- `noise_w`
- `execution_provider`
- `initialize()`
- `synthesize()`
- `synthesize_async()`
- `synthesize_streaming()`

- Upstream evidence:
  - `piper-plus/src/cpp/main.cpp`
  - `piper-plus/README.md`
- Godot evidence:
  - `godot-piper-plus/src/piper_tts.h`
  - `godot-piper-plus/src/piper_tts.cpp`

Recommended priority: `P1`

### 5. Custom dictionary support is partial and internally inconsistent

Two separate issues exist on the Godot side.

First, upstream actually injects the custom dictionary into the runtime synthesis path. Godot currently bundles the dictionary code and has an editor UI, but synthesis initialization only wires `dictionary_path` into OpenJTalk dictionary lookup.

Second, the Godot dictionary editor writes an array-based structure, while the bundled C++ dictionary parser expects the upstream object-based JSON v1/v2 formats. This means editor-created dictionaries are not guaranteed to match runtime expectations.

- Upstream evidence:
  - `piper-plus/src/cpp/main.cpp`
  - `piper-plus/src/cpp/custom_dictionary.cpp`
- Godot evidence:
  - `godot-piper-plus/src/piper_tts.cpp`
  - `godot-piper-plus/src/piper_core/custom_dictionary.cpp`
  - `godot-piper-plus/addons/piper_plus/dictionary_editor.gd`

Recommended priority: `P0`

### 6. Output-mode parity is incomplete

Godot has streaming playback, but upstream supports a wider set of output behaviors:

- raw PCM output
- raw phoneme streaming
- timing sidecar generation
- output directory / chunk-oriented workflows
- richer CLI-side inspection workflows

That is not necessarily a one-to-one API requirement for Godot, but some parts are candidates for parity:

- timing output for lip sync / subtitle synchronization
- raw phoneme input for deterministic pronunciation testing
- optional offline inspection / dry-run mode

- Upstream evidence:
  - `piper-plus/src/cpp/main.cpp`
  - `piper-plus/src/cpp/piper.hpp`
  - `piper-plus/src/cpp/tests/test_streaming_raw_phonemes.cpp`
  - `piper-plus/src/cpp/tests/test_phoneme_timing.cpp`
- Godot evidence:
  - `godot-piper-plus/src/piper_tts.cpp`
  - `godot-piper-plus/src/piper_tts.h`
  - `godot-piper-plus/docs/test_strategy.md`

Recommended priority: `P1`

### 7. Upstream `openjtalk_optimized` path is not ported

Upstream includes an additional optimized OpenJTalk path and dedicated tests. Godot does not include the optimized layer and even documents it as not adopted.

- Upstream evidence:
  - `piper-plus/src/cpp/openjtalk_optimized.c`
  - `piper-plus/src/cpp/openjtalk_optimized.h`
  - `piper-plus/src/cpp/tests/test_openjtalk_optimized.cpp`
- Godot evidence:
  - `godot-piper-plus/src/piper_core/` has no `openjtalk_optimized.*`
  - `godot-piper-plus/docs/test_strategy.md` marks optimized OpenJTalk as unnecessary / not present

Recommended priority: `P2`

### 8. Upstream test coverage is broader than Godot coverage

Compared with upstream, the Godot repository is still missing dedicated coverage for:

- multilingual G2P
- model manager / download utils
- question markers
- raw phoneme streaming
- phoneme timing
- text-input and CLI-style inspection paths
- optimized OpenJTalk

Godot does have its own strengths, including GDExtension integration tests and headless Godot execution, but the parity audit still shows upstream feature coverage that is absent on the Godot side.

- Upstream evidence:
  - `piper-plus/src/cpp/tests/CMakeLists.txt`
  - `piper-plus/test/test_multilingual_phonemizer.py`
  - `piper-plus/src/python/tests/`
- Godot evidence:
  - `godot-piper-plus/docs/test_strategy.md`
  - `godot-piper-plus/tests/`
  - `godot-piper-plus/test/project/test_piper_tts.gd`

Recommended priority: `P1`

### 9. CUDA-specific control is missing from Godot

Godot already supports `CPU`, `CoreML`, `DirectML`, `NNAPI`, and `Auto`, which is appropriate for many game deployment targets. Upstream CLI and Rust tools also expose CUDA and GPU device selection.

This is a real parity gap, but it may be lower priority than multilingual runtime support because:

- Godot already covers the non-CUDA providers that map well to target export platforms.
- CUDA is less relevant for in-editor or shipped game runtime scenarios than for desktop tooling.

- Upstream evidence:
  - `piper-plus/src/cpp/main.cpp`
  - `piper-plus/README.md`
  - `piper-plus/CHANGELOG.md`
- Godot evidence:
  - `godot-piper-plus/src/piper_tts.h`
  - `godot-piper-plus/src/piper_tts.cpp`
  - `godot-piper-plus/docs/milestones.md`

Recommended priority: `P2`

## Gaps That Are Probably Out of Scope for Godot

These are genuine upstream capabilities, but they should not automatically be treated as missing Godot plugin work.

### 1. WebUI / Docker / Hugging Face demos / WebAssembly

Upstream ships end-user and developer surfaces outside the core runtime:

- WebUI
- Docker images
- Hugging Face demo space
- WebAssembly browser demos

These are useful ecosystem assets, but they are not natural parity targets for a Godot GDExtension.

- Upstream evidence:
  - `piper-plus/README.md`
  - `piper-plus/docs/features/webui.md`
  - `piper-plus/docker/`
  - `piper-plus/src/wasm/openjtalk-web/`
  - `piper-plus/huggingface-space/`
- Godot evidence:
  - `godot-piper-plus` is structured as a Godot addon / GDExtension, not a standalone runtime product.

### 2. C# CLI / Rust CLI / PyPI / NuGet / crates.io packaging

Upstream 1.8.0 expanded beyond the original C++ and Python surfaces into multiple packaging ecosystems and CLIs. These are not direct Godot parity targets.

- Upstream evidence:
  - `piper-plus/CHANGELOG.md`
  - `piper-plus/README.md`
  - `piper-plus/src/csharp/`
  - `piper-plus/src/rust/`
- Godot evidence:
  - `godot-piper-plus` only packages a Godot addon and its native binaries.

### 3. Training stack and training-time optimizations

The following are clearly upstream scope rather than Godot plugin scope:

- WavLM discriminator
- FP16 mixed precision training
- EMA
- DDP / multi-GPU training
- ONNX export tooling
- multilingual base-model training assets

These matter to model production, but not because the Godot plugin should implement training. They matter only indirectly, because the Godot runtime must stay compatible with model config expectations produced by upstream.

- Upstream evidence:
  - `piper-plus/README.md`
  - `piper-plus/CHANGELOG.md`
  - `piper-plus/docs/guides/training/training-guide.md`
  - `piper-plus/docs/guides/training/wavlm-guide.md`
  - `piper-plus/src/python/`
- Godot evidence:
  - `godot-piper-plus/docs/milestones.md` is scoped as an inference plugin roadmap.

## Recommended Follow-up Order

1. Fix the current Godot-side correctness gaps before chasing more parity:
   - custom dictionary editor format mismatch
   - custom dictionary runtime integration
   - stale documentation around English support and test status
2. Add multilingual runtime support:
   - English GPL-free G2P
   - language selection API
   - multilingual phonemizer imports
3. Add a Godot-appropriate model management layer:
   - catalog import
   - alias resolution
   - config fallback
   - optional default model directory policy
4. Decide which upstream advanced runtime controls belong in Godot:
   - timing output
   - raw phoneme input
   - test/dry-run support
   - sentence silence configuration
5. Expand parity-oriented tests:
   - multilingual runtime tests
   - model management tests
   - timing / raw phoneme tests
   - question marker coverage

## Key Evidence Files

Upstream `piper-plus`:

- `README.md`
- `CHANGELOG.md`
- `VERSION`
- `src/cpp/main.cpp`
- `src/cpp/model_manager.hpp`
- `src/cpp/model_manager.cpp`
- `src/cpp/piper.hpp`
- `src/cpp/piper.cpp`
- `src/cpp/english_phonemize.hpp`
- `src/cpp/language_detector.cpp`
- `src/cpp/openjtalk_optimized.c`
- `src/cpp/tests/CMakeLists.txt`
- `src/python/piper_train/model_manager.py`
- `src/python/piper_train/phonemize/registry.py`
- `src/python_run/piper/voices.json`

Current `godot-piper-plus`:

- `README.md`
- `docs/milestones.md`
- `docs/test_strategy.md`
- `src/piper_tts.h`
- `src/piper_tts.cpp`
- `src/piper_core/piper.hpp`
- `src/piper_core/piper.cpp`
- `src/piper_core/custom_dictionary.cpp`
- `addons/piper_plus/model_downloader.gd`
- `addons/piper_plus/dictionary_editor.gd`
- `tests/`
- `test/project/test_piper_tts.gd`
