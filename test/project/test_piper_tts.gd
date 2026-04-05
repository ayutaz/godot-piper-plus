extends "res://test_base.gd"

const DEFAULT_JA_TEST_TEXT := "こんにちは"
const DEFAULT_EN_TEST_TEXT := "hello from godot"
const BUNDLED_MODEL_PATH := "res://models/multilingual-test-medium.onnx"
const BUNDLED_CONFIG_PATH := "res://models/multilingual-test-medium.onnx.json"
const BUNDLED_DICT_PATH := "res://models/openjtalk_dic"

var _async_completed_audio = null
var _async_failed_error := ""

func _addon_available() -> bool:
    return ClassDB.class_exists("PiperTTS")

func _create_tts():
    if not _addon_available():
        return null
    return ClassDB.instantiate("PiperTTS")

func _cleanup_tts(tts) -> void:
    if is_instance_valid(tts):
        tts.stop()
        tts.free()

func _has_property(object: Object, property_name: String) -> bool:
    if object == null:
        return false
    for property_info in object.get_property_list():
        if String(property_info.get("name", "")) == property_name:
            return true
    return false

func _require_method(object: Object, method_name: String) -> bool:
    if object == null or not object.has_method(method_name):
        failures.append("PiperTTS should expose %s()" % method_name)
        return false
    return true

func _require_property(object: Object, property_name: String) -> bool:
    if not _has_property(object, property_name):
        failures.append("PiperTTS should expose property %s" % property_name)
        return false
    return true

func _model_bundle() -> Dictionary:
    if FileAccess.file_exists(BUNDLED_MODEL_PATH) and FileAccess.file_exists(BUNDLED_CONFIG_PATH):
        var bundled = {
            "model_path": BUNDLED_MODEL_PATH,
            "config_path": BUNDLED_CONFIG_PATH,
            "dictionary_path": "",
        }
        if DirAccess.dir_exists_absolute(ProjectSettings.globalize_path(BUNDLED_DICT_PATH)):
            bundled["dictionary_path"] = BUNDLED_DICT_PATH
        return bundled

    var model_path = OS.get_environment("PIPER_TEST_MODEL_PATH")
    var config_path = OS.get_environment("PIPER_TEST_CONFIG_PATH")
    var dict_path = OS.get_environment("PIPER_TEST_DICT_PATH")

    if model_path.is_empty():
        return {}

    return {
        "model_path": model_path,
        "config_path": config_path,
        "dictionary_path": dict_path,
    }

func _resolve_bundle_config_path(bundle: Dictionary) -> String:
    var explicit_config := String(bundle.get("config_path", ""))
    if not explicit_config.is_empty():
        return explicit_config

    var model_path := String(bundle.get("model_path", ""))
    if model_path.is_empty():
        return ""

    var sibling_config := "%s.json" % model_path
    if FileAccess.file_exists(sibling_config):
        return sibling_config

    var directory_config := model_path.get_base_dir().path_join("config.json")
    if FileAccess.file_exists(directory_config):
        return directory_config

    return ""

func _load_bundle_config(bundle: Dictionary) -> Dictionary:
    var config_path := _resolve_bundle_config_path(bundle)
    if config_path.is_empty():
        return {}

    var text = FileAccess.get_file_as_string(config_path)
    if text.is_empty():
        return {}

    var parsed = JSON.parse_string(text)
    if typeof(parsed) != TYPE_DICTIONARY:
        return {}

    return parsed

func _preferred_test_language_code(bundle: Dictionary) -> String:
    var env_language := OS.get_environment("PIPER_TEST_LANGUAGE_CODE").strip_edges()
    if not env_language.is_empty():
        return env_language

    var parsed := _load_bundle_config(bundle)
    var language: Variant = parsed.get("language", {})
    if typeof(language) == TYPE_DICTIONARY and String(language.get("code", "")) == "multilingual":
        return "en"

    return ""

func _test_text(bundle: Dictionary) -> String:
    var env_text := OS.get_environment("PIPER_TEST_TEXT")
    if not env_text.is_empty():
        return env_text

    if _preferred_test_language_code(bundle) == "en":
        return DEFAULT_EN_TEST_TEXT

    return DEFAULT_JA_TEST_TEXT

func _on_synthesis_completed_for_test(audio) -> void:
    _async_completed_audio = audio

func _on_synthesis_failed_for_test(error: String) -> void:
    _async_failed_error = error

func _configure_test_model(tts) -> bool:
    var bundle = _model_bundle()
    if bundle.is_empty():
        return false

    if not FileAccess.file_exists(bundle["model_path"]):
        return false

    var resolved_config := _resolve_bundle_config_path(bundle)
    if resolved_config.is_empty():
        return false

    tts.model_path = bundle["model_path"]
    if not String(bundle["config_path"]).is_empty():
        tts.config_path = bundle["config_path"]
    if not String(bundle["dictionary_path"]).is_empty():
        tts.dictionary_path = bundle["dictionary_path"]

    var preferred_language := _preferred_test_language_code(bundle)
    if not preferred_language.is_empty():
        tts.language_code = preferred_language

    return true

func _absolute_test_path(path: String) -> String:
    if path.begins_with("res://") or path.begins_with("user://"):
        return ProjectSettings.globalize_path(path)
    return path

func _has_compiled_openjtalk_dictionary(bundle: Dictionary) -> bool:
    var dictionary_path := String(bundle.get("dictionary_path", ""))
    if dictionary_path.is_empty():
        return false

    var absolute_path := _absolute_test_path(dictionary_path)
    if not DirAccess.dir_exists_absolute(absolute_path):
        return false

    for required_file in ["sys.dic", "unk.dic", "matrix.bin", "char.bin", "dicrc"]:
        if not FileAccess.file_exists(absolute_path.path_join(required_file)):
            return false

    return true

func _expected_sample_rate(bundle: Dictionary) -> int:
    var config_path := _resolve_bundle_config_path(bundle)
    if config_path.is_empty():
        return 22050

    var text = FileAccess.get_file_as_string(config_path)
    if text.is_empty():
        return 22050

    var parsed = JSON.parse_string(text)
    if typeof(parsed) != TYPE_DICTIONARY:
        return 22050

    var audio = parsed.get("audio", {})
    if typeof(audio) == TYPE_DICTIONARY:
        return int(audio.get("sample_rate", 22050))

    return 22050

func list_test_names() -> Array[String]:
    return [
        "test_node_creation",
        "test_properties",
        "test_speech_rate_range",
        "test_execution_provider_enum",
        "test_initialize_without_model",
        "test_synthesize_without_init",
        "test_synthesize_async_without_init",
        "test_is_ready_default",
        "test_is_processing_default",
        "test_initialize_with_model",
        "test_directory_model_path_resolution",
        "test_language_code_normalization",
        "test_gpu_device_id_clamp",
        "test_invalid_openjtalk_library_path_falls_back",
        "test_initialize_with_config_fallback",
        "test_inspect_text",
        "test_inspect_request_with_phonemes",
        "test_synthesize_basic",
        "test_synthesize_phoneme_string",
        "test_synthesize_phoneme_string_with_silence_map",
        "test_reject_negative_phoneme_silence",
        "test_question_marker_phoneme_string",
        "test_synthesize_request_with_sentence_silence",
        "test_synthesize_async",
        "test_audio_stream_format",
        "test_last_synthesis_result_timing",
    ]

func run_test(method_name: String) -> void:
    if not has_method(method_name):
        failures.append("Unknown test: %s" % method_name)
        return

    if method_name == "test_synthesize_async":
        await Callable(self, method_name).call()
        return

    Callable(self, method_name).call()

func test_node_creation() -> void:
    var tts = _create_tts()
    if tts == null:
        skip("PiperTTS class is unavailable")
        return
    assert_not_null(tts, "PiperTTS node should be creatable")
    _cleanup_tts(tts)

func test_properties() -> void:
    var tts = _create_tts()
    if tts == null:
        skip("PiperTTS class is unavailable")
        return
    if not _require_property(tts, "sentence_silence_seconds") or not _require_property(tts, "phoneme_silence_seconds"):
        _cleanup_tts(tts)
        return
    tts.model_path = "res://voice.onnx"
    tts.config_path = "res://voice.onnx.json"
    tts.dictionary_path = "res://dict"
    tts.openjtalk_library_path = "res://bin/openjtalk_native.dll"
    tts.custom_dictionary_path = "res://custom_dictionary.json"
    tts.speaker_id = 3
    tts.language_id = 1
    tts.language_code = "en"
    tts.noise_scale = 1.2
    tts.noise_w = 0.6
    tts.sentence_silence_seconds = 0.35
    tts.phoneme_silence_seconds = {"a": 0.05, "?!": 0.1}
    tts.gpu_device_id = 2

    assert_equal(tts.model_path, "res://voice.onnx", "model_path should round-trip")
    assert_equal(tts.config_path, "res://voice.onnx.json", "config_path should round-trip")
    assert_equal(tts.dictionary_path, "res://dict", "dictionary_path should round-trip")
    assert_equal(tts.openjtalk_library_path, "res://bin/openjtalk_native.dll", "openjtalk_library_path should round-trip")
    assert_equal(tts.custom_dictionary_path, "res://custom_dictionary.json", "custom_dictionary_path should round-trip")
    assert_equal(tts.speaker_id, 3, "speaker_id should round-trip")
    assert_equal(tts.language_id, 1, "language_id should round-trip")
    assert_equal(tts.language_code, "en", "language_code should round-trip")
    assert_equal(tts.noise_scale, 1.2, "noise_scale should round-trip")
    assert_equal(tts.noise_w, 0.6, "noise_w should round-trip")
    assert_equal(tts.sentence_silence_seconds, 0.35, "sentence_silence_seconds should round-trip")
    assert_equal(tts.phoneme_silence_seconds, {"a": 0.05, "?!": 0.1}, "phoneme_silence_seconds should round-trip")
    assert_equal(tts.gpu_device_id, 2, "gpu_device_id should round-trip")
    _cleanup_tts(tts)

func test_speech_rate_range() -> void:
    var tts = _create_tts()
    if tts == null:
        skip("PiperTTS class is unavailable")
        return
    tts.speech_rate = 0.01
    assert_equal(tts.speech_rate, 0.1, "speech_rate should clamp to the minimum")
    tts.speech_rate = 9.0
    assert_equal(tts.speech_rate, 5.0, "speech_rate should clamp to the maximum")
    _cleanup_tts(tts)

func test_execution_provider_enum() -> void:
    if not _addon_available():
        skip("PiperTTS class is unavailable")
        return
    assert_equal(ClassDB.class_get_integer_constant("PiperTTS", "EP_CPU"), 0, "EP_CPU should match the bound enum")
    assert_equal(ClassDB.class_get_integer_constant("PiperTTS", "EP_COREML"), 1, "EP_COREML should match the bound enum")
    assert_equal(ClassDB.class_get_integer_constant("PiperTTS", "EP_DIRECTML"), 2, "EP_DIRECTML should match the bound enum")
    assert_equal(ClassDB.class_get_integer_constant("PiperTTS", "EP_NNAPI"), 3, "EP_NNAPI should match the bound enum")
    assert_equal(ClassDB.class_get_integer_constant("PiperTTS", "EP_AUTO"), 4, "EP_AUTO should match the bound enum")
    assert_equal(ClassDB.class_get_integer_constant("PiperTTS", "EP_CUDA"), 5, "EP_CUDA should match the bound enum")
    await Engine.get_main_loop().process_frame

func test_initialize_without_model() -> void:
    var tts = _create_tts()
    if tts == null:
        skip("PiperTTS class is unavailable")
        return
    assert_equal(tts.initialize(), ERR_UNCONFIGURED, "initialize() should reject missing model_path")
    _cleanup_tts(tts)

func test_synthesize_without_init() -> void:
    var tts = _create_tts()
    if tts == null:
        skip("PiperTTS class is unavailable")
        return
    var audio = tts.synthesize(DEFAULT_JA_TEST_TEXT)
    assert_true(audio == null, "synthesize() should fail before initialize()")
    _cleanup_tts(tts)

func test_synthesize_async_without_init() -> void:
    var tts = _create_tts()
    if tts == null:
        skip("PiperTTS class is unavailable")
        return
    assert_equal(tts.synthesize_async(DEFAULT_JA_TEST_TEXT), ERR_UNCONFIGURED, "synthesize_async() should fail before initialize()")
    _cleanup_tts(tts)

func test_is_ready_default() -> void:
    var tts = _create_tts()
    if tts == null:
        skip("PiperTTS class is unavailable")
        return
    assert_false(tts.is_ready(), "PiperTTS should start in a not-ready state")
    _cleanup_tts(tts)

func test_is_processing_default() -> void:
    var tts = _create_tts()
    if tts == null:
        skip("PiperTTS class is unavailable")
        return
    assert_false(tts.is_processing(), "PiperTTS should start in an idle state")
    _cleanup_tts(tts)

func test_initialize_with_model() -> void:
    var tts = _create_tts()
    if tts == null:
        skip("PiperTTS class is unavailable")
        return
    if not _configure_test_model(tts):
        skip("test model bundle is not available in res://models or PIPER_TEST_* env vars")
        _cleanup_tts(tts)
        return

    assert_equal(tts.initialize(), OK, "initialize() should succeed with a valid model bundle")
    assert_true(tts.is_ready(), "PiperTTS should be ready after initialize()")
    _cleanup_tts(tts)

func test_directory_model_path_resolution() -> void:
    var tts = _create_tts()
    if tts == null:
        skip("PiperTTS class is unavailable")
        return
    var bundle = _model_bundle()
    if bundle.is_empty():
        skip("test model bundle is not available in res://models or PIPER_TEST_* env vars")
        _cleanup_tts(tts)
        return

    var model_directory := String(bundle["model_path"]).get_base_dir()
    if model_directory.is_empty():
        skip("test model bundle does not expose a model directory")
        _cleanup_tts(tts)
        return

    tts.model_path = model_directory
    tts.config_path = ""
    if not String(bundle.get("dictionary_path", "")).is_empty():
        tts.dictionary_path = bundle["dictionary_path"]

    var preferred_language := _preferred_test_language_code(bundle)
    if not preferred_language.is_empty():
        tts.language_code = preferred_language

    assert_equal(tts.initialize(), OK, "initialize() should resolve a model from the directory path")
    assert_true(tts.is_ready(), "PiperTTS should be ready after resolving a model directory")
    _cleanup_tts(tts)

func test_language_code_normalization() -> void:
    var tts = _create_tts()
    if tts == null:
        skip("PiperTTS class is unavailable")
        return
    var bundle = _model_bundle()
    if bundle.is_empty():
        skip("test model bundle is not available in res://models or PIPER_TEST_* env vars")
        _cleanup_tts(tts)
        return
    if not _configure_test_model(tts):
        skip("test model bundle is not available in res://models or PIPER_TEST_* env vars")
        _cleanup_tts(tts)
        return

    tts.language_code = " EN_us "
    assert_equal(tts.initialize(), OK, "initialize() should accept normalized language_code aliases")

    if not _require_method(tts, "inspect_text"):
        _cleanup_tts(tts)
        return
    var inspected: Dictionary = tts.inspect_text(DEFAULT_EN_TEST_TEXT)
    assert_equal(String(inspected.get("resolved_language_code", "")), "en", "inspect_text() should resolve language_code aliases to the canonical code")
    assert_equal(int(inspected.get("resolved_language_id", -1)), 1, "inspect_text() should resolve EN_us to language_id=1 for the bundled multilingual model")
    _cleanup_tts(tts)

func test_gpu_device_id_clamp() -> void:
    var tts = _create_tts()
    if tts == null:
        skip("PiperTTS class is unavailable")
        return
    if not _require_property(tts, "gpu_device_id"):
        _cleanup_tts(tts)
        return
    tts.gpu_device_id = -4
    assert_equal(tts.gpu_device_id, 0, "gpu_device_id should clamp negative values to 0")
    tts.gpu_device_id = 3
    assert_equal(tts.gpu_device_id, 3, "gpu_device_id should store positive values")
    _cleanup_tts(tts)

func test_invalid_openjtalk_library_path_falls_back() -> void:
    var tts = _create_tts()
    if tts == null:
        skip("PiperTTS class is unavailable")
        return
    var bundle = _model_bundle()
    if bundle.is_empty():
        skip("test model bundle is not available in res://models or PIPER_TEST_* env vars")
        _cleanup_tts(tts)
        return
    if String(bundle.get("dictionary_path", "")).is_empty():
        skip("OpenJTalk dictionary is not available in the bundled test assets")
        _cleanup_tts(tts)
        return
    if not _has_compiled_openjtalk_dictionary(bundle):
        skip("compiled OpenJTalk dictionary is not available for builtin backend fallback test")
        _cleanup_tts(tts)
        return
    if not _configure_test_model(tts):
        skip("test model bundle is not available in res://models or PIPER_TEST_* env vars")
        _cleanup_tts(tts)
        return

    tts.language_code = "ja"
    tts.openjtalk_library_path = "res://missing/openjtalk_native.dll"
    assert_equal(tts.initialize(), OK, "initialize() should fall back to the builtin OpenJTalk backend when the native library path is invalid")

    var inspected: Dictionary = tts.inspect_text(DEFAULT_JA_TEST_TEXT)
    var phoneme_sentences: Array = inspected.get("phoneme_sentences", [])
    assert_true(phoneme_sentences.size() > 0, "inspect_text() should still resolve Japanese phonemes after native backend fallback")

    var audio = tts.synthesize(DEFAULT_JA_TEST_TEXT)
    assert_not_null(audio, "synthesize() should still work after native backend fallback")
    _cleanup_tts(tts)

func test_initialize_with_config_fallback() -> void:
    var tts = _create_tts()
    if tts == null:
        skip("PiperTTS class is unavailable")
        return
    if not _configure_test_model(tts):
        skip("test model bundle is not available in res://models or PIPER_TEST_* env vars")
        _cleanup_tts(tts)
        return

    tts.config_path = ""
    assert_equal(tts.initialize(), OK, "initialize() should resolve config_path from the model path")
    assert_true(tts.is_ready(), "PiperTTS should be ready after fallback config resolution")
    _cleanup_tts(tts)

func test_inspect_text() -> void:
    var tts = _create_tts()
    if tts == null:
        skip("PiperTTS class is unavailable")
        return
    var bundle = _model_bundle()
    if bundle.is_empty():
        skip("test model bundle is not available in res://models or PIPER_TEST_* env vars")
        _cleanup_tts(tts)
        return
    if not _configure_test_model(tts):
        skip("test model bundle is not available in res://models or PIPER_TEST_* env vars")
        _cleanup_tts(tts)
        return

    if tts.initialize() != OK:
        failures.append("initialize() failed for inspect_text")
        _cleanup_tts(tts)
        return

    if not _require_method(tts, "inspect_text") or not _require_method(tts, "get_last_inspection_result"):
        _cleanup_tts(tts)
        return
    var inspected: Dictionary = tts.inspect_text(_test_text(bundle))
    assert_equal(String(inspected.get("input_mode", "")), "text", "inspect_text() should report text input_mode")

    var phoneme_sentences: Array = inspected.get("phoneme_sentences", [])
    var phoneme_id_sentences: Array = inspected.get("phoneme_id_sentences", [])
    assert_true(phoneme_sentences.size() > 0, "inspect_text() should return at least one phoneme sentence")
    assert_equal(phoneme_sentences.size(), phoneme_id_sentences.size(), "inspect_text() should return matching phoneme and phoneme_id sentence counts")

    if _preferred_test_language_code(bundle) == "en":
        assert_equal(String(inspected.get("resolved_language_code", "")), "en", "inspect_text() should resolve the configured English language code")
    else:
        assert_true(int(inspected.get("resolved_language_id", -1)) >= -1, "inspect_text() should expose a resolved language id")

    assert_equal(tts.get_last_inspection_result(), inspected, "inspect_text() should update get_last_inspection_result()")
    _cleanup_tts(tts)

func test_inspect_request_with_phonemes() -> void:
    var tts = _create_tts()
    if tts == null:
        skip("PiperTTS class is unavailable")
        return
    var bundle = _model_bundle()
    if bundle.is_empty():
        skip("test model bundle is not available in res://models or PIPER_TEST_* env vars")
        _cleanup_tts(tts)
        return
    if not _configure_test_model(tts):
        skip("test model bundle is not available in res://models or PIPER_TEST_* env vars")
        _cleanup_tts(tts)
        return

    if tts.initialize() != OK:
        failures.append("initialize() failed for inspect_request_with_phonemes")
        _cleanup_tts(tts)
        return

    if not _require_method(tts, "inspect_request") or not _require_method(tts, "get_last_inspection_result"):
        _cleanup_tts(tts)
        return
    var request := {
        "phonemes": PackedStringArray(["h", "ə", "l", "o"]),
        "language_code": "en",
    }
    var inspected: Dictionary = tts.inspect_request(request)
    assert_equal(String(inspected.get("input_mode", "")), "phoneme_string", "inspect_request() should report phoneme_string input_mode for phoneme arrays")

    var phoneme_sentences: Array = inspected.get("phoneme_sentences", [])
    assert_equal(phoneme_sentences.size(), 1, "inspect_request() should keep raw phoneme input as a single sentence")
    if phoneme_sentences.size() == 1:
        var sentence: PackedStringArray = phoneme_sentences[0]
        assert_equal(sentence, PackedStringArray(["h", "ə", "l", "o"]), "inspect_request() should preserve the supplied phoneme tokens")

    var phoneme_id_sentences: Array = inspected.get("phoneme_id_sentences", [])
    assert_equal(phoneme_id_sentences.size(), 1, "inspect_request() should return one phoneme ID sentence for raw phoneme input")
    if phoneme_id_sentences.size() == 1:
        var ids: PackedInt64Array = phoneme_id_sentences[0]
        assert_true(ids.size() >= 4, "inspect_request() should resolve raw phonemes to IDs")

    assert_equal(tts.get_last_inspection_result(), inspected, "inspect_request() should update get_last_inspection_result()")
    _cleanup_tts(tts)

func test_synthesize_basic() -> void:
    var tts = _create_tts()
    if tts == null:
        skip("PiperTTS class is unavailable")
        return
    var bundle = _model_bundle()
    if bundle.is_empty():
        skip("test model bundle is not available in res://models or PIPER_TEST_* env vars")
        _cleanup_tts(tts)
        return
    if not _configure_test_model(tts):
        skip("test model bundle is not available in res://models or PIPER_TEST_* env vars")
        _cleanup_tts(tts)
        return

    if tts.initialize() != OK:
        failures.append("initialize() failed for synthesize_basic")
        _cleanup_tts(tts)
        return

    var audio = tts.synthesize(_test_text(bundle))
    assert_not_null(audio, "synthesize() should return AudioStreamWAV")
    if audio != null:
        assert_true(audio.data.size() > 0, "AudioStreamWAV should contain PCM data")
    _cleanup_tts(tts)

func test_synthesize_phoneme_string() -> void:
    var tts = _create_tts()
    if tts == null:
        skip("PiperTTS class is unavailable")
        return
    var bundle = _model_bundle()
    if bundle.is_empty():
        skip("test model bundle is not available in res://models or PIPER_TEST_* env vars")
        _cleanup_tts(tts)
        return
    if not _configure_test_model(tts):
        skip("test model bundle is not available in res://models or PIPER_TEST_* env vars")
        _cleanup_tts(tts)
        return

    if tts.initialize() != OK:
        failures.append("initialize() failed for synthesize_phoneme_string")
        _cleanup_tts(tts)
        return

    if not _require_method(tts, "synthesize_phoneme_string") or not _require_method(tts, "get_last_synthesis_result"):
        _cleanup_tts(tts)
        return
    var audio = tts.synthesize_phoneme_string("a i u")
    assert_not_null(audio, "synthesize_phoneme_string() should return AudioStreamWAV")
    if audio != null:
        assert_true(audio.data.size() > 0, "synthesize_phoneme_string() should produce PCM data")

    var result: Dictionary = tts.get_last_synthesis_result()
    assert_equal(String(result.get("input_mode", "")), "phoneme_string", "synthesize_phoneme_string() should record phoneme_string input_mode")
    _cleanup_tts(tts)

func test_synthesize_phoneme_string_with_silence_map() -> void:
    var tts = _create_tts()
    if tts == null:
        skip("PiperTTS class is unavailable")
        return
    var bundle = _model_bundle()
    if bundle.is_empty():
        skip("test model bundle is not available in res://models or PIPER_TEST_* env vars")
        _cleanup_tts(tts)
        return
    if not _configure_test_model(tts):
        skip("test model bundle is not available in res://models or PIPER_TEST_* env vars")
        _cleanup_tts(tts)
        return

    if tts.initialize() != OK:
        failures.append("initialize() failed for synthesize_phoneme_string_with_silence_map")
        _cleanup_tts(tts)
        return

    if not _require_method(tts, "synthesize_request"):
        _cleanup_tts(tts)
        return

    var baseline = tts.synthesize_request({"phoneme_string": "a i"})
    var with_silence = tts.synthesize_request({
        "phoneme_string": "a i",
        "phoneme_silence_seconds": {"a": 0.1},
    })
    assert_not_null(baseline, "baseline phoneme_string synthesis should succeed")
    assert_not_null(with_silence, "phoneme_silence_seconds should be accepted for phoneme_string synthesis")
    if baseline != null and with_silence != null:
        assert_true(with_silence.data.size() > baseline.data.size(), "phoneme_silence_seconds should increase output PCM length for raw phoneme input")
    _cleanup_tts(tts)

func test_reject_negative_phoneme_silence() -> void:
    var tts = _create_tts()
    if tts == null:
        skip("PiperTTS class is unavailable")
        return
    var bundle = _model_bundle()
    if bundle.is_empty():
        skip("test model bundle is not available in res://models or PIPER_TEST_* env vars")
        _cleanup_tts(tts)
        return
    if not _configure_test_model(tts):
        skip("test model bundle is not available in res://models or PIPER_TEST_* env vars")
        _cleanup_tts(tts)
        return

    if tts.initialize() != OK:
        failures.append("initialize() failed for reject_negative_phoneme_silence")
        _cleanup_tts(tts)
        return

    if not _require_method(tts, "synthesize_request"):
        _cleanup_tts(tts)
        return

    var audio = tts.synthesize_request({
        "phoneme_string": "a i",
        "phoneme_silence_seconds": {"a": -0.1},
    })
    assert_true(audio == null, "negative phoneme_silence_seconds should be rejected")
    _cleanup_tts(tts)

func test_question_marker_phoneme_string() -> void:
    var tts = _create_tts()
    if tts == null:
        skip("PiperTTS class is unavailable")
        return
    var bundle = _model_bundle()
    if bundle.is_empty():
        skip("test model bundle is not available in res://models or PIPER_TEST_* env vars")
        _cleanup_tts(tts)
        return
    if not _configure_test_model(tts):
        skip("test model bundle is not available in res://models or PIPER_TEST_* env vars")
        _cleanup_tts(tts)
        return

    if tts.initialize() != OK:
        failures.append("initialize() failed for question_marker_phoneme_string")
        _cleanup_tts(tts)
        return

    if not _require_method(tts, "inspect_phoneme_string") or not _require_method(tts, "synthesize_phoneme_string"):
        _cleanup_tts(tts)
        return
    var inspected: Dictionary = tts.inspect_phoneme_string("a ?! a")
    var phoneme_sentences: Array = inspected.get("phoneme_sentences", [])
    assert_equal(phoneme_sentences.size(), 1, "inspect_phoneme_string() should keep raw phoneme input as a single sentence")
    if phoneme_sentences.size() == 1:
        var sentence: PackedStringArray = phoneme_sentences[0]
        assert_equal(sentence, PackedStringArray(["a", "?!", "a"]), "inspect_phoneme_string() should preserve question-marker phonemes")

    var audio = tts.synthesize_phoneme_string("a ?! a")
    assert_not_null(audio, "synthesize_phoneme_string() should support question-marker phonemes")
    _cleanup_tts(tts)

func test_synthesize_request_with_sentence_silence() -> void:
    var tts = _create_tts()
    if tts == null:
        skip("PiperTTS class is unavailable")
        return
    var bundle = _model_bundle()
    if bundle.is_empty():
        skip("test model bundle is not available in res://models or PIPER_TEST_* env vars")
        _cleanup_tts(tts)
        return
    if not _configure_test_model(tts):
        skip("test model bundle is not available in res://models or PIPER_TEST_* env vars")
        _cleanup_tts(tts)
        return

    if tts.initialize() != OK:
        failures.append("initialize() failed for synthesize_request_with_sentence_silence")
        _cleanup_tts(tts)
        return

    if not _require_method(tts, "synthesize_request") or not _require_method(tts, "get_last_synthesis_result"):
        _cleanup_tts(tts)
        return
    var multi_sentence_text := "hello. hello."
    if _preferred_test_language_code(bundle) != "en":
        multi_sentence_text = "こんにちは。こんにちは。"

    var baseline = tts.synthesize_request({
        "text": multi_sentence_text,
        "sentence_silence_seconds": 0.0,
    })
    var with_silence = tts.synthesize_request({
        "text": multi_sentence_text,
        "sentence_silence_seconds": 0.25,
    })

    assert_not_null(baseline, "synthesize_request() should synthesize a baseline multi-sentence request")
    assert_not_null(with_silence, "synthesize_request() should synthesize with sentence silence overrides")
    if baseline != null and with_silence != null:
        assert_true(with_silence.data.size() > baseline.data.size(), "sentence_silence_seconds should increase output PCM length for multi-sentence input")

    var result: Dictionary = tts.get_last_synthesis_result()
    assert_equal(float(result.get("sentence_silence_seconds", -1.0)), 0.25, "synthesize_request() should expose the applied sentence_silence_seconds override")
    _cleanup_tts(tts)

func test_synthesize_async() -> void:
    var tts = _create_tts()
    if tts == null:
        skip("PiperTTS class is unavailable")
        return
    var bundle = _model_bundle()
    if bundle.is_empty():
        skip("test model bundle is not available in res://models or PIPER_TEST_* env vars")
        _cleanup_tts(tts)
        return
    if not _configure_test_model(tts):
        skip("test model bundle is not available in res://models or PIPER_TEST_* env vars")
        _cleanup_tts(tts)
        return

    if tts.initialize() != OK:
        failures.append("initialize() failed for synthesize_async")
        _cleanup_tts(tts)
        return

    _async_completed_audio = null
    _async_failed_error = ""
    tts.synthesis_completed.connect(_on_synthesis_completed_for_test)
    tts.synthesis_failed.connect(_on_synthesis_failed_for_test)

    assert_equal(tts.synthesize_async(_test_text(bundle)), OK, "synthesize_async() should start successfully")

    var deadline = Time.get_ticks_msec() + 15000
    while _async_completed_audio == null and _async_failed_error.is_empty() and Time.get_ticks_msec() < deadline:
        await Engine.get_main_loop().process_frame

    if _async_completed_audio == null and _async_failed_error.is_empty():
        await Engine.get_main_loop().process_frame

    assert_true(_async_failed_error.is_empty(), "synthesize_async() should not emit synthesis_failed")
    assert_not_null(_async_completed_audio, "synthesize_async() should emit synthesis_completed")
    _cleanup_tts(tts)

func test_audio_stream_format() -> void:
    var tts = _create_tts()
    if tts == null:
        skip("PiperTTS class is unavailable")
        return
    var bundle = _model_bundle()
    if bundle.is_empty():
        skip("test model bundle is not available in res://models or PIPER_TEST_* env vars")
        _cleanup_tts(tts)
        return
    if not _configure_test_model(tts):
        skip("test model bundle is not available in res://models or PIPER_TEST_* env vars")
        _cleanup_tts(tts)
        return
    var expected_rate = _expected_sample_rate(bundle)
    if tts.initialize() != OK:
        failures.append("initialize() failed for audio_stream_format")
        _cleanup_tts(tts)
        return

    var audio = tts.synthesize(_test_text(bundle))
    assert_not_null(audio, "synthesize() should return AudioStreamWAV")
    if audio != null:
        assert_equal(audio.format, AudioStreamWAV.FORMAT_16_BITS, "Audio should be 16-bit PCM")
        assert_equal(audio.mix_rate, expected_rate, "Audio mix rate should match the model config")
        assert_false(audio.stereo, "Audio should be mono")
        assert_true(audio.data.size() > 0, "Audio data should not be empty")
    _cleanup_tts(tts)

func test_last_synthesis_result_timing() -> void:
    var tts = _create_tts()
    if tts == null:
        skip("PiperTTS class is unavailable")
        return
    var bundle = _model_bundle()
    if bundle.is_empty():
        skip("test model bundle is not available in res://models or PIPER_TEST_* env vars")
        _cleanup_tts(tts)
        return
    if not _configure_test_model(tts):
        skip("test model bundle is not available in res://models or PIPER_TEST_* env vars")
        _cleanup_tts(tts)
        return

    if tts.initialize() != OK:
        failures.append("initialize() failed for last_synthesis_result_timing")
        _cleanup_tts(tts)
        return

    if not _require_method(tts, "get_last_synthesis_result"):
        _cleanup_tts(tts)
        return
    var audio = tts.synthesize(_test_text(bundle))
    assert_not_null(audio, "synthesize() should return audio before timing metadata is checked")

    var result: Dictionary = tts.get_last_synthesis_result()
    assert_equal(String(result.get("input_mode", "")), "text", "get_last_synthesis_result() should record text input_mode")
    assert_true(bool(result.get("has_timing_info", false)), "get_last_synthesis_result() should expose timing metadata")
    assert_equal(int(result.get("sample_rate", 0)), _expected_sample_rate(bundle), "get_last_synthesis_result() should expose the resolved sample rate")

    var phoneme_timings: Array = result.get("phoneme_timings", [])
    assert_true(phoneme_timings.size() > 0, "get_last_synthesis_result() should expose at least one phoneme timing entry")
    if phoneme_timings.size() > 0:
        var timing: Dictionary = phoneme_timings[0]
        assert_true(timing.has("phoneme"), "phoneme timing entries should expose phoneme")
        assert_true(timing.has("start_time"), "phoneme timing entries should expose start_time")
        assert_true(timing.has("end_time"), "phoneme timing entries should expose end_time")
        assert_true(float(timing.get("end_time", -1.0)) >= float(timing.get("start_time", 0.0)), "phoneme timing entries should have end_time >= start_time")
    _cleanup_tts(tts)
