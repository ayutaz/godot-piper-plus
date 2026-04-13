extends "res://test_base.gd"

const DEFAULT_JA_TEST_TEXT := "こんにちは"
const DEFAULT_EN_TEST_TEXT := "hello from godot"
const BUNDLED_MODEL_PATH := "res://models/multilingual-test-medium.onnx"
const BUNDLED_CONFIG_PATH := "res://models/multilingual-test-medium.onnx.json"
const BUNDLED_DICT_PATH := "res://piper_plus_assets/dictionaries/open_jtalk_dic_utf_8-1.11"
const LEGACY_BUNDLED_DICT_PATH := "res://models/openjtalk_dic"
const MISSING_WEB_DICT_PATH := "res://missing/open_jtalk_dic_utf_8-1.11"

var _async_completed_audio = null
var _async_failed_error := ""
var _streaming_completed := false

func _is_web_runtime() -> bool:
    return OS.has_feature("web")

func _is_web_smoke() -> bool:
    return OS.has_feature("web_smoke")

func _web_smoke_scenario() -> String:
    if not _is_web_smoke():
        return ""

    var scenario := OS.get_environment("PIPER_WEB_SMOKE_SCENARIO").strip_edges().to_lower()
    if not scenario.is_empty():
        return scenario

    if _is_web_runtime() and ClassDB.class_exists("JavaScriptBridge"):
        var js_value: Variant = JavaScriptBridge.eval(
            "(globalThis.__PIPER_WEB_SMOKE_SCENARIO || '').toString()",
            true
        )
        scenario = String(js_value).strip_edges().to_lower()

    return scenario

func _addon_available() -> bool:
    return ClassDB.class_exists("PiperTTS")

func _create_tts():
    if not _addon_available():
        return null
    return ClassDB.instantiate("PiperTTS")

func _cleanup_tts(tts) -> void:
    if is_instance_valid(tts):
        tts.stop()
        var parent: Node = tts.get_parent()
        if parent != null:
            parent.remove_child(tts)
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
        if _dictionary_has_required_files(BUNDLED_DICT_PATH):
            bundled["dictionary_path"] = BUNDLED_DICT_PATH
        elif _dictionary_has_required_files(LEGACY_BUNDLED_DICT_PATH):
            bundled["dictionary_path"] = LEGACY_BUNDLED_DICT_PATH
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

func _repo_root_path() -> String:
    var current := ProjectSettings.globalize_path("res://")
    for _i in range(8):
        var candidate := current.path_join("tests/fixtures/multilingual_capability_matrix.json")
        if FileAccess.file_exists(candidate):
            return current
        var parent := current.get_base_dir()
        if parent == current or parent.is_empty():
            break
        current = parent
    return ""

func _load_multilingual_capability_matrix() -> Array:
    var repo_root := _repo_root_path()
    if repo_root.is_empty():
        return []
    var fixture_path := repo_root.path_join("tests/fixtures/multilingual_capability_matrix.json")
    if not FileAccess.file_exists(fixture_path):
        return []

    var text := FileAccess.get_file_as_string(fixture_path)
    if text.is_empty():
        return []

    var parsed = JSON.parse_string(text)
    if typeof(parsed) != TYPE_ARRAY:
        return []
    return parsed

func _matrix_row_by_code(rows: Array, language_code: String) -> Dictionary:
    for row in rows:
        if typeof(row) == TYPE_DICTIONARY and String(row.get("language_code", "")) == language_code:
            return row
    return {}

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

func _on_streaming_ended_for_test() -> void:
    _streaming_completed = true

func _create_streaming_playback() -> Dictionary:
    var tree := Engine.get_main_loop() as SceneTree
    if tree == null or tree.root == null:
        return {}

    var container := Node.new()
    tree.root.add_child(container)

    var player := AudioStreamPlayer.new()
    var generator := AudioStreamGenerator.new()
    generator.mix_rate = 22050
    generator.buffer_length = 0.1
    player.stream = generator
    container.add_child(player)
    player.play()

    for _i in range(30):
        var playback = player.get_stream_playback()
        if playback is AudioStreamGeneratorPlayback:
            return {
                "container": container,
                "player": player,
                "playback": playback,
            }
        await tree.process_frame

    container.queue_free()
    return {}

func _configure_test_model(tts, include_dictionary: bool = true) -> bool:
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
	if include_dictionary and not String(bundle["dictionary_path"]).is_empty():
		tts.dictionary_path = bundle["dictionary_path"]
	elif _is_web_runtime():
		# Prevent Web runtime auto-fallback from staging OpenJTalk when an English-only
		# smoke scenario intentionally wants to exercise no-dictionary initialization.
		tts.dictionary_path = MISSING_WEB_DICT_PATH

	var preferred_language := _preferred_test_language_code(bundle)
	if not preferred_language.is_empty():
		tts.language_code = preferred_language

    return true

func _absolute_test_path(path: String) -> String:
    if path.begins_with("res://") or path.begins_with("user://"):
        return ProjectSettings.globalize_path(path)
    return path

func _path_points_to_required_openjtalk_files(base_path: String) -> bool:
    for required_file in ["sys.dic", "unk.dic", "matrix.bin", "char.bin"]:
        if not FileAccess.file_exists(base_path.path_join(required_file)):
            return false
    return true

func _dictionary_has_required_files(dictionary_path: String) -> bool:
    if dictionary_path.is_empty():
        return false

    if dictionary_path.begins_with("res://") or dictionary_path.begins_with("user://"):
        return _path_points_to_required_openjtalk_files(dictionary_path)

    var absolute_path := _absolute_test_path(dictionary_path)
    return _path_points_to_required_openjtalk_files(absolute_path)

func _has_compiled_openjtalk_dictionary(bundle: Dictionary) -> bool:
    var dictionary_path := String(bundle.get("dictionary_path", ""))
    return _dictionary_has_required_files(dictionary_path)

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
    if _is_web_smoke():
        return [
            "test_node_creation",
            "test_properties",
            "test_execution_provider_enum",
            "test_runtime_contract",
            "test_runtime_contract_missing_web_dictionary",
            "test_initialize_with_model",
            "test_initialize_with_config_fallback",
            "test_web_non_cpu_execution_provider_rejected",
            "test_web_openjtalk_native_rejected",
            "test_japanese_dictionary_error_surface",
            "test_japanese_request_time_dictionary_error_surface",
            "test_japanese_text_input_with_dictionary",
            "test_synthesize_basic",
        ]

    return [
        "test_node_creation",
        "test_properties",
        "test_speech_rate_range",
        "test_execution_provider_enum",
        "test_runtime_contract",
        "test_runtime_contract_missing_web_dictionary",
        "test_runtime_state",
        "test_language_capabilities_without_init_is_side_effect_free",
        "test_editor_download_catalog_paths",
        "test_preview_controller_session_config",
        "test_initialize_without_model",
        "test_synthesize_without_init",
        "test_synthesize_async_without_init",
        "test_is_ready_default",
        "test_is_processing_default",
        "test_initialize_with_model",
        "test_directory_model_path_resolution",
        "test_language_capabilities",
        "test_language_code_normalization",
        "test_language_code_exact_match_selection_mode",
        "test_multilingual_explicit_language_variants",
        "test_multilingual_unsupported_language_rejected_for_text",
        "test_multilingual_language_selector_conflict_rejected",
        "test_gpu_device_id_clamp",
        "test_invalid_openjtalk_library_path_falls_back",
        "test_japanese_dictionary_error_surface",
        "test_japanese_request_time_dictionary_error_surface",
        "test_japanese_text_input_with_dictionary",
        "test_web_non_cpu_execution_provider_rejected",
        "test_web_openjtalk_native_rejected",
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
        "test_synthesize_async_request",
        "test_synthesize_streaming_request",
        "test_audio_stream_format",
        "test_last_synthesis_result_timing",
    ]

func get_test_tags(method_name: String) -> PackedStringArray:
    match method_name:
        "test_runtime_contract", "test_initialize_with_config_fallback", "test_web_non_cpu_execution_provider_rejected", "test_web_openjtalk_native_rejected":
            return PackedStringArray(["core", "web-smoke"])
        "test_initialize_with_model", "test_synthesize_basic":
            return PackedStringArray(["en", "web-smoke"])
        "test_runtime_contract_missing_web_dictionary", "test_japanese_dictionary_error_surface", "test_japanese_request_time_dictionary_error_surface", "test_japanese_text_input_with_dictionary":
            return PackedStringArray(["ja", "web-smoke"])
        _:
            return PackedStringArray()

func run_test(method_name: String) -> void:
    if not has_method(method_name):
        failures.append("Unknown test: %s" % method_name)
        return

    if method_name == "test_synthesize_async" or method_name == "test_synthesize_async_request":
        await Callable(self, method_name).call()
        return
    if method_name == "test_synthesize_streaming_request":
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
    assert_true(tts.has_method("synthesize_async_request"), "PiperTTS should expose synthesize_async_request()")
    assert_true(tts.has_method("synthesize_streaming_request"), "PiperTTS should expose synthesize_streaming_request()")
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

func test_runtime_contract() -> void:
    var tts = _create_tts()
    if tts == null:
        skip("PiperTTS class is unavailable")
        return

    if not _require_method(tts, "get_runtime_contract"):
        _cleanup_tts(tts)
        return

    var contract: Dictionary = tts.get_runtime_contract()
    assert_true(contract.has("is_web_export"), "get_runtime_contract() should expose is_web_export")
    assert_true(contract.has("execution_provider_policy"), "get_runtime_contract() should expose execution_provider_policy")
    assert_true(contract.has("supports_non_cpu_execution_provider"), "get_runtime_contract() should expose supports_non_cpu_execution_provider")
    assert_true(contract.has("supports_openjtalk_native"), "get_runtime_contract() should expose supports_openjtalk_native")
    assert_true(contract.has("resource_source_mode"), "get_runtime_contract() should expose resource_source_mode")
    assert_true(contract.has("resource_path_mode"), "get_runtime_contract() should expose resource_path_mode")
    assert_true(contract.has("preview_support_tier"), "get_runtime_contract() should expose preview_support_tier")
    assert_true(contract.has("phase1_minimal_synthesize_mode"), "get_runtime_contract() should expose phase1_minimal_synthesize_mode")
    assert_true(contract.has("phase1_supported_text_frontends"), "get_runtime_contract() should expose phase1_supported_text_frontends")
    assert_true(contract.has("phase1_excluded_features"), "get_runtime_contract() should expose phase1_excluded_features")
    assert_true(contract.has("supports_japanese_text_input"), "get_runtime_contract() should expose supports_japanese_text_input")
    assert_true(contract.has("required_japanese_text_assets"), "get_runtime_contract() should expose required_japanese_text_assets")
    assert_true(contract.has("openjtalk_dictionary_bootstrap_mode"), "get_runtime_contract() should expose openjtalk_dictionary_bootstrap_mode")
    assert_true(contract.has("resolved_dictionary_path"), "get_runtime_contract() should expose resolved_dictionary_path")
    assert_true(contract.has("runtime_state"), "get_runtime_contract() should expose runtime_state")

    if OS.has_feature("web"):
        var supported_frontends: PackedStringArray = contract.get("phase1_supported_text_frontends", PackedStringArray())
        var excluded_features: PackedStringArray = contract.get("phase1_excluded_features", PackedStringArray())
        var required_japanese_assets: PackedStringArray = contract.get("required_japanese_text_assets", PackedStringArray())
        assert_true(bool(contract.get("is_web_export", false)), "web builds should report is_web_export=true")
        assert_equal(String(contract.get("execution_provider_policy", "")), "cpu_only", "web builds should report a CPU-only execution policy")
        assert_false(bool(contract.get("supports_non_cpu_execution_provider", true)), "web builds should reject non-CPU execution providers")
        assert_false(bool(contract.get("supports_openjtalk_native", true)), "web builds should reject openjtalk-native")
        assert_false(bool(contract.get("supports_openjtalk_library_path", true)), "web builds should reject openjtalk-native library paths")
        assert_equal(String(contract.get("resource_source_mode", "")), "godot_file_access", "web builds should source runtime resources through FileAccess")
        assert_equal(String(contract.get("resource_path_mode", "")), "memory_fileaccess", "web builds should describe the FileAccess-backed resource mode")
        assert_equal(String(contract.get("preview_support_tier", "")), "preview", "web builds should report preview support tier")
        assert_equal(String(contract.get("phase1_minimal_synthesize_mode", "")), "en_text_cmu_dict_or_ja_text_openjtalk_dict_or_phoneme_string", "web builds should report the minimal Phase 1 synthesize mode once Japanese staged assets are present")
        assert_true(supported_frontends.has("en_text_cmu_dict"), "web builds should report the English CMU dict frontend as the Phase 1 minimal text path")
        assert_true(supported_frontends.has("ja_text_openjtalk_dict"), "web builds should report the Japanese OpenJTalk frontend once the staged dictionary is available")
        assert_false(excluded_features.has("japanese_text_input"), "web builds should no longer report Japanese text input as a Phase 1 exclusion after dictionary bootstrap support lands")
        assert_false(excluded_features.has("openjtalk_dictionary_bootstrap"), "web builds should no longer report OpenJTalk dictionary bootstrap as a Phase 1 exclusion after staged assets are supported")
        assert_true(bool(contract.get("supports_japanese_text_input", false)), "web builds should expose Japanese text input support when a staged dictionary is present")
        assert_equal(String(contract.get("openjtalk_dictionary_bootstrap_mode", "")), "staged_asset", "web builds should describe the staged OpenJTalk dictionary bootstrap mode")
        assert_true(required_japanese_assets.has("open_jtalk_dic_utf_8-1.11"), "web builds should report the staged OpenJTalk dictionary asset requirement")
        assert_false(String(contract.get("resolved_dictionary_path", "")).is_empty(), "web builds should resolve a staged OpenJTalk dictionary path")
    else:
        var native_supported_frontends: PackedStringArray = contract.get("phase1_supported_text_frontends", PackedStringArray())
        var native_excluded_features: PackedStringArray = contract.get("phase1_excluded_features", PackedStringArray())
        assert_false(bool(contract.get("is_web_export", true)), "native builds should report is_web_export=false")
        assert_equal(String(contract.get("execution_provider_policy", "")), "multi_provider", "native builds should report multi-provider execution policy")
        assert_true(bool(contract.get("supports_non_cpu_execution_provider", false)), "native builds should allow non-CPU execution providers")
        assert_true(bool(contract.get("supports_openjtalk_native", false)), "native builds should allow openjtalk-native")
        assert_true(bool(contract.get("supports_openjtalk_library_path", false)), "native builds should allow openjtalk-native library paths")
        assert_equal(String(contract.get("resource_source_mode", "")), "filesystem", "native builds should use filesystem resource loading")
        assert_equal(String(contract.get("resource_path_mode", "")), "globalize_path", "runtime contract should describe the current path strategy")
        assert_equal(String(contract.get("preview_support_tier", "")), "native", "native builds should report native support tier")
        assert_equal(String(contract.get("phase1_minimal_synthesize_mode", "")), "platform_default", "native builds should report the default non-web runtime mode")
        assert_equal(native_supported_frontends.size(), 0, "native builds should not advertise Web Phase 1 frontend restrictions")
        assert_equal(native_excluded_features.size(), 0, "native builds should not advertise Web Phase 1 exclusions")
        assert_true(bool(contract.get("supports_japanese_text_input", false)), "native builds should expose Japanese text input support")
        assert_equal(String(contract.get("openjtalk_dictionary_bootstrap_mode", "")), "filesystem", "native builds should describe filesystem dictionary bootstrap")

    _cleanup_tts(tts)

func test_runtime_contract_missing_web_dictionary() -> void:
    if not OS.has_feature("web"):
        skip("web runtime only")
        return

    var tts = _create_tts()
    if tts == null:
        skip("PiperTTS class is unavailable")
        return
    if not _require_method(tts, "get_runtime_contract"):
        _cleanup_tts(tts)
        return

    var bundle = _model_bundle()
    if bundle.is_empty():
        skip("test model bundle is not available in res://models or PIPER_TEST_* env vars")
        _cleanup_tts(tts)
        return

    tts.model_path = bundle["model_path"]
    tts.config_path = _resolve_bundle_config_path(bundle)
    tts.language_code = "ja"
    tts.dictionary_path = "res://missing/open_jtalk_dic_utf_8-1.11"

    var contract: Dictionary = tts.get_runtime_contract()
    var supported_frontends: PackedStringArray = contract.get("phase1_supported_text_frontends", PackedStringArray())
    var excluded_features: PackedStringArray = contract.get("phase1_excluded_features", PackedStringArray())

    assert_false(bool(contract.get("supports_japanese_text_input", true)), "web builds should report Japanese text input as unavailable when the staged dictionary is missing")
    assert_equal(String(contract.get("openjtalk_dictionary_bootstrap_mode", "")), "missing_required_asset", "web builds should describe missing staged dictionary assets")
    assert_equal(String(contract.get("phase1_minimal_synthesize_mode", "")), "en_text_cmu_dict_or_phoneme_string", "web builds should fall back to the English-only minimal synthesize mode when the staged dictionary is missing")
    assert_false(supported_frontends.has("ja_text_openjtalk_dict"), "web builds should not advertise the Japanese OpenJTalk frontend when the staged dictionary is missing")
    assert_true(excluded_features.has("japanese_text_input"), "web builds should report Japanese text input as excluded when the staged dictionary is missing")
    assert_true(excluded_features.has("openjtalk_dictionary_bootstrap"), "web builds should report dictionary bootstrap as excluded when the staged dictionary is missing")
    assert_true(String(contract.get("resolved_dictionary_path", "")).is_empty(), "web builds should not report a resolved dictionary path when the staged dictionary is missing")

    _cleanup_tts(tts)

func test_runtime_state() -> void:
    var tts = _create_tts()
    if tts == null:
        skip("PiperTTS class is unavailable")
        return

    if not _require_method(tts, "get_runtime_state"):
        _cleanup_tts(tts)
        return

    assert_equal(String(tts.get_runtime_state()), "uninitialized", "new PiperTTS instances should start in the uninitialized state")

    if not _configure_test_model(tts, not (_is_web_smoke() and _web_smoke_scenario() == "en")):
        skip("Bundled or environment-provided test model is unavailable")
        _cleanup_tts(tts)
        return

    assert_equal(tts.initialize(), OK, "initialize() should succeed before checking runtime_state")
    assert_equal(String(tts.get_runtime_state()), "ready", "successful initialize() should move runtime_state to ready")
    _cleanup_tts(tts)

func test_language_capabilities_without_init_is_side_effect_free() -> void:
    var tts = _create_tts()
    if tts == null:
        skip("PiperTTS class is unavailable")
        return

    if not _require_method(tts, "get_language_capabilities") or not _require_method(tts, "get_last_error"):
        _cleanup_tts(tts)
        return

    assert_true(tts.get_last_error().is_empty(), "get_last_error() should start empty before initialize()")
    var capabilities: Dictionary = tts.get_language_capabilities()
    assert_true(capabilities.is_empty(), "get_language_capabilities() should return an empty dictionary before initialize()")
    assert_true(tts.get_last_error().is_empty(), "get_language_capabilities() should not mutate get_last_error() when uninitialized")
    _cleanup_tts(tts)

func test_editor_download_catalog_paths() -> void:
    var catalog_script = load("res://addons/piper_plus/download_catalog.gd")
    assert_not_null(catalog_script, "download_catalog.gd should be loadable")
    if catalog_script == null:
        return

    var keys: PackedStringArray = catalog_script.list_model_item_keys()
    assert_true(keys.has("css10"), "download catalog should expose the css10 preset")
    var item: Dictionary = catalog_script.get_item_definition("css10")
    assert_equal(String(item.get("dest", "")), "res://piper_plus_assets/models/css10/", "download catalog should install models into the project asset root")
    assert_equal(String(item.get("legacy_dest", "")), "res://addons/piper_plus/models/css10/", "download catalog should preserve the legacy addon asset fallback")
    assert_equal(String(catalog_script.get_canonical_model_path("css10")), "res://piper_plus_assets/models/css10/css10-ja-6lang-fp16.onnx", "download catalog should expose the canonical project model path")
    var multilingual_item: Dictionary = catalog_script.get_item_definition("multilingual-test-medium")
    assert_equal(String(multilingual_item.get("recommended_dictionary_key", "")), "naist-jdic", "multilingual Web test bundles should advertise the OpenJTalk dictionary dependency")

func test_preview_controller_session_config() -> void:
    var controller_script = load("res://addons/piper_plus/preview_controller.gd")
    assert_not_null(controller_script, "preview_controller.gd should be loadable")
    if controller_script == null:
        return

    var tts = _create_tts()
    if tts == null:
        skip("PiperTTS class is unavailable")
        return

    tts.model_path = "res://piper_plus_assets/models/css10/css10-ja-6lang-fp16.onnx"
    tts.dictionary_path = "res://piper_plus_assets/dictionaries/open_jtalk_dic_utf_8-1.11"
    tts.language_code = "en"
    tts.execution_provider = ClassDB.class_get_integer_constant("PiperTTS", "EP_CPU")

    var config: Dictionary = controller_script.build_session_config(tts)
    assert_equal(String(config.get("model_path", "")), tts.model_path, "preview controller should snapshot model_path")
    assert_equal(String(config.get("dictionary_path", "")), tts.dictionary_path, "preview controller should snapshot dictionary_path")
    assert_equal(String(config.get("language_code", "")), "en", "preview controller should snapshot language_code")
    assert_equal(int(config.get("execution_provider", -1)), tts.execution_provider, "preview controller should snapshot execution_provider")
    _cleanup_tts(tts)

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
    if not _configure_test_model(tts, false):
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

func test_language_capabilities() -> void:
    var tts = _create_tts()
    if tts == null:
        skip("PiperTTS class is unavailable")
        return
    var bundle = _model_bundle()
    if bundle.is_empty():
        skip("test model bundle is not available in res://models or PIPER_TEST_* env vars")
        _cleanup_tts(tts)
        return
    if not _configure_test_model(tts, not (_is_web_smoke() and _web_smoke_scenario() == "en")):
        skip("test model bundle is not available in res://models or PIPER_TEST_* env vars")
        _cleanup_tts(tts)
        return

    assert_equal(tts.initialize(), OK, "initialize() should succeed before querying language capabilities")
    if not _require_method(tts, "get_language_capabilities") or not _require_method(tts, "get_last_error"):
        _cleanup_tts(tts)
        return

    var capabilities: Dictionary = tts.get_language_capabilities()
    var available_codes: PackedStringArray = capabilities.get("available_language_codes", PackedStringArray())
    var auto_route_codes: PackedStringArray = capabilities.get("auto_route_language_codes", PackedStringArray())
    var explicit_only_codes: PackedStringArray = capabilities.get("explicit_only_language_codes", PackedStringArray())
    var phoneme_only_codes: PackedStringArray = capabilities.get("phoneme_only_language_codes", PackedStringArray())
    var preview_codes: PackedStringArray = capabilities.get("preview_language_codes", PackedStringArray())
    var experimental_codes: PackedStringArray = capabilities.get("experimental_language_codes", PackedStringArray())
    var language_entries: Array = capabilities.get("languages", [])

    assert_true(bool(capabilities.get("has_language_id_map", false)), "get_language_capabilities() should report the multilingual model language_id_map")
    assert_true(bool(capabilities.get("supports_text_input", false)), "get_language_capabilities() should report that the multilingual model supports text input")
    assert_equal(String(capabilities.get("default_language_code", "")), "en", "get_language_capabilities() should default the multilingual model to en")

    var expected_languages := {
        "ja": {"support_tier": "preview", "frontend_backend": "openjtalk", "routing_mode": "auto", "text_supported": true, "auto_supported": true},
        "en": {"support_tier": "preview", "frontend_backend": "cmu_dict", "routing_mode": "auto", "text_supported": true, "auto_supported": true},
        "es": {"support_tier": "experimental", "frontend_backend": "rule_based", "routing_mode": "explicit_only", "text_supported": true, "auto_supported": false},
        "fr": {"support_tier": "experimental", "frontend_backend": "rule_based", "routing_mode": "explicit_only", "text_supported": true, "auto_supported": false},
        "pt": {"support_tier": "experimental", "frontend_backend": "rule_based", "routing_mode": "explicit_only", "text_supported": true, "auto_supported": false},
        "zh": {"support_tier": "phoneme_only", "frontend_backend": "raw_phoneme_only", "routing_mode": "phoneme_only", "text_supported": false, "auto_supported": false},
    }

    for language_code in expected_languages.keys():
        var expected: Dictionary = expected_languages[language_code]
        assert_true(available_codes.has(language_code), "get_language_capabilities() should list %s as an available language" % language_code)
        var entry := _matrix_row_by_code(language_entries, language_code)
        assert_false(entry.is_empty(), "get_language_capabilities().languages should include %s" % language_code)
        if entry.is_empty():
            continue
        assert_equal(String(entry.get("support_tier", "")), String(expected.get("support_tier", "")), "get_language_capabilities().languages should expose support_tier for %s" % language_code)
        assert_equal(String(entry.get("frontend_backend", "")), String(expected.get("frontend_backend", "")), "get_language_capabilities().languages should expose frontend_backend for %s" % language_code)
        assert_equal(String(entry.get("routing_mode", "")), String(expected.get("routing_mode", "")), "get_language_capabilities().languages should expose routing_mode for %s" % language_code)
        assert_equal(bool(entry.get("text_supported", false)), bool(expected.get("text_supported", false)), "get_language_capabilities().languages should expose text_supported for %s" % language_code)
        assert_equal(bool(entry.get("auto_supported", false)), bool(expected.get("auto_supported", false)), "get_language_capabilities().languages should expose auto_supported for %s" % language_code)

    for code in ["ja", "en"]:
        assert_true(auto_route_codes.has(code), "get_language_capabilities() should include %s in auto_route_language_codes" % code)
        assert_true(preview_codes.has(code), "get_language_capabilities() should include %s in preview_language_codes" % code)
        assert_true(explicit_only_codes.has(code) == false, "get_language_capabilities() should not list %s in explicit_only_language_codes" % code)
    for code in ["es", "fr", "pt"]:
        assert_true(explicit_only_codes.has(code), "get_language_capabilities() should include %s in explicit_only_language_codes" % code)
        assert_true(experimental_codes.has(code), "get_language_capabilities() should include %s in experimental_language_codes" % code)
    assert_true(phoneme_only_codes.has("zh"), "get_language_capabilities() should include zh in phoneme_only_language_codes")

    assert_true(tts.get_last_error().is_empty(), "successful get_language_capabilities() should not set last_error")
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
    assert_equal(String(inspected.get("selection_mode", "")), "language_code_base", "inspect_text() should report language_code_base for EN_us")
    assert_true(tts.get_last_error().is_empty(), "successful multilingual inspection should clear last_error")
    _cleanup_tts(tts)

func test_language_code_exact_match_selection_mode() -> void:
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

    tts.language_code = "fr"
    assert_equal(tts.initialize(), OK, "initialize() should accept exact canonical language_code values")

    if not _require_method(tts, "inspect_text") or not _require_method(tts, "get_last_synthesis_result"):
        _cleanup_tts(tts)
        return

    var inspected: Dictionary = tts.inspect_text("salut ami")
    assert_equal(String(inspected.get("resolved_language_code", "")), "fr", "inspect_text() should resolve exact language_code values")
    assert_equal(int(inspected.get("resolved_language_id", -1)), 4, "inspect_text() should resolve fr to language_id=4 for the bundled multilingual model")
    assert_equal(String(inspected.get("selection_mode", "")), "language_code_exact", "inspect_text() should report language_code_exact for canonical code matches")
    var resolved_segments: Array = inspected.get("resolved_segments", [])
    assert_true(resolved_segments.size() > 0, "inspect_text() should expose resolved_segments for multilingual routing")
    if resolved_segments.size() > 0:
        var first_segment: Dictionary = resolved_segments[0]
        assert_equal(String(first_segment.get("language_code", "")), "fr", "resolved_segments should report the routed language code")
        assert_false(bool(first_segment.get("is_phoneme_input", true)), "text routing should mark resolved_segments as text input")

    var audio = tts.synthesize("salut ami")
    assert_not_null(audio, "synthesize() should work for exact canonical language routing")
    var synth_result: Dictionary = tts.get_last_synthesis_result()
    assert_equal(String(synth_result.get("resolved_language_code", "")), "fr", "get_last_synthesis_result() should include the resolved canonical language code")
    assert_equal(int(synth_result.get("resolved_language_id", -1)), 4, "get_last_synthesis_result() should include the resolved language_id")
    assert_equal(String(synth_result.get("selection_mode", "")), "language_code_exact", "get_last_synthesis_result() should include the selection mode")
    resolved_segments = synth_result.get("resolved_segments", [])
    assert_true(resolved_segments.size() > 0, "get_last_synthesis_result() should include resolved_segments")
    _cleanup_tts(tts)

func test_multilingual_explicit_language_variants() -> void:
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

    tts.language_code = " FR_fr "
    assert_equal(tts.initialize(), OK, "initialize() should accept normalized French language_code aliases")

    if not _require_method(tts, "inspect_text") or not _require_method(tts, "get_last_error"):
        _cleanup_tts(tts)
        return
    var inspected: Dictionary = tts.inspect_text("salut ami")
    var phoneme_sentences: Array = inspected.get("phoneme_sentences", [])
    assert_true(phoneme_sentences.size() > 0, "inspect_text() should return phonemes for explicit French text routing")
    assert_equal(String(inspected.get("resolved_language_code", "")), "fr", "inspect_text() should resolve FR_fr to the canonical French language code")
    assert_equal(int(inspected.get("resolved_language_id", -1)), 4, "inspect_text() should resolve FR_fr to language_id=4 for the bundled multilingual model")
    assert_equal(String(inspected.get("selection_mode", "")), "language_code_base", "inspect_text() should report language_code_base for FR_fr")

    var audio = tts.synthesize("salut ami")
    assert_not_null(audio, "synthesize() should work for explicit French text routing")
    var synth_result: Dictionary = tts.get_last_synthesis_result()
    assert_equal(String(synth_result.get("resolved_language_code", "")), "fr", "get_last_synthesis_result() should report the canonical French code")
    assert_equal(int(synth_result.get("resolved_language_id", -1)), 4, "get_last_synthesis_result() should report language_id=4 for French routing")
    assert_equal(String(synth_result.get("selection_mode", "")), "language_code_base", "get_last_synthesis_result() should report language_code_base for FR_fr")
    _cleanup_tts(tts)

func test_multilingual_unsupported_language_rejected_for_text() -> void:
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

    assert_equal(tts.initialize(), OK, "initialize() should still succeed when no explicit language is preconfigured")

    if not _require_method(tts, "inspect_request") or not _require_method(tts, "get_last_error"):
        _cleanup_tts(tts)
        return
    var inspected: Dictionary = tts.inspect_request({
        "text": "你好",
        "language_code": "zh",
    })
    assert_true(inspected.is_empty(), "inspect_request() should reject unsupported multilingual text routing for zh")
    var last_error: Dictionary = tts.get_last_error()
    assert_equal(String(last_error.get("code", "")), "ERR_LANGUAGE_UNSUPPORTED_FOR_TEXT", "inspect_request() should record an unsupported-language error code")
    assert_equal(String(last_error.get("stage", "")), "inspect_request", "inspect_request() should record its failure stage")
    assert_true(tts.get_last_inspection_result().is_empty(), "failed multilingual inspection should not update get_last_inspection_result()")

    var audio = tts.synthesize_request({
        "text": "你好",
        "language_code": "zh",
    })
    assert_true(audio == null, "synthesize_request() should reject unsupported multilingual text routing for zh")
    last_error = tts.get_last_error()
    assert_equal(String(last_error.get("code", "")), "ERR_LANGUAGE_UNSUPPORTED_FOR_TEXT", "synthesize_request() should record an unsupported-language error code")
    assert_equal(String(last_error.get("stage", "")), "synthesize_request", "synthesize_request() should record its failure stage")
    _cleanup_tts(tts)

func test_multilingual_language_selector_conflict_rejected() -> void:
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

    assert_equal(tts.initialize(), OK, "initialize() should succeed before checking language selector conflicts")
    if not _require_method(tts, "inspect_request") or not _require_method(tts, "get_last_error"):
        _cleanup_tts(tts)
        return

    var conflict_request := {
        "text": "salut ami",
        "language_code": "fr",
        "language_id": 1,
    }
    var inspected: Dictionary = tts.inspect_request(conflict_request)
    assert_true(inspected.is_empty(), "inspect_request() should reject conflicting language_code and language_id selectors")
    var last_error: Dictionary = tts.get_last_error()
    assert_equal(String(last_error.get("code", "")), "ERR_LANGUAGE_SELECTOR_CONFLICT", "inspect_request() should expose a selector conflict error code")
    assert_equal(String(last_error.get("stage", "")), "inspect_request", "inspect_request() should report the selector conflict stage")

    var audio = tts.synthesize_request(conflict_request)
    assert_true(audio == null, "synthesize_request() should reject conflicting language selectors")
    last_error = tts.get_last_error()
    assert_equal(String(last_error.get("code", "")), "ERR_LANGUAGE_SELECTOR_CONFLICT", "synthesize_request() should expose a selector conflict error code")
    assert_equal(String(last_error.get("stage", "")), "synthesize_request", "synthesize_request() should report the selector conflict stage")
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
    if not _configure_test_model(tts, false):
        skip("test model bundle is not available in res://models or PIPER_TEST_* env vars")
        _cleanup_tts(tts)
        return

    tts.config_path = ""
    assert_equal(tts.initialize(), OK, "initialize() should resolve config_path from the model path")
    assert_true(tts.is_ready(), "PiperTTS should be ready after fallback config resolution")
    _cleanup_tts(tts)

func test_japanese_dictionary_error_surface() -> void:
    var tts = _create_tts()
    if tts == null:
        skip("PiperTTS class is unavailable")
        return
    var bundle = _model_bundle()
    if bundle.is_empty():
        skip("test model bundle is not available in res://models or PIPER_TEST_* env vars")
        _cleanup_tts(tts)
        return
    if not _require_method(tts, "get_last_error"):
        _cleanup_tts(tts)
        return

    tts.model_path = bundle["model_path"]
    tts.config_path = _resolve_bundle_config_path(bundle)
    tts.language_code = "ja"
    tts.dictionary_path = "res://missing/open_jtalk_dic_utf_8-1.11"

    assert_equal(tts.initialize(), ERR_UNCONFIGURED, "initialize() should reject Japanese text input when the OpenJTalk dictionary is missing")
    var last_error: Dictionary = tts.get_last_error()
    assert_equal(String(last_error.get("code", "")), "ERR_OPENJTALK_DICTIONARY_NOT_READY", "missing Japanese dictionary should produce a machine-readable error code")
    assert_equal(String(last_error.get("stage", "")), "initialize", "missing Japanese dictionary should report initialize as the failing stage")
    assert_equal(String(last_error.get("resolved_language_code", "")), "ja", "missing Japanese dictionary should preserve the resolved Japanese language code")
    _cleanup_tts(tts)

func test_japanese_request_time_dictionary_error_surface() -> void:
    var tts = _create_tts()
    if tts == null:
        skip("PiperTTS class is unavailable")
        return
    var bundle = _model_bundle()
    if bundle.is_empty():
        skip("test model bundle is not available in res://models or PIPER_TEST_* env vars")
        _cleanup_tts(tts)
        return
    if not _require_method(tts, "get_last_error"):
        _cleanup_tts(tts)
        return

    tts.model_path = bundle["model_path"]
    tts.config_path = _resolve_bundle_config_path(bundle)
    tts.language_code = "en"
    tts.dictionary_path = "res://missing/open_jtalk_dic_utf_8-1.11"

    assert_equal(tts.initialize(), OK, "initialize() should still succeed when the active language is English")

    var inspect_request := {
        "text": DEFAULT_JA_TEST_TEXT,
        "language_code": "ja",
    }
    var inspected: Dictionary = tts.inspect_request(inspect_request)
    assert_true(inspected.is_empty(), "inspect_request() should reject Japanese text when the OpenJTalk dictionary is missing")
    var inspect_error: Dictionary = tts.get_last_error()
    assert_equal(String(inspect_error.get("code", "")), "ERR_OPENJTALK_DICTIONARY_NOT_READY", "inspect_request() should expose a machine-readable missing dictionary error")
    assert_equal(String(inspect_error.get("stage", "")), "inspect_request", "inspect_request() should report the failing stage")
    assert_equal(String(inspect_error.get("resolved_language_code", "")), "ja", "inspect_request() should preserve the resolved Japanese language code")

    var audio = tts.synthesize_request(inspect_request)
    assert_true(audio == null, "synthesize_request() should reject Japanese text when the OpenJTalk dictionary is missing")
    var synth_error: Dictionary = tts.get_last_error()
    assert_equal(String(synth_error.get("code", "")), "ERR_OPENJTALK_DICTIONARY_NOT_READY", "synthesize_request() should expose a machine-readable missing dictionary error")
    assert_equal(String(synth_error.get("stage", "")), "synthesize_request", "synthesize_request() should report the failing stage")
    assert_equal(String(synth_error.get("resolved_language_code", "")), "ja", "synthesize_request() should preserve the resolved Japanese language code")
    _cleanup_tts(tts)

func test_japanese_text_input_with_dictionary() -> void:
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
        skip("compiled OpenJTalk dictionary is not available for Japanese text input test")
        _cleanup_tts(tts)
        return

    tts.model_path = bundle["model_path"]
    tts.config_path = _resolve_bundle_config_path(bundle)
    tts.dictionary_path = bundle["dictionary_path"]
    tts.language_code = "ja"

    assert_equal(tts.initialize(), OK, "initialize() should succeed for Japanese text input when the OpenJTalk dictionary is staged")

    var inspected: Dictionary = tts.inspect_text(DEFAULT_JA_TEST_TEXT)
    var phoneme_sentences: Array = inspected.get("phoneme_sentences", [])
    assert_true(phoneme_sentences.size() > 0, "inspect_text() should resolve Japanese phonemes when the staged dictionary is available")
    assert_equal(String(inspected.get("resolved_language_code", "")), "ja", "inspect_text() should report Japanese as the resolved language")

    var audio = tts.synthesize(DEFAULT_JA_TEST_TEXT)
    assert_not_null(audio, "synthesize() should produce audio for Japanese text input when the staged dictionary is available")
    var synth_result: Dictionary = tts.get_last_synthesis_result()
    assert_equal(String(synth_result.get("resolved_language_code", "")), "ja", "synthesize() should report Japanese as the resolved language")
    _cleanup_tts(tts)

func test_web_non_cpu_execution_provider_rejected() -> void:
    if not _is_web_runtime():
        skip("web runtime only")
        return

    var tts = _create_tts()
    if tts == null:
        skip("PiperTTS class is unavailable")
        return
    if not _configure_test_model(tts, false):
        skip("test model bundle is not available in res://models or PIPER_TEST_* env vars")
        _cleanup_tts(tts)
        return
    if not _require_method(tts, "get_last_error"):
        _cleanup_tts(tts)
        return

    tts.execution_provider = ClassDB.class_get_integer_constant("PiperTTS", "EP_CUDA")
    assert_equal(tts.initialize(), ERR_UNAVAILABLE, "Web initialize() should reject non-CPU execution providers")
    var last_error: Dictionary = tts.get_last_error()
    assert_equal(String(last_error.get("code", "")), "ERR_UNSUPPORTED_EXECUTION_PROVIDER", "Web initialize() should expose an unsupported execution provider error code")
    assert_equal(String(last_error.get("stage", "")), "initialize", "Web initialize() should report the failure stage")
    _cleanup_tts(tts)

func test_web_openjtalk_native_rejected() -> void:
    if not _is_web_runtime():
        skip("web runtime only")
        return

    var tts = _create_tts()
    if tts == null:
        skip("PiperTTS class is unavailable")
        return
    if not _configure_test_model(tts, false):
        skip("test model bundle is not available in res://models or PIPER_TEST_* env vars")
        _cleanup_tts(tts)
        return
    if not _require_method(tts, "get_last_error"):
        _cleanup_tts(tts)
        return

    tts.openjtalk_library_path = "res://addons/piper_plus/bin/openjtalk_native.js"
    assert_equal(tts.initialize(), ERR_UNAVAILABLE, "Web initialize() should reject openjtalk-native shared libraries")
    var last_error: Dictionary = tts.get_last_error()
    assert_equal(String(last_error.get("code", "")), "ERR_OPENJTALK_NATIVE_UNSUPPORTED", "Web initialize() should expose an unsupported openjtalk-native error code")
    assert_equal(String(last_error.get("stage", "")), "initialize", "Web initialize() should report the failure stage")
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
    var resolved_segments: Array = inspected.get("resolved_segments", [])
    assert_equal(resolved_segments.size(), 1, "inspect_request() should expose a single resolved segment for raw phoneme input")
    if resolved_segments.size() == 1:
        var first_segment: Dictionary = resolved_segments[0]
        assert_true(bool(first_segment.get("is_phoneme_input", false)), "raw phoneme inspection should mark resolved_segments as phoneme input")

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
    if not _configure_test_model(tts, false):
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
    if _require_method(tts, "get_runtime_state"):
        assert_equal(String(tts.get_runtime_state()), "busy", "synthesize_async() should move runtime_state to busy while work is in flight")

    var deadline = Time.get_ticks_msec() + 15000
    while _async_completed_audio == null and _async_failed_error.is_empty() and Time.get_ticks_msec() < deadline:
        await Engine.get_main_loop().process_frame

    if _async_completed_audio == null and _async_failed_error.is_empty():
        await Engine.get_main_loop().process_frame

    assert_true(_async_failed_error.is_empty(), "synthesize_async() should not emit synthesis_failed")
    assert_not_null(_async_completed_audio, "synthesize_async() should emit synthesis_completed")
    if _require_method(tts, "get_runtime_state"):
        assert_equal(String(tts.get_runtime_state()), "ready", "completed synthesize_async() should restore runtime_state to ready")
    _cleanup_tts(tts)

func test_synthesize_async_request() -> void:
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
        failures.append("initialize() failed for synthesize_async_request")
        _cleanup_tts(tts)
        return

    if not _require_method(tts, "synthesize_async_request") or not _require_method(tts, "get_last_synthesis_result"):
        _cleanup_tts(tts)
        return

    _async_completed_audio = null
    _async_failed_error = ""
    tts.synthesis_completed.connect(_on_synthesis_completed_for_test)
    tts.synthesis_failed.connect(_on_synthesis_failed_for_test)

    assert_equal(tts.synthesize_async_request({
        "phoneme_string": "a i",
        "language_code": "en",
    }), OK, "synthesize_async_request() should start successfully for raw phoneme input")

    var deadline = Time.get_ticks_msec() + 15000
    while _async_completed_audio == null and _async_failed_error.is_empty() and Time.get_ticks_msec() < deadline:
        await Engine.get_main_loop().process_frame

    if _async_completed_audio == null and _async_failed_error.is_empty():
        await Engine.get_main_loop().process_frame

    assert_true(_async_failed_error.is_empty(), "synthesize_async_request() should not emit synthesis_failed")
    assert_not_null(_async_completed_audio, "synthesize_async_request() should emit synthesis_completed")
    var synth_result: Dictionary = tts.get_last_synthesis_result()
    assert_equal(String(synth_result.get("input_mode", "")), "phoneme_string", "synthesize_async_request() should preserve phoneme_string input_mode")
    var resolved_segments: Array = synth_result.get("resolved_segments", [])
    assert_true(resolved_segments.size() > 0, "synthesize_async_request() should include resolved_segments")
    if resolved_segments.size() > 0:
        var first_segment: Dictionary = resolved_segments[0]
        assert_true(bool(first_segment.get("is_phoneme_input", false)), "raw phoneme async synthesis should mark resolved_segments as phoneme input")
    _cleanup_tts(tts)

func test_synthesize_streaming_request() -> void:
    var tts = _create_tts()
    if tts == null:
        skip("PiperTTS class is unavailable")
        return
    var tree := Engine.get_main_loop() as SceneTree
    if tree == null or tree.root == null:
        skip("SceneTree is unavailable")
        _cleanup_tts(tts)
        return
    tree.root.add_child(tts)
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
        failures.append("initialize() failed for synthesize_streaming_request")
        _cleanup_tts(tts)
        return

    if not _require_method(tts, "synthesize_streaming_request") or not _require_method(tts, "get_last_synthesis_result") or not _require_method(tts, "get_last_error"):
        _cleanup_tts(tts)
        return

    var playback_setup := await _create_streaming_playback()
    if playback_setup.is_empty():
        skip("AudioStreamGeneratorPlayback is unavailable in this headless environment")
        _cleanup_tts(tts)
        return

    if tree != null and tree.root != null and tts.get_parent() == null:
        tree.root.add_child(tts)

    _streaming_completed = false
    tts.streaming_ended.connect(_on_streaming_ended_for_test)

    var request := {
        "text": "hello from godot",
        "language_code": "en",
    }
    assert_equal(tts.synthesize_streaming_request(request, playback_setup["playback"]), OK, "synthesize_streaming_request() should start successfully")
    assert_true(tts.get_last_error().is_empty(), "synthesize_streaming_request() should clear last_error after starting successfully")
    if _require_method(tts, "get_runtime_state"):
        assert_equal(String(tts.get_runtime_state()), "busy", "streaming synthesis should move runtime_state to busy while work is in flight")

    var deadline = Time.get_ticks_msec() + 15000
    while not _streaming_completed and Time.get_ticks_msec() < deadline:
        await tree.process_frame

    assert_true(_streaming_completed, "synthesize_streaming_request() should emit streaming_ended")
    var synth_result: Dictionary = tts.get_last_synthesis_result()
    assert_equal(String(synth_result.get("input_mode", "")), "text", "streaming synthesis should record text input_mode")
    assert_equal(String(synth_result.get("resolved_language_code", "")), "en", "streaming synthesis should expose the resolved language code")
    assert_equal(int(synth_result.get("resolved_language_id", -1)), 1, "streaming synthesis should expose the resolved language_id")
    assert_equal(String(synth_result.get("selection_mode", "")), "language_code_exact", "streaming synthesis should expose the selection mode")
    assert_true((synth_result.get("resolved_segments", []) as Array).size() > 0, "streaming synthesis should expose resolved_segments")
    assert_true(tts.get_last_error().is_empty(), "completed streaming synthesis should leave last_error empty")
    if _require_method(tts, "get_runtime_state"):
        assert_equal(String(tts.get_runtime_state()), "ready", "completed streaming synthesis should restore runtime_state to ready")

    var container = playback_setup.get("container")
    if container != null and is_instance_valid(container):
        container.queue_free()
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
