#ifndef PIPER_H_
#define PIPER_H_

#include <array>
#include <fstream>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include <onnxruntime_cxx_api.h>
#include "phoneme_ids.hpp"

#include "json.hpp"

using json = nlohmann::json;

namespace piper {

enum ExecutionProvider {
    EP_CPU = 0,
    EP_COREML = 1,
    EP_DIRECTML = 2,
    EP_NNAPI = 3,
    EP_AUTO = 4,
};

typedef int64_t SpeakerId;

struct PiperConfig {
  // Empty config - eSpeak and tashkeel removed for GDExtension
};

enum PhonemeType {
  TextPhonemes = 0,
  OpenJTalkPhonemes = 1
};

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

  // Extra silence
  float sentenceSilenceSeconds = 0.2f;
  std::optional<std::map<piper::Phoneme, float>> phonemeSilenceSeconds;
};

struct ModelConfig {
  int numSpeakers;

  // speaker name -> id
  std::optional<std::map<std::string, SpeakerId>> speakerIdMap;
};

struct ModelSession {
  Ort::Env env;
  Ort::SessionOptions options;
  Ort::AllocatorWithDefaultOptions allocator;
  Ort::Session onnx;
  bool hasDurationOutput = false;  // Whether model outputs duration information
  bool hasProsodyInput = false;    // Whether model accepts prosody_features input
  bool hasMultiSpeaker = false;    // Whether model has sid (speaker ID) input

  ModelSession() : onnx(nullptr){};
};

struct PhonemeInfo {
  std::string phoneme;     // Phoneme string
  float start_time;        // Start time in seconds
  float end_time;          // End time in seconds
  int start_frame;         // Start frame index
  int end_frame;           // End frame index
};

struct SynthesisResult {
  double inferSeconds;
  double audioSeconds;
  double realTimeFactor;
  std::vector<PhonemeInfo> phonemeTimings;  // Phoneme timing information
  bool hasTimingInfo = false;                // Whether timing info is available
};

struct Voice {
  json configRoot;
  PhonemizeConfig phonemizeConfig;
  SynthesisConfig synthesisConfig;
  ModelConfig modelConfig;
  ModelSession session;
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
               std::optional<SpeakerId> &speakerId, int executionProvider = EP_CPU);

// Phonemize text and synthesize audio
void textToAudio(PiperConfig &config, Voice &voice, std::string text,
                 std::vector<int16_t> &audioBuffer, SynthesisResult &result,
                 const std::function<void()> &audioCallback);

// Synthesize audio directly from phonemes
void phonemesToAudio(PiperConfig &config, Voice &voice,
                     const std::vector<Phoneme> &phonemes,
                     std::vector<int16_t> &audioBuffer,
                     SynthesisResult &result,
                     const std::function<void()> &audioCallback = nullptr);

} // namespace piper

#endif // PIPER_H_
