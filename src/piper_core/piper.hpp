#ifndef PIPER_H_
#define PIPER_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <onnxruntime_cxx_api.h>
#include "phoneme_ids.hpp"

#include "json.hpp"

using json = nlohmann::json;

namespace piper {

class CustomDictionary;

enum ExecutionProvider {
    EP_CPU = 0,
    EP_COREML = 1,
    EP_DIRECTML = 2,
    EP_NNAPI = 3,
    EP_AUTO = 4,
    EP_CUDA = 5,
};

typedef int64_t SpeakerId;
typedef int64_t LanguageId;

struct PiperConfig {
  // Empty config - eSpeak and tashkeel removed for GDExtension
};

enum PhonemeType {
  TextPhonemes = 0,
  OpenJTalkPhonemes = 1,
  MultilingualPhonemes = 2,
};

inline bool usesOpenJTalk(PhonemeType type) {
  return type == OpenJTalkPhonemes || type == MultilingualPhonemes;
}

struct PhonemizeConfig {
  PhonemeType phonemeType = TextPhonemes;
  std::optional<std::map<Phoneme, std::vector<Phoneme>>> phonemeMap;
  std::map<Phoneme, std::vector<PhonemeId>> phonemeIdMap;

  PhonemeId idPad = 0; // padding (optionally interspersed)
  PhonemeId idBos = 1; // beginning of sentence
  PhonemeId idEos = 2; // end of sentence
  bool interspersePad = true;
};

struct SynthesisConfig {
  // VITS inference settings
  float noiseScale = 0.667f;
  float lengthScale = 1.0f;
  float noiseW = 0.8f;

  // Audio settings
  int sampleRate = 22050;
  int sampleWidth = 2; // 16-bit
  int channels = 1;    // mono

  // Speaker id from 0 to numSpeakers - 1
  std::optional<SpeakerId> speakerId;

  // Language id from 0 to numLanguages - 1
  std::optional<LanguageId> languageId;

  // Extra silence
  float sentenceSilenceSeconds = 0.2f;
  std::optional<std::map<piper::Phoneme, float>> phonemeSilenceSeconds;
};

struct ModelConfig {
  int numSpeakers = 0;
  int numLanguages = 1;

  // speaker name -> id
  std::optional<std::map<std::string, SpeakerId>> speakerIdMap;

  // language code -> id
  std::optional<std::map<std::string, LanguageId>> languageIdMap;
};

struct ModelSession {
  Ort::Env env;
  Ort::SessionOptions options;
  Ort::AllocatorWithDefaultOptions allocator;
  Ort::Session onnx;
  std::vector<uint8_t> modelData;
  bool hasDurationOutput = false;  // Whether model outputs duration information
  bool hasProsodyInput = false;    // Whether model accepts prosody_features input
  bool hasMultiSpeaker = false;    // Whether model has sid (speaker ID) input
  bool hasLidInput = false;        // Whether model has lid (language ID) input
  std::string lidInputName = "lid";

  ModelSession() : onnx(nullptr){};
};

struct PhonemeInfo {
  std::string phoneme;     // Phoneme string
  float start_time;        // Start time in seconds
  float end_time;          // End time in seconds
  int start_frame;         // Start frame index
  int end_frame;           // End frame index
};

struct ResolvedSegment {
  std::string text;
  std::string languageCode;
  std::optional<LanguageId> languageId;
  bool isPhonemeInput = false;
};

struct SynthesisResult {
  double inferSeconds = 0.0;
  double audioSeconds = 0.0;
  double realTimeFactor = 0.0;
  std::vector<PhonemeInfo> phonemeTimings;  // Phoneme timing information
  bool hasTimingInfo = false;                // Whether timing info is available
  std::vector<ResolvedSegment> resolvedSegments;
};

struct InspectionResult {
  std::vector<std::vector<Phoneme>> phonemeSentences;
  std::vector<std::vector<PhonemeId>> phonemeIdSentences;
  std::map<Phoneme, std::size_t> missingPhonemes;
  std::optional<LanguageId> resolvedLanguageId;
  std::vector<ResolvedSegment> resolvedSegments;
};

struct Voice {
  json configRoot;
  PhonemizeConfig phonemizeConfig;
  SynthesisConfig synthesisConfig;
  ModelConfig modelConfig;
  ModelSession session;
  std::shared_ptr<CustomDictionary> customDictionary;
  std::unordered_map<std::string, std::string> cmuDict;
  std::unordered_map<int, std::string> pinyinSingleDict;
  std::unordered_map<std::string, std::string> pinyinPhraseDict;
};

// True if the string is a single UTF-8 codepoint
bool isSingleCodepoint(std::string s);

// Get the first UTF-8 codepoint of a string
Phoneme getCodepoint(std::string s);

// Convert a phoneme to a readable UTF-8 string.
std::string phonemeToString(Phoneme ph);

// Get version of Piper
std::string getVersion();

// Load JSON config information for phonemization/synthesis/model metadata.
bool parseJsonConfigFromString(const std::string &jsonText, json &configRoot,
                               std::string *errorMessage = nullptr);
void parsePhonemizeConfig(json &configRoot, PhonemizeConfig &phonemizeConfig);
void parseSynthesisConfig(json &configRoot, SynthesisConfig &synthesisConfig);
void parseModelConfig(json &configRoot, ModelConfig &modelConfig);

// Extract per-phoneme timing information from duration outputs.
std::vector<PhonemeInfo> extractTimingsFromDurations(
    const std::vector<float> &durations,
    const std::vector<PhonemeId> &phonemeIds,
    const PhonemeIdMap &idMap,
    int hopSize,
    int sampleRate,
    PhonemeType phonemeType);

// Audio helpers used by tests and conversion code.
void scaleAudioToInt16(const float *audio, std::size_t audioCount,
                       std::vector<int16_t> &audioBuffer);
std::array<uint8_t, 44> createWavHeader(std::size_t sampleCount,
                                        int sampleRate,
                                        int channels = 1,
                                        int sampleWidth = 2);

// Must be called before using textToAudio
void initialize(PiperConfig &config);

// Clean up
void terminate(PiperConfig &config);

// Load Onnx model and JSON config file
void loadVoice(PiperConfig &config, std::string modelPath,
               std::string modelConfigPath, Voice &voice,
               std::optional<SpeakerId> &speakerId, int executionProvider = EP_CPU,
               int executionDeviceId = 0);

void loadVoice(PiperConfig &config, std::vector<uint8_t> modelData,
               const std::string &modelSourceLabel,
               const std::string &modelConfigJson,
               const std::string &modelConfigSourceLabel, Voice &voice,
               std::optional<SpeakerId> &speakerId, int executionProvider = EP_CPU,
               int executionDeviceId = 0,
               const std::optional<std::string> &cmuDictJson = std::nullopt,
               const std::string &cmuDictSourceLabel = {},
               const std::optional<std::string> &pinyinSingleDictJson = std::nullopt,
               const std::string &pinyinSingleDictSourceLabel = {},
               const std::optional<std::string> &pinyinPhraseDictJson = std::nullopt,
               const std::string &pinyinPhraseDictSourceLabel = {});

// Phonemize text and synthesize audio
void textToAudio(PiperConfig &config, Voice &voice, std::string text,
                 const SynthesisConfig &synthesisConfig,
                 std::vector<int16_t> &audioBuffer, SynthesisResult &result,
                 const std::function<void()> &audioCallback);

// Synthesize audio directly from phonemes
void phonemesToAudio(PiperConfig &config, Voice &voice,
                     const std::vector<Phoneme> &phonemes,
                     const SynthesisConfig &synthesisConfig,
                     std::vector<int16_t> &audioBuffer,
                     SynthesisResult &result,
                     const std::function<void()> &audioCallback = nullptr);

// Inspect phonemization and phoneme-id conversion without ONNX inference.
void inspectText(PiperConfig &config, Voice &voice, std::string text,
                 const SynthesisConfig &synthesisConfig,
                 InspectionResult &result);
void inspectPhonemes(Voice &voice, const std::vector<Phoneme> &phonemes,
                     const SynthesisConfig &synthesisConfig,
                     InspectionResult &result);

} // namespace piper

#endif // PIPER_H_
