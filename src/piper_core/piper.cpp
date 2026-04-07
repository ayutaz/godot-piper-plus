#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <filesystem>
#include <regex>
#include <set>
#include <unordered_map>

#include <onnxruntime_cxx_api.h>
#include "spdlog/spdlog.h"

#include "json.hpp"
#include "piper.hpp"
#include "custom_dictionary.hpp"
#include "english_phonemize.hpp"
#include "language_detector.hpp"
#include "multilingual_phonemize.hpp"
#include "phoneme_ids.hpp"
#include "piper_test_utils.hpp"
#include "utf8.h"
#include "openjtalk_phonemize.hpp"
#include "phoneme_parser.hpp"

#ifdef USE_ARM64_NEON
#include "audio_neon.hpp"
#endif

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <io.h>
#define access _access
#define F_OK 0
#else
#include <unistd.h>
#endif

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif


namespace piper {

#ifdef _PIPER_VERSION
// https://stackoverflow.com/questions/47346133/how-to-use-a-define-inside-a-format-string
#define _STR(x) #x
#define STR(x) _STR(x)
const std::string VERSION = STR(_PIPER_VERSION);
#else
const std::string VERSION = "";
#endif

const std::string instanceName{"piper"};

std::string getVersion() { return VERSION; }
static const int DEFAULT_HOP_SIZE = 256;

namespace {

std::string language_code_from_id(const Voice &voice,
                                  const std::optional<LanguageId> &languageId) {
  if (!languageId.has_value() || !voice.modelConfig.languageIdMap) {
    return {};
  }

  for (const auto &[code, id] : *voice.modelConfig.languageIdMap) {
    if (id == *languageId) {
      return code;
    }
  }

  return {};
}

std::string normalize_language_code(const std::string &code) {
  std::string normalized = code;
  auto is_not_space = [](unsigned char ch) { return !std::isspace(ch); };
  normalized.erase(normalized.begin(),
                   std::find_if(normalized.begin(), normalized.end(), is_not_space));
  normalized.erase(std::find_if(normalized.rbegin(), normalized.rend(), is_not_space).base(),
                   normalized.end());
  std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                 [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  std::replace(normalized.begin(), normalized.end(), '_', '-');
  return normalized;
}

std::optional<LanguageId> language_id_from_code(const Voice &voice,
                                                const std::string &languageCode) {
  if (!voice.modelConfig.languageIdMap) {
    return std::nullopt;
  }

  const std::string normalizedCode = normalize_language_code(languageCode);
  for (const auto &[code, id] : *voice.modelConfig.languageIdMap) {
    if (normalize_language_code(code) == normalizedCode) {
      return id;
    }
  }

  return std::nullopt;
}

bool is_han_codepoint(char32_t cp) {
  return (cp >= 0x4E00 && cp <= 0x9FFF) ||
         (cp >= 0x3400 && cp <= 0x4DBF) ||
         (cp >= 0xF900 && cp <= 0xFAFF);
}

bool is_kana_codepoint(char32_t cp) {
  return (cp >= 0x3040 && cp <= 0x309F) ||
         (cp >= 0x30A0 && cp <= 0x30FF) ||
         (cp >= 0x31F0 && cp <= 0x31FF) ||
         (cp >= 0xFF65 && cp <= 0xFF9F);
}

bool contains_han_without_kana(const std::string &text) {
  if (!utf8::is_valid(text.begin(), text.end())) {
    return false;
  }

  bool hasHan = false;
  bool hasKana = false;
  auto it = text.begin();
  while (it != text.end()) {
    const uint32_t cp = utf8::unchecked::next(it);
    hasHan = hasHan || is_han_codepoint(static_cast<char32_t>(cp));
    hasKana = hasKana || is_kana_codepoint(static_cast<char32_t>(cp));
  }

  return hasHan && !hasKana;
}

bool is_openjtalk_boundary_token(Phoneme phoneme) {
  return phoneme == 0x5E ||   // ^
         phoneme == 0x24 ||   // $
         phoneme == 0x3F ||   // ?
         phoneme == 0xE016 || // ?!
         phoneme == 0xE017 || // ?.
         phoneme == 0xE018;   // ?~
}

std::vector<ProsodyFeature> make_zero_prosody(std::size_t count) {
  return std::vector<ProsodyFeature>(count, {0, 0, 0});
}

struct MultilingualSegmentResult {
  std::vector<Phoneme> phonemes;
  std::vector<ProsodyFeature> prosody;
};

MultilingualSegmentResult phonemize_multilingual_segment(
    Voice &voice, const std::string &languageCode, const std::string &text,
    bool useProsody) {
  MultilingualSegmentResult result;
  std::vector<std::vector<Phoneme>> languagePhonemes;
  std::vector<std::vector<ProsodyFeature>> languageProsody;

  if (languageCode == "ja") {
    if (useProsody) {
      phonemize_openjtalk_with_prosody(text, languagePhonemes, languageProsody);
    } else {
      phonemize_openjtalk(text, languagePhonemes);
    }

    if (languagePhonemes.empty() && !text.empty()) {
      throw std::runtime_error(
          "OpenJTalk is not available or failed to process Japanese text. "
          "Cannot synthesize Japanese without OpenJTalk.");
    }

    for (std::size_t sentenceIndex = 0; sentenceIndex < languagePhonemes.size();
         ++sentenceIndex) {
      const auto &jaSentence = languagePhonemes[sentenceIndex];
      const auto &jaProsody =
          sentenceIndex < languageProsody.size()
              ? languageProsody[sentenceIndex]
              : make_zero_prosody(jaSentence.size());

      for (std::size_t phonemeIndex = 0; phonemeIndex < jaSentence.size();
           ++phonemeIndex) {
        if (is_openjtalk_boundary_token(jaSentence[phonemeIndex])) {
          continue;
        }

        result.phonemes.push_back(jaSentence[phonemeIndex]);
        if (useProsody) {
          if (phonemeIndex < jaProsody.size()) {
            result.prosody.push_back(jaProsody[phonemeIndex]);
          } else {
            result.prosody.push_back({0, 0, 0});
          }
        }
      }
    }

    return result;
  }

  if (languageCode == "en") {
    if (voice.cmuDict.empty()) {
      throw std::runtime_error(
          "English CMU dictionary is not loaded. Provide cmudict_data.json next "
          "to the model, config, or addons/piper_plus/dictionaries.");
    }
    phonemize_english(text, languagePhonemes, voice.cmuDict);
  } else if (languageCode == "es") {
    phonemize_spanish(text, languagePhonemes);
  } else if (languageCode == "fr") {
    phonemize_french(text, languagePhonemes);
  } else if (languageCode == "pt") {
    phonemize_portuguese(text, languagePhonemes);
  } else {
    throw std::runtime_error(getMultilingualTextSupportError(languageCode));
  }

  if (languagePhonemes.empty() && !text.empty()) {
    throw std::runtime_error("Multilingual phonemization failed for language '" +
                             languageCode + "'.");
  }

  for (const auto &sentence : languagePhonemes) {
    result.phonemes.insert(result.phonemes.end(), sentence.begin(), sentence.end());
    if (useProsody) {
      auto zeroProsody = make_zero_prosody(sentence.size());
      result.prosody.insert(result.prosody.end(),
                            zeroProsody.begin(), zeroProsody.end());
    }
  }

  return result;
}

std::vector<std::filesystem::path> get_cmudict_candidates(
    const std::string &modelPath, const std::string &modelConfigPath) {
  const std::filesystem::path modelDir = std::filesystem::path(modelPath).parent_path();
  const std::filesystem::path configDir =
      std::filesystem::path(modelConfigPath).parent_path();
  std::vector<std::filesystem::path> candidates = {
      modelDir / "cmudict_data.json",
      configDir / "cmudict_data.json",
      modelDir / "cmudict.json",
      configDir / "cmudict.json",
      modelDir.parent_path() / "dictionaries" / "cmudict_data.json",
      configDir.parent_path() / "dictionaries" / "cmudict_data.json",
      modelDir.parent_path() / "dictionaries" / "cmudict.json",
      configDir.parent_path() / "dictionaries" / "cmudict.json",
  };

  std::error_code ec;
  std::filesystem::path current = std::filesystem::current_path(ec);
  if (!ec) {
    for (int depth = 0; depth < 6; ++depth) {
      const std::filesystem::path repoDictDir =
          current / "addons" / "piper_plus" / "dictionaries";
      candidates.push_back(repoDictDir / "cmudict_data.json");
      candidates.push_back(repoDictDir / "cmudict.json");

      const std::filesystem::path parent = current.parent_path();
      if (parent == current || parent.empty()) {
        break;
      }
      current = parent;
    }
  }

  return candidates;
}

} // namespace

void initialize(PiperConfig &config) {
  spdlog::info("Initialized piper");
}

void terminate(PiperConfig &config) {
  spdlog::info("Terminated piper");
}

static int resolveExecutionDeviceId(int executionProvider, int requestedDeviceId) {
  if (requestedDeviceId > 0) {
    return requestedDeviceId;
  }

  if (executionProvider == EP_CUDA) {
    const char *envValue = std::getenv("PIPER_GPU_DEVICE_ID");
    if (envValue && envValue[0] != '\0') {
      try {
        int envDeviceId = std::stoi(envValue);
        if (envDeviceId >= 0) {
          return envDeviceId;
        }
      } catch (const std::exception &) {
        spdlog::warn("Ignoring invalid PIPER_GPU_DEVICE_ID='{}'", envValue);
      }
    }
  }

  return std::max(0, requestedDeviceId);
}

void loadModel(std::string modelPath, ModelSession &session,
               int executionProvider = EP_CPU, int executionDeviceId = 0) {
  spdlog::debug("loadModel called with path: {}, EP: {}, device_id: {}",
                modelPath, executionProvider, executionDeviceId);

  // Create env
  try {
    session.env = Ort::Env(OrtLoggingLevel::ORT_LOGGING_LEVEL_WARNING,
                           instanceName.c_str());
    session.env.DisableTelemetryEvents();
  } catch (const std::exception& e) {
    spdlog::error("Failed to create ONNX Runtime environment: {}", e.what());
    throw;
  }

  // Try to append GPU EP
  bool using_gpu_ep = false;
#if defined(__EMSCRIPTEN__)
  if (executionProvider != EP_CPU) {
    throw std::runtime_error(
        "Web build supports only EP_CPU execution_provider.");
  }
#else
  if (executionProvider != EP_CPU) {
    std::string ep_name;
    std::unordered_map<std::string, std::string> ep_options;
    const int resolvedDeviceId =
        resolveExecutionDeviceId(executionProvider, executionDeviceId);
    switch (executionProvider) {
      case EP_COREML: ep_name = "CoreML"; break;
      case EP_DIRECTML: ep_name = "DML"; break;
      case EP_NNAPI: ep_name = "NNAPI"; break;
      case EP_CUDA:
        ep_name = "CUDA";
        break;
      default: break;
    }
    if (!ep_name.empty()) {
      try {
        if (executionProvider == EP_CUDA) {
          OrtCUDAProviderOptions cudaOptions;
          cudaOptions.device_id = resolvedDeviceId;
          cudaOptions.cudnn_conv_algo_search = OrtCudnnConvAlgoSearchHeuristic;
          session.options.AppendExecutionProvider_CUDA(cudaOptions);
        } else {
          if (executionProvider == EP_DIRECTML && resolvedDeviceId > 0) {
            ep_options["device_id"] = std::to_string(resolvedDeviceId);
          }
          session.options.AppendExecutionProvider(ep_name, ep_options);
        }
        using_gpu_ep = true;
        if (executionProvider == EP_CUDA || executionProvider == EP_DIRECTML) {
          spdlog::info("Using {} execution provider (device_id={})", ep_name,
                       resolvedDeviceId);
        } else {
          spdlog::info("Using {} execution provider", ep_name);
        }
      } catch (const Ort::Exception& e) {
        spdlog::warn("{} EP not available, falling back to CPU: {}", ep_name, e.what());
        // Reset options since they might be in an inconsistent state
        session.options = Ort::SessionOptions();
      } catch (const std::exception& e) {
        spdlog::warn("{} EP setup failed, falling back to CPU: {}", ep_name, e.what());
        session.options = Ort::SessionOptions();
      }
    }
  }
#endif

  // Configure session options
  if (using_gpu_ep) {
    // GPU EPs benefit from graph optimization
    session.options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
  } else {
    session.options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_DISABLE_ALL);
  }
  session.options.DisableCpuMemArena();
  session.options.DisableMemPattern();
  session.options.DisableProfiling();

  auto startTime = std::chrono::steady_clock::now();

#ifdef _WIN32
  auto modelPathW = std::filesystem::path(modelPath).wstring();
  auto modelPathStr = modelPathW.c_str();
#else
  auto modelPathStr = modelPath.c_str();
#endif

  // Create session with fallback
  try {
    session.onnx = Ort::Session(session.env, modelPathStr, session.options);
  } catch (const std::exception& e) {
    if (using_gpu_ep) {
      spdlog::warn("Session creation with GPU EP failed, retrying with CPU: {}", e.what());
      session.options = Ort::SessionOptions();
      session.options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_DISABLE_ALL);
      session.options.DisableCpuMemArena();
      session.options.DisableMemPattern();
      session.options.DisableProfiling();
      session.onnx = Ort::Session(session.env, modelPathStr, session.options);
    } else {
      throw;
    }
  }

  auto endTime = std::chrono::steady_clock::now();
  spdlog::debug("Loaded onnx model in {} second(s)",
                std::chrono::duration<double>(endTime - startTime).count());

  // Check if model has duration output
  size_t numOutputNodes = session.onnx.GetOutputCount();
  if (numOutputNodes >= 2) {
    // Check if second output is named "durations"
    auto outputName = session.onnx.GetOutputNameAllocated(1, session.allocator);
    if (std::string(outputName.get()) == "durations") {
      session.hasDurationOutput = true;
      spdlog::debug("Model supports duration output for phoneme timing");
    }
  }

  // Check model inputs for optional features
  size_t numInputNodes = session.onnx.GetInputCount();
  for (size_t i = 0; i < numInputNodes; i++) {
    auto inputName = session.onnx.GetInputNameAllocated(i, session.allocator);
    std::string name(inputName.get());
    if (name == "prosody_features") {
      session.hasProsodyInput = true;
      spdlog::debug("Model supports prosody features input (A1/A2/A3)");
    } else if (name == "sid") {
      session.hasMultiSpeaker = true;
      spdlog::debug("Model supports multi-speaker (sid input)");
    } else if (name == "lid" || name == "language_id") {
      session.hasLidInput = true;
      session.lidInputName = name;
      spdlog::debug("Model supports language selection ({} input)", name);
    }
  }
}

// Load Onnx model and JSON config file
void loadVoice(PiperConfig &config, std::string modelPath,
               std::string modelConfigPath, Voice &voice,
               std::optional<SpeakerId> &speakerId, int executionProvider,
               int executionDeviceId) {
  spdlog::debug("loadVoice called with modelPath={}, configPath={}", modelPath, modelConfigPath);
  spdlog::debug("Parsing voice config at {}", modelConfigPath);
  std::ifstream modelConfigFile(modelConfigPath);
  if (!modelConfigFile.is_open()) {
    throw std::runtime_error("Failed to open model config file: " + modelConfigPath);
  }
  voice.configRoot = json::parse(modelConfigFile);

  parsePhonemizeConfig(voice.configRoot, voice.phonemizeConfig);
  parseSynthesisConfig(voice.configRoot, voice.synthesisConfig);
  parseModelConfig(voice.configRoot, voice.modelConfig);

  if (voice.modelConfig.numSpeakers > 1) {
    // Multi-speaker model
    if (speakerId) {
      voice.synthesisConfig.speakerId = speakerId;
    } else {
      // Default speaker
      voice.synthesisConfig.speakerId = 0;
    }
  }

  spdlog::debug("Voice contains {} speaker(s)", voice.modelConfig.numSpeakers);

  if (voice.phonemizeConfig.phonemeType == TextPhonemes ||
      voice.phonemizeConfig.phonemeType == MultilingualPhonemes) {
    for (const auto &candidate : get_cmudict_candidates(modelPath, modelConfigPath)) {
      if (!candidate.empty() && std::filesystem::exists(candidate)) {
        if (loadCmuDict(candidate.string(), voice.cmuDict)) {
          spdlog::info("Loaded English CMU dictionary from {}", candidate.string());
          break;
        }
      }
    }

    if (voice.cmuDict.empty() &&
        (voice.phonemizeConfig.phonemeType == TextPhonemes ||
         (voice.modelConfig.languageIdMap &&
          voice.modelConfig.languageIdMap->count("en") > 0))) {
      spdlog::warn("English CMU dictionary was not found. English text phonemization will fail until cmudict_data.json is provided.");
    }
  }

  loadModel(modelPath, voice.session, executionProvider, executionDeviceId);

} /* loadVoice */

// Phoneme ids to WAV audio
void synthesize(std::vector<PhonemeId> &phonemeIds,
                const SynthesisConfig &synthesisConfig, ModelSession &session,
                std::vector<int16_t> &audioBuffer, SynthesisResult &result,
                Voice *voice = nullptr,
                std::vector<int64_t> *prosodyFeatures = nullptr,
                std::optional<LanguageId> languageIdOverride = std::nullopt) {
  spdlog::debug("Synthesizing audio for {} phoneme id(s)", phonemeIds.size());

  auto memoryInfo = Ort::MemoryInfo::CreateCpu(
      OrtAllocatorType::OrtArenaAllocator, OrtMemType::OrtMemTypeDefault);

  // Allocate
  std::vector<int64_t> phonemeIdLengths{(int64_t)phonemeIds.size()};
  std::vector<float> scales{synthesisConfig.noiseScale,
                            synthesisConfig.lengthScale,
                            synthesisConfig.noiseW};

  std::vector<Ort::Value> inputTensors;
  std::vector<int64_t> phonemeIdsShape{1, (int64_t)phonemeIds.size()};
  inputTensors.push_back(Ort::Value::CreateTensor<int64_t>(
      memoryInfo, phonemeIds.data(), phonemeIds.size(), phonemeIdsShape.data(),
      phonemeIdsShape.size()));

  std::vector<int64_t> phomemeIdLengthsShape{(int64_t)phonemeIdLengths.size()};
  inputTensors.push_back(Ort::Value::CreateTensor<int64_t>(
      memoryInfo, phonemeIdLengths.data(), phonemeIdLengths.size(),
      phomemeIdLengthsShape.data(), phomemeIdLengthsShape.size()));

  std::vector<int64_t> scalesShape{(int64_t)scales.size()};
  inputTensors.push_back(
      Ort::Value::CreateTensor<float>(memoryInfo, scales.data(), scales.size(),
                                      scalesShape.data(), scalesShape.size()));

  // Build input names dynamically based on model capabilities
  std::vector<const char *> inputNamesVec = {"input", "input_lengths", "scales"};

  // Add speaker id only for multi-speaker models
  // NOTE: These must be kept outside the "if" below to avoid being deallocated.
  std::vector<int64_t> speakerId{
      (int64_t)synthesisConfig.speakerId.value_or(0)};
  std::vector<int64_t> speakerIdShape{(int64_t)speakerId.size()};

  const std::optional<LanguageId> effectiveLanguageId =
      languageIdOverride.has_value() ? languageIdOverride : synthesisConfig.languageId;
  int64_t resolvedLanguageId =
      static_cast<int64_t>(effectiveLanguageId.value_or(0));
  if (voice != nullptr &&
      (resolvedLanguageId < 0 ||
       resolvedLanguageId >= static_cast<int64_t>(voice->modelConfig.numLanguages))) {
    spdlog::warn("language_id {} is out of range for this model (num_languages={}); using 0",
                 resolvedLanguageId, voice->modelConfig.numLanguages);
    resolvedLanguageId = 0;
  }
  std::vector<int64_t> languageId{resolvedLanguageId};
  std::vector<int64_t> languageIdShape{(int64_t)languageId.size()};

  if (session.hasMultiSpeaker) {
    inputTensors.push_back(Ort::Value::CreateTensor<int64_t>(
        memoryInfo, speakerId.data(), speakerId.size(), speakerIdShape.data(),
        speakerIdShape.size()));
    inputNamesVec.push_back("sid");
  }

  if (session.hasLidInput) {
    inputTensors.push_back(Ort::Value::CreateTensor<int64_t>(
        memoryInfo, languageId.data(), languageId.size(), languageIdShape.data(),
        languageIdShape.size()));
    inputNamesVec.push_back(session.lidInputName.c_str());
  }

  // Add prosody features if model supports them and they are provided
  // prosodyFeatures is a flat array of [a1, a2, a3, a1, a2, a3, ...] for each phoneme
  std::vector<int64_t> zeroProsody;
  if (session.hasProsodyInput) {
    std::vector<int64_t> prosodyShape{1, (int64_t)phonemeIds.size(), 3};
    if (prosodyFeatures && prosodyFeatures->size() == phonemeIds.size() * 3) {
      inputTensors.push_back(Ort::Value::CreateTensor<int64_t>(
          memoryInfo, prosodyFeatures->data(), prosodyFeatures->size(),
          prosodyShape.data(), prosodyShape.size()));
    } else {
      // Use zeros if no prosody features provided
      zeroProsody.resize(phonemeIds.size() * 3, 0);
      inputTensors.push_back(Ort::Value::CreateTensor<int64_t>(
          memoryInfo, zeroProsody.data(), zeroProsody.size(),
          prosodyShape.data(), prosodyShape.size()));
    }
    inputNamesVec.push_back("prosody_features");
  }

  // Check if we should get duration output
  std::vector<const char *> outputNamesVec;
  outputNamesVec.push_back("output");
  if (session.hasDurationOutput) {
    outputNamesVec.push_back("durations");
  }

  // Infer
  auto startTime = std::chrono::steady_clock::now();
  auto outputTensors = session.onnx.Run(
      Ort::RunOptions{nullptr}, inputNamesVec.data(), inputTensors.data(),
      inputTensors.size(), outputNamesVec.data(), outputNamesVec.size());
  auto endTime = std::chrono::steady_clock::now();

  if (outputTensors.empty() || (!outputTensors.front().IsTensor())) {
    throw std::runtime_error("Invalid output tensors");
  }
  auto inferDuration = std::chrono::duration<double>(endTime - startTime);
  result.inferSeconds = inferDuration.count();

  const float *audio = outputTensors.front().GetTensorData<float>();
  auto audioShape =
      outputTensors.front().GetTensorTypeAndShapeInfo().GetShape();
  int64_t audioCount = audioShape[audioShape.size() - 1];

  result.audioSeconds = (double)audioCount / (double)synthesisConfig.sampleRate;
  result.realTimeFactor = 0.0;
  if (result.audioSeconds > 0) {
    result.realTimeFactor = result.inferSeconds / result.audioSeconds;
  }
  spdlog::debug("Synthesized {} second(s) of audio in {} second(s)",
                result.audioSeconds, result.inferSeconds);

#ifdef USE_ARM64_NEON
  float maxAudioValue = findMaxAudioValueNEON(audio, audioCount);
  float audioScale = (32767.0f / std::max(0.01f, maxAudioValue));
  // Resize buffer to final size for NEON implementation
  audioBuffer.resize(audioCount);
  scaleAndConvertAudioNEON(audio, audioBuffer.data(), audioCount, audioScale);
#else
  scaleAudioToInt16(audio, static_cast<std::size_t>(audioCount), audioBuffer);
#endif

  // Extract phoneme timing information if available
  if (session.hasDurationOutput && outputTensors.size() >= 2 && voice != nullptr) {
    auto& durationTensor = outputTensors[1];
    if (durationTensor.IsTensor()) {
      const float *durations = durationTensor.GetTensorData<float>();
      auto durationShape = durationTensor.GetTensorTypeAndShapeInfo().GetShape();
      size_t durationCount = 1;
      for (auto dim : durationShape) {
        durationCount *= dim;
      }

      // Convert durations to vector
      std::vector<float> durationVec(durations, durations + durationCount);

      // Extract timing information
      // Get hop_size from config
      int hopSize = DEFAULT_HOP_SIZE;
      if (voice->configRoot.contains("audio") &&
          voice->configRoot["audio"].contains("hop_size")) {
        hopSize = voice->configRoot["audio"]["hop_size"];
      }

      result.phonemeTimings = extractTimingsFromDurations(
          durationVec, phonemeIds,
          voice->phonemizeConfig.phonemeIdMap,
          hopSize,
          voice->synthesisConfig.sampleRate,
          voice->phonemizeConfig.phonemeType
      );
      result.hasTimingInfo = true;

      spdlog::debug("Extracted timing for {} phonemes", result.phonemeTimings.size());
    }
  }

  // Clean up
  for (std::size_t i = 0; i < outputTensors.size(); i++) {
    Ort::detail::OrtRelease(outputTensors[i].release());
  }

  for (std::size_t i = 0; i < inputTensors.size(); i++) {
    Ort::detail::OrtRelease(inputTensors[i].release());
  }
}

// ----------------------------------------------------------------------------

// Phonemize text and synthesize audio
void textToAudio(PiperConfig &config, Voice &voice, std::string text,
                 const SynthesisConfig &synthesisConfig,
                 std::vector<int16_t> &audioBuffer, SynthesisResult &result,
                 const std::function<void()> &audioCallback) {
  result.inferSeconds = 0.0;
  result.audioSeconds = 0.0;
  result.realTimeFactor = 0.0;
  result.phonemeTimings.clear();
  result.hasTimingInfo = false;
  result.resolvedSegments.clear();

  if (voice.customDictionary) {
    text = applyCustomDictionaryToTextSegments(text, voice.customDictionary.get());
  }

  std::size_t sentenceSilenceSamples = 0;
  if (synthesisConfig.sentenceSilenceSeconds > 0) {
    sentenceSilenceSamples = (std::size_t)(
        synthesisConfig.sentenceSilenceSeconds *
        synthesisConfig.sampleRate * synthesisConfig.channels);
  }

  // Parse text for [[ phonemes ]] notation
  auto textSegments = parsePhonemeNotation(text);

  // Phonemes for each sentence
  spdlog::debug("Phonemizing text: {}", text);
  std::vector<std::vector<Phoneme>> phonemes;
  std::vector<std::optional<LanguageId>> sentenceLanguageIds;
  const std::optional<LanguageId> requestLanguageId = synthesisConfig.languageId;
  const std::optional<std::string> requestLanguageCode =
      language_code_from_id(voice, requestLanguageId);

  // Prosody features for each sentence (only used for OpenJTalk with prosody-enabled models)
  std::vector<std::vector<ProsodyFeature>> allProsodyFeatures;
  bool useProsody = voice.session.hasProsodyInput &&
                    usesOpenJTalk(voice.phonemizeConfig.phonemeType);

  // Process each segment
  for (const auto& segment : textSegments) {
    if (segment.isPhonemes) {
      // Direct phoneme input
      spdlog::debug("Processing direct phoneme input: {}", segment.text);
      auto parsedPhonemes = parsePhonemeString(segment.text, static_cast<int>(voice.phonemizeConfig.phonemeType));

      // Add as a single "sentence"
      phonemes.push_back(parsedPhonemes);
      sentenceLanguageIds.push_back(requestLanguageId);
      result.resolvedSegments.push_back({
          segment.text,
          requestLanguageCode.value_or(std::string()),
          requestLanguageId,
          true,
      });

      // Add empty prosody features for direct phoneme input
      if (useProsody) {
        std::vector<ProsodyFeature> emptyProsody = make_zero_prosody(parsedPhonemes.size());
        allProsodyFeatures.push_back(std::move(emptyProsody));
      }
    } else {
      // Regular text - phonemize as usual
      std::vector<std::vector<Phoneme>> segmentPhonemes;
      std::vector<std::vector<ProsodyFeature>> segmentProsody;

      if (voice.phonemizeConfig.phonemeType == OpenJTalkPhonemes) {
        // Japanese OpenJTalk phonemizer
        if (useProsody) {
          // Use prosody-aware phonemizer
          phonemize_openjtalk_with_prosody(segment.text, segmentPhonemes, segmentProsody);
        } else {
          phonemize_openjtalk(segment.text, segmentPhonemes);
        }

        // If OpenJTalk failed, we cannot process Japanese text
        if (segmentPhonemes.empty() && !segment.text.empty()) {
          throw std::runtime_error("OpenJTalk is not available or failed to process Japanese text. "
                                   "Cannot synthesize Japanese without OpenJTalk.");
        }
        if (!segment.text.empty()) {
          result.resolvedSegments.push_back({
              segment.text,
              "ja",
              language_id_from_code(voice, "ja"),
              false,
          });
        }
      } else if (voice.phonemizeConfig.phonemeType == TextPhonemes) {
        if (voice.cmuDict.empty()) {
          throw std::runtime_error("English CMU dictionary is not loaded. Provide cmudict_data.json next to the model or config.");
        }

        phonemize_english(segment.text, segmentPhonemes, voice.cmuDict);
        if (segmentPhonemes.empty() && !segment.text.empty()) {
          throw std::runtime_error("English phonemization failed for non-empty input.");
        }
        if (!segment.text.empty()) {
          result.resolvedSegments.push_back({
              segment.text,
              "en",
              language_id_from_code(voice, "en"),
              false,
          });
        }
      } else if (voice.phonemizeConfig.phonemeType == MultilingualPhonemes) {
        const MultilingualRoutingPlan routingPlan =
            planMultilingualTextRouting(voice, segment.text, requestLanguageCode,
                                        requestLanguageId);

        if (!requestLanguageId.has_value() &&
            contains_han_without_kana(segment.text)) {
          throw std::runtime_error(
              "Multilingual text containing Han characters without Kana is ambiguous. "
              "Please set language_code explicitly.");
        }

        for (const auto &languageSegment : routingPlan.segments) {
          MultilingualSegmentResult combinedResult =
              phonemize_multilingual_segment(
                  voice, languageSegment.lang, languageSegment.text, useProsody);
          segmentPhonemes.push_back(std::move(combinedResult.phonemes));
          sentenceLanguageIds.push_back(language_id_from_code(voice, languageSegment.lang));
          result.resolvedSegments.push_back({
              languageSegment.text,
              languageSegment.lang,
              language_id_from_code(voice, languageSegment.lang),
              false,
          });
          if (useProsody) {
            segmentProsody.push_back(std::move(combinedResult.prosody));
          }
        }
      } else {
        throw std::runtime_error("Unsupported phoneme type.");
      }

      // Add all sentences from this segment
      for (size_t i = 0; i < segmentPhonemes.size(); i++) {
        phonemes.push_back(std::move(segmentPhonemes[i]));
        if (sentenceLanguageIds.size() < phonemes.size()) {
          sentenceLanguageIds.push_back(requestLanguageId);
        }

        if (useProsody) {
          if (i < segmentProsody.size()) {
            allProsodyFeatures.push_back(std::move(segmentProsody[i]));
          } else {
            // Fallback: create zero prosody features
            std::vector<ProsodyFeature> zeroProsody =
                make_zero_prosody(phonemes.back().size());
            allProsodyFeatures.push_back(std::move(zeroProsody));
          }
        }
      }
    }
  }

  // Synthesize each sentence independently.
  std::vector<PhonemeId> phonemeIds;
  std::map<Phoneme, std::size_t> missingPhonemes;
  double timingOffsetSeconds = 0.0;
  size_t sentenceIdx = 0;
  for (auto phonemesIter = phonemes.begin(); phonemesIter != phonemes.end();
       ++phonemesIter, ++sentenceIdx) {
    std::vector<Phoneme> &sentencePhonemes = *phonemesIter;

    if (spdlog::should_log(spdlog::level::debug)) {
      // DEBUG log for phonemes in readable format
      std::string phonemesStr;
      for (auto phoneme : sentencePhonemes) {
        phonemesStr += phonemeToString(phoneme);
        phonemesStr += " ";
      }
      // Remove trailing space
      if (!phonemesStr.empty()) {
        phonemesStr.pop_back();
      }

      spdlog::debug("Converting {} phoneme(s) to ids: {}",
                    sentencePhonemes.size(), phonemesStr);
    }

    std::vector<std::shared_ptr<std::vector<Phoneme>>> phrasePhonemes;
    std::vector<SynthesisResult> phraseResults;
    std::vector<size_t> phraseSilenceSamples;

    // Use phoneme/id map from config
    PhonemeIdConfig idConfig;
    idConfig.phonemeIdMap =
        std::make_shared<PhonemeIdMap>(voice.phonemizeConfig.phonemeIdMap);
    idConfig.interspersePad = voice.phonemizeConfig.interspersePad;

    if (synthesisConfig.phonemeSilenceSeconds) {
      // Split into phrases
      const std::map<Phoneme, float> &phonemeSilenceSeconds =
          *synthesisConfig.phonemeSilenceSeconds;

      auto currentPhrasePhonemes = std::make_shared<std::vector<Phoneme>>();
      phrasePhonemes.push_back(currentPhrasePhonemes);

      for (auto sentencePhonemesIter = sentencePhonemes.begin();
           sentencePhonemesIter != sentencePhonemes.end();
           sentencePhonemesIter++) {
        Phoneme &currentPhoneme = *sentencePhonemesIter;
        currentPhrasePhonemes->push_back(currentPhoneme);

        if (phonemeSilenceSeconds.count(currentPhoneme) > 0) {
          // Split at phrase boundary
          phraseSilenceSamples.push_back(
              (std::size_t)(phonemeSilenceSeconds.at(currentPhoneme) *
                            synthesisConfig.sampleRate *
                            synthesisConfig.channels));

          currentPhrasePhonemes = std::make_shared<std::vector<Phoneme>>();
          phrasePhonemes.push_back(currentPhrasePhonemes);
        }
      }
    } else {
      // Use all phonemes
      phrasePhonemes.push_back(
          std::make_shared<std::vector<Phoneme>>(sentencePhonemes));
    }

    // Ensure results/samples are the same size
    while (phraseResults.size() < phrasePhonemes.size()) {
      phraseResults.emplace_back();
    }

    while (phraseSilenceSamples.size() < phrasePhonemes.size()) {
      phraseSilenceSamples.push_back(0);
    }

    // phonemes -> ids -> audio
    for (size_t phraseIdx = 0; phraseIdx < phrasePhonemes.size(); phraseIdx++) {
      if (phrasePhonemes[phraseIdx]->size() <= 0) {
        continue;
      }

      // phonemes -> ids
      phonemes_to_ids(*(phrasePhonemes[phraseIdx]), idConfig, phonemeIds,
                      missingPhonemes);
      if (spdlog::should_log(spdlog::level::debug)) {
        // DEBUG log for phoneme ids
        std::stringstream phonemeIdsStr;
        for (auto phonemeId : phonemeIds) {
          phonemeIdsStr << phonemeId << ", ";
        }

        spdlog::debug("Converted {} phoneme(s) to {} phoneme id(s): {}",
                      phrasePhonemes[phraseIdx]->size(), phonemeIds.size(),
                      phonemeIdsStr.str());
      }

      // ids -> audio
      std::vector<int64_t> *prosodyPtr = nullptr;
      std::vector<int64_t> prosodyFlat;

      if (useProsody && sentenceIdx < allProsodyFeatures.size()) {
        // Convert prosody features to flat array matching phonemeIds length
        // Format: [a1, a2, a3, a1, a2, a3, ...] for each phoneme ID
        const auto &sentenceProsody = allProsodyFeatures[sentenceIdx];

        // With intersperse padding, phonemeIds has format:
        // PAD, P1, PAD, P2, PAD, ..., PN, PAD
        // So phonemeIds.size() = 2 * num_phonemes + 1 (when interspersePad=true)
        // Prosody features are per original phoneme (before padding)

        size_t numPhonemeIds = phonemeIds.size();
        prosodyFlat.resize(numPhonemeIds * 3, 0);  // Initialize with zeros

        // Debug: Log prosody mapping details
        spdlog::debug("Prosody mapping debug:");
        spdlog::debug("  phonemeIds.size() = {}", phonemeIds.size());
        spdlog::debug("  sentenceProsody.size() = {}", sentenceProsody.size());
        spdlog::debug("  interspersePad = {}", voice.phonemizeConfig.interspersePad);

        if (voice.phonemizeConfig.interspersePad) {
          // Map prosody to odd positions (1, 3, 5, ...) which are real phonemes
          size_t prosodyIdx = 0;
          for (size_t i = 1; i < numPhonemeIds && prosodyIdx < sentenceProsody.size(); i += 2) {
            prosodyFlat[i * 3 + 0] = sentenceProsody[prosodyIdx].a1;
            prosodyFlat[i * 3 + 1] = sentenceProsody[prosodyIdx].a2;
            prosodyFlat[i * 3 + 2] = sentenceProsody[prosodyIdx].a3;
            prosodyIdx++;
          }
        } else {
          // Direct mapping - detect special tokens by ID
          // Special tokens (^=1, $=2, ?=3, #=4, [=5, ]=6) get zero prosody
          // Real phonemes get prosody from sentenceProsody in order
          prosodyFlat.clear();
          prosodyFlat.reserve(numPhonemeIds * 3);

          size_t prosodyIdx = 0;

          for (size_t i = 0; i < phonemeIds.size(); i++) {
            PhonemeId id = phonemeIds[i];

            // Special tokens: ^=1, $=2, ?=3, #=4, [=5, ]=6
            if (id >= 1 && id <= 6) {
              // Special token -> zero prosody
              prosodyFlat.push_back(0);
              prosodyFlat.push_back(0);
              prosodyFlat.push_back(0);
            } else {
              // Real phoneme -> use prosody data
              if (prosodyIdx < sentenceProsody.size()) {
                int64_t a1 = sentenceProsody[prosodyIdx].a1;
                int64_t a2 = sentenceProsody[prosodyIdx].a2;
                int64_t a3 = sentenceProsody[prosodyIdx].a3;

                prosodyFlat.push_back(a1);
                prosodyFlat.push_back(a2);
                prosodyFlat.push_back(a3);

                prosodyIdx++;
              } else {
                // Safety fallback: zero prosody
                prosodyFlat.push_back(0);
                prosodyFlat.push_back(0);
                prosodyFlat.push_back(0);
              }
            }
          }
        }

        prosodyPtr = &prosodyFlat;
        spdlog::debug("Using prosody features: {} phoneme IDs, {} original prosody values",
                      numPhonemeIds, sentenceProsody.size());
      }

      std::vector<int16_t> phraseAudioBuffer;
      synthesize(phonemeIds, synthesisConfig, voice.session,
                 phraseAudioBuffer, phraseResults[phraseIdx], &voice,
                 prosodyPtr,
                 sentenceIdx < sentenceLanguageIds.size()
                     ? sentenceLanguageIds[sentenceIdx]
                     : std::optional<LanguageId>());
      audioBuffer.insert(audioBuffer.end(), phraseAudioBuffer.begin(),
                         phraseAudioBuffer.end());

      if (phraseResults[phraseIdx].hasTimingInfo) {
        result.hasTimingInfo = true;
        for (auto timing : phraseResults[phraseIdx].phonemeTimings) {
          timing.start_time += static_cast<float>(timingOffsetSeconds);
          timing.end_time += static_cast<float>(timingOffsetSeconds);
          result.phonemeTimings.push_back(std::move(timing));
        }
      }

      // Add end of phrase silence
      for (std::size_t i = 0; i < phraseSilenceSamples[phraseIdx]; i++) {
        audioBuffer.push_back(0);
      }

      result.audioSeconds += phraseResults[phraseIdx].audioSeconds;
      result.inferSeconds += phraseResults[phraseIdx].inferSeconds;
      timingOffsetSeconds += phraseResults[phraseIdx].audioSeconds;
      if (phraseSilenceSamples[phraseIdx] > 0) {
        result.audioSeconds +=
            static_cast<double>(phraseSilenceSamples[phraseIdx]) /
            static_cast<double>(synthesisConfig.sampleRate);
        timingOffsetSeconds += static_cast<double>(phraseSilenceSamples[phraseIdx]) /
                               static_cast<double>(synthesisConfig.sampleRate);
      }

      phonemeIds.clear();
    }

    // Add end of sentence silence
    if (sentenceSilenceSamples > 0) {
      for (std::size_t i = 0; i < sentenceSilenceSamples; i++) {
        audioBuffer.push_back(0);
      }
      result.audioSeconds += static_cast<double>(sentenceSilenceSamples) /
                             static_cast<double>(synthesisConfig.sampleRate);
      timingOffsetSeconds += static_cast<double>(sentenceSilenceSamples) /
                             static_cast<double>(synthesisConfig.sampleRate);
    }

    if (audioCallback) {
      // Call back must copy audio since it is cleared afterwards.
      audioCallback();
      audioBuffer.clear();
    }

    phonemeIds.clear();
  }

  if (missingPhonemes.size() > 0) {
    spdlog::warn("Missing {} phoneme(s) from phoneme/id map!",
                 missingPhonemes.size());

    for (auto phonemeCount : missingPhonemes) {
      std::string phonemeStr;
      utf8::append(phonemeCount.first, std::back_inserter(phonemeStr));
      spdlog::warn("Missing \"{}\" (\\u{:04X}): {} time(s)", phonemeStr,
                   (uint32_t)phonemeCount.first, phonemeCount.second);
    }
  }

  if (result.audioSeconds > 0) {
    result.realTimeFactor = result.inferSeconds / result.audioSeconds;
  }

} /* textToAudio */

void inspectText(PiperConfig &config, Voice &voice, std::string text,
                 const SynthesisConfig &synthesisConfig,
                 InspectionResult &result) {
  result.phonemeSentences.clear();
  result.phonemeIdSentences.clear();
  result.missingPhonemes.clear();
  result.resolvedLanguageId.reset();
  result.resolvedSegments.clear();

  if (voice.customDictionary) {
    text = applyCustomDictionaryToTextSegments(text, voice.customDictionary.get());
  }

  auto textSegments = parsePhonemeNotation(text);
  bool useProsody = voice.session.hasProsodyInput &&
                    usesOpenJTalk(voice.phonemizeConfig.phonemeType);
  const std::optional<LanguageId> requestLanguageId = synthesisConfig.languageId;
  const std::optional<std::string> requestLanguageCode =
      language_code_from_id(voice, requestLanguageId);

  for (const auto &segment : textSegments) {
    if (segment.isPhonemes) {
      result.phonemeSentences.push_back(parsePhonemeString(
          segment.text, static_cast<int>(voice.phonemizeConfig.phonemeType)));
      result.resolvedSegments.push_back({
          segment.text,
          requestLanguageCode.value_or(std::string()),
          requestLanguageId,
          true,
      });
      continue;
    }

    std::vector<std::vector<Phoneme>> segmentPhonemes;
    std::vector<std::vector<ProsodyFeature>> segmentProsody;

    if (voice.phonemizeConfig.phonemeType == OpenJTalkPhonemes) {
      if (useProsody) {
        phonemize_openjtalk_with_prosody(segment.text, segmentPhonemes, segmentProsody);
      } else {
        phonemize_openjtalk(segment.text, segmentPhonemes);
      }

      if (segmentPhonemes.empty() && !segment.text.empty()) {
        throw std::runtime_error("OpenJTalk is not available or failed to process Japanese text.");
      }
      if (!segment.text.empty()) {
        result.resolvedSegments.push_back({
            segment.text,
            "ja",
            language_id_from_code(voice, "ja"),
            false,
        });
      }
    } else if (voice.phonemizeConfig.phonemeType == TextPhonemes) {
      if (voice.cmuDict.empty()) {
        throw std::runtime_error("English CMU dictionary is not loaded. Provide cmudict_data.json next to the model or config.");
      }

      phonemize_english(segment.text, segmentPhonemes, voice.cmuDict);
      if (segmentPhonemes.empty() && !segment.text.empty()) {
        throw std::runtime_error("English phonemization failed for non-empty input.");
      }
      if (!segment.text.empty()) {
        result.resolvedSegments.push_back({
            segment.text,
            "en",
            language_id_from_code(voice, "en"),
            false,
        });
      }
    } else if (voice.phonemizeConfig.phonemeType == MultilingualPhonemes) {
      const MultilingualRoutingPlan routingPlan =
          planMultilingualTextRouting(voice, segment.text, requestLanguageCode,
                                      requestLanguageId);

      if (!requestLanguageId.has_value() &&
          contains_han_without_kana(segment.text)) {
        throw std::runtime_error(
            "Multilingual text containing Han characters without Kana is ambiguous. "
            "Please set language_code explicitly.");
      }

      for (const auto &languageSegment : routingPlan.segments) {
        MultilingualSegmentResult combinedResult =
            phonemize_multilingual_segment(
                voice, languageSegment.lang, languageSegment.text, useProsody);
        segmentPhonemes.push_back(std::move(combinedResult.phonemes));
        result.resolvedSegments.push_back({
            languageSegment.text,
            languageSegment.lang,
            language_id_from_code(voice, languageSegment.lang),
            false,
        });
      }
      if (routingPlan.resolvedLanguageId.has_value()) {
        result.resolvedLanguageId = routingPlan.resolvedLanguageId;
      }
    } else {
      throw std::runtime_error("Unsupported phoneme type.");
    }

    for (auto &sentence : segmentPhonemes) {
      result.phonemeSentences.push_back(std::move(sentence));
    }
  }

  if (!result.resolvedLanguageId.has_value()) {
    result.resolvedLanguageId = requestLanguageId;
  }

  PhonemeIdConfig idConfig;
  idConfig.phonemeIdMap =
      std::make_shared<PhonemeIdMap>(voice.phonemizeConfig.phonemeIdMap);
  idConfig.interspersePad = voice.phonemizeConfig.interspersePad;
  idConfig.addBos = true;
  idConfig.addEos = true;

  for (const auto &sentencePhonemes : result.phonemeSentences) {
    std::vector<PhonemeId> ids;
    phonemes_to_ids(sentencePhonemes, idConfig, ids, result.missingPhonemes);
    result.phonemeIdSentences.push_back(std::move(ids));
  }
}

// Synthesize audio directly from phonemes
void phonemesToAudio(PiperConfig &config, Voice &voice,
                     const std::vector<Phoneme> &phonemes,
                     const SynthesisConfig &synthesisConfig,
                     std::vector<int16_t> &audioBuffer,
                     SynthesisResult &result,
                     const std::function<void()> &audioCallback) {
  result.inferSeconds = 0.0;
  result.audioSeconds = 0.0;
  result.realTimeFactor = 0.0;
  result.phonemeTimings.clear();
  result.hasTimingInfo = false;
  result.resolvedSegments.clear();
  const std::optional<LanguageId> requestLanguageId = synthesisConfig.languageId;
  const std::optional<std::string> requestLanguageCode =
      language_code_from_id(voice, requestLanguageId);
  result.resolvedSegments.push_back({
      "",
      requestLanguageCode.value_or(std::string()),
      requestLanguageId,
      true,
  });

  std::size_t sentenceSilenceSamples = 0;
  if (synthesisConfig.sentenceSilenceSeconds > 0) {
    sentenceSilenceSamples = static_cast<std::size_t>(
        synthesisConfig.sentenceSilenceSeconds *
        synthesisConfig.sampleRate * synthesisConfig.channels);
  }

  std::vector<std::vector<Phoneme>> phrasePhonemes;
  std::vector<std::size_t> phraseSilenceSamples;
  if (synthesisConfig.phonemeSilenceSeconds) {
    const std::map<Phoneme, float> &phonemeSilenceSeconds =
        *synthesisConfig.phonemeSilenceSeconds;
    phrasePhonemes.emplace_back();

    for (const auto &phoneme : phonemes) {
      phrasePhonemes.back().push_back(phoneme);
      auto silenceIter = phonemeSilenceSeconds.find(phoneme);
      if (silenceIter == phonemeSilenceSeconds.end()) {
        continue;
      }

      phraseSilenceSamples.push_back(static_cast<std::size_t>(
          silenceIter->second * synthesisConfig.sampleRate *
          synthesisConfig.channels));
      phrasePhonemes.emplace_back();
    }
  } else {
    phrasePhonemes.push_back(phonemes);
  }

  while (phraseSilenceSamples.size() < phrasePhonemes.size()) {
    phraseSilenceSamples.push_back(0);
  }

  std::vector<PhonemeId> phonemeIds;
  std::map<Phoneme, std::size_t> missingPhonemes;
  double timingOffsetSeconds = 0.0;

  PhonemeIdConfig idConfig;
  idConfig.phonemeIdMap =
      std::make_shared<PhonemeIdMap>(voice.phonemizeConfig.phonemeIdMap);
  idConfig.interspersePad = voice.phonemizeConfig.interspersePad;
  idConfig.addBos = true;
  idConfig.addEos = true;

  for (std::size_t phraseIdx = 0; phraseIdx < phrasePhonemes.size(); ++phraseIdx) {
    if (phrasePhonemes[phraseIdx].empty()) {
      continue;
    }

    phonemes_to_ids(phrasePhonemes[phraseIdx], idConfig, phonemeIds, missingPhonemes);

    SynthesisResult phraseResult;
    std::vector<int16_t> phraseAudioBuffer;
    synthesize(phonemeIds, synthesisConfig, voice.session,
               phraseAudioBuffer, phraseResult, &voice);
    audioBuffer.insert(audioBuffer.end(), phraseAudioBuffer.begin(),
                       phraseAudioBuffer.end());

    if (phraseResult.hasTimingInfo) {
      result.hasTimingInfo = true;
      for (auto timing : phraseResult.phonemeTimings) {
        timing.start_time += static_cast<float>(timingOffsetSeconds);
        timing.end_time += static_cast<float>(timingOffsetSeconds);
        result.phonemeTimings.push_back(std::move(timing));
      }
    }

    result.audioSeconds += phraseResult.audioSeconds;
    result.inferSeconds += phraseResult.inferSeconds;
    timingOffsetSeconds += phraseResult.audioSeconds;

    for (std::size_t i = 0; i < phraseSilenceSamples[phraseIdx]; ++i) {
      audioBuffer.push_back(0);
    }
    if (phraseSilenceSamples[phraseIdx] > 0) {
      const double silenceSeconds =
          static_cast<double>(phraseSilenceSamples[phraseIdx]) /
          static_cast<double>(synthesisConfig.sampleRate);
      result.audioSeconds += silenceSeconds;
      timingOffsetSeconds += silenceSeconds;
    }
  }

  if (sentenceSilenceSamples > 0) {
    for (std::size_t i = 0; i < sentenceSilenceSamples; ++i) {
      audioBuffer.push_back(0);
    }
    const double silenceSeconds =
        static_cast<double>(sentenceSilenceSamples) /
        static_cast<double>(synthesisConfig.sampleRate);
    result.audioSeconds += silenceSeconds;
    timingOffsetSeconds += silenceSeconds;
  }

  if (!missingPhonemes.empty()) {
    for (auto &[phoneme, count] : missingPhonemes) {
      spdlog::warn("Missing phoneme: '{}' ({})", phonemeToString(phoneme), count);
    }
  }

  if (result.audioSeconds > 0) {
    result.realTimeFactor = result.inferSeconds / result.audioSeconds;
  }

  if (audioCallback) {
    audioCallback();
  }

} /* phonemesToAudio */

void inspectPhonemes(Voice &voice, const std::vector<Phoneme> &phonemes,
                     const SynthesisConfig &synthesisConfig,
                     InspectionResult &result) {
  result.phonemeSentences.clear();
  result.phonemeIdSentences.clear();
  result.missingPhonemes.clear();
  const std::optional<LanguageId> requestLanguageId = synthesisConfig.languageId;
  const std::optional<std::string> requestLanguageCode =
      language_code_from_id(voice, requestLanguageId);
  result.resolvedLanguageId = requestLanguageId;
  result.resolvedSegments.clear();
  result.resolvedSegments.push_back({
      "",
      requestLanguageCode.value_or(std::string()),
      requestLanguageId,
      true,
  });

  result.phonemeSentences.push_back(phonemes);

  PhonemeIdConfig idConfig;
  idConfig.phonemeIdMap =
      std::make_shared<PhonemeIdMap>(voice.phonemizeConfig.phonemeIdMap);
  idConfig.interspersePad = voice.phonemizeConfig.interspersePad;
  idConfig.addBos = true;
  idConfig.addEos = true;

  std::vector<PhonemeId> ids;
  phonemes_to_ids(phonemes, idConfig, ids, result.missingPhonemes);
  result.phonemeIdSentences.push_back(std::move(ids));
}

} // namespace piper
