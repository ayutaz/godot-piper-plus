// phoneme_ids.hpp - Replacement for piper-phonemize/phoneme_ids.hpp
// Provides Phoneme/PhonemeId types and phonemes_to_ids() without GPL dependencies
#ifndef PIPER_PHONEME_IDS_HPP
#define PIPER_PHONEME_IDS_HPP

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace piper {

typedef char32_t Phoneme;
typedef int64_t PhonemeId;
typedef std::map<Phoneme, std::vector<PhonemeId>> PhonemeIdMap;

struct PhonemeIdConfig {
	std::shared_ptr<PhonemeIdMap> phonemeIdMap;
	PhonemeId idPad = 0;
	PhonemeId idBos = 1;
	PhonemeId idEos = 2;
	bool interspersePad = true;
	bool addBos = true;
	bool addEos = true;
};

// Convert phonemes to phoneme IDs using the phoneme ID map
inline void phonemes_to_ids(const std::vector<Phoneme> &phonemes,
                            PhonemeIdConfig &idConfig,
                            std::vector<PhonemeId> &phonemeIds,
                            std::map<Phoneme, std::size_t> &missingPhonemes) {
	phonemeIds.clear();

	if (idConfig.addBos) {
		phonemeIds.push_back(idConfig.idBos);
		if (idConfig.interspersePad) {
			phonemeIds.push_back(idConfig.idPad);
		}
	}

	for (auto phoneme : phonemes) {
		if (idConfig.phonemeIdMap) {
			auto it = idConfig.phonemeIdMap->find(phoneme);
			if (it != idConfig.phonemeIdMap->end()) {
				for (auto id : it->second) {
					phonemeIds.push_back(id);
					if (idConfig.interspersePad) {
						phonemeIds.push_back(idConfig.idPad);
					}
				}
			} else {
				missingPhonemes[phoneme]++;
			}
		}
	}

	if (idConfig.addEos) {
		phonemeIds.push_back(idConfig.idEos);
	}
}

} // namespace piper

#endif // PIPER_PHONEME_IDS_HPP
