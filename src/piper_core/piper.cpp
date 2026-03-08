#include <array>
#include <chrono>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <filesystem>
#include <unordered_map>
#include <regex>

#include <onnxruntime_cxx_api.h>
#include "spdlog/spdlog.h"

#include "json.hpp"
#include "piper.hpp"
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

void initialize(PiperConfig &config) {
  spdlog::info("Initialized piper");
}

void terminate(PiperConfig &config) {
  spdlog::info("Terminated piper");
}

void loadModel(std::string modelPath, ModelSession &session, int executionProvider = EP_CPU) {
  spdlog::debug("loadModel called with path: {}, EP: {}", modelPath, executionProvider);

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
  if (executionProvider != EP_CPU) {
    std::string ep_name;
    switch (executionProvider) {
      case EP_COREML: ep_name = "CoreML"; break;
      case EP_DIRECTML: ep_name = "DML"; break;
      case EP_NNAPI: ep_name = "NNAPI"; break;
      default: break;
    }
    if (!ep_name.empty()) {
      try {
        std::unordered_map<std::string, std::string> ep_options;
        session.options.AppendExecutionProvider(ep_name, ep_options);
        using_gpu_ep = true;
        spdlog::info("Using {} execution provider", ep_name);
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
    }
  }
}

// Load Onnx model and JSON config file
void loadVoice(PiperConfig &config, std::string modelPath,
               std::string modelConfigPath, Voice &voice,
               std::optional<SpeakerId> &speakerId, int executionProvider) {
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

  loadModel(modelPath, voice.session, executionProvider);

} /* loadVoice */

// Phoneme ids to WAV audio
void synthesize(std::vector<PhonemeId> &phonemeIds,
                SynthesisConfig &synthesisConfig, ModelSession &session,
                std::vector<int16_t> &audioBuffer, SynthesisResult &result,
                Voice *voice = nullptr,
                std::vector<int64_t> *prosodyFeatures = nullptr) {
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

  if (session.hasMultiSpeaker) {
    inputTensors.push_back(Ort::Value::CreateTensor<int64_t>(
        memoryInfo, speakerId.data(), speakerId.size(), speakerIdShape.data(),
        speakerIdShape.size()));
    inputNamesVec.push_back("sid");
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
                 std::vector<int16_t> &audioBuffer, SynthesisResult &result,
                 const std::function<void()> &audioCallback) {

  std::size_t sentenceSilenceSamples = 0;
  if (voice.synthesisConfig.sentenceSilenceSeconds > 0) {
    sentenceSilenceSamples = (std::size_t)(
        voice.synthesisConfig.sentenceSilenceSeconds *
        voice.synthesisConfig.sampleRate * voice.synthesisConfig.channels);
  }

  // Parse text for [[ phonemes ]] notation
  auto textSegments = parsePhonemeNotation(text);

  // Phonemes for each sentence
  spdlog::debug("Phonemizing text: {}", text);
  std::vector<std::vector<Phoneme>> phonemes;

  // Prosody features for each sentence (only used for OpenJTalk with prosody-enabled models)
  std::vector<std::vector<ProsodyFeature>> allProsodyFeatures;
  bool useProsody = voice.session.hasProsodyInput &&
                    voice.phonemizeConfig.phonemeType == OpenJTalkPhonemes;

  // Process each segment
  for (const auto& segment : textSegments) {
    if (segment.isPhonemes) {
      // Direct phoneme input
      spdlog::debug("Processing direct phoneme input: {}", segment.text);
      auto parsedPhonemes = parsePhonemeString(segment.text, static_cast<int>(voice.phonemizeConfig.phonemeType));

      // Add as a single "sentence"
      phonemes.push_back(parsedPhonemes);

      // Add empty prosody features for direct phoneme input
      if (useProsody) {
        std::vector<ProsodyFeature> emptyProsody(parsedPhonemes.size(), {0, 0, 0});
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
      } else {
        // TextPhonemes: not supported in GDExtension (no espeak, no codepoints)
        throw std::runtime_error("Only OpenJTalkPhonemes phoneme type is currently supported.");
      }

      // Add all sentences from this segment
      for (size_t i = 0; i < segmentPhonemes.size(); i++) {
        phonemes.push_back(std::move(segmentPhonemes[i]));

        if (useProsody) {
          if (i < segmentProsody.size()) {
            allProsodyFeatures.push_back(std::move(segmentProsody[i]));
          } else {
            // Fallback: create zero prosody features
            std::vector<ProsodyFeature> zeroProsody(phonemes.back().size(), {0, 0, 0});
            allProsodyFeatures.push_back(std::move(zeroProsody));
          }
        }
      }
    }
  }

  // Synthesize each sentence independently.
  std::vector<PhonemeId> phonemeIds;
  std::map<Phoneme, std::size_t> missingPhonemes;
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

    if (voice.synthesisConfig.phonemeSilenceSeconds) {
      // Split into phrases
      std::map<Phoneme, float> &phonemeSilenceSeconds =
          *voice.synthesisConfig.phonemeSilenceSeconds;

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
              (std::size_t)(phonemeSilenceSeconds[currentPhoneme] *
                            voice.synthesisConfig.sampleRate *
                            voice.synthesisConfig.channels));

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

      synthesize(phonemeIds, voice.synthesisConfig, voice.session, audioBuffer,
                 phraseResults[phraseIdx], &voice, prosodyPtr);

      // Add end of phrase silence
      for (std::size_t i = 0; i < phraseSilenceSamples[phraseIdx]; i++) {
        audioBuffer.push_back(0);
      }

      result.audioSeconds += phraseResults[phraseIdx].audioSeconds;
      result.inferSeconds += phraseResults[phraseIdx].inferSeconds;

      phonemeIds.clear();
    }

    // Add end of sentence silence
    if (sentenceSilenceSamples > 0) {
      for (std::size_t i = 0; i < sentenceSilenceSamples; i++) {
        audioBuffer.push_back(0);
      }
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

// Synthesize audio directly from phonemes
void phonemesToAudio(PiperConfig &config, Voice &voice,
                     const std::vector<Phoneme> &phonemes,
                     std::vector<int16_t> &audioBuffer,
                     SynthesisResult &result,
                     const std::function<void()> &audioCallback) {

  // Convert phonemes to IDs
  std::vector<PhonemeId> phonemeIds;
  std::map<Phoneme, std::size_t> missingPhonemes;

  PhonemeIdConfig idConfig;
  idConfig.phonemeIdMap =
      std::make_shared<PhonemeIdMap>(voice.phonemizeConfig.phonemeIdMap);
  idConfig.interspersePad = voice.phonemizeConfig.interspersePad;

  // The phonemes_to_ids function handles BOS/EOS automatically based on addBos/addEos flags
  idConfig.addBos = true;
  idConfig.addEos = true;

  // Convert phonemes to IDs
  phonemes_to_ids(phonemes, idConfig, phonemeIds, missingPhonemes);

  // Report missing phonemes
  if (!missingPhonemes.empty()) {
    for (auto& [phoneme, count] : missingPhonemes) {
      spdlog::warn("Missing phoneme: '{}' ({})", phonemeToString(phoneme), count);
    }
  }

  // Synthesize audio
  synthesize(phonemeIds, voice.synthesisConfig, voice.session, audioBuffer, result, &voice);

  // Call the audio callback if provided
  if (audioCallback) {
    audioCallback();
  }

} /* phonemesToAudio */

} // namespace piper
