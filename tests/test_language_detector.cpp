#include <filesystem>
#include <fstream>

#include <gtest/gtest.h>

#include "json.hpp"
#include "language_detector.hpp"
#include "multilingual_phonemize.hpp"

namespace {

struct CapabilityRow {
	std::string model_family;
	std::string language_code;
	std::vector<std::string> aliases;
	std::string support_tier;
	std::string frontend_backend;
	std::string routing_mode;
	bool text_supported = false;
	bool auto_supported = false;
	int expected_language_id = -1;
	std::string default_latin_language = "en";
	std::string sample_text;
	std::string sample_segment_text;
	std::string expected_phoneme_preview;
	std::string expected_error_contains;
};

std::string sentence_to_utf8(const std::vector<piper::Phoneme> &sentence) {
	std::string result;
	for (piper::Phoneme phoneme : sentence) {
		result += piper::phonemeToString(phoneme);
	}
	return result;
}

std::filesystem::path find_fixture_path() {
	const std::filesystem::path relative =
			std::filesystem::path("tests") / "fixtures" / "multilingual_capability_matrix.json";
	std::filesystem::path current = std::filesystem::current_path();
	for (int depth = 0; depth < 8; ++depth) {
		const std::filesystem::path candidate = current / relative;
		if (std::filesystem::exists(candidate)) {
			return candidate;
		}

		if (!current.has_parent_path()) {
			break;
		}
		const std::filesystem::path parent = current.parent_path();
		if (parent == current) {
			break;
		}
		current = parent;
	}

	return {};
}

std::vector<CapabilityRow> load_capability_matrix() {
	const std::filesystem::path fixture_path = find_fixture_path();
	if (fixture_path.empty()) {
		return {};
	}

	std::ifstream input(fixture_path);
	if (!input.is_open()) {
		return {};
	}

	nlohmann::json root;
	input >> root;
	if (!root.is_array()) {
		return {};
	}

	std::vector<CapabilityRow> rows;
	rows.reserve(root.size());
	for (const auto &item : root) {
		CapabilityRow row;
		row.model_family = item.value("model_family", "");
		row.language_code = item.value("language_code", "");
		row.support_tier = item.value("support_tier", "");
		row.frontend_backend = item.value("frontend_backend", "");
		row.routing_mode = item.value("routing_mode", "");
		row.text_supported = item.value("text_supported", false);
		row.auto_supported = item.value("auto_supported", false);
		row.expected_language_id = item.value("expected_language_id", -1);
		row.default_latin_language = item.value("default_latin_language", "en");
		row.sample_text = item.value("sample_text", "");
		row.sample_segment_text = item.value("sample_segment_text", "");
		row.expected_phoneme_preview = item.value("expected_phoneme_preview", "");
		row.expected_error_contains = item.value("expected_error_contains", "");
		if (item.contains("aliases") && item["aliases"].is_array()) {
			for (const auto &alias : item["aliases"]) {
				row.aliases.push_back(alias.get<std::string>());
			}
		}
		rows.push_back(std::move(row));
	}

	return rows;
}

std::filesystem::path find_generated_doc_path() {
	const std::filesystem::path relative =
			std::filesystem::path("docs") / "generated" / "multilingual_capability_matrix.md";
	std::filesystem::path current = std::filesystem::current_path();
	for (int depth = 0; depth < 8; ++depth) {
		const std::filesystem::path candidate = current / relative;
		if (std::filesystem::exists(candidate)) {
			return candidate;
		}

		if (!current.has_parent_path()) {
			break;
		}
		const std::filesystem::path parent = current.parent_path();
		if (parent == current) {
			break;
		}
		current = parent;
	}

	return {};
}

std::string read_generated_doc() {
	const std::filesystem::path doc_path = find_generated_doc_path();
	if (doc_path.empty()) {
		return {};
	}

	std::ifstream input(doc_path);
	if (!input.is_open()) {
		return {};
	}

	return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

std::string tier_label(const CapabilityRow &row) {
	if (row.support_tier == "preview") {
		return "preview";
	}
	if (row.support_tier == "experimental" && row.routing_mode == "explicit_only") {
		return "experimental explicit-only";
	}
	if (row.support_tier == "phoneme_only") {
		return "phoneme-only";
	}
	return row.support_tier;
}

const CapabilityRow *find_row(
		const std::vector<CapabilityRow> &rows, const std::string &language_code) {
	for (const auto &row : rows) {
		if (row.language_code == language_code) {
			return &row;
		}
	}
	return nullptr;
}

std::vector<std::vector<piper::Phoneme>> phonemize_by_language(
		const std::string &language_code, const std::string &text) {
	std::vector<std::vector<piper::Phoneme>> phonemes;
	if (language_code == "es") {
		piper::phonemize_spanish(text, phonemes);
	} else if (language_code == "fr") {
		piper::phonemize_french(text, phonemes);
	} else if (language_code == "pt") {
		piper::phonemize_portuguese(text, phonemes);
	}
	return phonemes;
}

} // namespace

TEST(LanguageDetectorTest, DetectsKana) {
	piper::UnicodeLanguageDetector detector({"ja", "en"});
	EXPECT_TRUE(detector.hasKana("こんにちは"));
	EXPECT_FALSE(detector.hasKana("hello"));
}

TEST(LanguageDetectorTest, SplitsJaEnSegments) {
	piper::UnicodeLanguageDetector detector({"ja", "en"});
	auto segments = detector.segmentText("Helloこんにちはworld");

	ASSERT_EQ(segments.size(), 3u);
	EXPECT_EQ(segments[0].lang, "en");
	EXPECT_EQ(segments[0].text, "Hello");
	EXPECT_EQ(segments[1].lang, "ja");
	EXPECT_EQ(segments[1].text, "こんにちは");
	EXPECT_EQ(segments[2].lang, "en");
	EXPECT_EQ(segments[2].text, "world");
}

TEST(LanguageDetectorTest, DominantLanguagePrefersMajority) {
	piper::UnicodeLanguageDetector detector({"ja", "en"});
	EXPECT_EQ(piper::detectDominantLanguage("Helloこんにちはworld", detector), "en");
}

TEST(MultilingualPhonemizeTest, WrapperUsesJaEnDefaults) {
	const auto rows = load_capability_matrix();
	ASSERT_FALSE(rows.empty()) << "tests/fixtures/multilingual_capability_matrix.json must be readable";

	const CapabilityRow *ja = find_row(rows, "ja");
	ASSERT_NE(ja, nullptr);

	auto segments = piper::segmentMultilingualText(
			ja->sample_segment_text, {"ja", "en"}, ja->default_latin_language);

	ASSERT_EQ(segments.size(), 3u);
	EXPECT_EQ(segments[0].lang, "en");
	EXPECT_EQ(segments[1].lang, "ja");
	EXPECT_EQ(segments[2].lang, "en");
}

TEST(MultilingualPhonemizeTest, SupportMatrixReflectsExpandedLanguages) {
	const auto rows = load_capability_matrix();
	ASSERT_FALSE(rows.empty()) << "tests/fixtures/multilingual_capability_matrix.json must be readable";

	for (const auto &row : rows) {
		const auto routing_mode = piper::getMultilingualTextRoutingMode(row.language_code);
		if (row.routing_mode == "auto") {
			EXPECT_EQ(routing_mode, piper::MultilingualTextRoutingMode::AutoDetect)
					<< row.language_code;
		} else if (row.routing_mode == "explicit_only") {
			EXPECT_EQ(routing_mode, piper::MultilingualTextRoutingMode::ExplicitOnly)
					<< row.language_code;
		} else if (row.routing_mode == "phoneme_only") {
			EXPECT_EQ(routing_mode, piper::MultilingualTextRoutingMode::Unsupported)
					<< row.language_code;
		} else {
			FAIL() << "Unknown routing_mode in capability matrix: " << row.routing_mode;
		}

		EXPECT_EQ(piper::supportsMultilingualTextPhonemization(row.language_code),
				row.text_supported) << row.language_code;
		EXPECT_EQ(piper::supportsMultilingualAutoRouting(row.language_code),
				row.auto_supported) << row.language_code;
		EXPECT_FALSE(row.frontend_backend.empty()) << row.language_code;
		EXPECT_FALSE(row.support_tier.empty()) << row.language_code;

		if (!row.expected_error_contains.empty()) {
			EXPECT_NE(piper::getMultilingualTextSupportError(row.language_code)
					.find(row.expected_error_contains), std::string::npos)
					<< row.language_code;
		}
	}
}

TEST(MultilingualPhonemizeTest, WrapperUsesConfiguredLatinDefaultLanguage) {
	const auto rows = load_capability_matrix();
	ASSERT_FALSE(rows.empty()) << "tests/fixtures/multilingual_capability_matrix.json must be readable";

	const CapabilityRow *fr = find_row(rows, "fr");
	ASSERT_NE(fr, nullptr);

	auto segments = piper::segmentMultilingualText(
			fr->sample_segment_text, {"ja", "en", "fr"}, fr->default_latin_language);

	ASSERT_EQ(segments.size(), 2u);
	EXPECT_EQ(segments[0].lang, "fr");
	EXPECT_EQ(segments[0].text, "salut");
	EXPECT_EQ(segments[1].lang, "ja");
	EXPECT_EQ(segments[1].text, "こんにちは");
}

TEST(MultilingualPhonemizeTest, SpanishRuleBasedPhonemizerProducesPhonemes) {
	const auto rows = load_capability_matrix();
	ASSERT_FALSE(rows.empty()) << "tests/fixtures/multilingual_capability_matrix.json must be readable";

	const CapabilityRow *es = find_row(rows, "es");
	ASSERT_NE(es, nullptr);

	const auto phonemes = phonemize_by_language(es->language_code, es->sample_text);

	ASSERT_EQ(phonemes.size(), 1u);
	EXPECT_EQ(sentence_to_utf8(phonemes[0]), es->expected_phoneme_preview);
}

TEST(MultilingualPhonemizeTest, FrenchRuleBasedPhonemizerProducesPhonemes) {
	const auto rows = load_capability_matrix();
	ASSERT_FALSE(rows.empty()) << "tests/fixtures/multilingual_capability_matrix.json must be readable";

	const CapabilityRow *fr = find_row(rows, "fr");
	ASSERT_NE(fr, nullptr);

	const auto phonemes = phonemize_by_language(fr->language_code, fr->sample_text);

	ASSERT_EQ(phonemes.size(), 1u);
	EXPECT_EQ(sentence_to_utf8(phonemes[0]), fr->expected_phoneme_preview);
}

TEST(MultilingualPhonemizeTest, PortugueseRuleBasedPhonemizerProducesPhonemes) {
	const auto rows = load_capability_matrix();
	ASSERT_FALSE(rows.empty()) << "tests/fixtures/multilingual_capability_matrix.json must be readable";

	const CapabilityRow *pt = find_row(rows, "pt");
	ASSERT_NE(pt, nullptr);

	const auto phonemes = phonemize_by_language(pt->language_code, pt->sample_text);

	ASSERT_EQ(phonemes.size(), 1u);
	EXPECT_EQ(sentence_to_utf8(phonemes[0]), pt->expected_phoneme_preview);
}

TEST(MultilingualPhonemizeTest, PhonemeOnlyLanguagesRejectTextInputCapability) {
	const auto rows = load_capability_matrix();
	ASSERT_FALSE(rows.empty()) << "tests/fixtures/multilingual_capability_matrix.json must be readable";

	const CapabilityRow *zh = find_row(rows, "zh");
	ASSERT_NE(zh, nullptr);

	EXPECT_FALSE(piper::supportsMultilingualTextPhonemization(zh->language_code));
	EXPECT_FALSE(piper::supportsMultilingualAutoRouting(zh->language_code));
	EXPECT_EQ(piper::getMultilingualTextRoutingMode(zh->language_code),
			piper::MultilingualTextRoutingMode::Unsupported);
	EXPECT_NE(piper::getMultilingualTextSupportError(zh->language_code).find(
					zh->expected_error_contains), std::string::npos);
}

TEST(MultilingualPhonemizeTest, GeneratedDocMatchesCapabilityMatrix) {
	const auto rows = load_capability_matrix();
	ASSERT_FALSE(rows.empty()) << "tests/fixtures/multilingual_capability_matrix.json must be readable";

	const std::string generated_doc = read_generated_doc();
	ASSERT_FALSE(generated_doc.empty()) << "docs/generated/multilingual_capability_matrix.md must be readable";

	EXPECT_NE(generated_doc.find("Generated from `tests/fixtures/multilingual_capability_matrix.json`"),
			std::string::npos);
	EXPECT_NE(generated_doc.find("experimental explicit-only"), std::string::npos);
	EXPECT_NE(generated_doc.find("phoneme-only"), std::string::npos);

	for (const auto &row : rows) {
		const std::string language_tag = "`" + row.language_code + "`";
		const std::string tier_tag = "`" + tier_label(row) + "`";
		const std::string backend_tag = "`" + row.frontend_backend + "`";
		EXPECT_NE(generated_doc.find(language_tag), std::string::npos) << row.language_code;
		EXPECT_NE(generated_doc.find(tier_tag), std::string::npos) << row.language_code;
		EXPECT_NE(generated_doc.find(backend_tag), std::string::npos) << row.language_code;
	}
}
