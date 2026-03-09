extends "res://test_base.gd"

const DEFAULT_TEST_TEXT := "こんにちは"
const BUNDLED_MODEL_PATH := "res://models/ja_JP-test-medium.onnx"
const BUNDLED_CONFIG_PATH := "res://models/ja_JP-test-medium.onnx.json"
const BUNDLED_DICT_PATH := "res://models/openjtalk_dic"

func _addon_available() -> bool:
    return ClassDB.class_exists("PiperTTS")

func _create_tts():
    if not _addon_available():
        return null
    var tts = ClassDB.instantiate("PiperTTS")
    if tts == null:
        return null
    Engine.get_main_loop().root.add_child(tts)
    return tts

func _cleanup_tts(tts) -> void:
    if is_instance_valid(tts):
        tts.stop()
        tts.queue_free()
        await Engine.get_main_loop().process_frame

func _model_bundle() -> Dictionary:
    if FileAccess.file_exists(BUNDLED_MODEL_PATH) and FileAccess.file_exists(BUNDLED_CONFIG_PATH):
        var bundled := {
            "model_path": BUNDLED_MODEL_PATH,
            "config_path": BUNDLED_CONFIG_PATH,
            "dictionary_path": "",
        }
        if DirAccess.dir_exists_absolute(ProjectSettings.globalize_path(BUNDLED_DICT_PATH)):
            bundled["dictionary_path"] = BUNDLED_DICT_PATH
        return bundled

    var model_path := OS.get_environment("PIPER_TEST_MODEL_PATH")
    var config_path := OS.get_environment("PIPER_TEST_CONFIG_PATH")
    var dict_path := OS.get_environment("PIPER_TEST_DICT_PATH")

    if model_path.is_empty():
        return {}

    if config_path.is_empty():
        config_path = "%s.json" % model_path

    return {
        "model_path": model_path,
        "config_path": config_path,
        "dictionary_path": dict_path,
    }

func _configure_test_model(tts) -> bool:
    var bundle := _model_bundle()
    if bundle.is_empty():
        return false

    if not FileAccess.file_exists(bundle["model_path"]):
        return false

    if not FileAccess.file_exists(bundle["config_path"]):
        return false

    tts.model_path = bundle["model_path"]
    tts.config_path = bundle["config_path"]
    if not String(bundle["dictionary_path"]).is_empty():
        tts.dictionary_path = bundle["dictionary_path"]

    return true

func _expected_sample_rate(bundle: Dictionary) -> int:
    var text := FileAccess.get_file_as_string(bundle["config_path"])
    if text.is_empty():
        return 22050

    var parsed = JSON.parse_string(text)
    if typeof(parsed) != TYPE_DICTIONARY:
        return 22050

    var audio := parsed.get("audio", {})
    if typeof(audio) == TYPE_DICTIONARY:
        return int(audio.get("sample_rate", 22050))

    return 22050

func test_node_creation() -> void:
    var tts := _create_tts()
    if tts == null:
        skip("PiperTTS class is unavailable")
        return
    assert_not_null(tts, "PiperTTS node should be creatable")
    await _cleanup_tts(tts)

func test_properties() -> void:
    var tts := _create_tts()
    if tts == null:
        skip("PiperTTS class is unavailable")
        return
    tts.model_path = "res://voice.onnx"
    tts.config_path = "res://voice.onnx.json"
    tts.dictionary_path = "res://dict"
    tts.speaker_id = 3
    tts.noise_scale = 1.2
    tts.noise_w = 0.6

    assert_equal(tts.model_path, "res://voice.onnx", "model_path should round-trip")
    assert_equal(tts.config_path, "res://voice.onnx.json", "config_path should round-trip")
    assert_equal(tts.dictionary_path, "res://dict", "dictionary_path should round-trip")
    assert_equal(tts.speaker_id, 3, "speaker_id should round-trip")
    assert_equal(tts.noise_scale, 1.2, "noise_scale should round-trip")
    assert_equal(tts.noise_w, 0.6, "noise_w should round-trip")
    await _cleanup_tts(tts)

func test_speech_rate_range() -> void:
    var tts := _create_tts()
    if tts == null:
        skip("PiperTTS class is unavailable")
        return
    tts.speech_rate = 0.01
    assert_equal(tts.speech_rate, 0.1, "speech_rate should clamp to the minimum")
    tts.speech_rate = 9.0
    assert_equal(tts.speech_rate, 5.0, "speech_rate should clamp to the maximum")
    await _cleanup_tts(tts)

func test_execution_provider_enum() -> void:
    if not _addon_available():
        skip("PiperTTS class is unavailable")
        return
    assert_equal(ClassDB.class_get_integer_constant("PiperTTS", "EP_CPU"), 0, "EP_CPU should match the bound enum")
    assert_equal(ClassDB.class_get_integer_constant("PiperTTS", "EP_COREML"), 1, "EP_COREML should match the bound enum")
    assert_equal(ClassDB.class_get_integer_constant("PiperTTS", "EP_DIRECTML"), 2, "EP_DIRECTML should match the bound enum")
    assert_equal(ClassDB.class_get_integer_constant("PiperTTS", "EP_NNAPI"), 3, "EP_NNAPI should match the bound enum")
    assert_equal(ClassDB.class_get_integer_constant("PiperTTS", "EP_AUTO"), 4, "EP_AUTO should match the bound enum")

func test_initialize_without_model() -> void:
    var tts := _create_tts()
    if tts == null:
        skip("PiperTTS class is unavailable")
        return
    assert_equal(tts.initialize(), ERR_UNCONFIGURED, "initialize() should reject missing model_path")
    await _cleanup_tts(tts)

func test_synthesize_without_init() -> void:
    var tts := _create_tts()
    if tts == null:
        skip("PiperTTS class is unavailable")
        return
    var audio = tts.synthesize(DEFAULT_TEST_TEXT)
    assert_true(audio == null, "synthesize() should fail before initialize()")
    await _cleanup_tts(tts)

func test_synthesize_async_without_init() -> void:
    var tts := _create_tts()
    if tts == null:
        skip("PiperTTS class is unavailable")
        return
    assert_equal(tts.synthesize_async(DEFAULT_TEST_TEXT), ERR_UNCONFIGURED, "synthesize_async() should fail before initialize()")
    await _cleanup_tts(tts)

func test_is_ready_default() -> void:
    var tts := _create_tts()
    if tts == null:
        skip("PiperTTS class is unavailable")
        return
    assert_false(tts.is_ready(), "PiperTTS should start in a not-ready state")
    await _cleanup_tts(tts)

func test_is_processing_default() -> void:
    var tts := _create_tts()
    if tts == null:
        skip("PiperTTS class is unavailable")
        return
    assert_false(tts.is_processing(), "PiperTTS should start in an idle state")
    await _cleanup_tts(tts)

func test_initialize_with_model() -> void:
    var tts := _create_tts()
    if tts == null:
        skip("PiperTTS class is unavailable")
        return
    if not _configure_test_model(tts):
        skip("test model bundle is not available in res://models or PIPER_TEST_* env vars")
        await _cleanup_tts(tts)
        return

    assert_equal(tts.initialize(), OK, "initialize() should succeed with a valid model bundle")
    assert_true(tts.is_ready(), "PiperTTS should be ready after initialize()")
    await _cleanup_tts(tts)

func test_synthesize_basic() -> void:
    var tts := _create_tts()
    if tts == null:
        skip("PiperTTS class is unavailable")
        return
    if not _configure_test_model(tts):
        skip("test model bundle is not available in res://models or PIPER_TEST_* env vars")
        await _cleanup_tts(tts)
        return

    if tts.initialize() != OK:
        failures.append("initialize() failed for synthesize_basic")
        await _cleanup_tts(tts)
        return

    var audio = tts.synthesize(DEFAULT_TEST_TEXT)
    assert_not_null(audio, "synthesize() should return AudioStreamWAV")
    if audio != null:
        assert_true(audio.data.size() > 0, "AudioStreamWAV should contain PCM data")
    await _cleanup_tts(tts)

func test_synthesize_async() -> void:
    var tts := _create_tts()
    if tts == null:
        skip("PiperTTS class is unavailable")
        return
    if not _configure_test_model(tts):
        skip("test model bundle is not available in res://models or PIPER_TEST_* env vars")
        await _cleanup_tts(tts)
        return

    if tts.initialize() != OK:
        failures.append("initialize() failed for synthesize_async")
        await _cleanup_tts(tts)
        return

    var completed_audio = null
    var failed_error := ""
    tts.synthesis_completed.connect(func(audio): completed_audio = audio)
    tts.synthesis_failed.connect(func(error): failed_error = error)

    assert_equal(tts.synthesize_async(DEFAULT_TEST_TEXT), OK, "synthesize_async() should start successfully")

    var deadline := Time.get_ticks_msec() + 5000
    while completed_audio == null and failed_error.is_empty() and Time.get_ticks_msec() < deadline:
        await Engine.get_main_loop().process_frame

    assert_true(failed_error.is_empty(), "synthesize_async() should not emit synthesis_failed")
    assert_not_null(completed_audio, "synthesize_async() should emit synthesis_completed")
    await _cleanup_tts(tts)

func test_audio_stream_format() -> void:
    var tts := _create_tts()
    if tts == null:
        skip("PiperTTS class is unavailable")
        return
    if not _configure_test_model(tts):
        skip("test model bundle is not available in res://models or PIPER_TEST_* env vars")
        await _cleanup_tts(tts)
        return

    var bundle := _model_bundle()
    var expected_rate := _expected_sample_rate(bundle)
    if tts.initialize() != OK:
        failures.append("initialize() failed for audio_stream_format")
        await _cleanup_tts(tts)
        return

    var audio = tts.synthesize(DEFAULT_TEST_TEXT)
    assert_not_null(audio, "synthesize() should return AudioStreamWAV")
    if audio != null:
        assert_equal(audio.format, AudioStreamWAV.FORMAT_16_BITS, "Audio should be 16-bit PCM")
        assert_equal(audio.mix_rate, expected_rate, "Audio mix rate should match the model config")
        assert_false(audio.stereo, "Audio should be mono")
        assert_true(audio.data.size() > 0, "Audio data should not be empty")
    await _cleanup_tts(tts)
