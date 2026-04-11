#include "piper_test_utils.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>

#include "spdlog/spdlog.h"
#include "utf8.h"

namespace piper {

namespace {

const float MAX_WAV_VALUE = 32767.0f;
const std::string UNKNOWN_PHONEME = "?";
const float JAPANESE_CL_OVERLAP_RATIO = 0.3f;

const std::unordered_map<char32_t, std::string> puaToPhoneme = {
    {0xE000, "a:"}, {0xE001, "i:"}, {0xE002, "u:"}, {0xE003, "e:"}, {0xE004, "o:"},
    {0xE005, "cl"}, {0xE006, "ky"}, {0xE007, "kw"}, {0xE008, "gy"}, {0xE009, "gw"},
    {0xE00A, "ty"}, {0xE00B, "dy"}, {0xE00C, "py"}, {0xE00D, "by"}, {0xE00E, "ch"},
    {0xE00F, "ts"}, {0xE010, "sh"}, {0xE011, "zy"}, {0xE012, "hy"}, {0xE013, "ny"},
    {0xE014, "my"}, {0xE015, "ry"}, {0xE016, "?!"}, {0xE017, "?."}, {0xE018, "?~"},
    {0xE019, "N_m"}, {0xE01A, "N_n"}, {0xE01B, "N_ng"}, {0xE01C, "N_uvular"}
};

void writeU16(std::array<uint8_t, 44> &header, std::size_t offset, uint16_t value) {
    header[offset] = static_cast<uint8_t>(value & 0xFF);
    header[offset + 1] = static_cast<uint8_t>((value >> 8) & 0xFF);
}

void writeU32(std::array<uint8_t, 44> &header, std::size_t offset, uint32_t value) {
    header[offset] = static_cast<uint8_t>(value & 0xFF);
    header[offset + 1] = static_cast<uint8_t>((value >> 8) & 0xFF);
    header[offset + 2] = static_cast<uint8_t>((value >> 16) & 0xFF);
    header[offset + 3] = static_cast<uint8_t>((value >> 24) & 0xFF);
}

} // namespace

std::string phonemeToString(Phoneme ph) {
    if (ph >= 0xE000 && ph <= 0xF8FF) {
        auto it = puaToPhoneme.find(ph);
        if (it != puaToPhoneme.end()) {
            return it->second;
        }
    }

    std::string result;
    utf8::append(ph, std::back_inserter(result));
    return result;
}

bool isSingleCodepoint(std::string s) {
    return utf8::distance(s.begin(), s.end()) == 1;
}

Phoneme getCodepoint(std::string s) {
    utf8::iterator characterIter(s.begin(), s.begin(), s.end());
    return *characterIter;
}

void parsePhonemizeConfig(json &configRoot, PhonemizeConfig &phonemizeConfig) {
    if (configRoot.contains("phoneme_type")) {
        auto phonemeTypeStr = configRoot["phoneme_type"].get<std::string>();
        if (phonemeTypeStr == "text") {
            phonemizeConfig.phonemeType = TextPhonemes;
            phonemizeConfig.interspersePad = true;
        } else if (phonemeTypeStr == "openjtalk") {
            phonemizeConfig.phonemeType = OpenJTalkPhonemes;
            phonemizeConfig.interspersePad = false;
        } else if (phonemeTypeStr == "multilingual" ||
                   phonemeTypeStr == "bilingual") {
            phonemizeConfig.phonemeType = MultilingualPhonemes;
            phonemizeConfig.interspersePad = true;
        }
    }

    if (configRoot.contains("phoneme_id_map")) {
        auto phonemeIdMapValue = configRoot["phoneme_id_map"];
        for (auto &fromPhonemeItem : phonemeIdMapValue.items()) {
            std::string fromPhoneme = fromPhonemeItem.key();
            if (!isSingleCodepoint(fromPhoneme)) {
                std::stringstream idsStr;
                for (auto &toIdValue : fromPhonemeItem.value()) {
                    idsStr << toIdValue.get<PhonemeId>() << ",";
                }

                spdlog::error("\"{}\" is not a single codepoint (ids={})", fromPhoneme, idsStr.str());
                throw std::runtime_error("Phonemes must be one codepoint (phoneme id map)");
            }

            auto fromCodepoint = getCodepoint(fromPhoneme);
            for (auto &toIdValue : fromPhonemeItem.value()) {
                phonemizeConfig.phonemeIdMap[fromCodepoint].push_back(toIdValue.get<PhonemeId>());
            }
        }
    }

    if (configRoot.contains("phoneme_map")) {
        if (!phonemizeConfig.phonemeMap) {
            phonemizeConfig.phonemeMap.emplace();
        }

        auto phonemeMapValue = configRoot["phoneme_map"];
        for (auto &fromPhonemeItem : phonemeMapValue.items()) {
            std::string fromPhoneme = fromPhonemeItem.key();
            if (!isSingleCodepoint(fromPhoneme)) {
                throw std::runtime_error("Phonemes must be one codepoint (phoneme map)");
            }

            auto fromCodepoint = getCodepoint(fromPhoneme);
            for (auto &toPhonemeValue : fromPhonemeItem.value()) {
                std::string toPhoneme = toPhonemeValue.get<std::string>();
                if (!isSingleCodepoint(toPhoneme)) {
                    throw std::runtime_error("Phonemes must be one codepoint (phoneme map)");
                }

                (*phonemizeConfig.phonemeMap)[fromCodepoint].push_back(getCodepoint(toPhoneme));
            }
        }
    }
}

void parseSynthesisConfig(json &configRoot, SynthesisConfig &synthesisConfig) {
    if (configRoot.contains("audio")) {
        auto audioValue = configRoot["audio"];
        if (audioValue.contains("sample_rate")) {
            synthesisConfig.sampleRate = audioValue.value("sample_rate", 22050);
        }
    }

    if (configRoot.contains("inference")) {
        auto inferenceValue = configRoot["inference"];
        if (inferenceValue.contains("noise_scale")) {
            synthesisConfig.noiseScale = inferenceValue.value("noise_scale", 0.667f);
        }
        if (inferenceValue.contains("length_scale")) {
            synthesisConfig.lengthScale = inferenceValue.value("length_scale", 1.0f);
        }
        if (inferenceValue.contains("noise_w")) {
            synthesisConfig.noiseW = inferenceValue.value("noise_w", 0.8f);
        }

        if (inferenceValue.contains("phoneme_silence")) {
            synthesisConfig.phonemeSilenceSeconds.emplace();
            auto phonemeSilenceValue = inferenceValue["phoneme_silence"];
            for (auto &phonemeItem : phonemeSilenceValue.items()) {
                std::string phonemeStr = phonemeItem.key();
                if (!isSingleCodepoint(phonemeStr)) {
                    throw std::runtime_error("Phonemes must be one codepoint (phoneme silence)");
                }

                (*synthesisConfig.phonemeSilenceSeconds)[getCodepoint(phonemeStr)] =
                    phonemeItem.value().get<float>();
            }
        }
    }
}

void parseModelConfig(json &configRoot, ModelConfig &modelConfig) {
    if (configRoot.contains("num_speakers")) {
        modelConfig.numSpeakers = configRoot["num_speakers"].get<SpeakerId>();
    }

    if (configRoot.contains("speaker_id_map")) {
        if (!modelConfig.speakerIdMap) {
            modelConfig.speakerIdMap.emplace();
        }

        auto speakerIdMapValue = configRoot["speaker_id_map"];
        for (auto &speakerItem : speakerIdMapValue.items()) {
            (*modelConfig.speakerIdMap)[speakerItem.key()] = speakerItem.value().get<SpeakerId>();
        }

        if (!configRoot.contains("num_speakers") && !modelConfig.speakerIdMap->empty()) {
            SpeakerId maxSpeakerId = 0;
            for (const auto &item : *modelConfig.speakerIdMap) {
                maxSpeakerId = std::max(maxSpeakerId, item.second);
            }
            modelConfig.numSpeakers = static_cast<int>(maxSpeakerId + 1);
        }
    }

    if (configRoot.contains("num_languages")) {
        modelConfig.numLanguages = configRoot["num_languages"].get<int>();
    }

    if (configRoot.contains("language_id_map")) {
        if (!modelConfig.languageIdMap) {
            modelConfig.languageIdMap.emplace();
        }

        auto languageIdMapValue = configRoot["language_id_map"];
        for (auto &languageItem : languageIdMapValue.items()) {
            (*modelConfig.languageIdMap)[languageItem.key()] =
                languageItem.value().get<LanguageId>();
        }

        if (!configRoot.contains("num_languages") && !modelConfig.languageIdMap->empty()) {
            LanguageId maxLanguageId = 0;
            for (const auto &item : *modelConfig.languageIdMap) {
                maxLanguageId = std::max(maxLanguageId, item.second);
            }
            modelConfig.numLanguages = static_cast<int>(maxLanguageId + 1);
        }
    }
}

bool parseJsonConfigFromString(const std::string &jsonText, json &configRoot,
                               std::string *errorMessage) {
    configRoot = json();
    try {
        configRoot = json::parse(jsonText);
        return true;
    } catch (const json::exception &e) {
        if (errorMessage) {
            *errorMessage = e.what();
        }
        return false;
    }
}

std::vector<PhonemeInfo> extractTimingsFromDurations(
    const std::vector<float> &durations,
    const std::vector<PhonemeId> &phonemeIds,
    const PhonemeIdMap &idMap,
    int hopSize,
    int sampleRate,
    PhonemeType phonemeType) {
    std::vector<PhonemeInfo> timings;
    std::unordered_map<PhonemeId, std::string> phonemeIdToStringMap;
    for (const auto &[phoneme, ids] : idMap) {
        if (!ids.empty()) {
            phonemeIdToStringMap[ids[0]] = phonemeToString(phoneme);
        }
    }

    float frameLength = static_cast<float>(hopSize) / sampleRate;
    float currentTime = 0.0f;
    int currentFrame = 0;

    for (std::size_t i = 0; i < phonemeIds.size() && i < durations.size(); ++i) {
        PhonemeId id = phonemeIds[i];
        float duration = durations[i];

        if (id == 0 || id == 1 || id == 2) {
            currentFrame += static_cast<int>(duration);
            currentTime += duration * frameLength;
            continue;
        }

        std::string phonemeStr = UNKNOWN_PHONEME;
        auto it = phonemeIdToStringMap.find(id);
        if (it != phonemeIdToStringMap.end()) {
            phonemeStr = it->second;
        } else if (id > 2 && id < 256) {
            phonemeStr = std::string(1, static_cast<char>(id));
        }

        PhonemeInfo info;
        info.phoneme = phonemeStr;
        info.start_time = currentTime;
        info.start_frame = currentFrame;

        currentFrame += static_cast<int>(duration);
        currentTime += duration * frameLength;

        info.end_time = currentTime;
        info.end_frame = currentFrame;
        timings.push_back(info);
    }

    if (usesOpenJTalk(phonemeType)) {
        for (std::size_t i = 0; i < timings.size(); ++i) {
            if (timings[i].phoneme == "cl" && i > 0) {
                float overlap = (timings[i].end_time - timings[i].start_time) * JAPANESE_CL_OVERLAP_RATIO;
                timings[i - 1].end_time += overlap;
                timings[i].start_time += overlap;
            }
        }
    }

    return timings;
}

void scaleAudioToInt16(const float *audio, std::size_t audioCount, std::vector<int16_t> &audioBuffer) {
    float maxAudioValue = 0.01f;
    for (std::size_t i = 0; i < audioCount; ++i) {
        float audioValue = std::abs(audio[i]);
        if (audioValue > maxAudioValue) {
            maxAudioValue = audioValue;
        }
    }

    float audioScale = MAX_WAV_VALUE / std::max(0.01f, maxAudioValue);
    audioBuffer.clear();
    audioBuffer.reserve(audioCount);

    for (std::size_t i = 0; i < audioCount; ++i) {
        int16_t intAudioValue = static_cast<int16_t>(
            std::clamp(audio[i] * audioScale,
                       static_cast<float>(std::numeric_limits<int16_t>::min()),
                       static_cast<float>(std::numeric_limits<int16_t>::max())));
        audioBuffer.push_back(intAudioValue);
    }
}

std::array<uint8_t, 44> createWavHeader(std::size_t sampleCount,
                                        int sampleRate,
                                        int channels,
                                        int sampleWidth) {
    uint32_t dataSize = static_cast<uint32_t>(sampleCount * channels * sampleWidth);
    uint32_t byteRate = static_cast<uint32_t>(sampleRate * channels * sampleWidth);
    uint16_t blockAlign = static_cast<uint16_t>(channels * sampleWidth);
    uint16_t bitsPerSample = static_cast<uint16_t>(sampleWidth * 8);

    std::array<uint8_t, 44> header{};
    std::memcpy(header.data(), "RIFF", 4);
    writeU32(header, 4, 36 + dataSize);
    std::memcpy(header.data() + 8, "WAVE", 4);
    std::memcpy(header.data() + 12, "fmt ", 4);
    writeU32(header, 16, 16);
    writeU16(header, 20, 1);
    writeU16(header, 22, static_cast<uint16_t>(channels));
    writeU32(header, 24, static_cast<uint32_t>(sampleRate));
    writeU32(header, 28, byteRate);
    writeU16(header, 32, blockAlign);
    writeU16(header, 34, bitsPerSample);
    std::memcpy(header.data() + 36, "data", 4);
    writeU32(header, 40, dataSize);
    return header;
}

} // namespace piper
