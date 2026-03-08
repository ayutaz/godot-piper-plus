#ifndef OPENJTALK_PHONEMIZE_H
#define OPENJTALK_PHONEMIZE_H

#include <string>
#include <vector>
#include <unordered_map>
#include "piper.hpp"
#include "openjtalk_wrapper.h"

namespace piper {

// Prosody info for a phoneme (A1/A2/A3 values from OpenJTalk)
struct ProsodyFeature {
    int a1;  // Relative position from accent nucleus
    int a2;  // Position in accent phrase (1-based)
    int a3;  // Total morae in accent phrase
};

// Phonemize Japanese text using OpenJTalk
void phonemize_openjtalk(const std::string &text,
                        std::vector<std::vector<Phoneme>> &phonemes);

// Phonemize Japanese text with prosody features
void phonemize_openjtalk_with_prosody(
    const std::string &text,
    std::vector<std::vector<Phoneme>> &phonemes,
    std::vector<std::vector<ProsodyFeature>> &prosodyFeatures);

} // namespace piper

#endif // OPENJTALK_PHONEMIZE_H